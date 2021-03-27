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
  ExhaustModeConfig(Ptr<Options> options);

  /// Lets outside know if exhaust-mode is active or not.
  bool active() { return active_; }

  /// Writes (b,t) onto batchEntry for next batch in a streaming mode. Returns
  /// false if no more batches.
  bool next(std::pair<size_t, size_t> &batchEntry);

private:
  /// Marks current activeIdx_ on sampleInfors_, to enable
  /// next()
  size_t activeIdx_;
  size_t samples_; ///< Number of samples_ per (b, t) entry.

  /// Whether active or not.This structure dumbs down to
  /// no-data, no-op if not active.
  bool active_;

  /// contains (b, t) generated in order and random_shuffle-d after.
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

  /// External API to stream batches from Batcher. Returns false when no more
  /// batches left to be created. Internally redirects to nextRandomBatch or
  /// cleaveBatch depending on exhaust mode being active or not.
  bool operator>>(Batch &batch);

private:
  // Loads sentences with sentences compiled from (tentatively) multiple
  // requests optimizing for both padding and priority.
  bool cleaveBatch(Batch &batch);

  /// Special purpose usage to sample random batches controlling batch-size and
  /// sequence-length to profile time taken for each (batch-size,
  /// sequence-length) configuration.
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
