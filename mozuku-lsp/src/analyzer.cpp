#include "analyzer.hpp"
#include "encoding_utils.hpp"
#include "grammar_checker.hpp"
#include "mecab_manager.hpp"
#include "mozuku/core/debug.hpp"
#include "pos_analyzer.hpp"
#include "text_processor.hpp"
#include "utf16.hpp"

#include <cabocha.h>
#include <iostream>
#include <mecab.h>

namespace MoZuku {

Analyzer::Analyzer() {
  if (debug::isEnabled()) {
    std::cerr << "[DEBUG] Analyzer created" << std::endl;
  }
}

Analyzer::~Analyzer() = default;

bool Analyzer::initialize(const MoZukuConfig &config) {
  config_ = config;
  mecab_manager_ =
      std::make_unique<mecab::MeCabManager>(config.analysis.enableCaboCha);

  if (debug::isEnabled()) {
    std::cerr << "[DEBUG] Initializing analyzer with config" << std::endl;
  }

  std::string mecabDicPath =
      config.mecab.dicPath.empty() ? "" : config.mecab.dicPath;
  std::string mecabCharset =
      config.mecab.charset.empty() ? "UTF-8" : config.mecab.charset;

  if (!mecab_manager_->initialize(mecabDicPath, mecabCharset)) {
    std::cerr << "[ERROR] Failed to initialize MeCab" << std::endl;
    return false;
  }

  system_charset_ = mecab_manager_->getSystemCharset();

  if (debug::isEnabled()) {
    std::cerr << "[DEBUG] Analyzer initialized successfully with charset: "
              << system_charset_ << std::endl;
  }

  return true;
}

Analyzer::PreparedText
Analyzer::prepareText(const std::string &text,
                      bool enforceMinJapaneseRatio) const {
  PreparedText prepared;
  if (text.empty()) {
    return prepared;
  }

  prepared.cleanText = text::TextProcessor::sanitizeUTF8(text);
  if (prepared.cleanText.empty()) {
    return prepared;
  }

  prepared.japaneseRatio =
      text::TextProcessor::calculateJapaneseRatio(prepared.cleanText);
  prepared.belowMinJapaneseRatio =
      enforceMinJapaneseRatio && config_.analysis.minJapaneseRatio > 0.0 &&
      prepared.japaneseRatio < config_.analysis.minJapaneseRatio;

  return prepared;
}

std::vector<TokenData>
Analyzer::analyzePreparedText(const PreparedText &prepared) {
  std::vector<TokenData> tokens;

  if (prepared.cleanText.empty()) {
    return tokens;
  }

  std::string systemText =
      encoding::utf8ToSystem(prepared.cleanText, system_charset_);

  MeCab::Tagger *tagger = mecab_manager_->getMeCabTagger();
  if (!tagger) {
    std::cerr << "[ERROR] MeCab tagger not available" << std::endl;
    return tokens;
  }

  const MeCab::Node *node = tagger->parseToNode(systemText.c_str());
  if (!node) {
    std::cerr << "[ERROR] MeCab parsing failed" << std::endl;
    return tokens;
  }

  TextOffsetMapper offsetMapper(prepared.cleanText);

  size_t currentBytePos = 0;

  for (const MeCab::Node *n = node; n; n = n->next) {
    if (n->stat == MECAB_BOS_NODE || n->stat == MECAB_EOS_NODE) {
      continue;
    }

    TokenData token;

    size_t surfaceLen = static_cast<size_t>(n->length);
    std::string systemSurface(n->surface, surfaceLen);
    token.surface = encoding::systemToUtf8(systemSurface, system_charset_);

    if (token.surface.empty())
      continue;

    while (currentBytePos < prepared.cleanText.size()) {
      size_t remainingBytes = prepared.cleanText.size() - currentBytePos;
      if (remainingBytes >= token.surface.size() &&
          prepared.cleanText.substr(currentBytePos, token.surface.size()) ==
              token.surface) {
        break;
      }
      currentBytePos++;
    }

    Position pos = offsetMapper.byteOffsetToPosition(currentBytePos);
    token.line = pos.line;
    token.startChar = pos.character;
    token.endChar = pos.character + utf8ToUtf16Length(token.surface);

    std::string systemFeature = n->feature ? std::string(n->feature) : "";
    token.feature = encoding::systemToUtf8(systemFeature, system_charset_);

    pos::POSAnalyzer::parseFeatureDetails(token.feature.c_str(), token.baseForm,
                                          token.reading, token.pronunciation,
                                          "UTF-8", // Already converted to UTF-8
                                          true     // Skip conversion
    );

    token.tokenType = pos::POSAnalyzer::mapPosToType(token.feature.c_str());
    token.tokenModifiers = pos::POSAnalyzer::computeModifiers(
        prepared.cleanText, currentBytePos, token.surface.size(),
        token.feature.c_str());

    tokens.push_back(token);
    currentBytePos += token.surface.size();
  }

  if (debug::isEnabled()) {
    std::cerr << "[DEBUG] Analysis completed: " << tokens.size()
              << " tokens generated" << std::endl;
  }

  return tokens;
}

std::vector<TokenData> Analyzer::analyzeText(const std::string &text) {
  if (debug::isEnabled()) {
    std::cerr << "[DEBUG] Analyzing text of length: " << text.size()
              << std::endl;
  }

  PreparedText prepared = prepareText(text, true);
  if (prepared.belowMinJapaneseRatio) {
    if (debug::isEnabled()) {
      std::cerr << "[DEBUG] Skipping analysis due to low Japanese ratio: "
                << prepared.japaneseRatio << " < "
                << config_.analysis.minJapaneseRatio << std::endl;
    }
    return {};
  }

  return analyzePreparedText(prepared);
}

std::vector<Diagnostic> Analyzer::checkGrammar(const std::string &text) {
  std::vector<Diagnostic> diagnostics;

  if (!config_.analysis.grammarCheck) {
    return diagnostics;
  }

  PreparedText prepared = prepareText(text, true);
  if (prepared.belowMinJapaneseRatio) {
    if (debug::isEnabled()) {
      std::cerr << "[DEBUG] Skipping grammar check due to low Japanese ratio: "
                << prepared.japaneseRatio << " < "
                << config_.analysis.minJapaneseRatio << std::endl;
    }
    return diagnostics;
  }

  if (debug::isEnabled()) {
    std::cerr << "[DEBUG] Starting grammar check" << std::endl;
  }

  std::vector<TokenData> tokens = analyzePreparedText(prepared);

  std::vector<SentenceBoundary> sentences =
      text::TextProcessor::splitIntoSentences(prepared.cleanText);

  grammar::GrammarChecker::checkGrammar(prepared.cleanText, tokens, sentences,
                                        diagnostics, &config_);

  if (debug::isEnabled()) {
    std::cerr << "[DEBUG] Grammar check completed: " << diagnostics.size()
              << " diagnostics generated" << std::endl;
  }

  return diagnostics;
}

std::vector<DependencyInfo>
Analyzer::analyzeDependencies(const std::string &text) {
  std::vector<DependencyInfo> dependencies;

  if (!mecab_manager_->isCaboChaAvailable()) {
    if (debug::isEnabled()) {
      std::cerr << "[DEBUG] CaboCha not available for dependency analysis"
                << std::endl;
    }
    return dependencies;
  }

  if (debug::isEnabled()) {
    std::cerr << "[DEBUG] Starting dependency analysis" << std::endl;
  }

  PreparedText prepared = prepareText(text, false);
  if (prepared.cleanText.empty()) {
    return dependencies;
  }

  std::string systemText =
      encoding::utf8ToSystem(prepared.cleanText, system_charset_);

  cabocha_t *parser = mecab_manager_->getCaboChaParser();
  if (!parser) {
    return dependencies;
  }

  const cabocha_tree_t *tree =
      cabocha_sparse_totree(parser, systemText.c_str());
  if (!tree) {
    return dependencies;
  }

  for (size_t i = 0;
       i < cabocha_tree_chunk_size(const_cast<cabocha_tree_t *>(tree)); ++i) {
    const cabocha_chunk_t *chunk =
        cabocha_tree_chunk(const_cast<cabocha_tree_t *>(tree), i);
    if (!chunk)
      continue;

    DependencyInfo dep;
    dep.chunkId = static_cast<int>(i);
    dep.headId = chunk->link;
    dep.score = chunk->score;

    size_t startToken = chunk->token_pos;
    size_t endToken = startToken + chunk->token_size;

    std::string chunkText;
    for (size_t j = startToken;
         j < endToken &&
         j < cabocha_tree_token_size(const_cast<cabocha_tree_t *>(tree));
         ++j) {
      const cabocha_token_t *token =
          cabocha_tree_token(const_cast<cabocha_tree_t *>(tree), j);
      if (token && token->surface) {
        std::string surface = encoding::systemToUtf8(
            std::string(token->surface), system_charset_);
        chunkText += surface;
      }
    }
    dep.text = chunkText;

    dependencies.push_back(dep);
  }

  if (debug::isEnabled()) {
    std::cerr << "[DEBUG] Dependency analysis completed: "
              << dependencies.size() << " chunks found" << std::endl;
  }

  return dependencies;
}

bool Analyzer::isInitialized() const {
  return mecab_manager_ && mecab_manager_->getMeCabTagger() != nullptr;
}

std::string Analyzer::getSystemCharset() const { return system_charset_; }

bool Analyzer::isCaboChaAvailable() const {
  return mecab_manager_ && mecab_manager_->isCaboChaAvailable();
}

} // namespace MoZuku
