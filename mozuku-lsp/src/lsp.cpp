#include "lsp.hpp"
#include "analyzer.hpp"
#include "comment_extractor.hpp"
#include "utf16.hpp"
#include "wikipedia.hpp"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <thread>

#include <tree_sitter/api.h>

using nlohmann::json;

static bool isDebugEnabled() {
  static bool initialized = false;
  static bool debug = false;
  if (!initialized) {
    debug = (std::getenv("MOZUKU_DEBUG") != nullptr);
    initialized = true;
  }
  return debug;
}

namespace {

struct LocalByteRange {
  size_t startByte{0};
  size_t endByte{0};
};

bool isEscaped(const std::string &text, size_t pos) {
  size_t count = 0;
  while (pos > count && text[pos - count - 1] == '\\') {
    ++count;
  }
  return (count % 2) == 1;
}

size_t findClosingDollar(const std::string &text, size_t pos) {
  for (size_t i = pos; i < text.size(); ++i) {
    if (text[i] == '$' && !isEscaped(text, i)) {
      return i;
    }
  }
  return std::string::npos;
}

size_t findClosingDoubleDollar(const std::string &text, size_t pos) {
  for (size_t i = pos; i + 1 < text.size(); ++i) {
    if (text[i] == '$' && text[i + 1] == '$' && !isEscaped(text, i)) {
      return i;
    }
  }
  return std::string::npos;
}

size_t findClosingCommand(const std::string &text, size_t pos,
                          const std::string &closing) {
  size_t current = pos;
  while (current < text.size()) {
    size_t found = text.find(closing, current);
    if (found == std::string::npos)
      return std::string::npos;
    if (!isEscaped(text, found))
      return found;
    current = found + closing.size();
  }
  return std::string::npos;
}

std::string processLatexMath(const std::string &text) { return text; }

std::string sanitizeLatexCommentText(const std::string &raw) {
  if (raw.empty())
    return raw;

  std::string sanitized = raw;
  sanitized[0] = ' ';
  size_t idx = 1;
  while (idx < sanitized.size() && sanitized[idx] == '%') {
    sanitized[idx] = ' ';
    ++idx;
  }
  while (idx < sanitized.size() &&
         (sanitized[idx] == ' ' || sanitized[idx] == '\t')) {
    sanitized[idx] = ' ';
    ++idx;
  }
  return sanitized;
}

std::vector<MoZuku::comments::CommentSegment>
collectLatexComments(const std::string &text) {
  std::vector<MoZuku::comments::CommentSegment> segments;
  size_t pos = 0;
  while (pos < text.size()) {
    size_t lineStart = pos;
    size_t lineEnd = text.find('\n', pos);
    if (lineEnd == std::string::npos)
      lineEnd = text.size();

    size_t current = lineStart;
    bool found = false;
    while (current < lineEnd) {
      if (text[current] == '%' && !isEscaped(text, current)) {
        found = true;
        break;
      }
      ++current;
    }

    if (found) {
      MoZuku::comments::CommentSegment segment;
      segment.startByte = current;
      segment.endByte = lineEnd;
      segment.sanitized =
          sanitizeLatexCommentText(text.substr(current, lineEnd - current));
      segments.push_back(std::move(segment));
    }

    if (lineEnd >= text.size())
      break;
    pos = lineEnd + 1;
  }

  return segments;
}

size_t utf8CharLen(unsigned char c) {
  if (c < 0x80)
    return 1;
  if ((c >> 5) == 0x6)
    return 2;
  if ((c >> 4) == 0xE)
    return 3;
  if ((c >> 3) == 0x1E)
    return 4;
  return 1;
}

std::vector<LocalByteRange> collectHtmlContentRanges(const std::string &text) {
  std::vector<LocalByteRange> ranges;
  const TSLanguage *language = MoZuku::comments::resolveLanguage("html");
  if (!language)
    return ranges;

  TSParser *parser = ts_parser_new();
  if (!parser)
    return ranges;

  std::unique_ptr<TSParser, decltype(&ts_parser_delete)> parserGuard(
      parser, &ts_parser_delete);
  if (!ts_parser_set_language(parser, language)) {
    return ranges;
  }

  TSTree *tree =
      ts_parser_parse_string(parser, nullptr, text.c_str(), text.size());
  if (!tree)
    return ranges;

  std::unique_ptr<TSTree, decltype(&ts_tree_delete)> treeGuard(tree,
                                                               &ts_tree_delete);

  TSNode root = ts_tree_root_node(tree);
  if (ts_node_is_null(root))
    return ranges;

  std::vector<TSNode> stack;
  stack.push_back(root);

  while (!stack.empty()) {
    TSNode node = stack.back();
    stack.pop_back();

    if (ts_node_is_null(node))
      continue;

    const char *type = ts_node_type(node);
    if (type && std::strcmp(type, "text") == 0) {
      size_t start = ts_node_start_byte(node);
      size_t end = ts_node_end_byte(node);
      if (start >= end || end > text.size())
        continue;

      size_t trimmedStart = start;
      while (trimmedStart < end &&
             std::isspace(static_cast<unsigned char>(text[trimmedStart]))) {
        ++trimmedStart;
      }
      size_t trimmedEnd = end;
      while (trimmedEnd > trimmedStart &&
             std::isspace(static_cast<unsigned char>(text[trimmedEnd - 1]))) {
        --trimmedEnd;
      }
      if (trimmedEnd > trimmedStart) {
        ranges.push_back({trimmedStart, trimmedEnd});
      }
      continue;
    }

    uint32_t childCount = ts_node_child_count(node);
    for (uint32_t i = 0; i < childCount; ++i) {
      TSNode child = ts_node_child(node, i);
      if (!ts_node_is_null(child)) {
        stack.push_back(child);
      }
    }
  }

  return ranges;
}

std::vector<LocalByteRange> collectLatexContentRanges(const std::string &text) {
  std::vector<LocalByteRange> ranges;
  size_t i = 0;
  while (i < text.size()) {
    unsigned char c = static_cast<unsigned char>(text[i]);
    if (c == '%' && !isEscaped(text, i)) {
      size_t lineEnd = text.find('\n', i);
      if (lineEnd == std::string::npos)
        break;
      i = lineEnd + 1;
      continue;
    }
    if (c == '$' && !isEscaped(text, i)) {
      if (i + 1 < text.size() && text[i + 1] == '$') {
        size_t closing = findClosingDoubleDollar(text, i + 2);
        if (closing == std::string::npos)
          break;
        i = closing + 2;
        continue;
      } else {
        size_t closing = findClosingDollar(text, i + 1);
        if (closing == std::string::npos)
          break;
        i = closing + 1;
        continue;
      }
    }
    if (c == '\\') {
      ++i;
      while (i < text.size()) {
        unsigned char ch = static_cast<unsigned char>(text[i]);
        if (!std::isalpha(ch) && ch != '@')
          break;
        ++i;
      }
      if (i < text.size() && text[i] == '*')
        ++i;
      continue;
    }
    if (c == '{' || c == '}') {
      ++i;
      continue;
    }
    if (std::isspace(c)) {
      ++i;
      continue;
    }

    size_t start = i;
    bool advanced = false;
    while (i < text.size()) {
      unsigned char d = static_cast<unsigned char>(text[i]);
      if (d == '\\' || d == '$' || d == '{' || d == '}' ||
          (d == '%' && !isEscaped(text, i))) {
        break;
      }
      if (d < 0x80) {
        if (std::isspace(d) || std::ispunct(d))
          break;
      }
      size_t len = utf8CharLen(d);
      i += len;
      advanced = true;
    }
    if (advanced) {
      ranges.push_back({start, i});
      continue;
    }
    // ensure progress to avoid infinite loop
    if (!advanced)
      ++i;
  }

  return ranges;
}

std::vector<LocalByteRange>
collectContentHighlightRanges(const std::string &languageId,
                              const std::string &text) {
  if (languageId == "html") {
    return collectHtmlContentRanges(text);
  }
  if (languageId == "latex") {
    return collectLatexContentRanges(text);
  }
  return {};
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
      if (isDebugEnabled()) {
        std::cerr << "[DEBUG] JSON parse error: " << e.what() << std::endl;
      }
    }
  }
}

