#ifndef SRC_BERGAMOT_OOV_HANDLER_H
#define SRC_BERGAMOT_OOV_HANDLER_H

/// Web data supplied for translation in bergamot project is likely to contain emojis, and sometimes OOV text (e.g:
/// Website has Hindi, Arabic and English and is configured to translate English). It is desirable to pass these through
/// as-is using a "replace-unknown from source" feature.
///
/// Related links:
///
/// * marian-dev#249: Replace <unk> with source token (https://github.com/marian-nmt/marian/issues/249)
/// * fairseq#832: Is there any option to replace unknown words using alignments of the attention mechanism?
///   (https://github.com/pytorch/fairseq/issues/832)
///
/// At the time of writing this, the student models used in bergamot are missing information in the trained models to
/// enable a "replace unknown" feature.
///
/// This machination has to be trickled down all the way from re-training (potentially browsermt/marian-dev might be
/// better). We assume the existence of some mechanism addressing the rare-word problem, like SentencePiece or Byte-Pair
/// Encoding.  Expects a vocabulary implementation that is aware of the unnormalized raw-text an unknown (oov) points
/// to, to be able to allocate reserved control tokens (this is analogous to register allocation).
///
/// This works only for true unknowns - say emoji or foreign characters, i.e no character decompositions are possible.
/// If languages share a common script, say like French and English, both using latin alphabet, SentencePiece (or BPE)
/// trained on English can potentially come up with a decomposition for French and this system will never know of
/// something that should be passed-through. Such mismatches have to be taken care of early on through other means; say
/// for example, language-detection.

#include <algorithm>
#include <random>

namespace marian {
namespace bergamot {

/// A unit for simplifying the description here.
struct Datum {
  Words &words;                     ///< marian::Words coming out of the vocab.
  std::vector<string_view> &views;  ///< views to the unnormalized raw text that 1:1 corresponds to words.
  std::string &line;  ///< string to which the views refer to. These are modified and views rebased accordingly. We
                      ///< might need to modify this in some capacity so we can work with blobs, for bergamot.
};

/// HardAlignmentInfo stores (s, t) pairs which 1:1 maps with each other. This can come from `fast_align` during
/// training or by reducing soft alignments obtained by using "attention" in an NMT model.
/// HardAlignmentInfo align; align[tIdx] = sIdx;
typedef vector<size_t> HardAlignmentInfo;

class OOVHandler {
 public:
  OOVHandler(std::vector<Word> controlTokenIdxs, Word unkId) : controlTokenIdxs_(controlTokenIdxs), unkId_(unkId) {}

  /// Transforms the source <unk> in source.words assigned with e_i and corresponding target.words <unk> assigned with
  /// e_i, using align. Will use source.views and target.views to check which <unk> are different from each other.
  void preprocess_training(Datum &source, Datum &target, const HardAlignmentInfo &align);

  /// During inference, only source can be preprocessed.
  void preprocess_inference(Datum &source);

  /// Transforms e_i in target dereferencing the corresponding e_i in source. Correspondence is obtained through align.
  /// Modifies target with updated string and rebased string_views.
  void postprocess_inference(const Datum &source, Datum &target, const HardAlignmentInfo &align);

 private:
  size_t countUnknowns(const std::vector<Words> &words) const;
  size_t allocateIndividual(Words &words, size_t startRegister);

  RegisterAllocator allocator_;
  const Word unkId_;
  std::vector<Word> controlTokenIdxs_;
};

class RegisterAllocator {
 public:
  RegisterAllocator(size_t numControlTokens, int seed = 42) : numControlTokens_(numControlTokens) {
    assert(numControlTokens_ > 0);
    registers_.resize(numControlTokens_);
    std::iota(registers_.begin(), registers_.end(), 1);

    randomGen_.seed(seed);
  }

  void allocate(size_t numTokens, std::vector<size_t> &allocatedRegisters, bool withReplacement = true) {
    if (withReplacement) {
      // This is efficient enough for small values of numControlTokens_(?).
      std::shuffle(registers_.begin(), registers_.end(), randomGen_);
      for (size_t i = 0; i < numTokens; i++) {
        allocatedRegisters.push_back(registers_[i]);
      }
    } else {
      std::sample(registers_.begin(), registers_.end(), std::back_inserter(allocatedRegisters), numTokens, randomGen_);
    }
  }

 private:
  size_t numControlTokens_;
  std::vector<size_t> registers_;
  std::mt19937 randomGen_;
};

}  // namespace bergamot
}  // namespace marian

#endif  // SRC_BERGAMOT_OOV_HANDLER_H
