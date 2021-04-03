/*
 * TranslationModelBindings.cpp
 *
 * Bindings for TranslationModel class
 */

#include <emscripten/bind.h>

#include "response.h"
#include "service.h"

using namespace emscripten;

typedef Service TranslationModel;
typedef Response TranslationResult;

// Binding code
EMSCRIPTEN_BINDINGS(translation_model) {
  class_<TranslationModel>("TranslationModel")
      .constructor<std::string>()
      .function("translate", &TranslationModel::translateMultiple);

  register_vector<std::string>("VectorString");
  register_vector<TranslationResult>("VectorTranslationResult");
}
