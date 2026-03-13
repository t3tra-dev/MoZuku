#include "mozuku/lsp/presenter.hpp"

#include "utf16.hpp"

#include <algorithm>
#include <iterator>

namespace {

nlohmann::json makeRangeJson(const Position &start, const Position &end) {
  return {{"start", {{"line", start.line}, {"character", start.character}}},
          {"end", {{"line", end.line}, {"character", end.character}}}};
}

nlohmann::json makeTokenRangeJson(const TokenData &token) {
  return makeRangeJson(Position{token.line, token.startChar},
                       Position{token.line, token.endChar});
}

} // namespace

namespace MoZuku::lsp {

Presenter::json Presenter::publishDiagnosticsParams(
    const std::string &uri, const std::vector<Diagnostic> &diags) const {
  json diagnostics = json::array();
  for (const auto &diag : diags) {
    diagnostics.push_back(
        {{"range", makeRangeJson(diag.range.start, diag.range.end)},
         {"severity", diag.severity},
         {"message", diag.message}});
  }

  return {{"uri", uri}, {"diagnostics", diagnostics}};
}

Presenter::json Presenter::commentHighlightsParams(
    const std::string &uri, const std::string &text,
    const std::vector<comments::CommentSegment> &segments) const {
  json ranges = json::array();
  TextOffsetMapper offsetMapper(text);

  for (const auto &segment : segments) {
    Position start = offsetMapper.byteOffsetToPosition(segment.startByte);
    Position end = offsetMapper.byteOffsetToPosition(segment.endByte);
    ranges.push_back(makeRangeJson(start, end));
  }

  return {{"uri", uri}, {"ranges", ranges}};
}

Presenter::json
Presenter::contentHighlightsParams(const std::string &uri,
                                   const std::string &text,
                                   const std::vector<ByteRange> &ranges) const {
  json lspRanges = json::array();
  TextOffsetMapper offsetMapper(text);

  for (const auto &range : ranges) {
    Position start = offsetMapper.byteOffsetToPosition(range.startByte);
    Position end = offsetMapper.byteOffsetToPosition(range.endByte);
    lspRanges.push_back(makeRangeJson(start, end));
  }

  return {{"uri", uri}, {"ranges", lspRanges}};
}

Presenter::json Presenter::semanticHighlightsParams(
    const std::string &uri, bool /*isJapanese*/,
    const std::vector<TokenData> &tokens) const {
  json tokenEntries = json::array();
  for (const auto &token : tokens) {
    tokenEntries.push_back({{"range", makeTokenRangeJson(token)},
                            {"type", token.tokenType},
                            {"modifiers", token.tokenModifiers}});
  }

  return {{"uri", uri}, {"tokens", tokenEntries}};
}

Presenter::json Presenter::semanticTokensData(
    const std::vector<TokenData> &tokens,
    const std::vector<std::string> &tokenTypes) const {
  json data = json::array();
  int prevLine = 0;
  int prevChar = 0;

  for (const auto &token : tokens) {
    int deltaLine = token.line - prevLine;
    int deltaChar =
        (deltaLine == 0) ? token.startChar - prevChar : token.startChar;

    auto typeIt =
        std::find(tokenTypes.begin(), tokenTypes.end(), token.tokenType);
    int typeIndex =
        (typeIt != tokenTypes.end())
            ? static_cast<int>(std::distance(tokenTypes.begin(), typeIt))
            : 0;

    data.push_back(deltaLine);
    data.push_back(deltaChar);
    data.push_back(token.endChar - token.startChar);
    data.push_back(typeIndex);
    data.push_back(token.tokenModifiers);

    prevLine = token.line;
    prevChar = token.startChar;
  }

  return data;
}

Presenter::json Presenter::hoverResult(const TokenData &token,
                                       const std::string &markdown) const {
  return {{"contents", {{"kind", "markdown"}, {"value", markdown}}},
          {"range", makeTokenRangeJson(token)}};
}

} // namespace MoZuku::lsp
