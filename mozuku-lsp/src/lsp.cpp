#include "lsp.hpp"
#include "analyzer.hpp"
#include "comment_extractor.hpp"
#include "mozuku/core/debug.hpp"
#include "pos_analyzer.hpp"
#include "text_processor.hpp"
#include "utf16.hpp"
#include "wikipedia.hpp"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <thread>

using nlohmann::json;

namespace {

bool readBoolOption(const json &obj, const char *key, bool &out) {
  if (!obj.contains(key)) {
    return false;
  }

  const auto &value = obj[key];
  if (value.is_boolean()) {
    out = value.get<bool>();
    return true;
  }

  if (value.is_number_integer()) {
    out = value.get<int>() != 0;
    return true;
  }

  return false;
}

} // namespace

LSPServer::LSPServer(std::istream &in, std::ostream &out) : in_(in), out_(out) {
  tokenTypes_ = {"noun",     "verb",   "adjective",   "adverb",
                 "particle", "aux",    "conjunction", "symbol",
                 "interj",   "prefix", "suffix",      "unknown"};
  tokenModifiers_ = {"proper", "numeric", "kana", "kanji"};

  // アナライザーを初期化
  analyzer_ = std::make_unique<MoZuku::Analyzer>();
}

bool LSPServer::readMessage(std::string &jsonPayload) {
  // 最小限のLSPヘッダー読み取り: Content-Length、空行、本文の順
  std::string line;
  size_t contentLength = 0;

  // 空行までヘッダーを読み取り
  while (std::getline(in_, line)) {
    if (!line.empty() && line.back() == '\r')
      line.pop_back();
    if (line.rfind("Content-Length:", 0) == 0) {
      contentLength = static_cast<size_t>(std::stoul(line.substr(15)));
    }
    if (line.empty())
      break; // 空行はヘッダー終了を示す
  }

  // ヘッダーを読み取れないかコンテント長が見つからない場合は失敗
  if (!contentLength || !in_.good())
    return false;

  // JSONペイロードを読み取り
  jsonPayload.resize(contentLength);
  in_.read(&jsonPayload[0], static_cast<std::streamsize>(contentLength));
  return in_.gcount() == static_cast<std::streamsize>(contentLength);
}

void LSPServer::reply(const json &msg) {
  std::string payload = msg.dump();
  out_ << "Content-Length: " << payload.size() << "\r\n\r\n" << payload;
  out_.flush();
}

void LSPServer::notify(const std::string &method, const json &params) {
  json msg = {{"jsonrpc", "2.0"}, {"method", method}, {"params", params}};
  reply(msg);
}

void LSPServer::handle(const json &req) {
  try {
    if (req.contains("method")) {
      std::string method = req["method"];

      if (method == "initialize") {
        reply(onInitialize(req["id"], req.value("params", json::object())));
      } else if (method == "initialized") {
        onInitialized();
      } else if (method == "textDocument/didOpen") {
        onDidOpen(req["params"]);
      } else if (method == "textDocument/didChange") {
        onDidChange(req["params"]);
      } else if (method == "textDocument/didSave") {
        onDidSave(req["params"]);
      } else if (method == "textDocument/semanticTokens/full") {
        reply(onSemanticTokensFull(req["id"],
                                   req.value("params", json::object())));
      } else if (method == "textDocument/semanticTokens/range") {
        reply(onSemanticTokensRange(req["id"],
                                    req.value("params", json::object())));
      } else if (method == "textDocument/hover") {
        reply(onHover(req["id"], req.value("params", json::object())));
      } else if (method == "textDocument/selectionRange") {
        reply(onSelectionRange(req["id"], req.value("params", json::object())));
      } else if (method == "shutdown") {
        reply(json{{"jsonrpc", "2.0"}, {"id", req["id"]}, {"result", nullptr}});
      } else if (method == "exit") {
        exit(0);
      }
    }
  } catch (const std::exception &e) {
    // クラッシュを避けるため基本的なエラーレスポンスを送信
    if (req.contains("id")) {
      json error = {{"jsonrpc", "2.0"},
                    {"id", req["id"]},
                    {"error", {{"code", -32603}, {"message", e.what()}}}};
      reply(error);
    }
  }
}

void LSPServer::run() {
  std::string jsonPayload;
  while (readMessage(jsonPayload)) {
    try {
      json req = json::parse(jsonPayload);
      handle(req);
    } catch (const json::parse_error &e) {
      if (MoZuku::debug::isEnabled()) {
        std::cerr << "[DEBUG] JSON parse error: " << e.what() << std::endl;
      }
    }
  }
}

