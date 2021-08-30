#include "service.h"

#include <string>
#include <utility>

#include "batch.h"
#include "definitions.h"

namespace marian {
namespace bergamot {

BlockingService::BlockingService(const Ptr<Options> &options)
    : requestId_(0), batchingPool_(), backend_(options, /*deviceId=*/0) {}

std::vector<Response> BlockingService::translateMultiple(std::shared_ptr<TranslationModel> translationModel,
                                                         std::vector<std::string> &&sources,
                                                         const ResponseOptions &responseOptions) {
  std::vector<Response> responses;
  responses.resize(sources.size());

  for (size_t i = 0; i < sources.size(); i++) {
    auto callback = [i, &responses](Response &&response) { responses[i] = std::move(response); };  //
    Ptr<Request> request =
        translationModel->makeRequest(requestId_++, std::move(sources[i]), callback, responseOptions);
    batchingPool_.enqueueRequest(translationModel, request);
  }

  Batch batch;
  Ptr<TranslationModel> model{nullptr};
  while (batchingPool_.generateBatch(model, batch)) {
    backend_.translateBatch(model, batch);
  }

  return responses;
}

AsyncService::AsyncService(const Ptr<Options> &options, size_t numWorkers)
    : requestId_(0), numWorkers_(numWorkers), safeBatchingPool_() {
  ABORT_IF(numWorkers_ == 0, "Number of workers should be at least 1 in a threaded workflow");
  workers_.reserve(numWorkers_);
  for (size_t cpuId = 0; cpuId < numWorkers_; cpuId++) {
    // Prepare graph backend to use.
    backends_.emplace_back(options, cpuId);

    // Consumer thread main-loop. Note that this is an infinite-loop unless the monitor is explicitly told to
    // shutdown, which happens in the destructor for this class.
    workers_.emplace_back([cpuId, this] {
      Batch batch;
      Ptr<TranslationModel> model{nullptr};
      while (safeBatchingPool_.generateBatch(model, batch)) {
        backends_[cpuId].translateBatch(model, batch);
      }
    });
  }
}

AsyncService::~AsyncService() {
  safeBatchingPool_.shutdown();
  for (std::thread &worker : workers_) {
    assert(worker.joinable());
    worker.join();
  }
}

void AsyncService::translate(std::shared_ptr<TranslationModel> translationModel, std::string &&source,
                             CallbackType callback, const ResponseOptions &responseOptions) {
  // Producer thread, a call to this function adds new work items. If batches are available, notifies workers waiting.
  Ptr<Request> request = translationModel->makeRequest(requestId_++, std::move(source), callback, responseOptions);
  safeBatchingPool_.enqueueRequest(translationModel, request);
}

}  // namespace bergamot
}  // namespace marian