json LSPServer::onInitialize(const json &id, const json &params) {
  // initializationOptionsから設定を抽出
  if (params.contains("initializationOptions")) {
    auto opts = params["initializationOptions"];

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
      if (analysis.contains("enableCaboCha") &&
          analysis["enableCaboCha"].is_boolean()) {
        config_.analysis.enableCaboCha = analysis["enableCaboCha"];
      }
      if (analysis.contains("grammarCheck") &&
          analysis["grammarCheck"].is_boolean()) {
        config_.analysis.grammarCheck = analysis["grammarCheck"];
      }
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
        if (warnings.contains("particleDuplicate") &&
            warnings["particleDuplicate"].is_boolean()) {
          config_.analysis.warnings.particleDuplicate =
              warnings["particleDuplicate"];
        }
        if (warnings.contains("particleSequence") &&
            warnings["particleSequence"].is_boolean()) {
          config_.analysis.warnings.particleSequence =
              warnings["particleSequence"];
        }
        if (warnings.contains("particleMismatch") &&
            warnings["particleMismatch"].is_boolean()) {
          config_.analysis.warnings.particleMismatch =
              warnings["particleMismatch"];
        }
        if (warnings.contains("sentenceStructure") &&
            warnings["sentenceStructure"].is_boolean()) {
          config_.analysis.warnings.sentenceStructure =
              warnings["sentenceStructure"];
        }
        if (warnings.contains("styleConsistency") &&
            warnings["styleConsistency"].is_boolean()) {
          config_.analysis.warnings.styleConsistency =
              warnings["styleConsistency"];
        }
        if (warnings.contains("redundancy") &&
            warnings["redundancy"].is_boolean()) {
          config_.analysis.warnings.redundancy = warnings["redundancy"];
        }
      }

      // ルールの有効/無効設定
      if (analysis.contains("rules") && analysis["rules"].is_object()) {
        auto rules = analysis["rules"];
        if (rules.contains("commaLimit") && rules["commaLimit"].is_boolean()) {
          config_.analysis.rules.commaLimit = rules["commaLimit"];
        }
        if (rules.contains("adversativeGa") &&
            rules["adversativeGa"].is_boolean()) {
          config_.analysis.rules.adversativeGa = rules["adversativeGa"];
        }
        if (rules.contains("duplicateParticleSurface") &&
            rules["duplicateParticleSurface"].is_boolean()) {
          config_.analysis.rules.duplicateParticleSurface =
              rules["duplicateParticleSurface"];
        }
        if (rules.contains("adjacentParticles") &&
            rules["adjacentParticles"].is_boolean()) {
          config_.analysis.rules.adjacentParticles = rules["adjacentParticles"];
        }
        if (rules.contains("conjunctionRepeat") &&
            rules["conjunctionRepeat"].is_boolean()) {
          config_.analysis.rules.conjunctionRepeat = rules["conjunctionRepeat"];
        }
        if (rules.contains("raDropping") && rules["raDropping"].is_boolean()) {
          config_.analysis.rules.raDropping = rules["raDropping"];
        }
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
                  {"hoverProvider", true}}}}}};
}

