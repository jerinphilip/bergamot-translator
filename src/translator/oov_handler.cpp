#include "oov_handler.h"

namespace marian {
namespace bergamot {

void OOVHandler::preprocess_training(Datum &source, Datum &target, const HardAlignmentInfo &align) {
  // Compute unknowns across source, target (union).
  size_t sourceUnks, targetUnks, commonUnks;
  sourceUnks = countUnknowns(source.words);
  targetUnks = countUnknowns(target.words);

  size_t commonUnks = 0;
  for (size_t t = 0; t < target.words.size(); t++) {
    size_t s = align[t];
    if (source.words[s] == unkId_ && target.words[t] == unkId_) {
      ++commonUnks;
    }
  }

  size_t numUnks = sourceUnks + targetUnks - 2 * commonUnks;

  // Allocate registers, balancing out sampling.
  std::vector<size_t> registers;
  allocator_.allocate(numUnks, registers);

  // All aligned (s, t) have matching control tokens.
  size_t registerIdx = 0;
  for (size_t t = 0; t < target.words.size(); t++) {
    size_t s = align[t];
    if (source.words[s] == unkId_ && target.words[t] == unkId_) {
      controlTokenIdx = controlTokenIdxs_[registers[registerIdx++]];
      source.words[s] = controlTokenIdx;
      target.words[t] = controlTokenIdx;
    }
  }

  // Cases: source unknown, without matching control token.
  registerIdx = allocateIndividual(source.words, registerIdx);
  registerIdx = allocateIndividual(target.words, registerIdx);
}

void OOVHandler::preprocess_inference(Datum &source) {
  size_t registerIdx = 0;
  size_t numUnks = countUnknowns(source.words);
  allocator_.allocate(numUnks, registers);
  allocateIndividual(source.words, registerIdx);
}

size_t OOVHandler::allocateIndividual(Words &words, size_t startRegister) {
  for (size_t t = 0; t < words.size(); t++) {
    if (words[t] == unkId_) {
      controlTokenIdx = controlTokenIdxs_[registers[startRegister++]];
      words[t] = controlTokenIdx;
    }
  }
  return startRegister;
}

size_t OOVHandler::countUnknowns(const std::vector<Words> &words) const {
  size_t numUnks = 0;
  for (auto &word : words) {
    if (word == unkId_) {
      numUnks++;
    }
  }
  return numUnks;
}

}  // namespace bergamot
}  // namespace marian
