#include "apps.h"

int main(int argc, char *argv[]) {
  using namespace marian::bergamot;
  marian::bergamot::ConfigParser configParser;
  configParser.parseArgs(argc, argv);
  auto &config = configParser.getConfig();
  BlockingService::Config serviceConfig;
  serviceConfig.cacheEnabled = config.cacheEnabled;
  serviceConfig.cacheSize = config.cacheSize;
  BlockingService service(serviceConfig);
  std::vector<std::shared_ptr<TranslationModel>> models;

  for (auto &modelConfigPath : config.modelConfigPaths) {
    TranslationModel::Config modelConfig = parseOptionsFromFilePath(modelConfigPath);
    MemoryBundle memoryBundle = getMemoryBundleFromConfig(modelConfig);
    std::shared_ptr<TranslationModel> model = std::make_shared<TranslationModel>(modelConfig, std::move(memoryBundle));
    models.push_back(model);
  }

  ResponseOptions responseOptions;

  // Read a large input text blob from stdin
  const std::string source = testapp::readFromStdin();

  auto model = models.front();
  // Round 1
  std::string buffer = source;
  Response firstResponse =
      service.translateMultiple(model, std::move(std::vector<std::string>{buffer}), responseOptions).front();

  auto statsFirstRun = service.cacheStats();
  LOG(info, "Cache Hits/Misses = {}/{}", statsFirstRun.hits, statsFirstRun.misses);
  ABORT_IF(statsFirstRun.hits != 0, "Expecting no cache hits, but hits found.");

  // Round 2; There should be cache hits
  buffer = source;
  Response secondResponse =
      service.translateMultiple(model, std::move(std::vector<std::string>{buffer}), responseOptions).front();

  auto statsSecondRun = service.cacheStats();

  LOG(info, "Cache Hits/Misses = {}/{}", statsSecondRun.hits, statsSecondRun.misses);
  ABORT_IF(statsSecondRun.hits <= 0, "At least one hit expected, none found.");
  if (statsSecondRun.hits != statsFirstRun.misses) {
    std::cerr << "Mismatch in expected hits (Hits, Misses = " << statsSecondRun.hits << ", " << statsSecondRun.misses
              << "). This can happen due to random eviction." << std::endl;
  }

  ABORT_IF(firstResponse.target.text != secondResponse.target.text,
           "Recompiled string provided different output when operated with cache. On the same hardware while using "
           "same path, this is expected to be same.");

  std::cout << firstResponse.target.text;
  return 0;
}
