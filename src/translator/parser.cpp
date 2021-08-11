#include "parser.h"

#include "common/build_info.h"
#include "common/config.h"
#include "common/regex.h"
#include "common/version.h"

namespace marian {
namespace bergamot {

std::istringstream &operator>>(std::istringstream &in, OpMode &mode) {
  std::string modeString;
  in >> modeString;
  std::unordered_map<std::string, OpMode> table = {
      {"wasm", OpMode::WASM},
      {"native", OpMode::NATIVE},
      {"decoder", OpMode::DECODER},
      {"test-source-sentences", OpMode::TEST_SOURCE_SENTENCES},
      {"test-target-sentences", OpMode::TEST_TARGET_SENTENCES},
      {"test-source-words", OpMode::TEST_SOURCE_WORDS},
      {"test-target-words", OpMode::TEST_TARGET_WORDS},
  };

  auto query = table.find(modeString);
  if (query != table.end()) {
    mode = query->second;
  } else {
    ABORT("Unknown mode {}", modeString);
  }
}

ConfigParser::ConfigParser() : app_{"Bergamot Options"} {
  addSpecialOptions(app_);
  addOptionsBoundToConfig(app_, config_);
};

void ConfigParser::parseArgs(int argc, char *argv[]) {
  try {
    app_.parse(argc, argv);
    handleSpecialOptions();
  } catch (const CLI::ParseError &e) {
    exit(app_.exit(e));
  }

  if (version_) {
    std::cerr << buildVersion() << std::endl;
    exit(0);
  }
}

void ConfigParser::addSpecialOptions(CLI::App &app) {
  app.add_flag("--build-info", build_info_, "Print build-info and exit");
  app.add_flag("--version", version_, "Print version-info and exit");
}

void ConfigParser::handleSpecialOptions() {
  if (build_info_) {
#ifndef _MSC_VER  // cmake build options are not available on MSVC based build.
    std::cerr << cmakeBuildOptionsAdvanced() << std::endl;
    exit(0);
#else   // _MSC_VER
    ABORT("build-info is not available on MSVC based build.");
#endif  // _MSC_VER
  }
}

void ConfigParser::addOptionsBoundToConfig(CLI::App &app, CLIConfig &config) {
  app.add_option("--model-config-paths", config.modelConfigPaths,
                 "Configuration files list, can be used for pivoting multiple models or multiple model workflows");

  app.add_option("--ssplit-prefix-file", config.ssplitPrefixFilePath,
                 "File with nonbreaking prefixes for sentence splitting.");

  app.add_option("--ssplit-mode", config.ssplitMode, "[paragraph, sentence, wrapped_text]");

  app.add_flag("--bytearray", config.byteArray,
               "Flag holds whether to construct service from bytearrays, only for testing purpose");

  app.add_flag("--check-bytearray", config.validateByteArray,
               "Flag holds whether to check the content of the bytearrays (true by default)");

  app.add_option("--cpu-threads", config.numWorkers, "Number of worker threads to use for translation");

  app_.add_option("--bergamot-mode", config.opMode, "Operating mode for bergamot: [wasm, native, decoder]");
}

std::shared_ptr<marian::Options> parseOptionsFromFilePath(const std::string &configPath, bool validate /*= true*/) {
  // Read entire string and redirect to parseOptionsFromString
  std::ifstream readStream(configPath);
  std::stringstream buffer;
  buffer << readStream.rdbuf();
  return parseOptionsFromString(buffer.str(), validate);
};

std::shared_ptr<marian::Options> parseOptionsFromString(const std::string &config, bool validate /*= true*/) {
  marian::Options options;

  marian::ConfigParser configParser(cli::mode::translation);

  // These are additional options we use to hijack for our own marian-replacement layer (for batching,
  // multi-request-compile etc) and hence goes into Ptr<Options>.
  configParser.addOption<size_t>("--max-length-break", "Bergamot Options",
                                 "Maximum input tokens to be processed in a single sentence.", 128);

  // This is a complete hijack of an existing option, so no need to add explicitly.
  // configParser.addOption<size_t>("--mini-batch-words", "Bergamot Options",
  //                                "Maximum input tokens to be processed in a single sentence.", 1024);

  const YAML::Node &defaultConfig = configParser.getConfig();

  options.merge(defaultConfig);

  // Parse configs onto defaultConfig.
  options.parse(config);
  YAML::Node configCopy = options.cloneToYamlNode();

  if (validate) {
    // Perform validation on parsed options only when requested
    marian::ConfigValidator validator(configCopy);
    validator.validateOptions(marian::cli::mode::translation);
  }

  return std::make_shared<marian::Options>(options);
}

}  // namespace bergamot
}  // namespace marian
