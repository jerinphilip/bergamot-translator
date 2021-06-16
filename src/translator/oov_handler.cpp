#include "oov_handler.h"

namespace marian {
namespace bergamot {

void ControlTokenAllocator::allocate(size_t numTokens, std::vector<size_t> &allocatedRegisters, bool withReplacement) {
  assert(numTokens < registers_.size());
  if (withReplacement) {
    std::cerr << "Not implemented yet.\n";
    std::abort();
  } else {
    std::sample(registers_.begin(), registers_.end(), std::back_inserter(allocatedRegisters), numTokens, randomGen_);
  }
}

void OOVHandler::preprocess_training(Datum &source, Datum &target, const HardAlignmentInfo &align) {
  // Compute unknowns across source, target (union).
  size_t sourceUnks, targetUnks, commonUnks;
  sourceUnks = countUnknowns(source.words);
  targetUnks = countUnknowns(target.words);

  commonUnks = 0;
  for (size_t t = 0; t < target.words.size(); t++) {
    size_t s = align[t];
    if (source.words[s] == unkWord_ && target.words[t] == unkWord_) {
      ++commonUnks;
    }
  }

  // #(A U B) = #A + #B - #(A^B)
  size_t numUnks = sourceUnks + targetUnks - commonUnks;

  // Allocate registers, balancing out sampling.
  std::vector<size_t> registers;
  allocator_.allocate(numUnks, registers);

  // All aligned (s, t) have matching control tokens. This guides the NMT attention properly.
  size_t registerIdx = 0;
  for (size_t t = 0; t < target.words.size(); t++) {
    size_t s = align[t];
    if (source.words[s] == unkWord_ && target.words[t] == unkWord_) {
      Word controlTokenIdx = controlTokenIdxs_[registers[registerIdx++]];
      source.words[s] = controlTokenIdx;
      target.words[t] = controlTokenIdx;
    }
  }

  // Cases: source unknown, without matching control token.
  registerIdx = allocateIndividual(source.words, registers, registerIdx);
  registerIdx = allocateIndividual(target.words, registers, registerIdx);
}

void OOVHandler::preprocess_inference(Datum &source) {
  std::vector<size_t> registers;
  size_t numUnks = countUnknowns(source.words);
  allocator_.allocate(numUnks, registers);

  size_t registerIdx = 0;
  allocateIndividual(source.words, registers, registerIdx);
}

size_t OOVHandler::allocateIndividual(Words &words, const std::vector<size_t> &registers, size_t startRegister) {
  for (size_t i = 0; i < words.size(); i++) {
    if (words[i] == unkWord_) {
      Word controlTokenIdx = controlTokenIdxs_[registers[startRegister++]];
      words[i] = controlTokenIdx;
    }
  }
  return startRegister;
}

size_t OOVHandler::countUnknowns(const Words &words) const {
  size_t numUnks = 0;
  for (auto &word : words) {
    if (word == unkWord_) {
      numUnks++;
    }
  }
  return numUnks;
}

}  // namespace bergamot
}  // namespace marian
