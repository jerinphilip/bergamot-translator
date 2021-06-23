#ifndef BERGAMOT_SENTENCE_RANGES_H_
#define BERGAMOT_SENTENCE_RANGES_H_

#include <cassert>
#include <set>
#include <utility>
#include <vector>

#include "data/types.h"
#include "definitions.h"

namespace marian {
namespace bergamot {

/// Annotation expresses sentence and token boundary information as ranges of
/// bytes in a string, but does not itself own the string.
///
/// See also AnnotatedText, which owns Annotation and the string. AnnotatedText
/// wraps these ByteRange functions to provide a string_view interface.
///
/// Text is divided into gaps (whitespace between sentences) and sentences like
/// so:
///   gap sentence gap sentence gap
/// Because gaps appear at the beginning and end of the text, there's always
/// one more gap than there are sentences.
///
/// The entire text is a unbroken sequence of tokens (i.e. the end of a token
/// is the beginning of the next token).  A gap is exactly one token containing
/// whatever whitespace is between the sentences.  A sentence is a sequence of
/// tokens.
///
/// Since we are using SentencePiece, a token can include whitespace.  The term
/// "word" is used, somewhat incorrectly, as a synonym of token.
///
/// A gap can be empty (for example there may not have been whitespace at the
/// beginning).  A sentence can also be empty (typically the translation system
/// produced empty output).  That's fine, these are just empty ranges as you
/// would expect.
class Annotation {
 public:
  /// Initially an empty string.  Populated by AnnotatedText.
  Annotation() {
    token_begin_.push_back(0);
    token_begin_.push_back(0);
    gap_.push_back(0);
  }

  size_t numSentences() const { return gap_.size() - 1; }

  /// Returns number of words in the sentence identified by `sentenceIdx`.
  size_t numWords(size_t sentenceIdx) const {
    return gap_[sentenceIdx + 1] - gap_[sentenceIdx] - 1 /* minus the gap */;
  }

  /// Returns a ByteRange representing `wordIdx` in sentence indexed by
  /// `sentenceIdx`. `wordIdx` follows 0-based indexing, and should be less than
  /// `.numWords()` for `sentenceIdx` for defined behaviour.
  ByteRange word(size_t sentenceIdx, size_t wordIdx) const {
    size_t tokenIdx = gap_[sentenceIdx] + 1 + wordIdx;
    return ByteRange{token_begin_[tokenIdx], token_begin_[tokenIdx + 1]};
  }

  /// Returns a ByteRange representing sentence corresponding to `sentenceIdx`.
  /// `sentenceIdx` follows 0-based indexing, and behaviour is defined only when
  /// less than `.numSentences()`.
  ByteRange sentence(size_t sentenceIdx) const {
    return ByteRange{
        token_begin_[gap_[sentenceIdx] + 1], /*end of whitespace before */
        token_begin_[gap_[sentenceIdx + 1]]  /*beginning of whitespace after */
    };
  }

  ByteRange gap(size_t gapIdx) const {
    size_t tokenIdx = gap_[gapIdx];
    return ByteRange{token_begin_[tokenIdx], token_begin_[tokenIdx + 1]};
  }

  // Let's know if a unit/piece is foreign to the vocabulary (<unk>) to enable replacements
  bool isUnknown(size_t sentenceIdx, size_t wordIdx) const {
    std::pair<size_t, size_t> query = std::make_pair(sentenceIdx, wordIdx);
    return unks_.find(query) != unks_.end();
  }

  // Marks a (sentenceIdx, wordIdx) as foreign to vocabulary <unk>
  void markUnknown(size_t sentenceIdx, size_t wordIdx) { unks_.insert(std::make_pair(sentenceIdx, wordIdx)); }

  void markUnknownsInLastSentence(std::vector<size_t> &unkIds) {
    size_t sentenceIdx = numSentences() - 1;
    for (auto &wordIdx : unkIds) {
      markUnknown(sentenceIdx, wordIdx);
    }
  }

 private:
  friend class AnnotatedText;
  /// Map from token index to byte offset at which it begins.  Token i is:
  ///   [token_begin_[i], token_begin_[i+1])
  /// The vector is padded so that these indices are always valid, even at the
  /// end.  So tokens_begin_.size() is the number of tokens plus 1.
  std::vector<size_t> token_begin_;

  /// Indices of tokens that correspond to gaps between sentences.  These are
  /// indices into token_begin_.
  /// Gap g is byte range:
  ///   [token_begin_[gap_[w]], token_begin_[gap_[w]+1])
  /// Sentence s is byte range:
  ///   [token_begin_[gap_[s]+1], token_begin_[gap_[s+1]])
  /// A sentence does not include whitespace at the beginning or end.
  ///
  /// gap_.size() == numSentences() + 1.
  ///
  /// Example: empty text "" -> just an empty gap.
  /// token_begin_ = {0, 0};
  /// gap_ = {0};
  ///
  /// Example: only space " " -> just a gap containing the space.
  /// token_begin_ = {0, 1};
  /// gap_ = {0};
  ///
  /// Example: one token "hi" -> empty gap, sentence with one token, empty gap
  /// token_begin_ = {0, 0, 2, 2};
  /// gap_ = {0, 2};
  std::vector<size_t> gap_;

