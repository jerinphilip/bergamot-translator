#ifndef SRC_BERGAMOT_SERVICE_H_
#define SRC_BERGAMOT_SERVICE_H_

#include <queue>
#include <thread>
#include <vector>

#include "cache.h"
#include "data/types.h"
#include "logging.h"
#include "quality_estimator.h"
#include "response.h"
#include "response_builder.h"
#include "tensors/tensor_allocator.h"
#include "text_processor.h"
#include "threadsafe_batching_pool.h"
#include "translation_model.h"
#include "translator/parser.h"
#include "vocabs.h"

namespace marian {
namespace bergamot {

class Workspace {
 public:
  Workspace(size_t deviceId, size_t workspaceSizeInMB)
      : device_(deviceId, DeviceType::cpu), precision_(typeFromString("float32")) {
    // We'll eventually get rid of this, but proof of concept faster?
    auto graph = New<ExpressionGraph>(/*inference=*/true);  // set the graph to be inference only
    graph->setDefaultElementType(precision_);
    graph->setDevice(device_);

    // TODO(@jerinphilip): Are we just fine without configuring? Does Backend have sane defaults?
    // This however does not affect memory allocations.
    graph->getBackend()->configureDevice(horribleOptionsHack());

    backend_ = graph->getBackend();

    tensors_ = New<TensorAllocator>(backend_);
    tensors_->reserve(workspaceSizeInMB);
  }

  Ptr<TensorAllocator> tensors() { return tensors_; }
  Ptr<TensorAllocator> cache() { return New<TensorAllocator>(backend_); }

  size_t id() const { return device_.no; }
  marian::DeviceId device() const { return device_; }
  marian::Type precision() const { return precision_; }

  void clear() { tensors_->clear(); }

 private:
  Ptr<TensorAllocator> tensors_{nullptr};
  const marian::DeviceId device_;
  const marian::Type precision_;
  Ptr<Backend> backend_;

  Ptr<Options> horribleOptionsHack() {
    Ptr<Options> options = std::make_shared<Options>();
    options->set<std::string>("gemm-precision", "int8shiftAlphaAll");
    options->set<bool>("dump-quantmult", false);
    options->set<float>("clip-gemm", 1.0);
    options->set<bool>("use-legacy-batching", false);
    return options;
  }
};

class BlockingService;
class AsyncService;

/// See AsyncService.
///
/// BlockingService is a not-threaded counterpart of AsyncService which can operate only in a blocking workflow (queue
/// a bunch of texts and optional args to translate, wait till the translation finishes).
class BlockingService {
 public:
  struct Config {
    /// Size in History items to be stored in the cache. A value of 0 means no caching. Loosely corresponds to sentences
    /// to cache in the real world. Note that cache has a random-eviction policy. The peak storage at full occupancy is
    /// controlled by this parameter. However, whether we attain full occupancy or not is controlled by random factors -
    /// specifically how uniformly the hash distributes.
    size_t cacheSize{0};

    size_t workspaceSizeInMB{1024};

    Logger::Config logger;  ///< Configurations for logging

    template <class App>
    static void addOptions(App &app, Config &config) {
      // Options will come here.
      app.add_option("--cache-size", config.cacheSize, "Number of entries to store in cache.");
      app.add_option("--workspace-size", config.workspaceSizeInMB, "Workspace size to use");

      Logger::Config::addOptions(app, config.logger);
    }
  };
  /// Construct a BlockingService with configuration loaded from an Options object. Does not require any keys, values
  /// to be set.
  BlockingService(const BlockingService::Config &config);

  /// Translate multiple text-blobs in a single *blocking* API call, providing ResponseOptions which applies across
  /// all text-blobs dictating how to construct Response. ResponseOptions can be used to enable/disable additional
  /// information like quality-scores, alignments etc.

  /// If you have async/multithread capabilities, it is recommended to work with AsyncService instead of this class.
  /// Note that due to batching differences and consequent floating-point rounding differences, this is not guaranteed
  /// to have the same output as AsyncService.

  /// @param [in] translationModel: TranslationModel to use for the request.
  /// @param [in] source: rvalue reference of the string to be translated
  /// @param [in] responseOptions: ResponseOptions per source-item indicating whether or not to include some member in
  /// the Response, also specify any additional configurable parameters.
  std::vector<Response> translateMultiple(std::shared_ptr<TranslationModel> translationModel,
                                          std::vector<std::string> &&source,
                                          const std::vector<ResponseOptions> &responseOptions);

  /// With the supplied two translation models, translate using first and then the second generating a response as if it
  /// were translated from first's source language to second's target langauge. Requires first's target to be second's
  /// source to work correctly - effectively implementing pivoting translation via an intermediate language.
  ///
  /// @param[in] first: TranslationModel capable of translating from source language to pivot language.
  /// @param[in] second: TranslationModel capable of translating between pivot and target language.
  /// @param[move] sources: The input source texts to be translated.
  /// @param[in] options: Options indicating whether or not to include optional members per source-text. See
  /// ResponseOptions.
  ///
  /// @returns responses corresponding to the source-text which can be used as if they were translated with
  /// translateMultiple.
  std::vector<Response> pivotMultiple(std::shared_ptr<TranslationModel> first, std::shared_ptr<TranslationModel> second,
                                      std::vector<std::string> &&sources,
                                      const std::vector<ResponseOptions> &responseOptions);
  TranslationCache::Stats cacheStats() { return cache_ ? cache_->stats() : TranslationCache::Stats(); }

