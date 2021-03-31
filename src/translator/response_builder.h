#ifndef SRC_BERGAMOT_RESPONSE_BUILDER_H_
#define SRC_BERGAMOT_RESPONSE_BUILDER_H_

#include "data/types.h"
#include "response.h"

// For now we will work with this, to avoid complaints another structure is hard
// to operate with.

typedef marian::Ptr<marian::Options> RequestParams;
// typedef TranslationRequest RequestParams;

namespace marian {
namespace bergamot {

/// ResponseBuilder is a callback functor. It is expected to be bound to a
/// Request after giving it the context of options, vocabs and promise to set.
/// It constructs the Response and it's members based on options
/// (quality=on|off, alignments=on|off, mappings=on|off, splitmode=sentence |
/// paragraph).

class ResponseBuilder {
public:
  /// @param [in] params: RequestParams, also an alias for TranslationRequest
  /// @param [in] vocabs: marian vocab object (used in decoding)
  //  @param [in] promise: promise to set with the constructed Response
  ResponseBuilder(RequestParams params, AnnotatedText &&source,
                  std::vector<Ptr<Vocab const>> &vocabs,
                  std::promise<Response> &&promise)
      : params_(params), source_(std::move(source)), vocabs_(&vocabs),
        promise_(std::move(promise)) {}

  /// Constructs and sets the promise of a Response object from obtained
  /// histories after translating.
  /// @param [in] histories: Histories obtained after translating the Request
  /// from which this functor is called.
  void operator()(Histories &&histories) {
    // TODO(jerinphilip) load RequestParams into options and turn build
    // functions on or off.
    // PART 1: Freeze Response and fix Request pipeline.
    // existingBuild(std::move(histories));

    // PART 2: Uncomment below and test the other half.
    replacementBuild(std::move(histories));
  }

  void existingBuild(Histories &&histories) {
    Response response(std::move(source_), std::move(histories), *vocabs_);
    promise_.set_value(std::move(response));
  }

  void replacementBuild(Histories &&histories) {
    // params_ is unused, but we can try something here.
    ABORT_IF(source_.numSentences() != histories.size(),
             "Mismatch in source and translated sentences");
    Response response;

    // Move source_ into response.
    response.source = std::move(source_);

    // Should be after source is set
    buildTranslatedText(histories, response);

    // Should always be after buildTranslatedText
    buildQualityScores(histories, response);

    buildAlignments(histories, response);

    // Once complete, set promise.
    promise_.set_value(std::move(response));
  }

private:
  /// Builds qualityScores from histories and writes to response. expects
  /// buildTranslatedText to be run before to be able to obtain target text and
  /// subword information.
  /// @param histories [in]
  /// @param response [out]
  void buildQualityScores(Histories &histories, Response &response);

  /// Builds alignments from histories and writes onto response.
  /// @param histories [in]
  /// @param response [out]
  void buildAlignments(Histories &histories, Response &response);

  /// Builds translated text and subword annotations and writes onto response.
  /// @param histories [in]
  /// @param response [out]
  void buildTranslatedText(Histories &histories, Response &response);

  // Data members are context/curried args for the functor.

  RequestParams params_;
  std::vector<Ptr<Vocab const>> *vocabs_; // vocabs are required for decoding
                                          // and any source validation checks.
  std::promise<Response> promise_; //  To be set when callback triggered and
                                   //  after Response constructed.
  AnnotatedText source_;
};
} // namespace bergamot
} // namespace marian

#endif //  SRC_BERGAMOT_RESPONSE_BUILDER_H_
