#ifndef SRC_BERGAMOT_RESPONSE_OPTIONS_H_
#define SRC_BERGAMOT_RESPONSE_OPTIONS_H_
#include <string>

namespace marian {
namespace bergamot {

/// ResponseOptions dictate how to construct a Response for an input string of
/// text to be translated.
struct ResponseOptions {
  bool qualityScores{false};  ///< Include quality-scores or not.
  bool alignment{false};      ///< Include alignments or not.
  bool HTML{false};           ///< Remove HTML tags from text and insert in output.
};

}  // namespace bergamot
}  // namespace marian

#endif  //  SRC_BERGAMOT_RESPONSE_OPTIONS_H_
