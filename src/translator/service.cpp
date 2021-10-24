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

std::vector<Response> BlockingService::pivotMultiple(std::shared_ptr<TranslationModel> first,
                                                     std::shared_ptr<TranslationModel> second,
                                                     std::vector<std::string> &&sources,
                                                     const ResponseOptions &responseOptions) {
  // Translate source to pivots. This is same as calling translateMultiple.
  std::vector<Response> sourcesToPivots;
  sourcesToPivots = translateMultiple(first, std::move(sources), responseOptions);

  // Translate pivots to targets, after we have outputs at pivot from first round. We cannot use translateMultiple here
  // because need consistency at pivot on both sides.
  std::vector<Response> pivotsToTargets;
  pivotsToTargets.resize(sourcesToPivots.size());

  for (size_t i = 0; i < sourcesToPivots.size(); i++) {
    AnnotatedText intermediate =
        sourcesToPivots[i].target;  // We cannot eliminate this copy, as we need two versions of intermediate. Holding
                                    // it in allows further use in makePivotRequest
    auto callback = [i, &pivotsToTargets](Response &&response) { pivotsToTargets[i] = std::move(response); };  //

    Ptr<Request> request = second->makePivotRequest(requestId_++, callback, std::move(intermediate), responseOptions);
    batchingPool_.enqueueRequest(second, request);
  }

  Batch batch;
  Ptr<TranslationModel> model{nullptr};
  while (batchingPool_.generateBatch(model, batch)) {
    model->translateBatch(/*deviceId=*/0, batch);
  }

  // Combine both sides. They're associated by indices.
  std::vector<Response> finalResponses;
  for (size_t i = 0; i < sourcesToPivots.size(); i++) {
    Response finalResponse = combine(sourcesToPivots[i], pivotsToTargets[i]);
    finalResponses.push_back(std::move(finalResponse));
  }

  return finalResponses;
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

void AsyncService::pivot(std::shared_ptr<TranslationModel> first, std::shared_ptr<TranslationModel> second,
                         std::string &&source, CallbackType clientCallback, const ResponseOptions &responseOptions) {
  // Need callback chaining to maintain async, honestly this would be easier to implement just for blocking.
  // When the first translation is ready, call the second.
  auto internalCallback = [this, clientCallback, second, responseOptions](Response &&firstHalf) {
    // We have both Responses at this callback, firstHalf is moved in, second half will be available when complete.
    AnnotatedText intermediate =
        firstHalf.target;  // We cannot eliminate this copy, as we need two versions of intermediate. Holding
                           // it in a copy allows moving the response into the lambda below.

    // https://stackoverflow.com/a/65606554/4565794
    // Move semantics only work on mutable lambdas, and can only be done once. It's only once in our case, so issok.
    auto joiningCallback = [this, firstHalf = std::move(firstHalf), clientCallback](Response &&secondHalf) mutable {
      Response finalResponse = combine(firstHalf, secondHalf);

      // Sentences should be consistent now, give way to client.
      clientCallback(std::move(finalResponse));
    };

    Ptr<Request> request =
        second->makePivotRequest(requestId_++, joiningCallback, std::move(intermediate), responseOptions);
    safeBatchingPool_.enqueueRequest(second, request);
  };

  // First call.
  translate(first, std::move(source), internalCallback, responseOptions);
}

void AsyncService::translate(std::shared_ptr<TranslationModel> translationModel, std::string &&source,
                             CallbackType callback, const ResponseOptions &responseOptions) {
  // Producer thread, a call to this function adds new work items. If batches are available, notifies workers waiting.
  Ptr<Request> request = translationModel->makeRequest(requestId_++, std::move(source), callback, responseOptions);
  safeBatchingPool_.enqueueRequest(translationModel, request);
}

}  // namespace bergamot
}  // namespace marian
