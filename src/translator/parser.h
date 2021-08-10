#ifndef SRC_BERGAMOT_PARSER_H
#define SRC_BERGAMOT_PARSER_H

#include "3rd_party/yaml-cpp/yaml.h"
#include "common/config_parser.h"
#include "common/config_validator.h"
#include "common/options.h"
#include "marian.h"

namespace marian {
namespace bergamot {

/// Equivalent to something offered by marian. Comes with free YAML parsing and a decent structure. Creates a space
/// different from marian configparser to avoid conflicts. Individual marian-config settings are either relayed
/// explicitly or loaded from a model config file. Unlike marian's way, we allow an empty ConfigParser to be created.
class ConfigParser {
 public:
  ConfigParser() : cli_(config_, "Marian: Fast Neural Machine Translation in C++", "General options", "", 40) {}
  ConfigParser(int argc, char** argv, bool validate = false) : ConfigParser() { parseOptions(argc, argv, validate); }

  template <typename T>
  ConfigParser& addOption(const std::string& args, const std::string& group, const std::string& help, const T val) {
    std::string previous_group = cli_.switchGroup(group);
    cli_.add<T>(args, help, val);
    cli_.switchGroup(previous_group);
    return *this;
  }

  template <typename T>
  ConfigParser& addOption(const std::string& args, const std::string& group, const std::string& help, const T val,
                          const T implicit_val) {
    std::string previous_group = cli_.switchGroup(group);
    cli_.add<T>(args, help, val)->implicit_val(implicit_val);
    cli_.switchGroup(previous_group);
    return *this;
  }

  template <typename T>
  ConfigParser& addOption(const std::string& args, const std::string& group, const std::string& help) {
    std::string previous_group = cli_.switchGroup(group);
    cli_.add<T>(args, help);
    cli_.switchGroup(previous_group);
    return *this;
  }

  Ptr<Options> parseOptions(int argc, char** argv, bool validate);
  std::string const& cmdLine() const { return cmdLine_; };

 private:
  cli::CLIWrapper cli_;
  YAML::Node config_;
  std::string cmdLine_;

  // Check if the config contains value for option key
  bool has(const std::string& key) const { return (bool)config_[key]; }

  // Return value for given option key cast to given type.
  // Abort if not set.
  template <typename T>
  T get(const std::string& key) const {
    ABORT_IF(!has(key), "CLI object has no key '{}'", key);
    return config_[key].as<T>();
  }
};

void addBaseOptions(ConfigParser& configParser);
YAML::Node loadConfigFile(const std::string& configFilePath);

std::shared_ptr<marian::Options> parseOptionsFromConfigFile(const std::string& configFilePath, bool validate = true);
std::shared_ptr<marian::Options> parseOptionsFromConfigString(const std::string& config, bool validate = true);

}  //  namespace bergamot
}  //  namespace marian

#endif  //  SRC_BERGAMOT_PARSER_H