void LSPServer::onInitialized() {
  // 初期化完了
}

void LSPServer::onDidOpen(const json &params) {
  std::string uri = params["textDocument"]["uri"];
  std::string text = params["textDocument"]["text"];
  docs_[uri] = text;
  if (params["textDocument"].contains("languageId") &&
      params["textDocument"]["languageId"].is_string()) {
    docLanguages_[uri] = params["textDocument"]["languageId"];
  }
  analyzeAndPublish(uri, text);
}

void LSPServer::onDidChange(const json &params) {
  std::string uri = params["textDocument"]["uri"];
  auto changes = params["contentChanges"];

  std::string &text = docs_[uri];
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

      size_t startOffset = computeByteOffset(text, startLine, startChar);
      size_t endOffset = computeByteOffset(text, endLine, endChar);

      std::string newText = change["text"];
      text.replace(startOffset, endOffset - startOffset, newText);
    } else {
      // ドキュメント全体の変更
      text = change["text"];
    }
  }

  // 最適化: 変更された行のみ再解析
  analyzeChangedLines(uri, text, oldText);
}

void LSPServer::onDidSave(const json &params) {
  std::string uri = params["textDocument"]["uri"];
  if (docs_.find(uri) != docs_.end()) {
    analyzeAndPublish(uri, docs_[uri]);
  }
}

json LSPServer::onSemanticTokensFull(const json &id, const json &params) {
  std::string uri = params["textDocument"]["uri"];
  if (docs_.find(uri) == docs_.end()) {
    return json{{"jsonrpc", "2.0"}, {"id", id}, {"result", nullptr}};
  }

  auto langIt = docLanguages_.find(uri);
  if (langIt == docLanguages_.end() || langIt->second != "japanese") {
    return json{{"jsonrpc", "2.0"}, {"id", id}, {"result", nullptr}};
  }

  json tokens = buildSemanticTokens(uri);
  return json{{"jsonrpc", "2.0"}, {"id", id}, {"result", {{"data", tokens}}}};
}

