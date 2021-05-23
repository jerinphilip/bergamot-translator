#ifndef SRC_BERGAMOT_TEXT_PROCESSOR_H_
#define SRC_BERGAMOT_TEXT_PROCESSOR_H_

#include <vector>

#include "aligned.h"
#include "annotation.h"
#include "data/types.h"
#include "data/vocab.h"
#include "definitions.h"
#include "ssplit.h"
#include "vocabs.h"

namespace marian {
namespace bergamot {

class TextProcessor {
  /// TextProcessor handles loading the sentencepiece vocabulary and also
  /// contains an instance of sentence-splitter based on ssplit.
  ///
  /// Used in Service to convert an incoming blog of text to a vector of
  /// sentences (vector of words). In addition, the ByteRanges of the
  /// source-tokens in unnormalized text are provided as string_views.
 public:
  explicit TextProcessor(Ptr<Options>, const Vocabs &vocabs, const std::string &ssplit_prefix_file);
  explicit TextProcessor(Ptr<Options>, const Vocabs &vocabs, const AlignedMemory &memory);

  /// Wrap into sentences of at most maxLengthBreak_ tokens and add to source.
  /// @param [in] blob: Input blob, will be bound to source and annotations on it stored.
  /// @param [out] source: AnnotatedText instance holding input and annotations of sentences and pieces
  /// @param [out] segments: marian::Word equivalents of the sentences processed and stored in AnnotatedText for
  /// consumption of marian translation pipeline.
  void process(std::string &&blob, AnnotatedText &source, Segments &segments);

 private:
  void parseCommonOptions(Ptr<Options> options);

  /// Tokenizes an input string, returns Words corresponding. Loads the
  /// corresponding byte-ranges into tokenRanges.
  Segment tokenize(const string_view &input, std::vector<string_view> &tokenRanges);

  /// Wrap into sentences of at most maxLengthBreak_ tokens and add to source.
  void wrap(Segment &sentence, std::vector<string_view> &tokenRanges, Segments &segments, AnnotatedText &source);

  const Vocabs &vocabs_;   ///< Vocabularies used to tokenize a sentence
  size_t maxLengthBreak_;  ///< Parameter used to wrap sentences to a maximum number of tokens

  /// SentenceSplitter compatible with moses sentence-splitter
  ug::ssplit::SentenceSplitter ssplit_;

  /// Mode of splitting, can be line ('\n') based, sentence or paragraph based.
  ug::ssplit::SentenceStream::splitmode ssplitMode_;
};

}  // namespace bergamot
}  // namespace marian

#endif  // SRC_BERGAMOT_TEXT_PROCESSOR_H_