 private:
  std::vector<Response> translateMultipleRaw(std::shared_ptr<TranslationModel> translationModel,
                                             std::vector<std::string> &&source,
                                             const std::vector<ResponseOptions> &responseOptions);

  ///  Numbering requests processed through this instance. Used to keep account of arrival times of the request. This
  ///  allows for using this quantity in priority based ordering.
  size_t requestId_;

  /// An aggregate batching pool associated with an async translating instance, which maintains an aggregate queue of
  /// requests compiled from  batching-pools of multiple translation models. Not thread-safe.
  AggregateBatchingPool batchingPool_;

  Config config_;

  // Logger which shuts down cleanly with service.
  Logger logger_;
  std::optional<TranslationCache> cache_;

  Workspace workspace_;
};

/// Effectively a threadpool, providing an API to take a translation request of a source-text, paramaterized by
/// TranslationModel to be used for translation. Configurability on optional items for the Response corresponding to a
/// request is provisioned through ResponseOptions.
class AsyncService {
 public:
  struct Config {
    size_t numWorkers{1};  ///< How many worker translation threads to spawn.
    size_t cacheSize{0};   ///< Size in History items to be stored in the cache. Loosely corresponds to sentences to
                           /// cache in the real world. A value of 0 means no caching.
    size_t workspaceSizeInMB{1024};
    Logger::Config logger;  // Configurations for logging

    template <class App>
    static void addOptions(App &app, Config &config) {
      app.add_option("--cpu-threads", config.numWorkers, "Workers to form translation backend");
      app.add_option("--cache-size", config.cacheSize, "Number of entries to store in cache.");
      app.add_option("--workspace-size", config.workspaceSizeInMB, "Workspace size to use");
      Logger::Config::addOptions(app, config.logger);
    }
  };
  /// Construct an AsyncService with configuration loaded from Options. Expects positive integer value for
  /// `cpu-threads`. Additionally requires options which configure AggregateBatchingPool.
  AsyncService(const AsyncService::Config &config);

  /// With the supplied TranslationModel, translate an input. A Response is constructed with optional items set/unset
  /// indicated via ResponseOptions. Upon completion translation of the input, the client supplied callback is
  /// triggered with the constructed Response. Concurrent-calls to this function are safe.
  ///
  /// @param [in] translationModel: TranslationModel to use for the request.
  /// @param [in] source: rvalue reference of the string to be translated. This is available as-is to the client later
  /// in the Response corresponding to this call along with the translated-text and meta-data.
  /// @param [in] callback: A callback function provided by the client which accepts an rvalue of a Response.
  /// @param [in] responseOptions: Options indicating whether or not to include some member in the Response, also
  /// specify any additional configurable parameters.
  void translate(std::shared_ptr<TranslationModel> translationModel, std::string &&source, CallbackType callback,
                 const ResponseOptions &options = ResponseOptions());

  /// With the supplied two translation models, translate using first and then the second generating a response as if it
  /// were translated from first's source language to second's target langauge. Requires first's target to be second's
  /// source to work correctly - effectively implementing pivoting translation via an intermediate language.
  ///
  /// @param[in] first: TranslationModel capable of translating from source language to pivot language.
  /// @param[in] second: TranslationModel capable of translating between pivot and target language.
  /// @param[move] source: The source text to be translated
  /// @param[in] clientCallback: The callback to be called with the constructed Response. Expects the callback to
  /// consume the Response.
  /// @param[in] options: Options indicating whether or not to include optional members in response and pass additional
  /// configurations. See ResponseOptions.
  void pivot(std::shared_ptr<TranslationModel> first, std::shared_ptr<TranslationModel> second, std::string &&source,
             CallbackType clientCallback, const ResponseOptions &options = ResponseOptions());

  /// Clears all pending requests.
  void clear();

  /// Thread joins and proper shutdown are required to be handled explicitly.
  /// If you do not want to wait, call `clear()` before destructor.
  ~AsyncService();

  TranslationCache::Stats cacheStats() { return cache_ ? cache_->stats() : TranslationCache::Stats(); }

 private:
  void translateRaw(std::shared_ptr<TranslationModel> translationModel, std::string &&source, CallbackType callback,
                    const ResponseOptions &options = ResponseOptions());

  AsyncService::Config config_;

  std::vector<std::thread> workers_;
  std::vector<Workspace> workspaces_;

  /// Stores requestId of active request. Used to establish
  /// ordering among requests and logging/book-keeping.

  /// Numbering requests processed through this instance. Used to keep account of arrival times of the request. This
  /// allows for using this quantity in priority based ordering.
  size_t requestId_;

  /// An aggregate batching pool associated with an async translating instance, which maintains an aggregate queue of
  /// requests compiled from  batching-pools of multiple translation models. The batching pool is wrapped around one
  /// object for thread-safety.
  ThreadsafeBatchingPool<AggregateBatchingPool> safeBatchingPool_;

  // Logger which shuts down cleanly with service.
  Logger logger_;
  std::optional<TranslationCache> cache_;
};

}  // namespace bergamot
}  // namespace marian

#endif  // SRC_BERGAMOT_SERVICE_H_
