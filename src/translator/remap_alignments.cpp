#include "remap_alignments.h"

#include "response.h"

namespace marian::bergamot {

std::vector<Alignment> remapAlignments(const Response &first, const Response &second) {
  // For each sentence.
  size_t m, n, p;
  std::vector<Alignment> alignments;
  for (size_t s = 0; s < first.source.numSentences(); s++) {
    // m x n x p tokens expected.
    auto SP = first.alignments[s];
    auto PT = second.alignments[s];
    m = SP.size();
    n = SP[0].size();
    p = PT[0].size();
    // ABORT_IF(n != PT.size(), "Matrix dimensions mismatch, look into character stuff");
    Alignment output(m, std::vector<float>(p));
    // Don't try matrix multiplication, lets craft alignment compilation with the rest.
    if (n == PT.size()) {
      for (size_t i = 0; i < m; i++) {
        for (size_t k = 0; k < p; k++) {
          for (size_t j = 0; j < n; j++) {
            output[i][k] += SP[i][j] * PT[j][k];
          }
        }
      }
    }

    alignments.push_back(output);
  }
  return alignments;
}
}  // namespace marian::bergamot
