#ifndef SRC_BERGAMOT_BATCH_TRANSLATOR_H_
#define SRC_BERGAMOT_BATCH_TRANSLATOR_H_

#include <string>
#include <vector>

#include "batch.h"
#include "common/utils.h"
#include "data/shortlist.h"
#include "definitions.h"
#include "request.h"
#include "translator/history.h"
#include "translator/scorers.h"

#ifndef WASM
#include "pcqueue.h"
#endif

namespace marian {
namespace bergamot {

class BatchStats {
public:
  BatchStats(size_t B, size_t T) : B(B), T(T) {
    matrix_ = std::unique_ptr<float>(new float[B * T]);
  }

  void set(size_t b, size_t t, float value) {
    LOG(info, "Received ({}, {}) = {}", b, t, value);
    (matrix_.get())[resolve(b, t)] = value;
  }

  float get(size_t b, size_t t) { return (matrix_.get())[resolve(b, t)]; }

  inline size_t resolve(size_t b, size_t t) { return b * T + t; }

  size_t B, T;

private:
  std::unique_ptr<float> matrix_;
};

class BatchTranslator {
  // Launches minimal marian-translation (only CPU at the moment) in individual
  // threads. Constructor launches each worker thread running mainloop().
  // mainloop runs until until it receives poison from the PCQueue. Threads are
  // shut down in Service which calls join() on the threads.

public:
  /**
   * Initialise the marian translator.
   * @param device DeviceId that performs translation. Could be CPU or GPU
   * @param vocabs Vector that contains ptrs to two vocabs
   * @param options Marian options object
   * @param model_memory byte array (aligned to 64!!!) that contains the bytes
   * of a model.bin. Provide a nullptr if not used.
   */
  explicit BatchTranslator(DeviceId const device,
                           std::vector<Ptr<Vocab const>> &vocabs,
                           Ptr<Options> options, const void *model_memory);

  // convenience function for logging. TODO(jerin)
  std::string _identifier() { return "worker" + std::to_string(device_.no); }
  void translate(Batch &batch);
  void initialize();

private:
  Ptr<Options> options_;
  DeviceId device_;
  std::vector<Ptr<Vocab const>> *vocabs_;
  Ptr<ExpressionGraph> graph_;
  std::vector<Ptr<Scorer>> scorers_;
  Ptr<data::ShortlistGenerator const> slgen_;
  const void *model_memory_;
  BatchStats stats_;
};

} // namespace bergamot
} // namespace marian

#endif //  SRC_BERGAMOT_BATCH_TRANSLATOR_H_