json LSPServer::onInitialize(const json &id, const json &params) {
  // initializationOptionsから設定を抽出
  if (params.contains("initializationOptions")) {
    auto opts = params["initializationOptions"];
    if (opts.contains("mozuku") && opts["mozuku"].is_object()) {
      opts = opts["mozuku"];
    }

    // MeCab設定
    if (opts.contains("mecab")) {
      auto mecab = opts["mecab"];
      if (mecab.contains("dicdir") && mecab["dicdir"].is_string()) {
        config_.mecab.dicPath = mecab["dicdir"];
      }
      if (mecab.contains("charset") && mecab["charset"].is_string()) {
        config_.mecab.charset = mecab["charset"];
      }
    }

    // 解析設定
    if (opts.contains("analysis")) {
      auto analysis = opts["analysis"];
      readBoolOption(analysis, "enableCaboCha", config_.analysis.enableCaboCha);
      readBoolOption(analysis, "grammarCheck", config_.analysis.grammarCheck);
      if (analysis.contains("minJapaneseRatio") &&
          analysis["minJapaneseRatio"].is_number()) {
        config_.analysis.minJapaneseRatio = analysis["minJapaneseRatio"];
      }
      if (analysis.contains("warningMinSeverity") &&
          analysis["warningMinSeverity"].is_number()) {
        config_.analysis.warningMinSeverity = analysis["warningMinSeverity"];
      }

      // 警告レベル設定
      if (analysis.contains("warnings") && analysis["warnings"].is_object()) {
        auto warnings = analysis["warnings"];
        readBoolOption(warnings, "particleDuplicate",
                       config_.analysis.warnings.particleDuplicate);
        readBoolOption(warnings, "particleSequence",
                       config_.analysis.warnings.particleSequence);
        readBoolOption(warnings, "particleMismatch",
                       config_.analysis.warnings.particleMismatch);
        readBoolOption(warnings, "sentenceStructure",
                       config_.analysis.warnings.sentenceStructure);
        readBoolOption(warnings, "styleConsistency",
                       config_.analysis.warnings.styleConsistency);
        readBoolOption(warnings, "redundancy",
                       config_.analysis.warnings.redundancy);
      }

      // ルールの有効/無効設定
      if (analysis.contains("rules") && analysis["rules"].is_object()) {
        auto rules = analysis["rules"];
        readBoolOption(rules, "commaLimit", config_.analysis.rules.commaLimit);
        readBoolOption(rules, "adversativeGa",
                       config_.analysis.rules.adversativeGa);
        readBoolOption(rules, "duplicateParticleSurface",
                       config_.analysis.rules.duplicateParticleSurface);
        readBoolOption(rules, "adjacentParticles",
                       config_.analysis.rules.adjacentParticles);
        readBoolOption(rules, "conjunctionRepeat",
                       config_.analysis.rules.conjunctionRepeat);
        readBoolOption(rules, "raDropping", config_.analysis.rules.raDropping);
        if (rules.contains("commaLimitMax") &&
            rules["commaLimitMax"].is_number_integer()) {
          config_.analysis.rules.commaLimitMax = rules["commaLimitMax"];
        }
        if (rules.contains("adversativeGaMax") &&
            rules["adversativeGaMax"].is_number_integer()) {
          config_.analysis.rules.adversativeGaMax = rules["adversativeGaMax"];
        }
        if (rules.contains("duplicateParticleSurfaceMaxRepeat") &&
            rules["duplicateParticleSurfaceMaxRepeat"].is_number_integer()) {
          config_.analysis.rules.duplicateParticleSurfaceMaxRepeat =
              rules["duplicateParticleSurfaceMaxRepeat"];
        }
        if (rules.contains("adjacentParticlesMaxRepeat") &&
            rules["adjacentParticlesMaxRepeat"].is_number_integer()) {
          config_.analysis.rules.adjacentParticlesMaxRepeat =
              rules["adjacentParticlesMaxRepeat"];
        }
        if (rules.contains("conjunctionRepeatMax") &&
            rules["conjunctionRepeatMax"].is_number_integer()) {
          config_.analysis.rules.conjunctionRepeatMax =
              rules["conjunctionRepeatMax"];
        }
      }
    }
  }

  return json{{"jsonrpc", "2.0"},
              {"id", id},
              {"result",
               {{"capabilities",
                 {{"textDocumentSync",
                   {{"openClose", true},
                    {"change", 2}, // Incremental
                    {"save", {{"includeText", false}}}}},
                  {"semanticTokensProvider",
                   {{"legend",
                     {{"tokenTypes", tokenTypes_},
                      {"tokenModifiers", tokenModifiers_}}},
                    {"range", true},
                    {"full", true}}},
                  {"hoverProvider", true},
                  {"selectionRangeProvider", true}}}}}};
}

