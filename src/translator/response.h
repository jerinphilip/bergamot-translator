#ifndef SRC_BERGAMOT_RESPONSE_H_
#define SRC_BERGAMOT_RESPONSE_H_

#include "data/alignment.h"
#include "data/types.h"
#include "definitions.h"
#include "sentence_ranges.h"
#include "translator/beam_search.h"

#include <cassert>
#include <string>
#include <vector>

namespace marian {
namespace bergamot {

/// Alignment is stored as a sparse matrix, this pretty much aligns with marian
/// internals but is brought here to maintain translator
/// agnosticism/independence.
struct Point {
  size_t src; ///< Index pointing to source ByteRange
  size_t tgt; ///< Index pointing to target ByteRange
  float prob; ///< Score between [0, 1] on indicating degree of alignment.
};

/// Alignment is a sparse matrix, where Points represent entries with values.
typedef std::vector<Point> Alignment;

/// -loglikelhoods of the sequence components as proxy to quality.
struct Quality {
  /// Certainty/uncertainty score for sequence.
  float sequence;
  /// Certainty/uncertainty for each word in the sequence.
  std::vector<float> word;
};

/// Response holds AnnotatedText(s) of source-text and translated text,
/// alignment information between source and target sub-words and sentences.
///
/// AnnotatedText provides an API to access markings of (sub)-word and
/// sentences boundaries, which are required to interpret Quality and
/// Alignment (s) at the moment.
struct Response {

public:
  /// \cond HIDDEN_PUBLIC
  /// Empty constructor, since there's explicit constructors this is required to
  /// mimic the case where a struct with empty members are created, to be moved
  /// in with legal values as they're written into.
  Response(){};

  // Move constructor.
  Response(Response &&other)
      : source(std::move(other.source)), target(std::move(other.target)),
        alignments(std::move(other.alignments)),
        qualityScores(std::move(other.qualityScores)){};

  // The following copy bans are not strictly required anymore since Annotation
  // is composed of the ByteRange primitive (which was previously string_view
  // and required to be bound to string), but makes movement efficient by
  // banning these letting compiler complain about copies.

#ifndef WASM_BINDINGS
  Response(const Response &) = delete;
  Response &operator=(const Response &) = delete;
#else
  Response(const Response &) = default;
  Response &operator=(const Response &) = default;
#endif

  /// \endcond

  /// Convenience function to obtain number of units translated. Same as
  /// `.source.numSentences()` and `.target.numSentences().` The processing of a
  /// text of into sentences are handled internally, and this information can be
  /// used to iterate through meaningful units of translation for which
  /// alignment and quality information are available.
  const size_t size() const { return source.numSentences(); }

  /// source text and annotations of (sub-)words and sentences.
  AnnotatedText source;

  /// translated text and annotations of (sub-)words and sentences.
  AnnotatedText target;

  /// -logprob of each word and negative log likelihood of sequence (sentence)
  /// normalized by length, for each sentence processed by the translator.
  /// Indices correspond to ranges accessible through respective Annotation on
  /// source or target.
  std::vector<Quality> qualityScores;

  /// Alignments between source and target. Each Alignment is a
  /// sparse matrix representation with indices corresponding
  /// to (sub-)words accessible through Annotation.
  std::vector<Alignment> alignments;

  const std::string &getOriginalText() const { return source.text; }

  const std::string &getTranslatedText() const { return target.text; }
};
} // namespace bergamot
} // namespace marian

#endif // SRC_BERGAMOT_RESPONSE_H_