json LSPServer::onSemanticTokensRange(const json &id, const json &params) {
  std::string uri = params["textDocument"]["uri"];
  if (docs_.find(uri) == docs_.end()) {
    return json{{"jsonrpc", "2.0"}, {"id", id}, {"result", nullptr}};
  }

  auto langIt = docLanguages_.find(uri);
  if (langIt == docLanguages_.end() || langIt->second != "japanese") {
    return json{{"jsonrpc", "2.0"}, {"id", id}, {"result", nullptr}};
  }

  json tokens = buildSemanticTokens(uri);
  return json{{"jsonrpc", "2.0"}, {"id", id}, {"result", {{"data", tokens}}}};
}

bool isNoun(const std::string &tokenType, const std::string &feature) {
  // tokenTypeが "noun" の場合
  if (tokenType == "noun") {
    return true;
  }

  // MeCabのfeature文字列から品詞を判定
  // feature形式:
  // "品詞,品詞細分類1,品詞細分類2,品詞細分類3,活用型,活用形,原形,読み,発音"
  if (!feature.empty()) {
    size_t commaPos = feature.find(',');
    if (commaPos != std::string::npos) {
      std::string mainPOS = feature.substr(0, commaPos);
      return mainPOS == "名詞";
    }
  }

  return false;
}

json LSPServer::onHover(const json &id, const json &params) {
  std::string uri = params["textDocument"]["uri"];
  if (docs_.find(uri) == docs_.end() ||
      docTokens_.find(uri) == docTokens_.end()) {
    return json{{"jsonrpc", "2.0"}, {"id", id}, {"result", nullptr}};
  }

  int line = params["position"]["line"];
  int character = params["position"]["character"];

  const auto docIt = docs_.find(uri);
  if (docIt == docs_.end()) {
    return json{{"jsonrpc", "2.0"}, {"id", id}, {"result", nullptr}};
  }

  // japanese 以外の言語では、コメント/コンテンツ範囲内でのみ hover を表示
  // (HTML: タグ内テキスト、LaTeX: タグ・数式以外のテキスト、その他: コメント内)
  auto langIt = docLanguages_.find(uri);
  bool isJapanese =
      (langIt != docLanguages_.end() && langIt->second == "japanese");

  if (!isJapanese) {
    size_t offset = computeByteOffset(docIt->second, line, character);
    bool insideComment = false;
    const auto segmentsIt = docCommentSegments_.find(uri);
    if (segmentsIt != docCommentSegments_.end()) {
      for (const auto &segment : segmentsIt->second) {
        if (offset >= segment.startByte && offset < segment.endByte) {
          insideComment = true;
          break;
        }
      }
    }

    bool insideContent = false;
    if (langIt != docLanguages_.end() &&
        (langIt->second == "html" || langIt->second == "latex")) {
      const auto contentIt = docContentHighlightRanges_.find(uri);
      if (contentIt != docContentHighlightRanges_.end()) {
        for (const auto &range : contentIt->second) {
          if (offset >= range.startByte && offset < range.endByte) {
            insideContent = true;
            break;
          }
        }
      }
    }

    if (!insideComment && !insideContent) {
      return json{{"jsonrpc", "2.0"}, {"id", id}, {"result", nullptr}};
    }
  }

  // 位置にあるトークンを検索
  const auto &tokens = docTokens_[uri];
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
      if (isNoun(token.tokenType, token.feature)) {
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
          if (isDebugEnabled()) {
            std::cerr << "[DEBUG] fetching Wikipedia: " << query << std::endl;
          }

          auto future = wikipedia::fetchSummary(query);

          std::thread([query, future = std::move(future)]() mutable {
            try {
              auto result = future.get();
              if (isDebugEnabled()) {
                std::cerr << "[DEBUG] Wikipedia取得完了: " << query
                          << ", ステータス: " << result.response_code
                          << std::endl;
              }
            } catch (const std::exception &e) {
              if (isDebugEnabled()) {
                std::cerr << "[DEBUG] Wikipedia取得失敗: " << query
                          << ", エラー: " << e.what() << std::endl;
              }
            }
          }).detach();
        }
      }

      return json{
          {"jsonrpc", "2.0"},
          {"id", id},
          {"result",
           {{"contents", {{"kind", "markdown"}, {"value", markdown.str()}}},
            {"range",
             {{"start", {{"line", token.line}, {"character", token.startChar}}},
              {"end",
               {{"line", token.line}, {"character", token.endChar}}}}}}}};
    }
  }

  return json{{"jsonrpc", "2.0"}, {"id", id}, {"result", nullptr}};
}

