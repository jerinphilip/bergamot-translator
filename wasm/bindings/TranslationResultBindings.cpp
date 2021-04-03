/*
 * Bindings for TranslationResult class
 *
 */

#include <emscripten/bind.h>
#include <vector>

// #include "TranslationResult.h"
#include "response.h"

typedef Response TranslationResult;

using namespace emscripten;

// Binding code
EMSCRIPTEN_BINDINGS(translation_result) {
  class_<TranslationResult>("TranslationResult")
      .constructor<>()
      .function("getOriginalText", &TranslationResult::source::text)
      .function("getTranslatedText", &TranslationResult::target::text);
}
