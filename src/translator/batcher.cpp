#include "batcher.h"
#include "batch.h"
#include "common/logging.h"
#include <cassert>

namespace marian {
namespace bergamot {

Batcher::Batcher(Ptr<Options> options) : exhaustConfig_(options) {
  miniBatchWords = options->get<int>("mini-batch-words");
  bucket_.resize(options->get<int>("max-length-break") + 1);
  ABORT_IF(bucket_.size() - 1 > miniBatchWords,
           "Fatal: max-length-break > mini-batch-words  will lead to sentences "
           "longer than what can fit in a batch.");

  // Configure exhaust mode config
}

void Batcher::addSentenceWithPriority(RequestSentence &sentence) {
  size_t bucket_id = sentence.numTokens();
  assert(bucket_id < bucket_.size());
  bucket_[bucket_id].insert(sentence);
}

bool Batcher::operator>>(Batch &batch) {
  if (exhaustConfig_.active()) {
    return nextRandomBatch(batch);
  } else {
    return cleaveBatch(batch);
  }
}

bool Batcher::cleaveBatch(Batch &batch) {
  // For now simply iterates on buckets and converts batches greedily.  This
  // has to be enhanced with optimizing over priority. The baseline
  // implementation should at least be as fast as marian's maxi-batch with full
  // corpus size as maxi-batch size.
  batch.clear();
  size_t paddedBatchSize = 0;

  for (size_t length = 0; length < bucket_.size(); length++) {
    auto p = bucket_[length].begin();
    while (p != bucket_[length].end()) {
      paddedBatchSize = (batch.size() + 1) * length;
      if (paddedBatchSize <= miniBatchWords) {
        auto q = p++;
        batch.add(*q);
        bucket_[length].erase(q);
      } else {
        // Check if elements exist
        assert(batch.size() > 0);
        return true;
      }
    }
  }

  bool isValidBatch = batch.size() > 0;
  return isValidBatch;
}

bool Batcher::nextRandomBatch(Batch &batchOut) {
  std::pair<size_t, size_t> batchInfo;

  while (exhaustConfig_.next(batchInfo)) {
    // Generate batch
    size_t batchSize = batchInfo.first;
    size_t seqLen = batchInfo.second;
    size_t available = bucket_[seqLen].size();

    // Only go forward if at least a sentence is available with this size, this
    // way at least by duplication generation of a batchSize x seqLen batch is
    // possible.
    if (available > 0) {
      Batch batch;

      // Generate a uniform batchSize number of indices in [0, available-1]
      std::uniform_int_distribution<size_t> dist(0, available - 1);
      static std::random_device rd;
      static std::mt19937 gen(rd());

      std::vector<size_t> idxs;
      for (size_t b = 0; b < batchSize; b++) {
        idxs.push_back(dist(gen));
      }

      // Sort and iterate the set container in O(N).
      std::sort(idxs.begin(), idxs.end());

      size_t sentenceIdx = 0;
      auto sentencePtr = bucket_[seqLen].begin();

      for (auto &idx : idxs) {
        // idxs are sorted. Increment sentenceIdx until idx is reached,
        // simultaneously incrementing sentencePtr.
        while (sentenceIdx < idx) {
          ++sentenceIdx;
          ++sentencePtr;
          ABORT_IF(
              sentencePtr == bucket_[seqLen].end(),
              "Somehow we have reached the end of container, this is illegal.");
        }

        // An ungodly amount of asserts to convert a possible race condition
        // into an ABORT.
        ABORT_IF(idx >= bucket_[seqLen].size(),
                 "idx out of bounds. Something's wrong!");
        ABORT_IF(idx != sentenceIdx, "idx != sentenceIdx. Something's wrong!");
        ABORT_IF(sentencePtr->numTokens() > seqLen,
                 "sentencePtr->numTokens() > seqLen. Something's wrong!");

        batch.add(*sentencePtr);
      }

      // LOG(info, "(idxs, seqLen) = ({}, {}), (eB, eT) = ({}, {})",
      // idxs.size(), seqLen,
      //     batch.size(), batch.maxLength());
      ABORT_IF(batch.size() != idxs.size(),
               "Something's off, check above block!");
      ABORT_IF(batch.maxLength() != seqLen,
               "Something's off, check above block!");

      batch.log();

      // Once complete assign the batch. This reduces the race condition to run
      // longer, but it might still exist.  TODO(jerinphilip): inspect.
      batchOut = batch;
      return true;
    }
  }
  return false;
}

void Batcher::addWholeRequest(Ptr<Request> request) {
  for (size_t i = 0; i < request->numSegments(); i++) {
    RequestSentence requestSentence(i, request);
    addSentenceWithPriority(requestSentence);
  }
}

ExhaustModeConfig::ExhaustModeConfig(Ptr<Options> options)
    : active_(options->get<bool>("exhaust-mode")), activeIdx_(0),
      samples_(options->get<int>("exhaust-mode-samples")) {
  if (active_) {
    // Offline create entries of (b, t) from mini-batch-words and
    // max-length-break
    size_t slack = options->get<int>("exhaust-mode-slack");
    size_t maxBatchSize = options->get<int>("mini-batch-words");
    size_t maxSeqLen = options->get<int>("max-length-break");
    for (size_t batchSize = 1; batchSize < maxBatchSize; batchSize++) {
      size_t maxSeqLenBound = maxBatchSize / (batchSize + 1);
      size_t safemaxSeqLenBound =
          std::min<size_t>(maxSeqLenBound + slack, maxSeqLen);
      for (size_t seqLen = 1; seqLen < safemaxSeqLenBound; seqLen++) {
        sampleInfos_.insert(
            sampleInfos_.end(),
            samples_, // samples_ number of (batchSize, seqLen) entries
            std::make_pair(batchSize, seqLen));
      }
    }

    // Random shuffle to remove patterned access
    std::random_shuffle(sampleInfos_.begin(), sampleInfos_.end());
  }
}

bool ExhaustModeConfig::next(std::pair<size_t, size_t> &batchEntry) {
  if (activeIdx_ >= sampleInfos_.size()) {
    return false;
  } else {
    batchEntry = sampleInfos_[activeIdx_];
    activeIdx_++;
    return true;
  }
}

} // namespace bergamot
} // namespace marian
