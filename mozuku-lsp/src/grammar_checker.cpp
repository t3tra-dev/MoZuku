#include "grammar_checker.hpp"
#include "mozuku/core/debug.hpp"
#include "pos_analyzer.hpp"
#include "utf16.hpp"
#include <iostream>

namespace MoZuku {
namespace grammar {

namespace {

struct RuleContext {
  const std::string &text;
  const std::vector<TokenData> &tokens;
  const std::vector<SentenceBoundary> &sentences;
  const TextOffsetMapper &offsets;
  const std::vector<size_t> &tokenBytePositions;
  int severity{2};
};

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

std::vector<size_t>
computeTokenBytePositions(const std::vector<TokenData> &tokens,
                          const TextOffsetMapper &offsetMapper) {
  std::vector<size_t> positions;
  positions.reserve(tokens.size());
  for (const auto &token : tokens) {
    positions.push_back(offsetMapper.tokenStartByteOffset(token));
  }
  return positions;
}

Range makeRange(const RuleContext &ctx, size_t startByte, size_t endByte) {
  Range range;
  range.start = ctx.offsets.byteOffsetToPosition(startByte);
  range.end = ctx.offsets.byteOffsetToPosition(endByte);
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

    if (debug::isEnabled()) {
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
      if (!pos::POSAnalyzer::isAdversativeGaFeature(ctx.tokens[i].feature)) {
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

    if (debug::isEnabled()) {
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

      if (!pos::POSAnalyzer::isParticleFeature(token.feature)) {
        continue;
      }

      std::string currentKey = pos::POSAnalyzer::particleKey(token.feature);

      if (hasLast && token.surface == lastSurface && currentKey == lastKey) {
        ++streak;
        if (streak > maxRepeat) {
          size_t currentEnd = bytePos + token.surface.size();
          Diagnostic diag;
          diag.range = makeRange(ctx, lastStartByte, currentEnd);
          diag.severity = ctx.severity;
          diag.message = "同じ助詞「" + token.surface + "」が連続しています";

          if (debug::isEnabled()) {
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

      bool currentIsParticle =
          pos::POSAnalyzer::isParticleFeature(token.feature);
      std::string currentKey = pos::POSAnalyzer::particleKey(token.feature);
      if (currentIsParticle && prevIsParticle && currentKey == prevKey &&
          bytePos == prevStartByte + prevToken.surface.size()) {
        ++streak;
        if (streak > maxRepeat) {
          size_t currentEnd = bytePos + token.surface.size();
          Diagnostic diag;
          diag.range = makeRange(ctx, prevStartByte, currentEnd);
          diag.severity = ctx.severity;
          diag.message = "助詞が連続して使われています";

          if (debug::isEnabled()) {
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
    if (!pos::POSAnalyzer::isConjunctionFeature(token.feature)) {
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

        if (debug::isEnabled()) {
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

    if (debug::isEnabled()) {
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

      if (debug::isEnabled()) {
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

  TextOffsetMapper offsetMapper(text);
  std::vector<size_t> tokenBytePositions =
      computeTokenBytePositions(tokens, offsetMapper);

  // ルール共通設定 (現状は警告レベル固定)
  const int severity = 2; // Warning
  const int minSeverity = config->analysis.warningMinSeverity;
  if (severity < minSeverity) {
    // 現在の最小レベルより軽い場合は何も報告しない
    return;
  }

  RuleContext ctx{text,    tokens, sentences, offsetMapper, tokenBytePositions,
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
