#ifndef SRC_BERGAMOT_BATCHER_H_
#define SRC_BERGAMOT_BATCHER_H_

#include "batch.h"
#include "common/options.h"
#include "data/corpus_base.h"
#include "definitions.h"
#include "request.h"

#ifndef WASM
#include "pcqueue.h"
#endif

#include <random>
#include <set>
#include <utility>
#include <vector>

namespace marian {
namespace bergamot {

class ExhaustModeConfig {
public:
  ExhaustModeConfig(Ptr<Options> options)
      : active_(options->get<bool>("exhaust-mode")), activeIdx_(0),
        samples_(options->get<int>("exhaust-mode-samples")) {
    if (active_) {
      // Offline create entries of (b, t) from mini-batch-words and
      // max-length-break
      size_t slack = options->get<int>("exhaust-mode-slack");
      size_t B = options->get<int>("mini-batch-words");
      size_t T = options->get<int>("max-length-break");
      for (size_t b = 1; b < B; b++) {
        size_t tBound = B / (b + 1);
        size_t safeTBound = std::min<size_t>(tBound + slack, T);
        for (size_t t = 1; t < safeTBound; t++) {
          sampleInfos_.insert(sampleInfos_.end(),
                              samples_, // samples_ number of (b, t) entries
                              std::make_pair(b, t));
          // LOG(info, "b={}, t={}, tBound={}, safeTBound={}", b, t, tBound,
          //     safeTBound);
          ABORT_IF(b * t > B, "Too large a batch detected, check code");
        }
      }

      // Random shuffle to remove patterned access
      std::random_shuffle(sampleInfos_.begin(), sampleInfos_.end());
    }
  }

  bool active() { return active_; }
  bool next(std::pair<size_t, size_t> &batchEntry) {
    if (activeIdx_ >= sampleInfos_.size()) {
      return false;
    } else {
      batchEntry = sampleInfos_[activeIdx_];
      activeIdx_++;
      return true;
    }
  }

private:
  size_t activeIdx_;
  size_t samples_;
  bool active_;
  std::vector<std::pair<size_t, size_t>> sampleInfos_;
};

class Batcher {
public:
  explicit Batcher(Ptr<Options> options);

  // RequestSentence incorporates (tentative) notions of priority with each
  // sentence. This method inserts the sentence into the internal data-structure
  // which maintains priority among sentences from multiple concurrent requests.
  void addSentenceWithPriority(RequestSentence &sentence);
  void addWholeRequest(Ptr<Request> request);

  bool operator>>(Batch &batch); // alias for cleaveBatch

private:
  // Loads sentences with sentences compiled from (tentatively) multiple
  // requests optimizing for both padding and priority.
  bool cleaveBatch(Batch &batch);
  bool nextRandomBatch(Batch &batch);
  size_t miniBatchWords;
  std::vector<std::set<RequestSentence>> bucket_;
  size_t batchNumber_{0};

  /// Exhaust mode to generate exhaustive (b, t) from batches with some
  /// randomness induced.
  ExhaustModeConfig exhaustConfig_;
};

} // namespace bergamot
} // namespace marian

#endif // SRC_BERGAMOT_BATCHER_H_
