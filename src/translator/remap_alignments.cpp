#include "remap_alignments.h"

#include "response.h"

namespace marian::bergamot {

std::vector<Alignment> remapAlignments(const Response &first, const Response &second) {
  // For each sentence.
  size_t m, n, p;
  std::vector<Alignment> alignments;
  for (size_t s = 0; s < first.source.numSentences(); s++) {
    // Rough sketch.
    // first has s_i, q_j
    // second has q'_j', t_k
    // we need to be able to express p(q'_j' | tk) in terms of q_j, and we're set. For this purpose, we take ByteRanges
    // from q_j, do arithmetic on overlapping probabilities on q'_j', and precompute a table.
    // There are optimizations probably, but we go for the safe choice.

    // m x n x p tokens expected.
    auto SP = first.alignments[s];
    auto PT = second.alignments[s];
    m = SP.size();
    n = SP[0].size();
    p = PT[0].size();

    // PT is n'xp, not nxp do to vocab mismatch
    Alignment rePT(n, std::vector<float>(p, 0.0));

    size_t t = 0, T = second.target.numWords(s);
    size_t w = 0, W = second.source.numWords(s);

    while (t < T and w < W) {
      ByteRange word = first.target.wordAsByteRange(s, w);
      // [word.begin, word.end] needs to be located in the other. Chances are the tokens map monotonically. We can
      // advance pointer as we consume.

      // Find [t, ...T] elements overlapping with word.begin, word.end
      ByteRange otherWord = second.source.wordAsByteRange(s, t);

      // Does otherword match exactly, if then we're good.
      if (word.begin == otherWord.begin and word.end == otherWord.end) {
        for (size_t j = 0; j < p; j++) {
          rePT[w][j] = PT[t][j];
        }
        ++t, ++w;
      } else {
        // How much of otherword contained?
        // l >= otherword.begin, r <= otherword.end
        size_t l, r;
        float denom, fraction;
        if (otherWord.size() == 0 || word.size() == 0) {
          fraction = 1.0f;
        } else {
          r = std::min<size_t>(otherWord.end, word.end);
          l = std::max<size_t>(otherWord.begin, word.begin);
          denom = static_cast<float>(otherWord.size());
          fraction = (float)(r - l) / denom;
        }

        // assert(0.0f <= fraction and fraction <= 1.0f);

        for (size_t j = 0; j < p; j++) {
          rePT[w][j] += (fraction * PT[t][j]);
        }

        // Did we fully consume otherWord
        if (r == otherWord.end) {
          ++t;
          // otherWord = second.source.wordAsByteRange(s, t);
        }

        if (r == word.end) {
          ++w;
          // word = first.source.wordAsByteRange(s, w);
        }
      }
    }

    // ABORT_IF(n != PT.size(), "Matrix dimensions mismatch, look into character stuff");
    Alignment output(m, std::vector<float>(p));
    // Don't try matrix multiplication, lets craft alignment compilation with the rest.
    if (n == rePT.size()) {
      for (size_t i = 0; i < m; i++) {
        for (size_t k = 0; k < p; k++) {
          for (size_t j = 0; j < n; j++) {
            output[i][k] += SP[i][j] * rePT[j][k];
          }
        }
      }
    }

    alignments.push_back(output);
  }
  return alignments;
}
}  // namespace marian::bergamot