void LSPServer::onInitialized() {
  // 初期化完了
}

LSPServer::DocumentState &LSPServer::ensureDocument(const std::string &uri) {
  return documents_[uri];
}

LSPServer::DocumentState *LSPServer::findDocument(const std::string &uri) {
  auto it = documents_.find(uri);
  return it == documents_.end() ? nullptr : &it->second;
}

const LSPServer::DocumentState *
LSPServer::findDocument(const std::string &uri) const {
  auto it = documents_.find(uri);
  return it == documents_.end() ? nullptr : &it->second;
}

bool LSPServer::isJapaneseLanguage(const DocumentState &document) {
  return document.languageId == "japanese";
}

void LSPServer::onDidOpen(const json &params) {
  std::string uri = params["textDocument"]["uri"];
  std::string text = params["textDocument"]["text"];
  auto &document = ensureDocument(uri);
  document.text = text;
  document.tokens.clear();
  document.tokensCached = false;
  document.diagnosticsByLine.clear();
  if (params["textDocument"].contains("languageId") &&
      params["textDocument"]["languageId"].is_string()) {
    document.languageId = params["textDocument"]["languageId"];
  } else {
    document.languageId.clear();
  }
  analyzeAndPublish(uri);
}

void LSPServer::onDidChange(const json &params) {
  std::string uri = params["textDocument"]["uri"];
  auto changes = params["contentChanges"];

  auto &document = ensureDocument(uri);
  std::string &text = document.text;
  std::string oldText = text;

  // 位置を維持するため変更を逆順に適用
  for (auto it = changes.rbegin(); it != changes.rend(); ++it) {
    auto change = *it;
    if (change.contains("range")) {
      // 範囲指定のインクリメンタル変更
      auto range = change["range"];
      int startLine = range["start"]["line"];
      int startChar = range["start"]["character"];
      int endLine = range["end"]["line"];
      int endChar = range["end"]["character"];

      size_t startOffset = positionToByteOffset(text, startLine, startChar);
      size_t endOffset = positionToByteOffset(text, endLine, endChar);

      std::string newText = change["text"];
      text.replace(startOffset, endOffset - startOffset, newText);
    } else {
      // ドキュメント全体の変更
      text = change["text"];
    }
  }

  document.tokensCached = false;
  document.tokens.clear();

  // 最適化: 変更された行のみ再解析
  analyzeChangedLines(uri, text, oldText);
}

void LSPServer::onDidSave(const json &params) {
  std::string uri = params["textDocument"]["uri"];
  if (findDocument(uri) != nullptr) {
    analyzeAndPublish(uri);
  }
}

json LSPServer::onSemanticTokensFull(const json &id, const json &params) {
  std::string uri = params["textDocument"]["uri"];
  const auto *document = findDocument(uri);
  if (!document) {
    return json{{"jsonrpc", "2.0"}, {"id", id}, {"result", nullptr}};
  }

  if (!isJapaneseLanguage(*document)) {
    return json{{"jsonrpc", "2.0"}, {"id", id}, {"result", nullptr}};
  }

  json tokens = buildSemanticTokens(uri);
  return json{{"jsonrpc", "2.0"}, {"id", id}, {"result", {{"data", tokens}}}};
}

json LSPServer::onSemanticTokensRange(const json &id, const json &params) {
  std::string uri = params["textDocument"]["uri"];
  const auto *document = findDocument(uri);
  if (!document) {
    return json{{"jsonrpc", "2.0"}, {"id", id}, {"result", nullptr}};
  }

  if (!isJapaneseLanguage(*document)) {
    return json{{"jsonrpc", "2.0"}, {"id", id}, {"result", nullptr}};
  }

  json tokens = buildSemanticTokens(uri);
  return json{{"jsonrpc", "2.0"}, {"id", id}, {"result", {{"data", tokens}}}};
}

