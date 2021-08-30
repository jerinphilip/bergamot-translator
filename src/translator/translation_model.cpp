#include "translation_model.h"

#include "batch.h"
#include "byte_array_util.h"
#include "common/logging.h"
#include "data/corpus.h"
#include "data/text_input.h"
#include "parser.h"
#include "translator/beam_search.h"

namespace marian {
namespace bergamot {

ReusableMarianBackend::ReusableMarianBackend(const Ptr<Options> &options, size_t deviceId)
    : device_(deviceId, DeviceType::cpu) {
  graph_ = New<ExpressionGraph>(true);  // set the graph to be inference only
  auto prec = options->get<std::vector<std::string>>("precision", {"float32"});
  graph_->setDefaultElementType(typeFromString(prec[0]));

  graph_->setDevice(device_);
  graph_->getBackend()->configureDevice(options);

  size_t workspaceSize = options->get<size_t>("workspace");
  graph_->reserveWorkspaceMB(workspaceSize);
}

void ReusableMarianBackend::translateBatch(Ptr<TranslationModel> model, Batch &batch) {
  /// TODO(jerinphilip): Optimize swaps properly.
  for (auto &scorer : model->scorerEnsemble_) {
    scorer->init(graph_);
    if (model->shortlistGenerator_) {
      scorer->setShortlistGenerator(model->shortListGenerator_);
    }
  }

  graph_->forward();

  // Swap stuff onto graph from model.

  auto search = New<BeamSearch>(model->options_, model->scorerEnsemble_, (model->vocabs_).target());
  auto histories = search->search(graph_, convertToMarianBatch(batch, model->vocabs_));

  batch.completeBatch(histories);
}

TranslationModel::TranslationModel(const Config &options, size_t replicas, MemoryBundle &&memory /*=MemoryBundle{}*/)
    : options_(options),
      memory_(std::move(memory)),
      vocabs_(options, std::move(memory_.vocabs)),
      textProcessor_(options, vocabs_, std::move(memory_.ssplitPrefixFile)),
      batchingPool_(options) {
  ABORT_IF(replicas == 0, "At least one replica needs to be created.");

  auto notEmpty = [](const AlignedMemory &memory) { return memory.size() > 0 && memory.begin() != nullptr; };

  // Load and retain shortlist
  int srcIdx = 0, trgIdx = 1;
  bool check = options_->get<bool>("check-bytearray", false);
  if (notEmpty(memory_.shortlist)) {
    shortlistGenerator_ = New<data::BinaryShortlistGenerator>(memory_.shortlist.begin(), memory_.shortlist.size(),
                                                              vocabs_.sources().front(), vocabs_.target(), srcIdx,
                                                              trgIdx, vocabs.isShared(), check);
  } else {
    ABORT_IF(!options_->hasAndNotEmpty("shortlist"), "Require at least a shortlist provided from command-line");
    shortlistGenerator_ = New<data::BinaryShortlistGenerator>(options_, vocabs_.sources().front(), vocabs_.target(),
                                                              srcIdx, trgIdx, vocabs_.isShared());
  }

  // Load and keep scorer(s).
  if (notEmpty(memory_.model)) {
    // If we have provided a byte array that contains the model memory, we can initialise the
    // model from there, as opposed to from reading in the config file
    ABORT_IF((uintptr_t)memory_.model.begin() % 256 != 0,
             "The provided memory_.model is not aligned to 256 bytes and will crash when vector instructions are used "
             "on it.");
    if (options_->get<bool>("check-bytearray", false)) {
      ABORT_IF(!validateBinaryModel(memory_.model, memory_.model.size()),
               "The binary file is invalid. Incomplete or corrupted download?");
    }

    // Marian supports multiple models initialised in this manner hence std::vector.
    // However we will only ever use 1 during decoding.
    const std::vector<const void *> container = {memory_.model.begin()};
    scorerEnsemble_ = createScorers(options_, container);
  } else {
    scorerEnsemble_ = createScorers(options_);
  }
}

// Make request process is shared between Async and Blocking workflow of translating.
Ptr<Request> TranslationModel::makeRequest(size_t requestId, std::string &&source, CallbackType callback,
                                           const ResponseOptions &responseOptions) {
  Segments segments;
  AnnotatedText annotatedSource;

  textProcessor_.process(std::move(source), annotatedSource, segments);
  ResponseBuilder responseBuilder(responseOptions, std::move(annotatedSource), vocabs_, callback);

  Ptr<Request> request = New<Request>(requestId, std::move(segments), std::move(responseBuilder));
  return request;
}

Ptr<marian::data::CorpusBatch> convertToMarianBatch(Batch &batch, const Vocabs &vocabs) {
  std::vector<data::SentenceTuple> batchVector;
  auto &sentences = batch.sentences();

  size_t batchSequenceNumber{0};
  for (auto &sentence : sentences) {
    data::SentenceTuple sentence_tuple(batchSequenceNumber);
    Segment segment = sentence.getUnderlyingSegment();
    sentence_tuple.push_back(segment);
    batchVector.push_back(sentence_tuple);

    ++batchSequenceNumber;
  }

  // Usually one would expect inputs to be [B x T], where B = batch-size and T = max seq-len among the sentences in the
  // batch. However, marian's library supports multi-source and ensembling through different source-vocabulary but same
  // target vocabulary. This means the inputs are 3 dimensional when converted into marian's library formatted batches.
  //
  // Consequently B x T projects to N x B x T, where N = ensemble size. This adaptation does not fully force the idea of
  // N = 1 (the code remains general, but N iterates only from 0-1 in the nested loop).

  size_t batchSize = batchVector.size();

  std::vector<size_t> sentenceIds;
  std::vector<int> maxDims;

  for (auto &example : batchVector) {
    if (maxDims.size() < example.size()) {
      maxDims.resize(example.size(), 0);
    }
    for (size_t i = 0; i < example.size(); ++i) {
      if (example[i].size() > static_cast<size_t>(maxDims[i])) {
        maxDims[i] = static_cast<int>(example[i].size());
      }
    }
    sentenceIds.push_back(example.getId());
  }

  using SubBatch = marian::data::SubBatch;
  std::vector<Ptr<SubBatch>> subBatches;
  for (size_t j = 0; j < maxDims.size(); ++j) {
    subBatches.emplace_back(New<SubBatch>(batchSize, maxDims[j], vocabs.sources().at(j)));
  }

  std::vector<size_t> words(maxDims.size(), 0);
  for (size_t i = 0; i < batchSize; ++i) {
    for (size_t j = 0; j < maxDims.size(); ++j) {
      for (size_t k = 0; k < batchVector[i][j].size(); ++k) {
        subBatches[j]->data()[k * batchSize + i] = batchVector[i][j][k];
        subBatches[j]->mask()[k * batchSize + i] = 1.f;
        words[j]++;
      }
    }
  }

  for (size_t j = 0; j < maxDims.size(); ++j) {
    subBatches[j]->setWords(words[j]);
  }

  using CorpusBatch = marian::data::CorpusBatch;
  Ptr<CorpusBatch> corpusBatch = New<CorpusBatch>(subBatches);
  corpusBatch->setSentenceIds(sentenceIds);
  return corpusBatch;
}

}  // namespace bergamot
}  // namespace marian