void LSPServer::analyzeAndPublish(const std::string &uri,
                                  const std::string &text) {
  if (!analyzer_->isInitialized()) {
    analyzer_->initialize(config_);
  }

  std::string analysisText = prepareAnalysisText(uri, text);

  std::vector<TokenData> tokens = analyzer_->analyzeText(analysisText);
  std::vector<Diagnostic> diags = analyzer_->checkGrammar(analysisText);

  docTokens_[uri] = tokens;
  cacheDiagnostics(uri, diags);

  // 診断情報を配信
  json diagnostics = json::array();
  for (const auto &diag : diags) {
    diagnostics.push_back({{"range",
                            {{"start",
                              {{"line", diag.range.start.line},
                               {"character", diag.range.start.character}}},
                             {"end",
                              {{"line", diag.range.end.line},
                               {"character", diag.range.end.character}}}}},
                           {"severity", diag.severity},
                           {"message", diag.message}});
  }

  notify("textDocument/publishDiagnostics",
         {{"uri", uri}, {"diagnostics", diagnostics}});

  // コンテンツ範囲を通知 (コメント範囲 or HTML/LaTeX のコンテンツ範囲)
  // HTML: タグ内テキスト、LaTeX: タグ・数式以外のテキスト
  const auto segmentsIt = docCommentSegments_.find(uri);
  if (segmentsIt != docCommentSegments_.end()) {
    sendCommentHighlights(uri, text, segmentsIt->second);
  } else {
    static const std::vector<MoZuku::comments::CommentSegment> kEmptySegments;
    sendCommentHighlights(uri, text, kEmptySegments);
  }

  const auto contentIt = docContentHighlightRanges_.find(uri);
  if (contentIt != docContentHighlightRanges_.end()) {
    sendContentHighlights(uri, text, contentIt->second);
  } else {
    static const std::vector<ByteRange> kEmptyContent;
    sendContentHighlights(uri, text, kEmptyContent);
  }

  sendSemanticHighlights(uri, tokens);
}

void LSPServer::analyzeChangedLines(const std::string &uri,
                                    const std::string &newText,
                                    const std::string &oldText) {
  // 変更された行を検出
  std::set<int> changedLines = findChangedLines(oldText, newText);

  // 変更行の診断情報を削除
  removeDiagnosticsForLines(uri, changedLines);

  // 現在は文書全体を再解析
  // TODO: パフォーマンス向上のため行単位の解析を実装
  analyzeAndPublish(uri, newText);
}

