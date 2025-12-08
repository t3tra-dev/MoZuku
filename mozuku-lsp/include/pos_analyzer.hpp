#pragma once

#include "analyzer.hpp"
#include <string>
#include <vector>

namespace MoZuku {
namespace pos {

class POSAnalyzer {
public:
  static std::string mapPosToType(const char *feature);

  static void parseFeatureDetails(const char *feature, std::string &baseForm,
                                  std::string &reading,
                                  std::string &pronunciation,
                                  const std::string &systemCharset,
                                  bool skipConversion = false);

  static DetailedPOS parseDetailedPOS(const char *feature,
                                      const std::string &systemCharset);

  static unsigned computeModifiers(const std::string &text, size_t start,
                                   size_t length, const char *feature);

private:
  static std::vector<std::string> splitFeature(const std::string &feature);

  static void analyzeCharacterTypes(const std::string &text, size_t start,
                                    size_t length, bool &hasKana,
                                    bool &hasKanji, bool &hasNumber);
};

} // namespace pos
} // namespace MoZuku
