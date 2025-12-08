#pragma once

#include "analyzer.hpp"
#include "lsp.hpp"
#include <string>
#include <vector>

namespace MoZuku {
namespace grammar {

class GrammarChecker {
public:
  static void checkGrammar(const std::string &text,
                           const std::vector<TokenData> &tokens,
                           const std::vector<SentenceBoundary> &sentences,
                           std::vector<Diagnostic> &diags,
                           const MoZukuConfig *config);
};

} // namespace grammar
} // namespace MoZuku