std::string LSPServer::prepareAnalysisText(const std::string &uri,
                                           const std::string &text) {
  auto langIt = docLanguages_.find(uri);
  if (langIt == docLanguages_.end()) {
    docCommentSegments_.erase(uri);
    docContentHighlightRanges_.erase(uri);
    return text;
  }

  const std::string &languageId = langIt->second;
  if (languageId == "japanese") {
    docCommentSegments_.erase(uri);
    docContentHighlightRanges_.erase(uri);
    return text;
  }

  // HTML: ドキュメント本文をハイライト (<div>text</div> の text 部分)
  if (languageId == "html") {
    std::vector<MoZuku::comments::CommentSegment> commentSegments =
        MoZuku::comments::extractComments(languageId, text);
    docCommentSegments_[uri] = commentSegments;

    std::vector<LocalByteRange> contentRanges = collectHtmlContentRanges(text);
    std::vector<ByteRange> contentByteRanges;
    contentByteRanges.reserve(contentRanges.size());
    for (const auto &range : contentRanges) {
      contentByteRanges.push_back(ByteRange{range.startByte, range.endByte});
    }
    // コメントも本文ハイライト対象に含める (クライアント側で装飾しやすくする)
    for (const auto &segment : commentSegments) {
      contentByteRanges.push_back(
          ByteRange{segment.startByte, segment.endByte});
    }
    docContentHighlightRanges_[uri] = std::move(contentByteRanges);

    // 全体をマスクしてコンテンツ部分のみ復元
    std::string masked = text;
    for (char &ch : masked) {
      if (ch != '\n' && ch != '\r') {
        ch = ' ';
      }
    }

    for (const auto &range : contentRanges) {
      if (range.startByte >= masked.size())
        continue;
      size_t len = std::min(range.endByte - range.startByte,
                            masked.size() - range.startByte);
      for (size_t i = 0; i < len; ++i) {
        masked[range.startByte + i] = text[range.startByte + i];
      }
    }

    for (const auto &segment : commentSegments) {
      if (segment.startByte >= masked.size())
        continue;
      size_t len =
          std::min(segment.sanitized.size(), masked.size() - segment.startByte);
      for (size_t i = 0; i < len; ++i) {
        masked[segment.startByte + i] = segment.sanitized[i];
      }
    }

    return masked;
  }

  // LaTeX: ドキュメント本文をハイライト (タグ・数式を除くテキスト部分)
  if (languageId == "latex") {
    std::vector<MoZuku::comments::CommentSegment> commentSegments =
        collectLatexComments(text);
    docCommentSegments_[uri] = commentSegments;

    std::vector<LocalByteRange> contentRanges = collectLatexContentRanges(text);
    std::vector<ByteRange> contentByteRanges;
    contentByteRanges.reserve(contentRanges.size());
    for (const auto &range : contentRanges) {
      contentByteRanges.push_back(ByteRange{range.startByte, range.endByte});
    }
    for (const auto &segment : commentSegments) {
      contentByteRanges.push_back(
          ByteRange{segment.startByte, segment.endByte});
    }
    docContentHighlightRanges_[uri] = std::move(contentByteRanges);

    // 全体をマスクしてコンテンツ部分のみ復元
    std::string masked = text;
    for (char &ch : masked) {
      if (ch != '\n' && ch != '\r') {
        ch = ' ';
      }
    }

    for (const auto &range : contentRanges) {
      if (range.startByte >= masked.size())
        continue;
      size_t len = std::min(range.endByte - range.startByte,
                            masked.size() - range.startByte);
      for (size_t i = 0; i < len; ++i) {
        masked[range.startByte + i] = text[range.startByte + i];
      }
    }

    for (const auto &segment : commentSegments) {
      if (segment.startByte >= masked.size())
        continue;
      size_t len =
          std::min(segment.sanitized.size(), masked.size() - segment.startByte);
      for (size_t i = 0; i < len; ++i) {
        masked[segment.startByte + i] = segment.sanitized[i];
      }
    }

    return masked;
  }

  if (!MoZuku::comments::isLanguageSupported(languageId)) {
    docCommentSegments_.erase(uri);
    docContentHighlightRanges_.erase(uri);
    return text;
  }

  // その他の言語: コメント部分をハイライト
  std::vector<MoZuku::comments::CommentSegment> segments =
      MoZuku::comments::extractComments(languageId, text);
  docCommentSegments_[uri] = segments;
  docContentHighlightRanges_.erase(uri);

  std::string masked = text;
  for (char &ch : masked) {
    if (ch != '\n' && ch != '\r') {
      ch = ' ';
    }
  }

  if (segments.empty()) {
    return masked;
  }

  const size_t docSize = masked.size();
  for (const auto &segment : segments) {
    if (segment.startByte >= docSize) {
      continue;
    }
    const std::string &sanitized = segment.sanitized;
    size_t maxCopy = std::min(docSize - segment.startByte, sanitized.size());
    for (size_t i = 0; i < maxCopy; ++i) {
      masked[segment.startByte + i] = sanitized[i];
    }
  }

  return masked;
}

void LSPServer::sendCommentHighlights(
    const std::string &uri, const std::string &text,
    const std::vector<MoZuku::comments::CommentSegment> &segments) {
  json ranges = json::array();

  std::vector<size_t> lineStarts = computeLineStarts(text);
  for (const auto &segment : segments) {
    Position start = byteOffsetToPosition(text, lineStarts, segment.startByte);
    Position end = byteOffsetToPosition(text, lineStarts, segment.endByte);

    json range = {
        {"start", {{"line", start.line}, {"character", start.character}}},
        {"end", {{"line", end.line}, {"character", end.character}}}};
    ranges.push_back(std::move(range));
  }

  notify("mozuku/commentHighlights", {{"uri", uri}, {"ranges", ranges}});
}

