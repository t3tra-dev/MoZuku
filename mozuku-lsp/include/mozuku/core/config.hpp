#pragma once

#include <string>

namespace MoZuku::core {

struct MeCabConfig {
  std::string dicPath;
  std::string charset = "UTF-8";
};

struct AnalysisConfig {
  bool enableCaboCha = true;
  bool grammarCheck = true;
  double minJapaneseRatio = 0.1;

  struct RuleToggles {
    bool commaLimit = true;
    bool adversativeGa = true;
    bool duplicateParticleSurface = true;
    bool adjacentParticles = true;
    bool conjunctionRepeat = true;
    bool raDropping = true;
    int commaLimitMax = 3;
    int adversativeGaMax = 1;
    int duplicateParticleSurfaceMaxRepeat = 1;
    int adjacentParticlesMaxRepeat = 1;
    int conjunctionRepeatMax = 1;
  } rules;

  struct WarningLevels {
    bool particleDuplicate = true;
    bool particleSequence = true;
    bool particleMismatch = true;
    bool sentenceStructure = false;
    bool styleConsistency = false;
    bool redundancy = false;
  } warnings;

  int warningMinSeverity = 2;
};

struct MoZukuConfig {
  MeCabConfig mecab;
  AnalysisConfig analysis;
};

} // namespace MoZuku::core

using MeCabConfig = MoZuku::core::MeCabConfig;
using AnalysisConfig = MoZuku::core::AnalysisConfig;
using MoZukuConfig = MoZuku::core::MoZukuConfig;
