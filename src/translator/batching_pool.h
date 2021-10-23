#ifndef SRC_BERGAMOT_BATCHING_POOL_H_
#define SRC_BERGAMOT_BATCHING_POOL_H_

#include <set>
#include <vector>

#include "batch.h"
#include "common/options.h"
#include "data/corpus_base.h"
#include "definitions.h"
#include "request.h"

namespace marian {
namespace bergamot {

class BatchingPool {
 public:
  // Breaking this further into multiple wrapped sentences will complicate the API for Annotation, requiring to have
  // knowledge of list of sentences mapping to a single previous translation unit. Annotation needs rework for a a more
  // friendly use-case suitable API in C++ to iterate sentences, tokens etc which is a TODO for later.
  //
  // For the time being, we add some slack, which only BatchingPool is aware of. Since the TextProcessor still wraps at
  // first request in, most of the Batches generated will be under max-length break.
  //
  // In the unlikely event of a few sentences overflowing, this allows the exceeding words to be put in the slack area.
  // Very few batches are expected to be generated at the higher length.
  //
  // In the event we get an overflow max-length-break + PIVOT_SLACK, the program is configured to abort.
  explicit BatchingPool(Ptr<Options> options);

  // RequestSentence incorporates (tentative) notions of priority with each
  // sentence. This method inserts the sentence into the internal data-structure
  // which maintains priority among sentences from multiple concurrent requests.
  size_t enqueueRequest(Ptr<Request> request);

  // Loads sentences with sentences compiled from (tentatively) multiple
  // requests optimizing for both padding and priority.
  size_t generateBatch(Batch &batch);

 private:
  size_t miniBatchWords_;
  std::vector<std::set<RequestSentence>> bucket_;
  size_t batchNumber_{0};
  size_t maxActiveBucketLength_;
  size_t pivotSlack_;
};

}  // namespace bergamot
}  // namespace marian

#endif  // SRC_BERGAMOT_BATCHING_POOL_H_
