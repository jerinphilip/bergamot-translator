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
/// Response is a marian internal class (not a bergamot-translator class)
/// holding source blob of text, vector of TokenRanges corresponding to each
/// sentence in the source text blob and histories obtained from translating
/// these sentences.
///
/// This class provides an API at a higher level in comparison to History to
/// access translations and additionally use string_view manipulations to
/// recover structure in translation from source-text's structure known
/// through reference string and string_view. As many of these computations
/// are not required until invoked, they are computed as required and stored
/// in data members where it makes sense to do so
/// (translation,translationTokenRanges).

typedef marian::data::SoftAlignment SoftAlignment;
typedef marian::data::WordAlignment WordAlignment;
typedef AnnotatedBlobT<string_view> AnnotatedBlob;

class Response {

public:
  Response(std::string &&source, SentenceRanges &&sourceRanges,
           Histories &&histories,
           // Required for constructing translation and TokenRanges within
           // translation lazily.
           std::vector<Ptr<Vocab const>> &vocabs);

  /// Move constructor.
  Response(Response &&other)
      : source(std::move(other.source)), target(std::move(other.target)),
        histories_(std::move(other.histories_)){};

  /// Prevents CopyConstruction.  sourceRanges_ is constituted
  /// by string_view and copying invalidates the data member.
  Response(const Response &) = delete;
  /// Prevents CopyAssignment.
  Response &operator=(const Response &) = delete;

  const size_t size() const { return source.numSentences(); }

  const Histories &histories() const { return histories_; }

  const SoftAlignment softAlignment(int idx) {
    NBestList onebest = histories_[idx]->nBest(1);
    Result result = onebest[0]; // Expecting only one result;
    auto hyp = std::get<1>(result);
    auto alignment = hyp->tracebackAlignment();
    return alignment;
  }

  bool empty() { return empty_; }
  static Response EmptyResponse() {
    Response response = Response();
    response.empty_ = true;
    return response;
  }

  const WordAlignment hardAlignment(int idx, float threshold = 1.f) {
    WordAlignment result;
    return data::ConvertSoftAlignToHardAlign(softAlignment(idx), threshold);
  }

  AnnotatedBlob source;
  AnnotatedBlob target;

  const std::string &translation() {
    LOG(info, "translation() will be deprecated now that target is public.");
    return target.blob;
  }

private:
  Response() {} // Default constructor to enable a static empty response.
  Histories histories_;
  bool empty_{true};
};
} // namespace bergamot
} // namespace marian

#endif // SRC_BERGAMOT_RESPONSE_H_