json LSPServer::onHover(const json &id, const json &params) {
  std::string uri = params["textDocument"]["uri"];
  const auto *document = findDocument(uri);
  if (!document || !document->tokensCached) {
    return json{{"jsonrpc", "2.0"}, {"id", id}, {"result", nullptr}};
  }

  int line = params["position"]["line"];
  int character = params["position"]["character"];

  // japanese 以外の言語では、コメント/コンテンツ範囲内でのみ hover を表示
  // (HTML: タグ内テキスト、LaTeX: タグ・数式以外のテキスト、その他: コメント内)
  bool isJapanese = isJapaneseLanguage(*document);

  if (!isJapanese) {
    size_t offset = positionToByteOffset(document->text, line, character);
    bool insideComment = false;
    for (const auto &segment : document->commentSegments) {
      if (offset >= segment.startByte && offset < segment.endByte) {
        insideComment = true;
        break;
      }
    }

    bool insideContent = false;
    if (document->languageId == "html" || document->languageId == "latex") {
      for (const auto &range : document->contentHighlightRanges) {
        if (offset >= range.startByte && offset < range.endByte) {
          insideContent = true;
          break;
        }
      }
    }

    if (!insideComment && !insideContent) {
      return json{{"jsonrpc", "2.0"}, {"id", id}, {"result", nullptr}};
    }
  }

  // 位置にあるトークンを検索
  const auto &tokens = document->tokens;
  for (const auto &token : tokens) {
    if (token.line == line && character >= token.startChar &&
        character < token.endChar) {
      std::ostringstream markdown;
      markdown << "**" << token.surface << "**\n";
      markdown << "```\n";
      markdown << token.feature << "\n";
      markdown << "```\n";
      if (!token.baseForm.empty()) {
        markdown << "**原形**: " << token.baseForm << "\n";
      }
      if (!token.reading.empty()) {
        markdown << "**読み**: " << token.reading << "\n";
      }
      if (!token.pronunciation.empty()) {
        markdown << "**発音**: " << token.pronunciation << "\n";
      }

      // 名詞の場合、Wikipediaサマリを追加
      if (token.tokenType == "noun" ||
          MoZuku::pos::POSAnalyzer::isNounFeature(token.feature)) {
        std::string query =
            token.baseForm.empty() ? token.surface : token.baseForm;

        auto &cache = wikipedia::WikipediaCache::getInstance();
        auto cached_entry = cache.getEntry(query);

        if (cached_entry) {
          if (cached_entry->response_code == 200) {
            markdown << "\n---\n";
            markdown << "**Wikipedia**: " << cached_entry->content;
          } else {
            markdown << "\n---\n";
            markdown << "**Wikipedia**: "
                     << wikipedia::getJapaneseErrorMessage(
                            cached_entry->response_code);
          }
        } else {
          if (MoZuku::debug::isEnabled()) {
            std::cerr << "[DEBUG] fetching Wikipedia: " << query << std::endl;
          }

          auto future = wikipedia::fetchSummary(query);

          std::thread([query, future = std::move(future)]() mutable {
            try {
              auto result = future.get();
              if (MoZuku::debug::isEnabled()) {
                std::cerr << "[DEBUG] Wikipedia取得完了: " << query
                          << ", ステータス: " << result.response_code
                          << std::endl;
              }
            } catch (const std::exception &e) {
              if (MoZuku::debug::isEnabled()) {
                std::cerr << "[DEBUG] Wikipedia取得失敗: " << query
                          << ", エラー: " << e.what() << std::endl;
              }
            }
          }).detach();
        }
      }

      return json{{"jsonrpc", "2.0"},
                  {"id", id},
                  {"result", presenter_.hoverResult(token, markdown.str())}};
    }
  }

  return json{{"jsonrpc", "2.0"}, {"id", id}, {"result", nullptr}};
}

