#pragma once

#include "mozuku/core/config.hpp"
#include "mozuku/core/types.hpp"

#include <memory>
#include <string>
#include <vector>

void analyzeText(const std::string &text, std::vector<TokenData> &tokens,
                 std::vector<Diagnostic> &diags,
                 const MoZukuConfig *config = nullptr);

void performGrammarDiagnostics(const std::string &text,
                               std::vector<Diagnostic> &diags);

namespace MoZuku {

namespace mecab {
class MeCabManager;
}

class Analyzer {
public:
  Analyzer();
  ~Analyzer();

  bool initialize(const MoZukuConfig &config);

  std::vector<TokenData> analyzeText(const std::string &text);
  std::vector<Diagnostic> checkGrammar(const std::string &text);
  std::vector<DependencyInfo> analyzeDependencies(const std::string &text);

  bool isInitialized() const;
  std::string getSystemCharset() const;
  bool isCaboChaAvailable() const;

private:
  struct PreparedText {
    std::string cleanText;
    double japaneseRatio{0.0};
    bool belowMinJapaneseRatio{false};
  };

  PreparedText prepareText(const std::string &text,
                           bool enforceMinJapaneseRatio) const;
  std::vector<TokenData> analyzePreparedText(const PreparedText &prepared);

  std::unique_ptr<mecab::MeCabManager> mecab_manager_;
  MoZukuConfig config_;
  std::string system_charset_;
};

} // namespace MoZuku
