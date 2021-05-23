#ifndef SRC_BERGAMOT_SENTENCE_SPLITTER_H_
#define SRC_BERGAMOT_SENTENCE_SPLITTER_H_

#include <string>

#include "aligned.h"
#include "common/options.h"
#include "data/types.h"
#include "definitions.h"
#include "ssplit.h"

namespace marian {
namespace bergamot {

class SentenceSplitter {
  // A wrapper around @ugermann's ssplit-cpp compiled from several places in
  // mts. Constructed based on options. Used in TextProcessor below to create
  // sentence-streams, which provide access to one sentence from blob of text at
  // a time.
 public:
  explicit SentenceSplitter(Ptr<Options> options, const std::string &ssplit_prefix_file);
  explicit SentenceSplitter(Ptr<Options> options, const AlignedMemory &memory);
  ug::ssplit::SentenceStream createSentenceStream(string_view const &input);

 private:
  void loadFromMemory(const AlignedMemory &memory);
  void loadFromFile(const std::string &filename);
  ug::ssplit::SentenceSplitter ssplit_;
  Ptr<Options> options_;
  ug::ssplit::SentenceStream::splitmode mode_;
  ug::ssplit::SentenceStream::splitmode string2splitmode(const std::string &m);
};

}  // namespace bergamot
}  // namespace marian

#endif  //  SRC_BERGAMOT_SENTENCE_SPLITTER_H_
