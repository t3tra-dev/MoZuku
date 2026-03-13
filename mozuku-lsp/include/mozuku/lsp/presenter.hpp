#pragma once

#include "comment_extractor.hpp"
#include "mozuku/core/types.hpp"

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace MoZuku::lsp {

class Presenter {
public:
  using json = nlohmann::json;

  json publishDiagnosticsParams(const std::string &uri,
                                const std::vector<Diagnostic> &diags) const;

  json commentHighlightsParams(
      const std::string &uri, const std::string &text,
      const std::vector<comments::CommentSegment> &segments) const;

  json contentHighlightsParams(const std::string &uri, const std::string &text,
                               const std::vector<ByteRange> &ranges) const;

  json semanticHighlightsParams(const std::string &uri, bool isJapanese,
                                const std::vector<TokenData> &tokens) const;

  json semanticTokensData(const std::vector<TokenData> &tokens,
                          const std::vector<std::string> &tokenTypes) const;

  json hoverResult(const TokenData &token, const std::string &markdown) const;
};

} // namespace MoZuku::lsp
