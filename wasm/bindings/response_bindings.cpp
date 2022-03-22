/*
 * Bindings for Response class
 *
 */

#include <emscripten/bind.h>

#include <string>
#include <vector>

#include "response.h"

using Response = marian::bergamot::Response;
using ByteRange = marian::bergamot::ByteRange;

// Discrete Probability Distribution, named Distribution for brevity.
using Distribution = std::vector<float>;
using Alignment = std::vector<Distribution>;

using namespace emscripten;

// Binding code
EMSCRIPTEN_BINDINGS(byte_range) {
  value_object<ByteRange>("ByteRange").field("begin", &ByteRange::begin).field("end", &ByteRange::end);
}

EMSCRIPTEN_BINDINGS(response) {
  register_vector<float>("Distribution");
  register_vector<Distribution>("Alignment");
  register_vector<Alignment>("Alignments");

  class_<Response>("Response")
      .constructor<>()
      .function("size", &Response::size)
      .function("getOriginalText", &Response::getOriginalText)
      .function("getTranslatedText", &Response::getTranslatedText)
      .function("getSourceSentence", &Response::getSourceSentenceAsByteRange)
      .function("getTranslatedSentence", &Response::getTargetSentenceAsByteRange)
      .function("getSourceTokens",
                std::function<std::vector<std::string>(Response &, size_t)>([](Response &response, size_t sentenceIdx) {
                  std::vector<std::string> result;
                  for (size_t wordIdx = 0; wordIdx < response.source.numWords(sentenceIdx); wordIdx++) {
                    marian::string_view word = response.source.word(sentenceIdx, wordIdx);
                    result.emplace_back(word.data(), word.size());
                  }
                  return result;
                }))
      .function("getTargetTokens",
                std::function<std::vector<std::string>(Response &, size_t)>([](Response &response, size_t sentenceIdx) {
                  std::vector<std::string> result;
                  for (size_t wordIdx = 0; wordIdx < response.target.numWords(sentenceIdx); wordIdx++) {
                    marian::string_view word = response.target.word(sentenceIdx, wordIdx);
                    result.emplace_back(word.data(), word.size());
                  }
                  return result;
                }))
      .property("alignments", &Response::alignments);

  register_vector<Response>("VectorResponse");
}
