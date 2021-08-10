
#include "aggregate_batching_pool.h"

namespace marian {
namespace bergamot {

AggregateBatchingPool::AggregateBatchingPool(Ptr<Options> options) {
  // TODO(@jerinphilip): Set aggregate limits
}

size_t AggregateBatchingPool::enqueueRequest(Ptr<TranslationModel> model, Ptr<Request> request) {
  model->enqueueRequest(request);
  aggregateQueue_.push(model);
  return request->numSegments();
}

size_t AggregateBatchingPool::generateBatch(Ptr<TranslationModel>& model, Batch& batch) {
  while (model == nullptr && !aggregateQueue_.empty()) {
    std::shared_ptr<TranslationModel> weakModel = aggregateQueue_.front();
    size_t numSentences = model->generateBatch(batch);
    if (numSentences > 0) {
      return numSentences;
    } else {
      // Try the next model's batching pool.
      aggregateQueue_.pop();
    }
  }
  return 0;
}

}  // namespace bergamot
}  // namespace marian
