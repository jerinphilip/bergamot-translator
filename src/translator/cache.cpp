#include "translator/cache.h"

#include <cstdlib>

namespace marian {
namespace bergamot {

#ifndef WASM_COMPATIBLE_SOURCE

ThreadSafeL4Cache::ThreadSafeL4Cache(Ptr<Options> options)
    : epochManagerConfig_(options->get<size_t>("cache-ebr-queue-size"),
                          std::chrono::milliseconds(options->get<size_t>("cache-ebr-interval")),
                          options->get<size_t>("cache-ebr-num-queues")),
      cacheConfig_(options->get<size_t>("cache-size") * 1024 * 1024,
                   std::chrono::seconds(options->get<size_t>("cache-time-to-live")),
                   options->get<bool>("cache-remove-expired")),
      service_(epochManagerConfig_),
      context_(service_.GetContext()) {
  // There is only a single cache we use. However, L4 construction API given by example demands we give it a string
  // identifier.
  const std::string cacheIdentifier = "global-cache";
  size_t numBuckets = options->get<size_t>("cache-buckets");
  hashTableIndex_ = service_.AddHashTable(L4::HashTableConfig(
      cacheIdentifier, L4::HashTableConfig::Setting{static_cast<uint32_t>(numBuckets)}, cacheConfig_));
}

bool ThreadSafeL4Cache::fetch(const marian::Words &words, ProcessedRequestSentence &processedRequestSentence) {
  auto &hashTable = context_[hashTableIndex_];

  /// We take marian's default hash function, which is templated for hash-type. We treat marian::Word (which are indices
  /// represented by size_t (unverified) and convert it into a uint64_t for hashing with more bits)
  ///
  /// With the use of a 64-bit hash, the probability of collision is as follows:
  /// 100K sentences: 2.710478508305414e-10
  ///   1M sentences: 2.7105026889628903e-8
  ///   1B sentences: 0.026741008873954697
  ///
  /// Note that there can still be collisions, in which case the translations returned from cache can be wrong. It's
  /// practically very unlikely (claims @XapaJiaMnu). To guarantee correctness, comparisons need to be made with the
  /// actual key, which is cannot be done with this approach, as the cache is unaware of the original marian::Words key.
  //
  /// TODO(@jerinphilip): Empirical evaluation that this is okay.

  KeyBytes keyBytes;
  auto key = cache_util::HashWords<uint64_t>()(words);

  // L4 requires uint8_t byte-stream, so we treat 64 bits as a uint8 array, with 8 members.
  keyBytes.m_data = reinterpret_cast<const std::uint8_t *>(&key);
  keyBytes.m_size = 8;

  ValueBytes valBytes;
  bool fetchSuccess = hashTable.Get(keyBytes, valBytes);
  if (fetchSuccess) {
    const char *data = reinterpret_cast<const char *>(valBytes.m_data);
    size_t size = valBytes.m_size;
    processedRequestSentence = ProcessedRequestSentence::fromBytes(data, size);
  }

  debug("After Fetch");
  return fetchSuccess;
}

void ThreadSafeL4Cache::insert(const marian::Words &words, const ProcessedRequestSentence &processedRequestSentence) {
  auto context = service_.GetContext();
  auto &hashTable = context[hashTableIndex_];

  KeyBytes keyBytes;

  auto key = cache_util::HashWords<uint64_t>()(words);

  // L4 requires uint8_t byte-stream, so we treat 64 bits as a uint8 array, with 8 members.

  keyBytes.m_data = reinterpret_cast<const std::uint8_t *>(&key);
  keyBytes.m_size = 8;

  ValueBytes valBytes;
  std::string serialized = processedRequestSentence.toBytes();

  valBytes.m_data = reinterpret_cast<const std::uint8_t *>(serialized.data());
  valBytes.m_size = sizeof(std::string::value_type) * serialized.size();

  debug("Before Add");
  hashTable.Add(keyBytes, valBytes);
  debug("After Add");
}

CacheStats ThreadSafeL4Cache::stats() const {
  auto &perfData = context_[hashTableIndex_].GetPerfData();
  CacheStats stats;
  stats.hits = perfData.Get(L4::HashTablePerfCounter::CacheHitCount);
  stats.misses = perfData.Get(L4::HashTablePerfCounter::CacheMissCount);
  return stats;
}

void ThreadSafeL4Cache::debug(std::string label) const {
  if (std::getenv("BERGAMOT_L4_CACHE_DEBUG")) {
    std::cerr << "--- L4: " << label << std::endl;
    auto &perfData = context_[hashTableIndex_].GetPerfData();
#define __l4inspect(key) std::cerr << #key << " " << perfData.Get(L4::HashTablePerfCounter::key) << std::endl;

    __l4inspect(CacheHitCount);
    __l4inspect(CacheMissCount);
    __l4inspect(RecordsCount);
    __l4inspect(EvictedRecordsCount);
    __l4inspect(TotalIndexSize);
    __l4inspect(TotalKeySize);
    __l4inspect(TotalValueSize);

    std::cerr << "---- " << std::endl;

#undef __l4inspect
  }
};

#endif

ThreadUnsafeLRUCache::ThreadUnsafeLRUCache(Ptr<Options> options)
    : storageSizeLimit_(options->get<size_t>("cache-size") * 1024 * 1024), storageSize_(0) {}

bool ThreadUnsafeLRUCache::fetch(const marian::Words &words, ProcessedRequestSentence &processedRequestSentence) {
  auto p = cache_.find(words);
  if (p != cache_.end()) {
    auto recordPtr = p->second;
    const std::string &value = recordPtr->value;
    processedRequestSentence = ProcessedRequestSentence::fromBytes(value.data(), value.size());

    // Refresh recently used
    auto record = *recordPtr;
    storage_.erase(recordPtr);
    storage_.insert(storage_.end(), std::move(record));

    ++stats_.hits;
    return true;
  }

  ++stats_.misses;
  return false;
}

void ThreadUnsafeLRUCache::insert(const marian::Words &words,
                                  const ProcessedRequestSentence &processedRequestSentence) {
  Record candidate{words, processedRequestSentence.toBytes()};
  auto removeCandidatePtr = storage_.begin();

  // Loop until not end and we have not found space to insert candidate adhering to configured storage limits.
  while (storageSize_ + candidate.size() > storageSizeLimit_ && removeCandidatePtr != storage_.end()) {
    storageSize_ -= removeCandidatePtr->size();
    storage_.erase(removeCandidatePtr);
    ++removeCandidatePtr;
  }

  // Only insert new candidate if we have space after adhering to storage-limit
  if (storageSize_ + candidate.size() <= storageSizeLimit_) {
    auto recordPtr = storage_.insert(storage_.end(), std::move(candidate));
    cache_.emplace(std::make_pair(words, recordPtr));
    storageSize_ += candidate.size();
  }
}

CacheStats ThreadUnsafeLRUCache::stats() const {
  CacheStats stats = stats_;
  return stats;
}

}  // namespace bergamot
}  // namespace marian
