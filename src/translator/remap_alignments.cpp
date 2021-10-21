#include "remap_alignments.h"

#include "response.h"

namespace marian::bergamot {

std::vector<Alignment> remapAlignments(Response &first, Response &second) {
  // For each sentence.
  size_t m, n, p;
  std::vector<Alignment> alignments;
  for (size_t s = 0; s < first.source.numSentences(); s++) {
    // m x n x p tokens expected.
    auto SP = first.alignments[s];
    auto PT = second.alignments[s];
    Alignment output(m, std::vector<float>(p));
    for (size_t i = 0; i < m; i++) {
      for (size_t k = 0; k < p; k++) {
        for (size_t j = 0; j < n; j++) {
          output[i][k] += SP[i][j] * PT[j][k];
        }
      }
    }

    alignments.push_back(output);
  }
  return alignments;
}
}  // namespace marian::bergamot
