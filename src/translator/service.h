#ifndef SRC_BERGAMOT_SERVICE_H_
#define SRC_BERGAMOT_SERVICE_H_

#include "batch_translator.h"
#include "batcher.h"
#include "data/types.h"
#include "pcqueue.h"
#include "response.h"
#include "text_processor.h"

#include <queue>
#include <vector>

namespace marian {
namespace bergamot {

class Service {

  // Service exposes methods to translate an incoming blob of text to the
  // Consumer of bergamot API.
  //
  // An example use of this API looks as follows:
  //
  //  options = ...;
  //  service = Service(options);
  //  std::string input_blob = "Hello World";
  //  std::future<Response>
  //      response = service.translate(std::move(input_blob));
  //  response.wait();
  //  Response result = response.get();

public:
  /**
   * @param options Marian options object
   * @param model_memory byte array (aligned to 64!!!) that contains the bytes
   * of a model.bin. Optional, defaults to nullptr when not used
   */
  explicit Service(Ptr<Options> options, const void *model_memory = nullptr);

  // Convenience accessor methods to extract these vocabulary outside service.
  // e.g: For use in decoding histories for marian-decoder replacement.
  Ptr<Vocab const> sourceVocab() const { return vocabs_.front(); }
  Ptr<Vocab const> targetVocab() const { return vocabs_.back(); }

  std::future<Response> translate(std::string &&input);
  void stop() {}

  ~Service();

private:
  void initialize_blocking_translator(Ptr<Options> options);
  void initialize_async_translators(Ptr<Options> options);
  void blocking_translate();
  void async_translate();

  // In addition to the common members (text_processor, requestId, vocabs_,
  // batcher) extends with a producer-consumer queue, vector of translator
  // instances owned by service each listening to the pcqueue in separate
  // threads.

  size_t numWorkers_;      // ORDER DEPENDENCY
  PCQueue<Batch> pcqueue_; // ORDER DEPENDENCY
  const void *model_memory_;
  std::vector<std::thread> workers_;
  std::vector<BatchTranslator> translators_;

  size_t requestId_;
  std::vector<Ptr<Vocab const>> vocabs_; // ORDER DEPENDENCY
  TextProcessor text_processor_;         // ORDER DEPENDENCY
  Batcher batcher_;
};

inline std::vector<Ptr<const Vocab>> loadVocabularies(Ptr<Options> options) {
  // @TODO: parallelize vocab loading for faster startup
  auto vfiles = options->get<std::vector<std::string>>("vocabs");
  // with the current setup, we need at least two vocabs: src and trg
  ABORT_IF(vfiles.size() < 2, "Insufficient number of vocabularies.");
  std::vector<Ptr<Vocab const>> vocabs(vfiles.size());
  std::unordered_map<std::string, Ptr<Vocab>> vmap;
  for (size_t i = 0; i < vocabs.size(); ++i) {
    auto m = vmap.emplace(std::make_pair(vfiles[i], Ptr<Vocab>()));
    if (m.second) { // new: load the vocab
      m.first->second = New<Vocab>(options, i);
      m.first->second->load(vfiles[i]);
    }
    vocabs[i] = m.first->second;
  }
  return vocabs;
}

} // namespace bergamot
} // namespace marian

#endif // SRC_BERGAMOT_SERVICE_H_
