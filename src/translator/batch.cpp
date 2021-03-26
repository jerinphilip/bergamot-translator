#include "batch.h"
#include "request.h"

namespace marian {
namespace bergamot {

void Batch::log() {
  LOG(info, "Batch(tokens={}, max-length={}, sentences_={})", numTokens_,
      maxLength_, sentences_.size());
}

void Batch::add(const RequestSentence &sentence) {
  sentences_.push_back(sentence);
  numTokens_ += sentence.numTokens();
  maxLength_ = std::max<size_t>(maxLength_, sentence.numTokens());
}

void Batch::completeBatch(const Histories &histories) {
  for (size_t i = 0; i < sentences_.size(); i++) {
    sentences_[i].completeSentence(histories[i]);
  }
}
} // namespace bergamot
} // namespace marian
