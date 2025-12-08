#pragma once

#include <memory>
#include <string>
#include <vector>

struct TokenData;
struct Diagnostic;

struct DetailedPOS {
  std::string mainPOS;       // 主品詞 (名詞, 動詞, 助詞...)
  std::string subPOS1;       // 品詞細分類1 (格助詞, 副助詞, 係助詞...)
  std::string subPOS2;       // 品詞細分類2
  std::string subPOS3;       // 品詞細分類3
  std::string inflection;    // 活用型
  std::string conjugation;   // 活用形
  std::string baseForm;      // 原形
  std::string reading;       // 読み
  std::string pronunciation; // 発音

  bool isParticle() const { return mainPOS == "助詞"; }
  bool isVerb() const { return mainPOS == "動詞"; }
  bool isNoun() const { return mainPOS == "名詞"; }
};

// Information about a particle (助詞) token
struct ParticleInfo {
  std::string surface;  // 表層形
  std::string function; // 格助詞, 副助詞, 係助詞, 接続助詞
  std::string role;     // より詳細な役割
  size_t position;      // 文中の位置 (バイト単位)
  int tokenIndex;       // トークン配列内のインデックス
  int sentenceId;       // 所属する文のID
};

// Sentence boundary information
struct SentenceBoundary {
  size_t start;     // 文の開始位置 (バイト単位)
  size_t end;       // 文の終了位置 (バイト単位)
  int sentenceId;   // 文のID
  std::string text; // 文の内容
};

// Dependency parsing information from CaboCha
struct DependencyInfo {
  int chunkId;      // チャンクID
  int headId;       // 係り先チャンクID
  double score;     // 係り受けスコア
  std::string text; // チャンクのテキスト
};

// Configuration structures (shared between LSP server and analyzer)
struct MeCabConfig {
  std::string dicPath;           // Dictionary directory path
  std::string charset = "UTF-8"; // Character encoding
};

struct AnalysisConfig {
  bool enableCaboCha = true; // Enable CaboCha dependency parsing
  bool grammarCheck = true;  // Enable grammar diagnostics
  double minJapaneseRatio =
      0.1; // Minimum Japanese character ratio for analysis

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

  // Enhanced grammar warning settings
  struct WarningLevels {
    bool particleDuplicate = true;  // 二重助詞警告
    bool particleSequence = true;   // 不適切助詞連続
    bool particleMismatch = true;   // 動詞-助詞不整合
    bool sentenceStructure = false; // 文構造問題 (実験的)
    bool styleConsistency = false;  // 文体混在 (実験的)
    bool redundancy = false;        // 冗長表現 (実験的)
  } warnings;

  int warningMinSeverity =
      2; // 最小警告レベル (1=Error, 2=Warning, 3=Info, 4=Hint)
};

struct MoZukuConfig {
  MeCabConfig mecab;
  AnalysisConfig analysis;
};

void analyzeText(const std::string &text, std::vector<TokenData> &tokens,
                 std::vector<Diagnostic> &diags,
                 const MoZukuConfig *config = nullptr);

void performGrammarDiagnostics(const std::string &text,
                               std::vector<Diagnostic> &diags);

size_t computeByteOffset(const std::string &text, int line, int character);

namespace MoZukuModifiers {
static constexpr unsigned Proper = 1u << 0;  // "proper"
static constexpr unsigned Numeric = 1u << 1; // "numeric"
static constexpr unsigned Kana = 1u << 2;    // "kana"
static constexpr unsigned Kanji = 1u << 3;   // "kanji"
} // namespace MoZukuModifiers

namespace MoZuku {

namespace mecab {
class MeCabManager;
}

class Analyzer {
public:
  Analyzer();
  ~Analyzer();

  bool initialize(const MoZukuConfig &config);

  std::vector<TokenData> analyzeText(const std::string &text);
  std::vector<Diagnostic> checkGrammar(const std::string &text);
  std::vector<DependencyInfo> analyzeDependencies(const std::string &text);

  bool isInitialized() const;
  std::string getSystemCharset() const;
  bool isCaboChaAvailable() const;

private:
  std::unique_ptr<mecab::MeCabManager> mecab_manager_;
  MoZukuConfig config_;
  std::string system_charset_;
};

} // namespace MoZuku