json LSPServer::onSelectionRange(const json &id, const json &params) {
  std::string uri = params["textDocument"]["uri"];
  auto *document = findDocument(uri);
  if (!document) {
    return json{{"jsonrpc", "2.0"}, {"id", id}, {"result", json::array()}};
  }

  if (!params.contains("positions") || !params["positions"].is_array()) {
    return json{{"jsonrpc", "2.0"}, {"id", id}, {"result", json::array()}};
  }

  // トークンキャッシュを確保
  if (!document->tokensCached) {
    if (!analyzer_->isInitialized()) {
      analyzer_->initialize(config_);
    }
    auto prepared = prepareDocument(*document);
    document->tokens = analyzer_->analyzeText(prepared.analysisText);
    document->tokensCached = true;
  }

  const std::string &text = document->text;
  TextOffsetMapper mapper(text);

  // 句読点区切りの文境界を取得
  auto sentences = MoZuku::text::TextProcessor::splitIntoSentences(text);

  json result = json::array();

  for (const auto &posJson : params["positions"]) {
    int line = posJson["line"];
    int character = posJson["character"];
    size_t byteOffset = mapper.positionToByteOffset(line, character);

    // === Level 3 (最外): 段落 (改行区切り) ===
    size_t paraStart = 0;
    for (size_t i = byteOffset; i > 0; --i) {
      if (text[i - 1] == '\n') {
        paraStart = i;
        break;
      }
    }

    size_t paraEnd = text.size();
    for (size_t i = byteOffset; i < text.size(); ++i) {
      if (text[i] == '\n') {
        paraEnd = i;
        break;
      }
    }

    Position paraStartPos = mapper.byteOffsetToPosition(paraStart);
    Position paraEndPos = mapper.byteOffsetToPosition(paraEnd);

    // 最外の段落レンジから構築開始
    json selRange = {
        {"range",
         {{"start",
           {{"line", paraStartPos.line},
            {"character", paraStartPos.character}}},
          {"end",
           {{"line", paraEndPos.line}, {"character", paraEndPos.character}}}}}};

    // === Level 2: 行/文 (句読点区切り) ===
    for (const auto &sentence : sentences) {
      if (byteOffset >= sentence.start && byteOffset < sentence.end) {
        // 末尾の改行・CRを除外した文末位置を算出
        size_t sentEnd = sentence.end;
        if (sentEnd > 0 && sentEnd <= text.size() &&
            text[sentEnd - 1] == '\n') {
          sentEnd--;
        }
        if (sentEnd > 0 && sentEnd <= text.size() &&
            text[sentEnd - 1] == '\r') {
          sentEnd--;
        }

        // 段落と異なる場合のみ文レベルを追加
        if (sentence.start != paraStart || sentEnd != paraEnd) {
          Position sentStartPos = mapper.byteOffsetToPosition(sentence.start);
          Position sentEndPos = mapper.byteOffsetToPosition(sentEnd);

          selRange = {{"range",
                       {{"start",
                         {{"line", sentStartPos.line},
                          {"character", sentStartPos.character}}},
                        {"end",
                         {{"line", sentEndPos.line},
                          {"character", sentEndPos.character}}}}},
                      {"parent", selRange}};
        }
        break;
      }
    }

    // === Level 1 (最内): 形態素 ===
    // カーソルが形態素境界(終端)にある場合も直前トークンを選べるようにする
    const TokenData *selectedToken = nullptr;
    const TokenData *lineFirstToken = nullptr;
    const TokenData *lineLastToken = nullptr;

    for (const auto &token : document->tokens) {
      if (token.line != line) {
        continue;
      }

      if (!lineFirstToken || token.startChar < lineFirstToken->startChar) {
        lineFirstToken = &token;
      }
      if (!lineLastToken || token.endChar > lineLastToken->endChar) {
        lineLastToken = &token;
      }

      // 通常ケース: トークン内部
      if (character >= token.startChar && character < token.endChar) {
        selectedToken = &token;
        break;
      }

      // 境界ケース: トークン終端ちょうど
      if (!selectedToken && character == token.endChar) {
        selectedToken = &token;
      }
    }

    // 行頭/行末などで境界上にある場合のフォールバック
    if (!selectedToken) {
      if (lineFirstToken && character <= lineFirstToken->startChar) {
        selectedToken = lineFirstToken;
      } else if (lineLastToken && character >= lineLastToken->endChar) {
        selectedToken = lineLastToken;
      }
    }

    if (selectedToken) {
      selRange = {{"range",
                   {{"start",
                     {{"line", selectedToken->line},
                      {"character", selectedToken->startChar}}},
                    {"end",
                     {{"line", selectedToken->line},
                      {"character", selectedToken->endChar}}}}},
                  {"parent", selRange}};
    }

    result.push_back(selRange);
  }

  return json{{"jsonrpc", "2.0"}, {"id", id}, {"result", result}};
}

