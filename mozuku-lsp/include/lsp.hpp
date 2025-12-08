#pragma once

#include "analyzer.hpp"
#include <cstddef>
#include <istream>
#include <memory>
#include <nlohmann/json.hpp>
#include <ostream>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "comment_extractor.hpp"

using json = nlohmann::json;

struct Position {
  int line{0};
  int character{0};
};

struct Range {
  Position start;
  Position end;
};

struct Diagnostic {
  Range range;
  int severity{2};
  std::string message;
};

struct TokenData {
  int line{0};
  int startChar{0};
  int endChar{0};
  std::string tokenType; // e.g. "noun", "verb" ...
  unsigned int tokenModifiers{0};

  std::string surface; // 表層形
  std::string
      feature; // 品詞,品詞細分類1,品詞細分類2,品詞細分類3,活用型,活用形,原形,読み,発音
  std::string baseForm;      // 原形
  std::string reading;       // 読み
  std::string pronunciation; // 発音
};

struct AnalyzerResult {
  std::vector<TokenData> tokens;
  std::vector<Diagnostic> diags;
};

struct ByteRange {
  size_t startByte{0};
  size_t endByte{0};
};

class LSPServer {
public:
  LSPServer(std::istream &in, std::ostream &out);
  void run();

private:
  std::istream &in_;
  std::ostream &out_;

  // インメモリテキストストア: uri -> 全テキスト
  std::unordered_map<std::string, std::string> docs_;
  // ドキュメントの言語ID: uri -> languageId
  std::unordered_map<std::string, std::string> docLanguages_;
  // hover用トークン情報: uri -> トークンデータ
  std::unordered_map<std::string, std::vector<TokenData>> docTokens_;
  // 行ベースの診断キャッシュ: uri -> 行番号 -> 診断情報
  std::unordered_map<std::string,
                     std::unordered_map<int, std::vector<Diagnostic>>>
      docDiagnostics_;
  // コメント解析に使用するセグメント
  std::unordered_map<std::string, std::vector<MoZuku::comments::CommentSegment>>
      docCommentSegments_;
  // HTML/LaTeX 本文ハイライト用の範囲
  std::unordered_map<std::string, std::vector<ByteRange>>
      docContentHighlightRanges_;
  std::vector<std::string> tokenTypes_;
  std::vector<std::string> tokenModifiers_;

  MoZukuConfig config_;

  std::unique_ptr<MoZuku::Analyzer> analyzer_;

  bool readMessage(std::string &jsonPayload);
  void reply(const json &msg);
  void notify(const std::string &method, const json &params);

  void handle(const json &req);

  json onInitialize(const json &id, const json &params);
  void onInitialized();
  void onDidOpen(const json &params);
  void onDidChange(const json &params);
  void onDidSave(const json &params);
  json onSemanticTokensFull(const json &id, const json &params);
  json onSemanticTokensRange(const json &id, const json &params);
  json onHover(const json &id, const json &params);

  void analyzeAndPublish(const std::string &uri, const std::string &text);
  void analyzeChangedLines(const std::string &uri, const std::string &newText,
                           const std::string &oldText);
  std::string prepareAnalysisText(const std::string &uri,
                                  const std::string &text);
  void sendCommentHighlights(
      const std::string &uri, const std::string &text,
      const std::vector<MoZuku::comments::CommentSegment> &segments);
  void sendSemanticHighlights(const std::string &uri,
                              const std::vector<TokenData> &tokens);
  void sendContentHighlights(const std::string &uri, const std::string &text,
                             const std::vector<ByteRange> &ranges);
  json buildSemanticTokens(const std::string &uri);
  json buildSemanticTokensFromTokens(const std::vector<TokenData> &tokens);

  void cacheDiagnostics(const std::string &uri,
                        const std::vector<Diagnostic> &diags);
  void removeDiagnosticsForLines(const std::string &uri,
                                 const std::set<int> &lines);
  std::vector<Diagnostic> getAllDiagnostics(const std::string &uri) const;
  std::set<int> findChangedLines(const std::string &oldText,
                                 const std::string &newText) const;
};
