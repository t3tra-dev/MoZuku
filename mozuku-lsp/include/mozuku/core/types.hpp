#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace MoZuku::core {

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
  std::string tokenType;
  unsigned int tokenModifiers{0};

  std::string surface;
  std::string feature;
  std::string baseForm;
  std::string reading;
  std::string pronunciation;
};

struct AnalyzerResult {
  std::vector<TokenData> tokens;
  std::vector<Diagnostic> diags;
};

struct ByteRange {
  size_t startByte{0};
  size_t endByte{0};
};

struct DetailedPOS {
  std::string mainPOS;
  std::string subPOS1;
  std::string subPOS2;
  std::string subPOS3;
  std::string inflection;
  std::string conjugation;
  std::string baseForm;
  std::string reading;
  std::string pronunciation;

  bool isParticle() const { return mainPOS == "助詞"; }
  bool isVerb() const { return mainPOS == "動詞"; }
  bool isNoun() const { return mainPOS == "名詞"; }
};

struct ParticleInfo {
  std::string surface;
  std::string function;
  std::string role;
  size_t position{0};
  int tokenIndex{0};
  int sentenceId{0};
};

struct SentenceBoundary {
  size_t start{0};
  size_t end{0};
  int sentenceId{0};
  std::string text;
};

struct DependencyInfo {
  int chunkId{0};
  int headId{0};
  double score{0.0};
  std::string text;
};

} // namespace MoZuku::core

using Position = MoZuku::core::Position;
using Range = MoZuku::core::Range;
using Diagnostic = MoZuku::core::Diagnostic;
using TokenData = MoZuku::core::TokenData;
using AnalyzerResult = MoZuku::core::AnalyzerResult;
using ByteRange = MoZuku::core::ByteRange;
using DetailedPOS = MoZuku::core::DetailedPOS;
using ParticleInfo = MoZuku::core::ParticleInfo;
using SentenceBoundary = MoZuku::core::SentenceBoundary;
using DependencyInfo = MoZuku::core::DependencyInfo;

namespace MoZukuModifiers {
static constexpr unsigned Proper = 1u << 0;
static constexpr unsigned Numeric = 1u << 1;
static constexpr unsigned Kana = 1u << 2;
static constexpr unsigned Kanji = 1u << 3;
} // namespace MoZukuModifiers