void LSPServer::sendContentHighlights(const std::string &uri,
                                      const std::string &text,
                                      const std::vector<ByteRange> &ranges) {
  json lspRanges = json::array();

  std::vector<size_t> lineStarts = computeLineStarts(text);
  for (const auto &range : ranges) {
    Position start = byteOffsetToPosition(text, lineStarts, range.startByte);
    Position end = byteOffsetToPosition(text, lineStarts, range.endByte);

    lspRanges.push_back(
        {{"start", {{"line", start.line}, {"character", start.character}}},
         {"end", {{"line", end.line}, {"character", end.character}}}});
  }

  notify("mozuku/contentHighlights", {{"uri", uri}, {"ranges", lspRanges}});
}

void LSPServer::sendSemanticHighlights(const std::string &uri,
                                       const std::vector<TokenData> &tokens) {
  auto langIt = docLanguages_.find(uri);
  bool isJapanese =
      (langIt != docLanguages_.end() && langIt->second == "japanese");

  // japanese の場合のみセマンティックハイライトを無効化
  // (.ja.txt, .ja.md は LSP 側のセマンティックトークンを使用)
  // HTML/LaTeX など他の言語は VS Code 拡張側の上塗りハイライトを使用
  if (isJapanese) {
    notify("mozuku/semanticHighlights",
           {{"uri", uri}, {"tokens", json::array()}});
    return;
  }

  json tokenEntries = json::array();
  for (const auto &token : tokens) {
    tokenEntries.push_back(
        {{"range",
          {{"start", {{"line", token.line}, {"character", token.startChar}}},
           {"end", {{"line", token.line}, {"character", token.endChar}}}}},
         {"type", token.tokenType},
         {"modifiers", token.tokenModifiers}});
  }

  notify("mozuku/semanticHighlights", {{"uri", uri}, {"tokens", tokenEntries}});
}

json LSPServer::buildSemanticTokens(const std::string &uri) {
  auto docIt = docs_.find(uri);
  if (docIt == docs_.end()) {
    return json::array();
  }

  auto cached = docTokens_.find(uri);
  if (cached != docTokens_.end()) {
    return buildSemanticTokensFromTokens(cached->second);
  }

  if (!analyzer_->isInitialized()) {
    analyzer_->initialize(config_);
  }

  std::string analysisText = prepareAnalysisText(uri, docIt->second);
  std::vector<TokenData> tokens = analyzer_->analyzeText(analysisText);
  docTokens_[uri] = tokens;

  return buildSemanticTokensFromTokens(tokens);
}

json LSPServer::buildSemanticTokensFromTokens(
    const std::vector<TokenData> &tokens) {
  json data = json::array();

  int prevLine = 0, prevChar = 0;

  for (const auto &token : tokens) {
    int deltaLine = token.line - prevLine;
    int deltaChar =
        (deltaLine == 0) ? token.startChar - prevChar : token.startChar;

    auto typeIt =
        std::find(tokenTypes_.begin(), tokenTypes_.end(), token.tokenType);
    int typeIndex =
        (typeIt != tokenTypes_.end())
            ? static_cast<int>(std::distance(tokenTypes_.begin(), typeIt))
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

void LSPServer::cacheDiagnostics(const std::string &uri,
                                 const std::vector<Diagnostic> &diags) {
  docDiagnostics_[uri].clear();

  for (const auto &diag : diags) {
    int line = diag.range.start.line;
    docDiagnostics_[uri][line].push_back(diag);
  }
}

void LSPServer::removeDiagnosticsForLines(const std::string &uri,
                                          const std::set<int> &lines) {
  if (docDiagnostics_.find(uri) == docDiagnostics_.end())
    return;

  auto &uriDiags = docDiagnostics_[uri];
  for (int line : lines) {
    uriDiags.erase(line);
  }
}

std::vector<Diagnostic>
LSPServer::getAllDiagnostics(const std::string &uri) const {
  std::vector<Diagnostic> allDiags;

  auto uriIt = docDiagnostics_.find(uri);
  if (uriIt != docDiagnostics_.end()) {
    for (const auto &linePair : uriIt->second) {
      for (const auto &diag : linePair.second) {
        allDiags.push_back(diag);
      }
    }
  }

  return allDiags;
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
