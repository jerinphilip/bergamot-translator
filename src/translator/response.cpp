#include "response.h"
#include "common/logging.h"
#include "data/alignment.h"
#include "sentence_ranges.h"

#include <utility>

namespace marian {
namespace bergamot {

Response::Response(AnnotatedText &&source_, Histories &&histories,
                   std::vector<Ptr<Vocab const>> &vocabs)
    : source(std::move(source_)) {
  // Reserving length at least as much as source_ seems like a reasonable thing
  // to do to avoid reallocations.
  LOG(info, "Source: {}, Histories: {}", source.numSentences(),
      histories.size());
  ABORT_IF(source.numSentences() != histories.size(), "Mismatch in sentences!");
  target.text.reserve(source.text.size());

  // In a first step, the decoded units (individual senteneces) are compiled
  // into a huge string. This is done by computing indices first and appending
  // to the string as each sentences are decoded.
  bool first{true};

  for (auto &history : histories) {
    // TODO(jerin): Change hardcode of nBest = 1
    NBestList onebest = history->nBest(1);

    Result result = onebest[0]; // Expecting only one result;
    Words words = std::get<0>(result);
    auto targetVocab = vocabs.back();

    std::string decoded;
    std::vector<string_view> targetMappings;
    targetVocab->decodeWithByteRanges(words, decoded, targetMappings);

    std::string delim = "";
    if (first) {
      first = false;
    } else {
      delim = " ";
    }

    target.appendSentence(delim, decoded, targetMappings);
    target.log();

    // Alignments
    // TODO(jerinphilip): The following double conversion might not be
    // necessary. Hard alignment can directly be exported, but this would mean
    // WASM bindings for a structure deep within marian source.
    auto hyp = std::get<1>(result);
    auto softAlignment = hyp->tracebackAlignment();
    auto hardAlignment = data::ConvertSoftAlignToHardAlign(
        softAlignment, /*threshold=*/0.2f); // TODO(jerinphilip): Make this a
                                            // configurable parameter.

    Alignment unified_alignment;
    for (auto &p : hardAlignment) {
      unified_alignment.emplace_back((Point){p.srcPos, p.tgtPos, p.prob});
    }

    alignments.push_back(std::move(unified_alignment));

    // Quality scores: Sequence level is obtained as normalized path scores.
    // Word level using hypothesis traceback. These are most-likely logprobs.
    auto normalizedPathScore = std::get<2>(result);
    auto wordQualities = hyp->tracebackWordScores();
    wordQualities.pop_back();
    qualityScores.push_back((Quality){normalizedPathScore, wordQualities});
  }
}
} // namespace bergamot
} // namespace marian
