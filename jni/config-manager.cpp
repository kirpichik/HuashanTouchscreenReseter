
#include "config-manager.h"
#include <fstream>
#include <vector>
#include <sstream>

namespace config {

  static void splitString(const std::string& str, std::vector<std::string>& split, char delim) {
    std::istringstream stream(str);
    std::string s;    
    while (std::getline(stream, s, delim))
      split.push_back(s);
  }

  bool Config::reloadConfig() {
    std::ifstream input;
    input.open(filename.c_str());

    if (!input.is_open())
      return false;

    std::string line;
    while(std::getline(input, line)) {
      if (line[0] == '#') // Комментарий
        continue;

      std::vector<std::string> split;
      splitString(line, split, '='); // Разделение имени параметра и аргумента

      if (!split.size())
        continue;

      if (split.size() != 2)
        return false;
    
      config.insert(std::pair<std::string, std::string>(split[0], split[1]));
    }

    return true;
  }

  bool Config::has(const std::string& key) {
    return config.find(key) != config.end();
  }

  std::string Config::get(const std::string& key) {
    std::map<std::string, std::string>::iterator iterat = config.find(key);
    return iterat == config.end() ? "" : iterat->second;
  }

  Config* Config::loadConfig(const std::string& filename) {
    Config* conf = new Config(filename);
    if (!conf->reloadConfig()) {
      delete conf;
      conf = NULL;
    }
    return conf;
  }


}
