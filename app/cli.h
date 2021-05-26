#ifndef BERGAMOT_APP_CLI_H
#define BERGAMOT_APP_CLI_H
#include <cstdlib>
#include <future>
#include <iostream>
#include <sstream>
#include <unordered_map>

#include "common/definitions.h"
#include "common/timer.h"
#include "common/utils.h"
#include "marian.h"
#include "translator/byte_array_util.h"
#include "translator/parser.h"
#include "translator/response.h"
#include "translator/response_options.h"
#include "translator/service.h"

namespace marian {
namespace bergamot {

// marian::bergamot:: makes life easier, won't need to prefix it everywhere and these classes plenty use constructs.

namespace app {

/// Previously bergamot-translator-app. Provides a command-line app on native which executes the code-path used by Web
/// Assembly. Expected to be maintained consistent with how the browser (Mozilla through WebAssembly) dictates its API
/// and tests be intact. Also used in [bergamot-evaluation](https://github.com/mozilla/bergamot-evaluation).
/// @param [cmdline]: Options to translate passed down to marian through Options.
/// @param [stdin] sentences as lines of text.
/// @param [stdout] translations for the sentences supplied in corresponding lines
struct wasm {
  void operator()(Ptr<Options> options) const {
    // Here, we take the command-line interface which is uniform across all apps. This is parsed into Ptr<Options> by
    // marian. However, mozilla does not allow a Ptr<Options> constructor and demands an std::string constructor since
    // std::string isn't marian internal unlike Ptr<Options>. Since this std::string path needs to be tested for mozilla
    // and since this class/CLI is intended at testing mozilla's path, we go from:
    //
    // cmdline -> Ptr<Options> -> std::string -> Service(std::string)
    //
    // Overkill, yes.

    std::string config = options->asYamlString();
    Service model(config);

    ResponseOptions responseOptions;
    std::vector<std::string> texts;

    for (std::string line; std::getline(std::cin, line);) {
      texts.emplace_back(line);
    }

    auto results = model.translateMultiple(std::move(texts), responseOptions);

    for (auto &result : results) {
      std::cout << result.getTranslatedText() << std::endl;
    }
  }
};

/// Application used to benchmark with marian-decoder from time-to-time. The implementation in this repository follows a
/// different route than marian-decoder  and routinely needs to be checked that the speeds while operating similar to
/// marian-decoder are not affected during the course of development.
///
/// Example usage:
/// [brt/speed-tests/test_wngt20_perf.sh](https://github.com/browsermt/bergamot-translator-tests/blob/main/speed-tests/test_wngt20_perf.sh).
///
/// Expected to be compatible with Translator[1] and marian-decoder[2].
///
/// - [1]
/// [marian-dev/../src/translator/translator.h](https://github.com/marian-nmt/marian-dev/blob/master/src/translator/translator.h)
/// - [2]
/// [marian-dev/../src/command/marian_decoder.cpp](https://github.com/marian-nmt/marian/blob/master/src/command/marian_decoder.cpp)
///
/// @param [cmdline] options constructed from command-line supplied arguments
/// @param [stdin] lines containing sentences, same as marian-decoder.
/// @param [stdout] translations of the sentences supplied via stdin in corresponding lines

struct decoder {
  void operator()(Ptr<Options> options) const {
    marian::timer::Timer decoderTimer;
    Service service(options);
    // Read a large input text blob from stdin
    std::ostringstream std_input;
    std_input << std::cin.rdbuf();
    std::string input = std_input.str();

    // Wait on future until Response is complete
    std::future<Response> responseFuture = service.translate(std::move(input));
    responseFuture.wait();
    const Response &response = responseFuture.get();

    for (size_t sentenceIdx = 0; sentenceIdx < response.size(); sentenceIdx++) {
      std::cout << response.target.sentence(sentenceIdx) << "\n";
    }
    LOG(info, "Total time: {:.5f}s wall", decoderTimer.elapsed());
  }
};

/// Command line interface to the test the features being developed as part of bergamot C++ library on native platform.
///
/// @param [cmdline]: options to build translator
/// @param [stdin]: Blob of text, read as a whole ; sentence-splitting etc handled internally.
/// @param [stdout]: Translation of the source text and additional information like sentences, alignments between source
/// and target tokens and quality scores.
struct native {
  void operator()(Ptr<Options> options) const {
    // Prepare memories for bytearrays (including model, shortlist and vocabs)
    MemoryBundle memoryBundle;

    if (options->get<bool>("bytearray")) {
      // Load legit values into bytearrays.
      memoryBundle = getMemoryBundleFromConfig(options);
    }

    Service service(options, std::move(memoryBundle));

    // Read a large input text blob from stdin
    std::ostringstream std_input;
    std_input << std::cin.rdbuf();
    std::string input = std_input.str();

    ResponseOptions responseOptions;
    responseOptions.qualityScores = true;
    responseOptions.alignment = true;
    responseOptions.alignmentThreshold = 0.2f;

    // Wait on future until Response is complete
    std::future<Response> responseFuture = service.translate(std::move(input), responseOptions);
    responseFuture.wait();
    Response response = responseFuture.get();

    std::cout << "[original]: " << response.source.text << '\n';
    std::cout << "[translated]: " << response.target.text << '\n';
    for (int sentenceIdx = 0; sentenceIdx < response.size(); sentenceIdx++) {
      std::cout << " [src Sentence]: " << response.source.sentence(sentenceIdx) << '\n';
      std::cout << " [tgt Sentence]: " << response.target.sentence(sentenceIdx) << '\n';
      std::cout << "Alignments" << '\n';
      typedef std::pair<size_t, float> Point;

      // Initialize a point vector.
      std::vector<std::vector<Point>> aggregate(response.source.numWords(sentenceIdx));

      // Handle alignments
      auto &alignments = response.alignments[sentenceIdx];
      for (auto &p : alignments) {
        aggregate[p.src].emplace_back(p.tgt, p.prob);
      }

      for (size_t src = 0; src < aggregate.size(); src++) {
        std::cout << response.source.word(sentenceIdx, src) << ": ";
        for (auto &p : aggregate[src]) {
          std::cout << response.target.word(sentenceIdx, p.first) << "(" << p.second << ") ";
        }
        std::cout << '\n';
      }

      // Handle quality.
      auto &quality = response.qualityScores[sentenceIdx];
      std::cout << "Quality: whole(" << quality.sequence << "), tokens below:" << '\n';
      size_t wordIdx = 0;
      bool first = true;
      for (auto &p : quality.word) {
        if (first) {
          first = false;
        } else {
          std::cout << " ";
        }
        std::cout << response.target.word(sentenceIdx, wordIdx) << "(" << p << ")";
        wordIdx++;
      }
      std::cout << '\n';
    }
    std::cout << "--------------------------\n";
    std::cout << '\n';
  }
};

typedef std::function<void(Ptr<Options>)> CLI;
static const std::unordered_map<std::string, CLI> ftable{
    {"wasm", app::wasm()}, {"native", app::native()}, {"decoder", app::decoder()}};

}  // namespace app

void execute(const std::string &mode, Ptr<Options> options) {
  auto namedFnPtr = app::ftable.find(mode);
  if (namedFnPtr != app::ftable.end()) {
    const std::string &fname = namedFnPtr->first;
    const app::CLI &fn = namedFnPtr->second;
    fn(options);
  } else {
    // Collect available functions to show error message.
    std::string availModes = "";
    bool first{false};
    for (auto &p : app::ftable) {
      if (first) {
        first = false;
      } else {
        availModes += ",";
      }
      availModes += p.first;
    }
    ABORT("Unknown --mode {}. Use one of: \{{}\}", mode, availModes);
  }
}

}  // namespace bergamot
}  // namespace marian

#endif  // BERGAMOT_APP_CLI_H