void LSPServer::analyzeAndPublish(const std::string &uri) {
  auto &document = ensureDocument(uri);
  const std::string &text = document.text;

  if (!analyzer_->isInitialized()) {
    analyzer_->initialize(config_);
  }

  auto prepared = prepareDocument(document);

  std::vector<TokenData> tokens = analyzer_->analyzeText(prepared.analysisText);
  std::vector<Diagnostic> diags =
      analyzer_->checkGrammar(prepared.analysisText);

  document.tokens = tokens;
  document.tokensCached = true;
  cacheDiagnostics(document, diags);

  notify("textDocument/publishDiagnostics",
         presenter_.publishDiagnosticsParams(uri, diags));

  // コンテンツ範囲を通知 (コメント範囲 or HTML/LaTeX のコンテンツ範囲)
  // HTML: タグ内テキスト、LaTeX: タグ・数式以外のテキスト
  static const std::vector<MoZuku::comments::CommentSegment> kEmptySegments;
  if (!document.commentSegments.empty()) {
    notify("mozuku/commentHighlights",
           presenter_.commentHighlightsParams(uri, text,
                                              document.commentSegments));
  } else {
    notify("mozuku/commentHighlights",
           presenter_.commentHighlightsParams(uri, text, kEmptySegments));
  }

  static const std::vector<ByteRange> kEmptyContent;
  if (!document.contentHighlightRanges.empty()) {
    notify("mozuku/contentHighlights",
           presenter_.contentHighlightsParams(uri, text,
                                              document.contentHighlightRanges));
  } else {
    notify("mozuku/contentHighlights",
           presenter_.contentHighlightsParams(uri, text, kEmptyContent));
  }

  bool isJapanese = isJapaneseLanguage(document);
  notify("mozuku/semanticHighlights",
         presenter_.semanticHighlightsParams(uri, isJapanese, tokens));
}

void LSPServer::analyzeChangedLines(const std::string &uri,
                                    const std::string &newText,
                                    const std::string &oldText) {
  // 変更された行を検出
  std::set<int> changedLines = findChangedLines(oldText, newText);

  // 変更行の診断情報を削除
  if (auto *document = findDocument(uri)) {
    removeDiagnosticsForLines(*document, changedLines);
  }

  // 現在は文書全体を再解析
  // TODO: パフォーマンス向上のため行単位の解析を実装
  analyzeAndPublish(uri);
}

MoZuku::analysis::ProcessedDocument
LSPServer::prepareDocument(DocumentState &document) {
  if (document.languageId.empty()) {
    document.commentSegments.clear();
    document.contentHighlightRanges.clear();
    return {document.text, {}, {}};
  }

  auto prepared = preprocessor_.prepare(document.languageId, document.text);
  document.commentSegments = prepared.commentSegments;
  document.contentHighlightRanges = prepared.contentHighlightRanges;

  return prepared;
}

json LSPServer::buildSemanticTokens(const std::string &uri) {
  auto *document = findDocument(uri);
  if (!document) {
    return json::array();
  }

  if (document->tokensCached) {
    return presenter_.semanticTokensData(document->tokens, tokenTypes_);
  }

  if (!analyzer_->isInitialized()) {
    analyzer_->initialize(config_);
  }

  auto prepared = prepareDocument(*document);
  std::vector<TokenData> tokens = analyzer_->analyzeText(prepared.analysisText);
  document->tokens = tokens;
  document->tokensCached = true;

  return presenter_.semanticTokensData(document->tokens, tokenTypes_);
}

void LSPServer::cacheDiagnostics(DocumentState &document,
                                 const std::vector<Diagnostic> &diags) {
  document.diagnosticsByLine.clear();

  for (const auto &diag : diags) {
    int line = diag.range.start.line;
    document.diagnosticsByLine[line].push_back(diag);
  }
}

void LSPServer::removeDiagnosticsForLines(DocumentState &document,
                                          const std::set<int> &lines) {
  for (int line : lines) {
    document.diagnosticsByLine.erase(line);
  }
}

std::set<int> LSPServer::findChangedLines(const std::string &oldText,
                                          const std::string &newText) const {
  std::set<int> changedLines;

  // シンプルな行単位の比較
  std::vector<std::string> oldLines, newLines;

  // 行に分割
  std::istringstream oldStream(oldText), newStream(newText);
  std::string line;
  while (std::getline(oldStream, line))
    oldLines.push_back(line);
  while (std::getline(newStream, line))
    newLines.push_back(line);

  size_t maxLines = std::max(oldLines.size(), newLines.size());
  for (size_t i = 0; i < maxLines; ++i) {
    bool oldExists = i < oldLines.size();
    bool newExists = i < newLines.size();

    if (!oldExists || !newExists || oldLines[i] != newLines[i]) {
      changedLines.insert(static_cast<int>(i));
    }
  }

  return changedLines;
}