  /// Sparse storage of unknowns in the text. Used to enable isUnknown(sentenceIdx, wordIdx)
  std::set<std::pair<size_t, size_t>> unks_;
};

/// AnnotatedText is effectively std::string text + Annotation, providing the
/// following additional desiderata.
///
/// 1. Access to processed string_views for convenience rather than ByteRanges
/// (which only provides index information).
///
/// 2. Transparently convert string_views into ByteRanges for the Annotation
/// referring to the text bound by this structure.
///
/// 3. Bind the text and annotations together, to move around as a meaningful
/// unit.
struct AnnotatedText {
 public:
  std::string text;       ///< Blob of string elements in annotation refers to.
  Annotation annotation;  ///< sentence and (sub-) word annotations.

  /// Construct an empty AnnotatedText. This is useful when the target string or
  /// ByteRanges are not known yet, but the public members can be used to
  /// populate it. One use-case, when translated-text is created decoding from
  /// histories and the ByteRanges only known after the string has been
  /// constructed.
  AnnotatedText() {}

  /// Construct moving in a string (for efficiency purposes, copying string
  /// constructor is disallowed).
  AnnotatedText(std::string &&text);

  /// Appends a sentence to the existing text and transparently rebases
  /// string_views.  Since this tracks only prefix, remember
  /// appendEndingWhitespace.
  /// The string_views must not already be in text.
  void appendSentence(string_view prefix, std::vector<string_view>::iterator tokens_begin,
                      std::vector<string_view>::iterator tokens_end);

  /// Mark unkIdxs supplied in the last sentence to be unknowns.
  void markUnknownsInLastSentence(std::vector<size_t> &unkIdxs);

  /// Append the whitespace at the end of input. string_view must not be in
  /// text.
  void appendEndingWhitespace(string_view whitespace);

  /// Record the existence of a sentence that is already in text.  The
  /// iterators are over string_views for each token that must be in text
  /// already.  This function must be called to record sentences in order.
  /// Normally the beginning of the sentence can be inferred from
  /// tokens_begin->data() but the tokens could be empty, so sentence_begin is
  /// required to know where the sentence is.
  void recordExistingSentence(std::vector<string_view>::iterator tokens_begin,
                              std::vector<string_view>::iterator tokens_end, const char *sentence_begin);

  /// Returns the number of sentences in the annotation structure.
  const size_t numSentences() const { return annotation.numSentences(); }

  /// Returns number of words in the sentece identified by sentenceIdx.
  const size_t numWords(size_t sentenceIdx) const { return annotation.numWords(sentenceIdx); }

  /// Returns a string_view representing wordIdx in sentenceIdx
  string_view word(size_t sentenceIdx, size_t wordIdx) const {
    return asStringView(annotation.word(sentenceIdx, wordIdx));
  }

  /// Returns a string_view representing sentence corresponding to sentenceIdx.
  string_view sentence(size_t sentenceIdx) const { return asStringView(annotation.sentence(sentenceIdx)); }

  /// Returns the string_view of the gap between two sentences in the container.
  ///
  /// More precisely where `i = sentenceIdx, N = numSentences()` for brevity:
  ///
  /// * For `i = 0`: The gap between the start of text and the 0th sentence.
  /// * For `i = 1...N-1`, returns the text comprising of the gap
  ///   between the `i`-th and `i+1`-th sentence.
  /// * For `i = N`, the gap between the last (N-1th) sentence and end of
  ///   text.
  /// @param sentenceIdx: Can be between `[0, numSentences()]`.
  string_view gap(size_t sentenceIdx) const { return asStringView(annotation.gap(sentenceIdx)); }

  /// Returns a ByteRange representing wordIdx in sentenceIdx
  ByteRange wordAsByteRange(size_t sentenceIdx, size_t wordIdx) const { return annotation.word(sentenceIdx, wordIdx); }

  /// Returns a ByteRange representing sentence corresponding to sentenceIdx.
  ByteRange sentenceAsByteRange(size_t sentenceIdx) const { return annotation.sentence(sentenceIdx); }

  // Let's know if a unit/piece is foreign to the vocabulary (<unk>) to enable replacements
  bool isUnknown(size_t sentenceIdx, size_t wordIdx) const { return annotation.isUnknown(sentenceIdx, wordIdx); }

 private:
  string_view asStringView(const ByteRange &byteRange) const {
    return string_view(text.data() + byteRange.begin, byteRange.size());
  }
};

}  // namespace bergamot
}  // namespace marian

#endif  //  BERGAMOT_SENTENCE_RANGES_H_
