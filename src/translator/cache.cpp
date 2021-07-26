#include "translator/cache.h"

#include <cstring>

namespace marian {
namespace bergamot {

namespace {

// Generic write/read from pointers

// write uses ostream, to get bytes/blob use with ostringstream(::binary)
template <class T>
void write(std::ostream &out, const T *data, size_t num = 1) {
  const char *cdata = reinterpret_cast<const char *>(data);
  out.write(cdata, num * sizeof(T));
}

// L4 stores the data as blobs. These use memcpy to construct parts from the blob given the start and size. It is the
// responsibility of the caller to prepare the container with the correct contiguous size so memcpy works correctly.
template <class T>
const char *copyInAndAdvance(const char *src, T *dest, size_t num = 1) {
  const void *vsrc = reinterpret_cast<const void *>(src);
  void *vdest = reinterpret_cast<void *>(dest);
  std::memcpy(vdest, vsrc, num * sizeof(T));
  return reinterpret_cast<const char *>(src + num * sizeof(T));
}

// Specializations to read and write vectors. The format stored is [size, v_0, v_1 ... v_size]
template <class T>
void writeVector(std::ostream &out, std::vector<T> v) {
  size_t size = v.size();
  write<size_t>(out, &size);
  write<T>(out, v.data(), v.size());
}

template <class T>
const char *copyInVectorAndAdvance(const char *src, std::vector<T> &v) {
  // Read in size of the vector
  size_t sizePrefix{0};
  src = copyInAndAdvance<size_t>(src, &sizePrefix);

  // Ensure contiguous memory location exists for memcpy inside copyInAndAdvance
  v.reserve(sizePrefix);

  // Read in the vector
  src = copyInAndAdvance<T>(src, v.data(), sizePrefix);
  return src;
}

}  // namespace

ProcessedRequestSentence::ProcessedRequestSentence() : sentenceScore_{0} {}

/// Construct from History
ProcessedRequestSentence::ProcessedRequestSentence(const History &history) {
  // Convert marian's lazy shallow-history, consolidating just the information we want.
  IPtr<Hypothesis> hypothesis;
  Result result = history.top();
  std::tie(words_, hypothesis, sentenceScore_) = result;
  softAlignment_ = hypothesis->tracebackAlignment();
  wordScores_ = hypothesis->tracebackWordScores();
}

std::string ProcessedRequestSentence::toBytes() const {
  // Note: write in order of member definitions in class.
  std::ostringstream out(std::ostringstream::binary);
  writeVector<marian::Word>(out, words_);

  size_t softAlignmentSize = softAlignment_.size();
  write<size_t>(out, &softAlignmentSize);
  for (auto &alignment : softAlignment_) {
    writeVector<float>(out, alignment);
  }

  write<float>(out, &sentenceScore_);
  writeVector<float>(out, wordScores_);
  return out.str();
}

/// Construct from stream of bytes
ProcessedRequestSentence ProcessedRequestSentence::fromBytes(char const *data, size_t size) {
  ProcessedRequestSentence sentence;
  char const *p = data;

  p = copyInVectorAndAdvance<marian::Word>(p, sentence.words_);

  size_t softAlignmentSize{0};
  p = copyInAndAdvance<size_t>(p, &softAlignmentSize);
  sentence.softAlignment_.resize(softAlignmentSize);

  for (size_t i = 0; i < softAlignmentSize; i++) {
    p = copyInVectorAndAdvance<float>(p, sentence.softAlignment_[i]);
  }

  p = copyInAndAdvance<float>(p, &sentence.sentenceScore_);
  p = copyInVectorAndAdvance<float>(p, sentence.wordScores_);
  return sentence;
}

LockLessClockCache::LockLessClockCache(const std::string &modelIdentifier, size_t sizeInBytes, size_t timeOutInSeconds,
                                       bool removeExpired /* = true */)
    : epochManagerConfig_(/*epochQueueSize=*/1000,
                          /*epochProcessingInterval=*/std::chrono::milliseconds(1000),
                          /*numActionQueues=*/4),
      cacheConfig_{sizeInBytes, std::chrono::seconds(timeOutInSeconds), removeExpired},
      modelIdentifier_(modelIdentifier),
      service_(epochManagerConfig_),
      context_(service_.GetContext()) {
  // Once a context is retrieved, the operations such as
  // operator[] on the context and Get() are lock-free.
  hashTableIndex_ = service_.AddHashTable(
      L4::HashTableConfig(modelIdentifier_, L4::HashTableConfig::Setting{/*numBuckets=*/10000}, cacheConfig_));
}

bool LockLessClockCache::fetch(const marian::Words &words, ProcessedRequestSentence &processedRequestSentence) {
  auto &hashTable = context_[hashTableIndex_];

  KeyBytes keyBytes;
  std::string keyStr = wordsToString(words);

  keyBytes.m_data = reinterpret_cast<const std::uint8_t *>(keyStr.data());
  keyBytes.m_size = sizeof(std::string::value_type) * keyStr.size();

  ValueBytes valBytes;
  bool fetchSuccess = hashTable.Get(keyBytes, valBytes);
  if (fetchSuccess) {
    processedRequestSentence =
        ProcessedRequestSentence::fromBytes(reinterpret_cast<const char *>(valBytes.m_data), valBytes.m_size);
  }

  debug("After Fetch");
  return fetchSuccess;
}

void LockLessClockCache::insert(const marian::Words &words, const ProcessedRequestSentence &processedRequestSentence) {
  auto context = service_.GetContext();
  auto &hashTable = context[hashTableIndex_];

  KeyBytes keyBytes;
  std::string keyStr = wordsToString(words);

  keyBytes.m_data = reinterpret_cast<const std::uint8_t *>(keyStr.data());
  keyBytes.m_size = sizeof(std::string::value_type) * keyStr.size();

  ValueBytes valBytes;
  std::string serialized = processedRequestSentence.toBytes();

  valBytes.m_data = reinterpret_cast<const std::uint8_t *>(serialized.data());
  valBytes.m_size = sizeof(std::string::value_type) * serialized.size();

  debug("Before Add");
  hashTable.Add(keyBytes, valBytes);
  debug("After Add");
}

CacheStats LockLessClockCache::stats() const {
  auto &perfData = context_[hashTableIndex_].GetPerfData();
  CacheStats stats;
  stats.hits = perfData.Get(L4::HashTablePerfCounter::CacheHitCount);
  stats.misses = perfData.Get(L4::HashTablePerfCounter::CacheMissCount);
  return stats;
}

void LockLessClockCache::debug(std::string label) const {
    /*
  std::cout << "--- L4: " << label << std::endl;
  auto &perfData = context_[hashTableIndex_].GetPerfData();
#define __l4inspect(key) std::cout << #key << " " << perfData.Get(L4::HashTablePerfCounter::key) << std::endl;

  __l4inspect(CacheHitCount);
  __l4inspect(CacheMissCount);
  __l4inspect(RecordsCount);
  __l4inspect(EvictedRecordsCount);
  __l4inspect(TotalIndexSize);
  __l4inspect(TotalKeySize);
  __l4inspect(TotalValueSize);

  std::cout << "---- " << std::endl;
  */
};

std::string LockLessClockCache::wordsToString(const marian::Words &words) {
  std::string repr;
  for (size_t i = 0; i < words.size(); i++) {
    if (i != 0) {
      repr += " ";
    }
    repr += words[i].toString();
  }
  return repr;
}

template <typename Key, typename Value, typename Hash>
void LRUCache<Key, Value, Hash>::insert(const Key key, const Value value) {
  std::lock_guard<std::mutex> guard(rwMutex_);
  if (storage_.size() + 1 > sizeCap_) {
    unsafeEvict();
  }
  unsafeInsert(key, value);
}

template <typename Key, typename Value, typename Hash>
bool LRUCache<Key, Value, Hash>::fetch(const Key key, Value &value) {
  std::lock_guard<std::mutex> guard(rwMutex_);
  auto mapItr = map_.find(key);
  if (mapItr == map_.end()) {
    ++stats_.misses;
    return false;
  } else {
    ++stats_.hits;

    auto record = mapItr->second;
    value = record->value;

    // If fetched, update least-recently-used by moving the element to the front of the list.
    storage_.erase(record);
    unsafeInsert(key, value);
    return true;
  }
}
template <typename Key, typename Value, typename Hash>
void LRUCache<Key, Value, Hash>::unsafeEvict() {
  // Evict LRU
  auto record = storage_.rbegin();
  map_.erase(/*key=*/record->key);
  storage_.pop_back();
}

template <typename Key, typename Value, typename Hash>
void LRUCache<Key, Value, Hash>::unsafeInsert(const Key key, const Value value) {
  storage_.push_front({key, value});
  map_.insert({key, storage_.begin()});
}

// This is a lazy hash, we'll fix this with something better later.
size_t WordsHashFn::operator()(const Words &words) const {
  std::string repr("");
  for (size_t idx = 0; idx < words.size(); idx++) {
    if (idx != 0) {
      repr += " ";
    }
    repr += words[idx].toString();
  }
  return std::hash<std::string>{}(repr);
}

template class LRUCache<int, int>;

}  // namespace bergamot
}  // namespace marian