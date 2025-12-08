#include "grammar_checker.hpp"
#include "pos_analyzer.hpp"
#include "utf16.hpp"
#include <cstdlib>
#include <iostream>

namespace MoZuku {
namespace grammar {

namespace {

struct RuleContext {
  const std::string &text;
  const std::vector<TokenData> &tokens;
  const std::vector<SentenceBoundary> &sentences;
  const std::vector<size_t> &lineStarts;
  const std::vector<size_t> &tokenBytePositions;
  int severity{2};
};

bool isAdversativeGa(const std::string &feature) {
  // MeCab: 品詞,品詞細分類1,品詞細分類2,品詞細分類3,活用型,活用形,原形,...
  // 逆接の接続助詞「が」: 助詞,接続助詞,*,*,*,*,が,ガ,ガ
  int fieldIndex = 0;
  size_t start = 0;
  size_t end = 0;

  std::string pos, sub1, base;
  while (end != std::string::npos) {
    end = feature.find(',', start);
    std::string part = feature.substr(
        start, end == std::string::npos ? std::string::npos : end - start);
    if (fieldIndex == 0)
      pos = part;
    else if (fieldIndex == 1)
      sub1 = part;
    else if (fieldIndex == 6)
      base = part;

    if (end == std::string::npos)
      break;
    start = end + 1;
    ++fieldIndex;
    if (fieldIndex > 6 && !base.empty()) {
      break;
    }
  }

  return pos == "助詞" && sub1 == "接続助詞" && base == "が";
}

bool isConjunction(const std::string &feature) {
  size_t comma = feature.find(',');
  std::string pos =
      (comma == std::string::npos) ? feature : feature.substr(0, comma);
  return pos == "接続詞";
}

bool isParticle(const std::string &feature) {
  size_t comma = feature.find(',');
  std::string pos =
      (comma == std::string::npos) ? feature : feature.substr(0, comma);
  return pos == "助詞";
}

std::string particleKey(const std::string &feature) {
  // "助詞,格助詞,一般,..." -> "助詞,格助詞"
  size_t firstComma = feature.find(',');
  if (firstComma == std::string::npos) {
    return feature;
  }
  size_t secondComma = feature.find(',', firstComma + 1);
  if (secondComma == std::string::npos) {
    return feature.substr(0, firstComma);
  }
  return feature.substr(0, secondComma);
}

DetailedPOS parsePos(const std::string &feature) {
  return MoZuku::pos::POSAnalyzer::parseDetailedPOS(feature.c_str(), "UTF-8");
}

bool isTargetVerb(const DetailedPOS &pos) {
  return pos.mainPOS == "動詞" && pos.subPOS1 == "自立" &&
         pos.inflection == "一段" && pos.conjugation == "未然形";
}

bool isRaWord(const DetailedPOS &pos) {
  return pos.mainPOS == "動詞" && pos.subPOS1 == "接尾" &&
         pos.baseForm == "れる";
}

bool isSpecialRaCase(const DetailedPOS &pos) {
  return pos.mainPOS == "動詞" &&
         (pos.baseForm == "来れる" || pos.baseForm == "見れる");
}

// UTF-16ベースのトークン位置をUTF-8バイトオフセットに変換
size_t toByteOffset(const TokenData &token, const std::string &text,
                    const std::vector<size_t> &lineStarts) {
  if (token.line >= static_cast<int>(lineStarts.size())) {
    return text.size();
  }

  size_t lineStart = lineStarts[token.line];
  size_t bytePos = lineStart;
  int utf16Pos = 0;

  while (bytePos < text.size() && utf16Pos < token.startChar &&
         text[bytePos] != '\n') {
    unsigned char c = static_cast<unsigned char>(text[bytePos]);
    int seqLen = (c >= 0xF0) ? 4 : (c >= 0xE0) ? 3 : (c >= 0xC0) ? 2 : 1;
    bytePos += seqLen;
    utf16Pos += (seqLen == 4) ? 2 : 1;
  }

  return bytePos;
}

std::vector<size_t>
computeTokenBytePositions(const std::vector<TokenData> &tokens,
                          const std::string &text,
                          const std::vector<size_t> &lineStarts) {
  std::vector<size_t> positions;
  positions.reserve(tokens.size());
  for (const auto &token : tokens) {
    positions.push_back(toByteOffset(token, text, lineStarts));
  }
  return positions;
}

Range makeRange(const RuleContext &ctx, size_t startByte, size_t endByte) {
  Range range;
  range.start = byteOffsetToPosition(ctx.text, ctx.lineStarts, startByte);
  range.end = byteOffsetToPosition(ctx.text, ctx.lineStarts, endByte);
  return range;
}

bool inSentence(size_t bytePos, const SentenceBoundary &sentence) {
  return bytePos >= sentence.start && bytePos < sentence.end;
}

// 文中の読点「、」の出現回数を数える
size_t countCommas(const std::string &text) {
  size_t count = 0;
  size_t pos = 0;
  const std::string mark = "、";

  while (pos < text.size()) {
    size_t found = text.find(mark, pos);
    if (found == std::string::npos) {
      break;
    }
    ++count;
    pos = found + mark.size();
  }
  return count;
}

} // namespace

static bool isDebugEnabled() {
  static bool initialized = false;
  static bool debug = false;
  if (!initialized) {
    debug = (std::getenv("MOZUKU_DEBUG") != nullptr);
    initialized = true;
  }
  return debug;
}

void checkCommaLimit(const RuleContext &ctx, std::vector<Diagnostic> &diags,
                     int limit) {
  if (limit <= 0)
    return;

  for (const auto &sentence : ctx.sentences) {
    size_t commaCount = countCommas(sentence.text);
    if (commaCount <= static_cast<size_t>(limit)) {
      continue;
    }

    Diagnostic diag;
    diag.range = makeRange(ctx, sentence.start, sentence.end);
    diag.severity = ctx.severity;
    diag.message = "一文に使用できる読点「、」は最大" + std::to_string(limit) +
                   "個までです (現在" + std::to_string(commaCount) + "個) ";

    if (isDebugEnabled()) {
      std::cerr << "[DEBUG] Comma limit exceeded in sentence "
                << sentence.sentenceId << ": count=" << commaCount << "\n";
    }

    diags.push_back(std::move(diag));
  }
}

void checkAdversativeGa(const RuleContext &ctx, std::vector<Diagnostic> &diags,
                        int maxCount) {
  if (maxCount <= 0)
    return;

  for (const auto &sentence : ctx.sentences) {
    size_t count = 0;
    for (size_t i = 0; i < ctx.tokens.size(); ++i) {
      if (!isAdversativeGa(ctx.tokens[i].feature)) {
        continue;
      }
      size_t bytePos = ctx.tokenBytePositions[i];
      if (inSentence(bytePos, sentence)) {
        ++count;
      }
    }

    if (count <= static_cast<size_t>(maxCount)) {
      continue;
    }

    Diagnostic diag;
    diag.range = makeRange(ctx, sentence.start, sentence.end);
    diag.severity = ctx.severity;
    diag.message = "逆接の接続助詞「が」が同一文で" +
                   std::to_string(maxCount + 1) + "回以上使われています (" +
                   std::to_string(count) + "回) ";

    if (isDebugEnabled()) {
      std::cerr << "[DEBUG] Adversative 'が' exceeded in sentence "
                << sentence.sentenceId << ": count=" << count << "\n";
    }

    diags.push_back(std::move(diag));
  }
}

void checkDuplicateParticleSurface(const RuleContext &ctx,
                                   std::vector<Diagnostic> &diags,
                                   int maxRepeat) {
  if (maxRepeat <= 0)
    return;

  for (const auto &sentence : ctx.sentences) {
    std::string lastSurface;
    std::string lastKey;
    size_t lastStartByte = 0;
    int streak = 1;
    bool hasLast = false;

    for (size_t i = 0; i < ctx.tokens.size(); ++i) {
      const auto &token = ctx.tokens[i];
      size_t bytePos = ctx.tokenBytePositions[i];
      if (!inSentence(bytePos, sentence)) {
        continue;
      }

      if (!isParticle(token.feature)) {
        continue;
      }

      std::string currentKey = particleKey(token.feature);

      if (hasLast && token.surface == lastSurface && currentKey == lastKey) {
        ++streak;
        if (streak > maxRepeat) {
          size_t currentEnd = bytePos + token.surface.size();
          Diagnostic diag;
          diag.range = makeRange(ctx, lastStartByte, currentEnd);
          diag.severity = ctx.severity;
          diag.message = "同じ助詞「" + token.surface + "」が連続しています";

          if (isDebugEnabled()) {
            std::cerr << "[DEBUG] Duplicate particle '" << token.surface
                      << "' in sentence " << sentence.sentenceId << "\n";
          }

          diags.push_back(std::move(diag));
        }
      } else {
        streak = 1;
        lastStartByte = bytePos;
      }

      lastSurface = token.surface;
      lastKey = currentKey;
      hasLast = true;
    }
  }
}

void checkAdjacentParticles(const RuleContext &ctx,
                            std::vector<Diagnostic> &diags, int maxRepeat) {
  if (maxRepeat <= 0)
    return;

  for (const auto &sentence : ctx.sentences) {
    bool prevIsParticle = false;
    std::string prevKey;
    TokenData prevToken;
    size_t prevStartByte = 0;
    int streak = 1;

    for (size_t i = 0; i < ctx.tokens.size(); ++i) {
      const auto &token = ctx.tokens[i];
      size_t bytePos = ctx.tokenBytePositions[i];
      if (!inSentence(bytePos, sentence)) {
        continue;
      }

      bool currentIsParticle = isParticle(token.feature);
      std::string currentKey = particleKey(token.feature);
      if (currentIsParticle && prevIsParticle && currentKey == prevKey &&
          bytePos == prevStartByte + prevToken.surface.size()) {
        ++streak;
        if (streak > maxRepeat) {
          size_t currentEnd = bytePos + token.surface.size();
          Diagnostic diag;
          diag.range = makeRange(ctx, prevStartByte, currentEnd);
          diag.severity = ctx.severity;
          diag.message = "助詞が連続して使われています";

          if (isDebugEnabled()) {
            std::cerr << "[DEBUG] Consecutive particles '" << prevToken.surface
                      << "' -> '" << token.surface << "' in sentence "
                      << sentence.sentenceId << "\n";
          }

          diags.push_back(std::move(diag));
        }
      } else {
        streak = 1;
        if (currentIsParticle) {
          prevStartByte = bytePos;
        }
      }

      prevIsParticle = currentIsParticle;
      if (currentIsParticle) {
        prevToken = token;
        prevStartByte = bytePos;
        prevKey = currentKey;
      }
    }
  }
}

void checkConjunctionRepeats(const RuleContext &ctx,
                             std::vector<Diagnostic> &diags, int maxRepeat) {
  if (maxRepeat <= 0)
    return;

  std::string lastSurface;
  size_t lastStartByte = 0;
  size_t lastEndByte = 0;
  int streak = 1;
  bool hasLast = false;

  for (size_t i = 0; i < ctx.tokens.size(); ++i) {
    const auto &token = ctx.tokens[i];
    if (!isConjunction(token.feature)) {
      continue;
    }

    size_t currentStart = ctx.tokenBytePositions[i];
    size_t currentEnd = currentStart + token.surface.size();

    bool separatedByNewline =
        hasLast && ctx.text.find('\n', lastEndByte) != std::string::npos &&
        ctx.text.find('\n', lastEndByte) < currentStart;

    if (hasLast && token.surface == lastSurface && !separatedByNewline) {
      ++streak;
      if (streak > maxRepeat) {
        Diagnostic diag;
        diag.range = makeRange(ctx, lastStartByte, currentEnd);
        diag.severity = ctx.severity;
        diag.message = "同じ接続詞「" + token.surface + "」が連続しています";

        if (isDebugEnabled()) {
          std::cerr << "[DEBUG] Duplicate conjunction '" << token.surface
                    << "' detected across punctuation\n";
        }

        diags.push_back(std::move(diag));
      }
    } else {
      streak = 1;
      lastStartByte = currentStart;
    }

    lastSurface = token.surface;
    lastStartByte = currentStart;
    lastEndByte = currentEnd;
    hasLast = true;
  }
}

void checkRaDropping(const RuleContext &ctx, std::vector<Diagnostic> &diags) {
  const std::string messageRa = "ら抜き言葉を使用しています";

  // 特殊ケース (単体で「来れる」「見れる」)
  for (size_t i = 0; i < ctx.tokens.size(); ++i) {
    const auto &token = ctx.tokens[i];
    DetailedPOS pos = parsePos(token.feature);
    if (!isSpecialRaCase(pos)) {
      continue;
    }

    size_t startByte = ctx.tokenBytePositions[i];
    size_t endByte = startByte + token.surface.size();
    Diagnostic diag;
    diag.range = makeRange(ctx, startByte, endByte);
    diag.severity = ctx.severity;
    diag.message = messageRa;
    diags.push_back(std::move(diag));

    if (isDebugEnabled()) {
      std::cerr << "[DEBUG] Ra-dropping special case detected: "
                << token.surface << "\n";
    }
  }

  // 2トークン組み合わせ (動詞一段未然形 + 接尾「れる」)
  DetailedPOS prevPos;
  TokenData prevToken;
  bool hasPrev = false;

  for (size_t i = 0; i < ctx.tokens.size(); ++i) {
    const auto &token = ctx.tokens[i];
    DetailedPOS pos = parsePos(token.feature);

    if (hasPrev && isTargetVerb(prevPos) && isRaWord(pos)) {
      size_t startByte = ctx.tokenBytePositions[i - 1];
      size_t endByte = ctx.tokenBytePositions[i] + token.surface.size();
      Diagnostic diag;
      diag.range = makeRange(ctx, startByte, endByte);
      diag.severity = ctx.severity;
      diag.message = messageRa;
      diags.push_back(std::move(diag));

      if (isDebugEnabled()) {
        std::cerr << "[DEBUG] Ra-dropping detected between tokens '"
                  << prevToken.surface << "' + '" << token.surface << "'\n";
      }
    }

    prevPos = pos;
    prevToken = token;
    hasPrev = true;
  }
}

void GrammarChecker::checkGrammar(
    const std::string &text, const std::vector<TokenData> &tokens,
    const std::vector<SentenceBoundary> &sentences,
    std::vector<Diagnostic> &diags, const MoZukuConfig *config) {
  if (!config || !config->analysis.grammarCheck) {
    return;
  }

  std::vector<size_t> lineStarts = computeLineStarts(text);
  std::vector<size_t> tokenBytePositions =
      computeTokenBytePositions(tokens, text, lineStarts);

  // ルール共通設定 (現状は警告レベル固定)
  const int severity = 2; // Warning
  const int minSeverity = config->analysis.warningMinSeverity;
  if (severity < minSeverity) {
    // 現在の最小レベルより軽い場合は何も報告しない
    return;
  }

  RuleContext ctx{text,    tokens, sentences, lineStarts, tokenBytePositions,
                  severity};

  if (config && config->analysis.rules.commaLimit) {
    checkCommaLimit(ctx, diags, config->analysis.rules.commaLimitMax);
  }
  if (config && config->analysis.rules.adversativeGa) {
    checkAdversativeGa(ctx, diags, config->analysis.rules.adversativeGaMax);
  }
  if (config && config->analysis.rules.duplicateParticleSurface) {
    checkDuplicateParticleSurface(
        ctx, diags, config->analysis.rules.duplicateParticleSurfaceMaxRepeat);
  }
  if (config && config->analysis.rules.adjacentParticles) {
    checkAdjacentParticles(ctx, diags,
                           config->analysis.rules.adjacentParticlesMaxRepeat);
  }
  if (config && config->analysis.rules.conjunctionRepeat) {
    checkConjunctionRepeats(ctx, diags,
                            config->analysis.rules.conjunctionRepeatMax);
  }
  if (config && config->analysis.rules.raDropping) {
    checkRaDropping(ctx, diags);
  }
}

} // namespace grammar
} // namespace MoZuku
