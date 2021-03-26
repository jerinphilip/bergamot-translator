#include "batcher.h"
#include "batch.h"
#include "common/logging.h"
#include <cassert>

namespace marian {
namespace bergamot {

Batcher::Batcher(Ptr<Options> options) : exhaustConfig_(options) {
  miniBatchWords = options->get<int>("mini-batch-words");
  bucket_.resize(options->get<int>("max-length-break") + 1);
  ABORT_IF(bucket_.size() - 1 > miniBatchWords,
           "Fatal: max-length-break > mini-batch-words  will lead to sentences "
           "longer than what can fit in a batch.");

  // Configure exhaust mode config
}

void Batcher::addSentenceWithPriority(RequestSentence &sentence) {
  size_t bucket_id = sentence.numTokens();
  assert(bucket_id < bucket_.size());
  bucket_[bucket_id].insert(sentence);
}

bool Batcher::operator>>(Batch &batch) {
  if (exhaustConfig_.active()) {
    return nextRandomBatch(batch);
  } else {
    return cleaveBatch(batch);
  }
}

bool Batcher::cleaveBatch(Batch &batch) {
  // For now simply iterates on buckets and converts batches greedily.  This
  // has to be enhanced with optimizing over priority. The baseline
  // implementation should at least be as fast as marian's maxi-batch with full
  // corpus size as maxi-batch size.
  batch.clear();
  size_t paddedBatchSize = 0;

  for (size_t length = 0; length < bucket_.size(); length++) {
    auto p = bucket_[length].begin();
    while (p != bucket_[length].end()) {
      paddedBatchSize = (batch.size() + 1) * length;
      if (paddedBatchSize <= miniBatchWords) {
        auto q = p++;
        batch.add(*q);
        bucket_[length].erase(q);
      } else {
        // Check if elements exist
        assert(batch.size() > 0);
        return true;
      }
    }
  }

  bool isValidBatch = batch.size() > 0;
  return isValidBatch;
}

bool Batcher::nextRandomBatch(Batch &batch) {
  std::pair<size_t, size_t> batchInfo;
  while (exhaustConfig_.next(batchInfo)) {
    // Generate batch
    size_t B = batchInfo.first;
    size_t T = batchInfo.second;
    size_t available = bucket_[T].size();
    if (available == 0) {
      /// I can't sample if there is none available.
      continue;
    } else {
      std::queue<size_t> idxs;
      std::uniform_int_distribution<> dist(0, available - 1);
      static std::random_device rd;
      static std::mt19937 gen(rd());

      for (size_t b = 0; b < B; b++) {
        idxs.push(dist(gen));
      }

      size_t activeIdx = 0;
      for (auto &p : bucket_[T]) {
        if (idxs.front() == activeIdx) {
          batch.add(p);
          idxs.pop();
        }
        ++activeIdx;
      }
      return true;
    }
  }
  return false;
}

void Batcher::addWholeRequest(Ptr<Request> request) {
  for (size_t i = 0; i < request->numSegments(); i++) {
    RequestSentence requestSentence(i, request);
    addSentenceWithPriority(requestSentence);
  }
}

} // namespace bergamot
} // namespace marian
