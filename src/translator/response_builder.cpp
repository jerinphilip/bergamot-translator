#include "response_builder.h"

#include "response_options.h"

namespace marian {
namespace bergamot {

void ResponseBuilder::buildQualityScores(Histories &histories, Response &response) {
  qualityEstimator_.computeQualityScores(histories, response);
}

void ResponseBuilder::buildAlignments(Histories &histories, Response &response) {
  for (auto &history : histories) {
    // TODO(jerin): Change hardcode of nBest = 1
    NBestList onebest = history->nBest(1);

    Result result = onebest[0];  // Expecting only one result;
    Words words = std::get<0>(result);
    auto hyp = std::get<1>(result);
    auto softAlignment = hyp->tracebackAlignment();
    response.alignments.push_back(softAlignment);
  }
}

void ResponseBuilder::buildTranslatedText(Histories &histories, Response &response) {
  // Reserving length at least as much as source_ seems like a reasonable
  // thing to do to avoid reallocations.
  response.target.text.reserve(response.source.text.size());

  for (size_t sentenceIdx = 0; sentenceIdx < histories.size(); sentenceIdx++) {
    // TODO(jerin): Change hardcode of nBest = 1

    auto &history = histories[sentenceIdx];
    NBestList onebest = history->nBest(1);

    Result result = onebest[0];  // Expecting only one result;
    Words words = std::get<0>(result);

    std::string decoded;
    std::vector<string_view> targetSentenceMappings;
    vocabs_.target()->decodeWithByteRanges(words, decoded, targetSentenceMappings, /*ignoreEOS=*/false);
    if (targetSentenceMappings.rbegin()->size() != 0) {
      std::cerr << "Input string: " << response.source.sentence(sentenceIdx) << std::endl;
      std::cerr << "Output string: " << decoded << std::endl;
      std::cerr << "Words"
                << "(" << words.size() << "): ";
      for (auto &word : words) {
        std::cerr << word.toString() << " ";
      }
      std::cerr << std::endl;

#define debugWord(x) std::cerr << #x << " " << x.toString() << std::endl;
      debugWord(marian::Word::NONE);
      debugWord(marian::Word::ZERO);
      debugWord(marian::Word::DEFAULT_EOS_ID);
      debugWord(marian::Word::DEFAULT_UNK_ID);
      debugWord(vocabs_.target()->getEosId());
#undef debugWord

      // What is the last word?
      marian::Words custom;
      custom.push_back(words.back());
      std::cerr << "The last word is: " << vocabs_.target()->decode(custom) << std::endl;
    }

    // For some reason, marian has decided to give us something with no EOS. Manually inserting an EOS.
    ABORT_IF(targetSentenceMappings.rbegin()->size() != 0, "No EOS in targetSentenceMappings");

    switch (responseOptions_.concatStrategy) {
      case ConcatStrategy::FAITHFUL: {
        // For each sentence, prepend the filler text between the corresponding
        // source-sentence and the source-sentence before.
        string_view pre = response.source.gap(sentenceIdx);
        response.target.appendSentence(pre, targetSentenceMappings.begin(), targetSentenceMappings.end());

        // If this is the last history to be decoded and translated-text
        // constructed, append the text till the end, which could be spaces or
        // empty.
        if (sentenceIdx + 1 == histories.size()) {
          response.target.appendEndingWhitespace(response.source.gap(sentenceIdx + 1));
        }
        break;
      }
      case ConcatStrategy::SPACE: {
        string_view delimiter = (sentenceIdx == 0) ? "" : " ";
        response.target.appendSentence(delimiter, targetSentenceMappings.begin(), targetSentenceMappings.end());
        break;
      }

      default:
        ABORT("Unknown concat-strategy");
    }
  }
}

}  // namespace bergamot
}  // namespace marian
