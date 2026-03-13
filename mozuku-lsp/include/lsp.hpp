#pragma once

#include "analyzer.hpp"
#include "mozuku/analysis/document_preprocessor.hpp"
#include "mozuku/core/config.hpp"
#include "mozuku/core/types.hpp"
#include "mozuku/lsp/presenter.hpp"
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

class LSPServer {
public:
  LSPServer(std::istream &in, std::ostream &out);
  void run();

private:
  struct DocumentState {
    std::string text;
    std::string languageId;
    std::vector<TokenData> tokens;
    bool tokensCached{false};
    std::unordered_map<int, std::vector<Diagnostic>> diagnosticsByLine;
    std::vector<MoZuku::comments::CommentSegment> commentSegments;
    std::vector<ByteRange> contentHighlightRanges;
  };

  std::istream &in_;
  std::ostream &out_;

  // ドキュメント単位の状態: uri -> テキスト/解析結果/補助メタデータ
  std::unordered_map<std::string, DocumentState> documents_;
  std::vector<std::string> tokenTypes_;
  std::vector<std::string> tokenModifiers_;

  MoZukuConfig config_;

  std::unique_ptr<MoZuku::Analyzer> analyzer_;
  MoZuku::analysis::DocumentPreprocessor preprocessor_;
  MoZuku::lsp::Presenter presenter_;

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
  json onSelectionRange(const json &id, const json &params);

  DocumentState &ensureDocument(const std::string &uri);
  DocumentState *findDocument(const std::string &uri);
  const DocumentState *findDocument(const std::string &uri) const;
  static bool isJapaneseLanguage(const DocumentState &document);

  void analyzeAndPublish(const std::string &uri);
  void analyzeChangedLines(const std::string &uri, const std::string &newText,
                           const std::string &oldText);
  MoZuku::analysis::ProcessedDocument prepareDocument(DocumentState &document);
  json buildSemanticTokens(const std::string &uri);

  void cacheDiagnostics(DocumentState &document,
                        const std::vector<Diagnostic> &diags);
  void removeDiagnosticsForLines(DocumentState &document,
                                 const std::set<int> &lines);
  std::set<int> findChangedLines(const std::string &oldText,
                                 const std::string &newText) const;
};
