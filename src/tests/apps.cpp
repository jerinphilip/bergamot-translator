#include "apps.h"

namespace marian {
namespace bergamot {
namespace testapp {

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
  std::ostringstream inputStream;
  inputStream << std::cin.rdbuf();
  std::string input = inputStream.str();

  std::promise<Response> responsePromise;
  std::future<Response> responseFuture = responsePromise.get_future();

  auto callback = [&responsePromise](Response &&response) { responsePromise.set_value(std::move(response)); };
  service.translate(std::move(input), callback, responseOptions);

  responseFuture.wait();

  Response response = responseFuture.get();
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
  std::ostringstream inputStream;
  inputStream << std::cin.rdbuf();
  std::string input = inputStream.str();

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

}  // namespace testapp
}  // namespace bergamot
}  // namespace marian
