#include "response.h"

#include "annotation.h"
#include "definitions.h"

namespace marian::bergamot {

// We're marginalizing q out of p(s | q) x p( q | t). However, we have different representations of q on source side to
// intermediate - p(s_i | q_j) and intermediate to target side - p(q'_j' | t_k).
//
// The input here is the ByteRanges in sQ (q towards source side), ByteRanges in Qt(q towards target side) and the
// alignment matrix computed with Qt to target T. This matrix p(q'_j' | t_k) is rewritten into p(q_j | t_k) by means of
// spreading the probability in the former over bytes and collecting it at the ranges specified by latter, using a two
// pointer accumulation strategy.
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
      // Do we have overlap?
      size_t l = std::max(qt->begin, sq->begin);
      size_t r = std::min(qt->end, sq->end);

      assert(l <= r);  // there should be overlap.

      size_t charCount = r - l;
      size_t probSpread = qt->size();
      float fraction = probSpread == 0 ? 1.0f : static_cast<float>(charCount) / static_cast<float>(probSpread);
      for (size_t t = 0; t < T.size(); t++) {
        remapped[t][i] += fraction * QtT[t][j];
      }

      // Which one is ahead? sq or qt or both end at same point?
      if (sq->end == qt->end) {
        sq++;
        qt++;
      } else if (sq->end > qt->end) {
        qt++;
      } else {  // sq->end < qt->end
        sq++;
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
  for (size_t sentenceId = 0; sentenceId < first.source.numSentences(); sentenceId++) {
    const Alignment &SsQ = first.alignments[sentenceId];
    const Alignment &QtT = second.alignments[sentenceId];

    size_t nS, nsQ, nQt, nT;
    std::vector<ByteRange> sQ, Qt, T;

    nS = first.source.numWords(sentenceId);
    nsQ = first.target.numWords(sentenceId);
    nQt = second.source.numWords(sentenceId);
    nT = second.target.numWords(sentenceId);

    for (size_t i = 0; i < nsQ; i++) {
      sQ.push_back(first.target.wordAsByteRange(sentenceId, i));
    }

    for (size_t i = 0; i < nQt; i++) {
      Qt.push_back(second.source.wordAsByteRange(sentenceId, i));
    }

    for (size_t i = 0; i < nT; i++) {
      T.push_back(second.target.wordAsByteRange(sentenceId, i));
    }

    // Reintrepret probability p(q'_j' | t_k) as p(q_j | t_k)
    Alignment sQT = transferThroughCharacters(sQ, Qt, T, QtT);

    // Marginalize out q_j.
    // p(s_i | t_k) = \sum_{j} p(s_i | q_j) x p(q_j | t_k)
    Alignment output(nT, std::vector<float>(nS, 0.0f));
    for (size_t ids = 0; ids < nS; ids++) {
      for (size_t idq = 0; idq < nsQ; idq++) {
        for (size_t idt = 0; idt < nT; idt++) {
          // Matrices are of for p(s | t) = P[t][s], hence idq appears on the extremes.
          output[idt][ids] += SsQ[idq][ids] * sQT[idt][idq];
        }
      }
    }

    alignments.push_back(output);
  }
  return alignments;
}

}  // namespace marian::bergamot
