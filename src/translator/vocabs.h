#pragma once

namespace marian {
namespace bergamot {

namespace {

template <class KeyType, class HashType, class HashFn, class LoadFn>
class VocabLoader {
 public:
  VocabLoader(Ptr<Options> options, std::vector<KeyType>& elements) {
    for (size_t i = 0; i < elements.size(); i++) {
      KeyType key = HashFn(elements[i]);
      auto m = vmap_.emplace(std::make_pair(key, Ptr<Vocab>()));
      if (m.second) {
        // new: load the vocab
        m.first->second = New<Vocab>(options, i);
        LoadFn(m.first->second, elements[i]);
      }
    }
  }

  void load(std::vector<KeyType> keys, std::vector<Ptr<Vocab const>>& srcVocabs, Ptr<Vocab const>& trgVocab) {
    for (auto& key : keys) {
      srcVocabs.push_back(vmap_[key]);
    }
    trgVocab = srcVocabs.back();
    srcVocabs.pop_back();
  }

 private:
  std::unordered_map<KeyType, Ptr<Vocab>> vmap_;
};

}  // namespace

/// Wrapper of Marian Vocab objects needed for translator.
/// Holds multiple source vocabularies and one target vocabulary
class Vocabs {
 public:
  /// Construct vocabs object from either byte-arrays or files
  Vocabs(Ptr<Options> options, std::vector<std::shared_ptr<AlignedMemory>>&& vocabMemories) {
    auto hashFn = [](std::shared_ptr<AlignedMemory>& memory) { return reinterpret_cast<uintptr_t>(memory.get()); };
    auto loadFn = [](Ptr<Vocab>& vocab, std::shared_ptr<AlignedMemory>& memory) {
      absl::string_view serialized = absl::string_view(memory->begin(), memory->size());
      vocab->loadFromSerialized(serialized);
    };

    VocabLoader<std::shared_ptr<AlignedMemory>, uintptr_t, decltype(hashFn), decltype(loadFn)> loader(options,
                                                                                                      vocabMemories);
    loader.load(vocabMemories, srcVocabs_, trgVocab_);
  }

  Vocabs(Ptr<Options> options, std::vector<std::string>&& vocabPaths) {
    auto hashFn = [](std::string& s) { return s; };
    auto loadFn = [](Ptr<Vocab>& vocab, std::string& path) { vocab->load(path); };
    VocabLoader<std::string, std::string, decltype(hashFn), decltype(loadFn)> loader(options, vocabPaths);
    loader.load(vocabPaths, srcVocabs_, trgVocab_);
  }

  /// Get all source vocabularies (as a vector)
  const std::vector<Ptr<Vocab const>>& sources() const { return srcVocabs_; }

  /// Get the target vocabulary
  const Ptr<Vocab const>& target() const { return trgVocab_; }

 private:
  std::vector<Ptr<Vocab const>> srcVocabs_;  // source vocabularies
  Ptr<Vocab const> trgVocab_;                // target vocabulary
};

}  // namespace bergamot
}  // namespace marian
