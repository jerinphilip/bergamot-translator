
#include <set>
#include <utility>

#include "catch.hpp"
#include "translator/oov_handler.h"

using namespace marian::bergamot;
using namespace marian;

TEST_CASE("ControlTokenAllocator class") {
  ControlTokenAllocator allocator(/*numTokens=*/20);

  auto debugPrint = [](std::vector<size_t> &registers) {
    for (auto &t : registers) {
      std::cout << t << " ";
    }
    std::cout << "\n";
  };

  std::vector<size_t> registers;
  // std::vector<bool> options = {false, true};
  std::vector<bool> options = {false};
  size_t iterations = 100;
  size_t numSamples = 10;
  for (auto withReplacement : options) {
    std::cout << (withReplacement ? "With" : "\nWithout") << " replacement: \n";
    for (size_t i = 0; i < iterations; i++) {
      registers.clear();
      allocator.allocate(numSamples, registers, withReplacement);
      debugPrint(registers);
    }
  }
}

TEST_CASE("OOVHandler class") {
  auto stringViewsFromPairs = [](std::string &ref, std::vector<std::pair<size_t, size_t>> &sourceMarkers) {
    std::vector<string_view> sourceStringViews;
    for (auto p : sourceMarkers) {
      const char *data = &(ref[p.first]);
      sourceStringViews.emplace_back(data, p.second);
    }
    return sourceStringViews;
  };

  auto wordsFromIdxs = [](std::vector<size_t> &wordIdxs) {
    Words words;
    for (auto &wordIdx : wordIdxs) {
      words.push_back(Word::fromWordIndex(wordIdx));
    }
    return words;
  };

  // Example 1
  std::string source =
      "Also Known As 🥺 Begging 🥺 Glossy Eyes 🥺 Simp; Apple Name 🥺 Pleading Face; Unicode Name 🥺 Face "
      "with Pleading Eyes";
  std::vector<std::pair<size_t, size_t>> sourceMarkers = {
      {0, 4},    //  ▁Also|Also|Also
      {4, 5},    //  ▁Know| Know| Know
      {9, 1},    //  n|n|n
      {10, 3},   //  ▁As| As| As
      {13, 1},   //  ▁| |
      {14, 4},   //  🥺|🥺| ⁇
      {18, 3},   //  ▁Be| Be| Be
      {21, 2},   //  gg|gg|gg
      {23, 3},   //  ing|ing|ing
      {26, 1},   //  ▁| |
      {27, 4},   //  🥺|🥺| ⁇
      {31, 4},   //  ▁Glo| Glo| Glo
      {35, 3},   //  ssy|ssy|ssy
      {38, 4},   //  ▁Eye| Eye| Eye
      {42, 1},   //  s|s|s
      {43, 1},   //  ▁| |
      {44, 4},   //  🥺|🥺| ⁇
      {48, 2},   //  ▁S| S| S
      {50, 3},   //  imp|imp|imp
      {53, 1},   //  ;|;|;
      {54, 6},   //  ▁Apple| Apple| Apple
      {60, 5},   //  ▁Name| Name| Name
      {65, 1},   //  ▁| |
      {66, 4},   //  🥺|🥺| ⁇
      {70, 2},   //  ▁P| P| P
      {72, 4},   //  lead|lead|lead
      {76, 3},   //  ing|ing|ing
      {79, 5},   //  ▁Face| Face| Face
      {84, 1},   //  ;|;|;
      {85, 4},   //  ▁Uni| Uni| Uni
      {89, 4},   //  code|code|code
      {93, 5},   //  ▁Name| Name| Name
      {98, 1},   //  ▁| |
      {99, 4},   //  🥺|🥺| ⁇
      {103, 5},  //  ▁Face| Face| Face
      {108, 5},  //  ▁with| with| with
      {113, 2},  //  ▁P| P| P
      {115, 4},  //  lead|lead|lead
      {119, 3},  //  ing|ing|ing
      {122, 4},  //  ▁Eye| Eye| Eye
      {126, 1}   //  s|s|s
  };
  std::vector<size_t> wordIdxs = {2782, 9882, 22,   510,  7,     1,  290,   4027, 46,  7,     1,   8037,  8778, 12275,
                                  4,    7,    1,    101,  11142, 60, 6725,  1748, 7,   1,     183, 19384, 46,   14757,
                                  60,   4662, 4274, 1748, 7,     1,  14757, 32,   183, 19384, 46,  12275, 4};

  std::vector<string_view> sourceViews = stringViewsFromPairs(source, sourceMarkers);
  Words sourceWords = wordsFromIdxs(wordIdxs);

  Datum sourceDatum = {sourceWords, sourceViews, source};

  marian::Words controlTokens;
  size_t numControlTokens = 20, offset = 32000;
  size_t unkId = 1;
  for (size_t w = offset; w < offset + numControlTokens; w++) {
    Word controlToken = Word::fromWordIndex(w);
    controlTokens.push_back(controlToken);
  }

  Word unkWord = Word::fromWordIndex(unkId);
  OOVHandler oovHandler(controlTokens, unkWord);
  oovHandler.preprocess_inference(sourceDatum);
  for (size_t i = 0; i < wordIdxs.size(); i++) {
    if (wordIdxs[i] == unkId) {
      auto transformed = sourceDatum.words[i].toWordIndex();
      CHECK(transformed != unkId);
      CHECK(transformed > offset);
      CHECK(transformed < offset + numControlTokens);
    }
  }
}
