#include "apps.h"

#include <random>

#include "common/timer.h"

namespace marian {
namespace bergamot {
namespace testapp {

// This is a curiously recurring pattern among the apps below
Response translateForResponse(Service &service, ResponseOptions &responseOptions, std::string &&input) {
  std::promise<Response> responsePromise;
  std::future<Response> responseFuture = responsePromise.get_future();
  auto callback = [&responsePromise](Response &&response) { responsePromise.set_value(std::move(response)); };

  service.translate(std::move(input), callback, responseOptions);

  responseFuture.wait();
  Response response = responseFuture.get();
  return response;
}

std::string readFromStdin() {
  std::ostringstream inputStream;
  inputStream << std::cin.rdbuf();
  return inputStream.str();
}

// Utility function, common for all testapps.
Response translateFromStdin(Ptr<Options> options, ResponseOptions responseOptions) {
  // Prepare memories for bytearrays (including model, shortlist and vocabs)
  MemoryBundle memoryBundle;

  if (options->get<bool>("bytearray")) {
    // Load legit values into bytearrays.
    memoryBundle = getMemoryBundleFromConfig(options);
  }

  Service service(options, std::move(memoryBundle));

  // Read a large input text blob from stdin
  std::string input = readFromStdin();
  Response response = translateForResponse(service, responseOptions, std::move(input));
  return response;
}

void annotatedTextWords(Ptr<Options> options, bool source) {
  ResponseOptions responseOptions;
  Response response = translateFromStdin(options, responseOptions);
  AnnotatedText &annotatedText = source ? response.source : response.target;
  for (size_t s = 0; s < annotatedText.numSentences(); s++) {
    for (size_t w = 0; w < annotatedText.numWords(s); w++) {
      std::cout << (w == 0 ? "" : "\t");
      std::cout << annotatedText.word(s, w);
    }
    std::cout << "\n";
  }
}

void annotatedTextSentences(Ptr<Options> options, bool source) {
  ResponseOptions responseOptions;
  Response response = translateFromStdin(options, responseOptions);
  AnnotatedText &annotatedText = source ? response.source : response.target;
  for (size_t s = 0; s < annotatedText.numSentences(); s++) {
    std::cout << annotatedText.sentence(s) << "\n";
  }
}

void translationCache(Ptr<Options> options) {
  // Prepare memories for bytearrays (including model, shortlist and vocabs)
  MemoryBundle memoryBundle;
  ResponseOptions responseOptions;

  if (options->get<bool>("bytearray")) {
    // Load legit values into bytearrays.
    memoryBundle = getMemoryBundleFromConfig(options);
  }

  Service service(options, std::move(memoryBundle));

  // Read a large input text blob from stdin
  const std::string input = readFromStdin();

#ifndef WASM_COMPATIBLE_SOURCE
  auto translateForResponse = [&service, &responseOptions](std::string input) {
    std::promise<Response> responsePromise;
    std::future<Response> responseFuture = responsePromise.get_future();
    auto callback = [&responsePromise](Response &&response) { responsePromise.set_value(std::move(response)); };

    service.translate(std::move(input), callback, responseOptions);

    responseFuture.wait();
    Response response = responseFuture.get();
    return response;
  };
#else
  auto translateForResponse = [&service, &responseOptions](std::string input) {
    ResponseOptions responseOptions;
    std::vector<Response> responses = service.translateMultiple({input}, responseOptions);
    return responses.front();
  };
#endif

  // Round 1
  Response firstResponse = translateForResponse(input);

  auto statsFirstRun = service.cacheStats();
  LOG(info, "Cache Hits/Misses = {}/{}", statsFirstRun.hits, statsFirstRun.misses);
  ABORT_IF(statsFirstRun.hits != 0, "Expecting no cache hits, but hits found.");

  // Round 2; There should be cache hits
  Response secondResponse = translateForResponse(input);

  auto statsSecondRun = service.cacheStats();
  LOG(info, "Cache Hits/Misses = {}/{}", statsSecondRun.hits, statsSecondRun.misses);
  ABORT_IF(statsSecondRun.hits != statsFirstRun.misses,
           "Mismatch in expected hits. This test is supposed to check if all previous misses are hit in second run. "
           "Ensure you give an input file and cache-size caps within reasonable limits.");

  ABORT_IF(firstResponse.target.text != secondResponse.target.text,
           "Recompiled string provided different output when operated with cache. On the same hardware while using "
           "same path, this is expected to be same.");

  std::cout << firstResponse.target.text;
}

void benchmarkCacheEditWorkflow(Ptr<Options> options) {
  std::cout << "Starting cache-warmup" << std::endl;
  Response response;

  {
    Service service(options);
    ResponseOptions responseOptions;
    std::string input = readFromStdin();

    // Running this once lets the tokenizer work it's magic in response.source (annotation).
    response = translateForResponse(service, responseOptions, std::move(input));
  }

  std::cout << "Completed first round of translations!" << std::endl;

  Service service(options);
  ResponseOptions responseOptions;
  // Hyperparameters
  std::mt19937 generator;
  generator.seed(42);

  enum class Action { ERROR_THEN_CORRECT_STOP, CORRECT_STOP, TYPE_THROUGH };
  std::discrete_distribution<> actionSampler({0.05, 0.15, 0.8});

  // A simple state machine which advances each step and ends after a finite number of steps. The choice of a bunch of
  // mistakes are probabilistic.
  size_t previousWordEnd = 0;
  const std::string &input = response.source.text;
  std::string buffer;
  Response editResponse;

  std::cerr << "Starting translation edit workflow" << std::endl;
  marian::timer::Timer taskTimer;
  for (size_t s = 0; s < response.source.numSentences(); s++) {
    if (s % 10 == 0) {
      LOG(info, "Editing sentence {} / {}", s + 1, response.source.numSentences());
      std::cerr << std::endl;
    }
    for (size_t w = 0; w < response.source.numWords(s); w++) {
      ByteRange currentWord = response.source.wordAsByteRange(s, w);
      int index = actionSampler(generator);
      Action action = static_cast<Action>(index);
      switch (action) {
        case Action::ERROR_THEN_CORRECT_STOP: {
          // Error once
          buffer = input.substr(0, previousWordEnd) + " 0xdeadbeef" /* highly unlikely error token */;
          editResponse = translateForResponse(service, responseOptions, std::move(buffer));

          // Backspace a token
          buffer = input.substr(0, previousWordEnd);
          editResponse = translateForResponse(service, responseOptions, std::move(buffer));

          // Correct
          buffer = input.substr(0, currentWord.end);
          editResponse = translateForResponse(service, responseOptions, std::move(buffer));
          break;
        }

        case Action::CORRECT_STOP: {
          buffer = input.substr(0, currentWord.end);
          editResponse = translateForResponse(service, responseOptions, std::move(buffer));
          break;
        }

        case Action::TYPE_THROUGH: {
          break;
        }

        default: {
          ABORT("Unknown action");
          break;
        }
      }
      previousWordEnd = currentWord.end;
    }
  }

  auto cacheStats = service.cacheStats();
  std::cout << "Hits / Misses = " << cacheStats.hits << "/ " << cacheStats.misses << std::endl;
  LOG(info, "Total time: {:.5f}s wall", taskTimer.elapsed());
}

}  // namespace testapp
}  // namespace bergamot
}  // namespace marian
