#include "remap_alignments.h"

#include "response.h"

namespace marian::bergamot {

// We're marginalizing q out of p(s | q) x p( q | t). However, we have different representations of q on source side and
// target side. sQ denotes something towards source side while Qt denotes something towards target. T denotes target.
// Smaller-case variables denote single elements.
Alignment transferThroughCharacters(const std::vector<ByteRange> &sQ, const std::vector<ByteRange> &Qt,
                                    const std::vector<ByteRange> &T, const Alignment &QtT) {
  // Initialize an empty alignment matrix.
  Alignment remapped(T.size(), std::vector<float>(sQ.size(), 0.0f));

  auto sq = sQ.begin();
  auto qt = Qt.begin();
  while (sq != sQ.end() && qt != Qt.end()) {
    size_t i, j;
    i = std::distance(sQ.begin(), sq);
    j = std::distance(Qt.begin(), qt);
    if (sq->begin == qt->begin && sq->end == qt->end) {
      for (size_t t = 0; t < T.size(); t++) {
        remapped[t][i] += QtT[t][j];
      }

      // Perfect match, move pointer from both.
      sq++, qt++;
    } else {
      // Do we have overlap? There may be an easier way to do this, exhausting for now.
      //                 qt:[begin, end)
      // sq:[begin, end)
      // All our computations are w.r.t sq; ie, we need remapped[sq];
      // There are two or three cases if exhausted for a fixed sq.
      // 1. qt is lagging behind with overlap (sQ[sq].begin
      // 2. qt is ahead with overlap.
      // 3. qt is within with overlap.
      //
      // Other cases:
      // 4. qt is ahead no overlap.
      // 5. qt is behind no overlap.
      //
      // Since we're traversing the same underlying string, we can set invariant such that everything before
      // sq.begin is already mapped to something. Case 5 should not be possible. Case 4 should not be possible
      // because that would mean we skipped a qt and didn't compute.

      assert(qt->size() != 0 && sq->size() != 0);
      // assert(!4), assert(!5)?

      if (sq->begin > qt->begin && sq->begin < qt->end) {
        // lagging behind, with overlap.
        size_t charCount = (sq->begin - qt->begin);
        size_t probSpread = qt->size();

        float fraction = static_cast<float>(charCount) / static_cast<float>(probSpread);
        for (size_t t = 0; t < T.size(); t++) {
          remapped[t][i] += fraction * QtT[t][j];
        }
        // Advance qt. Now a lagging behind pointer is updated qt has additional overlaps with current sq;
        ++qt;
      } else if (sq->end > qt->begin && sq->end < qt->end) {
        size_t charCount = (sq->end - qt->begin);
        size_t probSpread = qt->size();

        float fraction = static_cast<float>(charCount) / static_cast<float>(probSpread);
        for (size_t t = 0; t < T.size(); t++) {
          remapped[t][i] += fraction * QtT[t][j];
        }

        // advance sq; Now the new sq will have overlaps with qt
        ++sq;
      } else if (sq->begin <= qt->begin && sq->end >= qt->end) {
        for (size_t t = 0; t < T.size(); t++) {
          remapped[t][i] += QtT[t][j];  // No need of fraction, all probability mass contained.
        }

        // The qt within sq is processed, next qt will have overlap and could be ahead, but can also just truncate
        // at the end of sq.
        ++qt;
        if (sq->end == qt->end) sq++;
      }
    }
  }

  // At the end, assert what we have is a valid probability distribution.
#ifdef DEBUG
  for (size_t t = 0; t < T.size(); t++) {
    float sum = 0.0f;
    for (size_t q = 0; q < sQ.size(); q++) {
      sum += remapped[t][q];
    }

    const float EPS = 1e-6;
    assert(std::abs(sum - 1.0f) < EPS);
  }
#endif

  return remapped;
}

std::vector<Alignment> remapAlignments(const Response &first, const Response &second) {
  std::vector<Alignment> alignments;
  for (size_t s = 0; s < first.source.numSentences(); s++) {
    auto SP = first.alignments[s];
    const Alignment &QtT = second.alignments[s];

    size_t nS, nsQ, nQt, nT;
    std::vector<ByteRange> sQ, Qt, T;

    nS = first.source.numWords(s);
    nsQ = first.target.numWords(s);
    nQt = second.source.numWords(s);
    nT = second.target.numWords(s);

    for (size_t i = 0; i < nsQ; i++) {
      sQ.push_back(first.target.wordAsByteRange(s, i));
    }

    for (size_t i = 0; i < nQt; i++) {
      Qt.push_back(second.source.wordAsByteRange(s, i));
    }

    for (size_t i = 0; i < nT; i++) {
      T.push_back(second.target.wordAsByteRange(s, i));
    }

    // Reintrepret probability p(q'_j' | t_k) as p(q_j | t_k)
    Alignment transferredPT = transferThroughCharacters(sQ, Qt, T, QtT);

    // Marginalize out q_j.
    // p(s_i | t_k) = \sum_{j} p(s_i | q_j) x p(q_j | t_k)
    Alignment output(nT, std::vector<float>(nS));
    for (size_t ids = 0; ids < nS; ids++) {
      for (size_t idq = 0; idq < nsQ; idq++) {
        for (size_t idt = 0; idt < nT; idt++) {
          output[idt][ids] += SP[idq][ids] * transferredPT[idt][idq];
        }
      }
    }

    alignments.push_back(output);
  }
  return alignments;
}
}  // namespace marian::bergamot