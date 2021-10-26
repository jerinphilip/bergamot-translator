#pragma once
#include <memory>
#include <mutex>
#include <vector>

#include "definitions.h"
#include "translator/history.h"

namespace marian::bergamot {
/// https://channel9.msdn.com/Shows/Going+Deep/C-and-Beyond-2012-Herb-Sutter-Concurrency-and-Parallelism
/// TimeStamp: 48:20
///
/// Note that this is not the same as a conditional variable, it's just a
/// wrapper which allows registering methods to be dispatched to the wrapped
/// object to get thread-safe equivalents of containers by the simplest and
/// least performant mutex.

template <class T>
class Monitor {
 private:
  T t_;
  mutable std::mutex mutex_;

 public:
  Monitor(T t = T{}) : t_{t} {}

  template <typename F>
  auto operator()(F f) const -> decltype(f(t_)) {
    std::lock_guard<std::mutex> _{mutex_};
    return f(t_);
  }
};

template <class Key, class Value, class Hash = std::hash<Key>, class Equals = std::equal_to<Key>>
class AtomicCache {
 public:
  struct Stats {
    size_t hits{0};
    size_t misses{0};
  };

  explicit AtomicCache(size_t size, size_t buckets)
      : records_(size), mutexBuckets_(buckets), load_(size, buckets), store_(size, buckets) {}

  std::pair<bool, Value> find(const Key &key) const {
    Value value;
    bool found = atomicLoad(key, value);
    return std::make_pair(found, value);
  }

  void store(const Key &key, Value value) { atomicStore(key, value); }

  const Stats stats() const { return stats_; }

  ~AtomicCache() {
    // Load.
    auto print = [](std::string tag, Counter &counter) {
      auto asList = [](std::string name, std::vector<size_t> V) {
        std::cerr << name << "  = [\n";
        for (size_t i = 0; i < V.size(); i++) {
          if (i != 0) std::cerr << ", ";
          std::cerr << V[i];
        }
        std::cerr << "]\n";
      };

      asList(tag + "[\"indexCount\"]", counter.indexCount);
      asList(tag + "[\"mutexCount\"]", counter.mutexCount);

      std::cerr << tag + "[\"processed\"] = " << counter.processed << "\n";
    };

    print("loadCounter", load_);
    print("storeCounter", store_);
  }

 private:
  using Record = std::pair<Key, Value>;

  bool atomicLoad(const Key &key, Value &value) const {
    // No probing, direct map onto records_
    size_t index = hash_(key) % records_.size();
    size_t mutexId = index % mutexBuckets_.size();

    load_.account(index, mutexId);

    std::lock_guard<std::mutex> lock(mutexBuckets_[mutexId]);
    const Record &candidate = records_[index];
    if (equals_(key, candidate.first)) {
      value = candidate.second;
      stats_.hits += 1;
      return true;
    } else {
      stats_.misses += 1;
    }

    return false;
  }

  void atomicStore(const Key &key, Value value) {
    // No probing, direct map onto records_
    size_t index = hash_(key) % records_.size();
    size_t mutexId = index % mutexBuckets_.size();

    store_.account(index, mutexId);

    std::lock_guard<std::mutex> lock(mutexBuckets_[mutexId]);
    Record &candidate = records_[index];

    candidate.first = key;
    candidate.second = value;
  }

  std::vector<Record> records_;

  mutable std::vector<std::mutex> mutexBuckets_;
  mutable Stats stats_;

  Hash hash_;
  Equals equals_;
  Monitor<std::ostream &> sync_cerr{std::cerr};

  struct Counter {
    Counter(size_t indexMax, size_t mutexMax) : indexCount(indexMax, 0), mutexCount(mutexMax, 0), processed(0) {}
    void account(size_t indexId, size_t mutexId) {
      indexCount[indexId]++;
      mutexCount[mutexId]++;
      processed++;
    }
    using _Counter = std::vector<size_t>;
    _Counter indexCount;
    _Counter mutexCount;
    size_t processed;
  };

  mutable Counter load_;
  mutable Counter store_;
};

typedef AtomicCache<size_t, Ptr<History>> TranslationCache;

}  // namespace marian::bergamot
