#include "service.h"

#include <string>
#include <utility>

#include "batch.h"
#include "byte_array_util.h"
#include "definitions.h"

namespace marian {
namespace bergamot {

BlockingService::BlockingService(const BlockingService::Config &config) : requestId_(0), batchingPool_() {}

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
    model->translateBatch(/*deviceId=*/0, batch);
  }

  return responses;
}

AsyncService::AsyncService(const AsyncService::Config &config) : requestId_(0), config_(config), safeBatchingPool_() {
  ABORT_IF(config_.numWorkers == 0, "Number of workers should be at least 1 in a threaded workflow");
  workers_.reserve(config_.numWorkers);
  for (size_t cpuId = 0; cpuId < config_.numWorkers; cpuId++) {
    workers_.emplace_back([cpuId, this] {
      // Consumer thread main-loop. Note that this is an infinite-loop unless the monitor is explicitly told to
      // shutdown, which happens in the destructor for this class.
      Batch batch;
      Ptr<TranslationModel> translationModel{nullptr};
      while (safeBatchingPool_.generateBatch(translationModel, batch)) {
        translationModel->translateBatch(cpuId, batch);
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

void AsyncService::pivotTranslate(std::shared_ptr<TranslationModel> first, std::shared_ptr<TranslationModel> second,
                                  std::string &&source, CallbackType clientCallback) {
  // Need callback chaining to maintain async, honestly this would be easier to implement just for blocking.
  // When the first translation is ready, call the second
  auto internalCallback = [this, clientCallback, second](Response &&firstHalf) {
    // Grab the segments from Response to operate. We may need to do things at a lower level.
    // FIXME: Copy of a string.
    std::string sourceText = firstHalf.target.text;
    auto joiningCallback = [this, sourceText = std::move(sourceText), firstHalf = std::move(firstHalf),
                            clientCallback](Response &&secondHalf) {
      // All the operations.
      Response finalResponse;
      finalResponse.source.text = firstHalf.source.text;
      finalResponse.target.text = secondHalf.target.text;

      // FIXME sentences can potentially be inconsisted, wrap can abort.
    };

    // Async call, but this will get called later.
    // Neater way to do this without blocking?

    translate(second, std::move(sourceText), joiningCallback);
  };

  // First call.
  translate(first, std::move(source), internalCallback);
}

void AsyncService::translate(std::shared_ptr<TranslationModel> translationModel, std::string &&source,
                             CallbackType callback, const ResponseOptions &responseOptions) {
  // Producer thread, a call to this function adds new work items. If batches are available, notifies workers waiting.
  Ptr<Request> request = translationModel->makeRequest(requestId_++, std::move(source), callback, responseOptions);
  safeBatchingPool_.enqueueRequest(translationModel, request);
}

}  // namespace bergamot
}  // namespace marian
