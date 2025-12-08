#include "pos_analyzer.hpp"
#include "encoding_utils.hpp"
#include "text_processor.hpp"

namespace MoZuku {
namespace pos {

std::string POSAnalyzer::mapPosToType(const char *feature) {
  if (!feature)
    return "unknown";

  std::string f = text::TextProcessor::sanitizeUTF8(std::string(feature));
  auto p = f.find(',');
  std::string pos = (p == std::string::npos) ? f : f.substr(0, p);

  if (pos.find("名詞") != std::string::npos)
    return "noun";
  if (pos.find("動詞") != std::string::npos)
    return "verb";
  if (pos.find("形容詞") != std::string::npos)
    return "adjective";
  if (pos.find("副詞") != std::string::npos)
    return "adverb";
  if (pos.find("助詞") != std::string::npos)
    return "particle";
  if (pos.find("助動詞") != std::string::npos)
    return "aux";
  if (pos.find("接続詞") != std::string::npos)
    return "conjunction";
  if (pos.find("記号") != std::string::npos)
    return "symbol";
  if (pos.find("感動詞") != std::string::npos)
    return "interj";
  if (pos.find("接頭詞") != std::string::npos)
    return "prefix";
  if (pos.find("接尾") != std::string::npos)
    return "suffix";

  return "unknown";
}

void POSAnalyzer::parseFeatureDetails(const char *feature,
                                      std::string &baseForm,
                                      std::string &reading,
                                      std::string &pronunciation,
                                      const std::string &systemCharset,
                                      bool skipConversion) {
  if (!feature)
    return;

  std::string f(feature);
  std::vector<std::string> fields = splitFeature(f);

  // IPAdic format:
  // 品詞,品詞細分類1,品詞細分類2,品詞細分類3,活用型,活用形,原形,読み,発音
  if (fields.size() >= 7 && fields[6] != "*") {
    baseForm = skipConversion
                   ? fields[6]
                   : encoding::systemToUtf8(fields[6], systemCharset);
  }
  if (fields.size() >= 8 && fields[7] != "*") {
    reading = skipConversion ? fields[7]
                             : encoding::systemToUtf8(fields[7], systemCharset);
  }
  if (fields.size() >= 9 && fields[8] != "*") {
    pronunciation = skipConversion
                        ? fields[8]
                        : encoding::systemToUtf8(fields[8], systemCharset);
  }
}

DetailedPOS POSAnalyzer::parseDetailedPOS(const char *feature,
                                          const std::string &systemCharset) {
  DetailedPOS pos;
  if (!feature)
    return pos;

  std::string f =
      (systemCharset == "UTF-8")
          ? std::string(feature)
          : encoding::systemToUtf8(std::string(feature), systemCharset);

  std::vector<std::string> fields = splitFeature(f);

  // Fill in the detailed POS structure
  if (fields.size() > 0)
    pos.mainPOS = fields[0];
  if (fields.size() > 1)
    pos.subPOS1 = fields[1];
  if (fields.size() > 2)
    pos.subPOS2 = fields[2];
  if (fields.size() > 3)
    pos.subPOS3 = fields[3];
  if (fields.size() > 4)
    pos.inflection = fields[4];
  if (fields.size() > 5)
    pos.conjugation = fields[5];
  if (fields.size() > 6 && fields[6] != "*")
    pos.baseForm = fields[6];
  if (fields.size() > 7 && fields[7] != "*")
    pos.reading = fields[7];
  if (fields.size() > 8 && fields[8] != "*")
    pos.pronunciation = fields[8];

  return pos;
}

unsigned POSAnalyzer::computeModifiers(const std::string &text, size_t start,
                                       size_t length, const char *feature) {
  unsigned mods = 0;
  bool hasKana = false, hasKanji = false, hasNumber = false;

  // Analyze character types in the token
  analyzeCharacterTypes(text, start, length, hasKana, hasKanji, hasNumber);

  // Set modifiers based on character types
  if (hasKana)
    mods |= 0x01; // Contains kana
  if (hasKanji)
    mods |= 0x02; // Contains kanji
  if (hasNumber)
    mods |= 0x04; // Contains numbers

  // Add POS-based modifiers
  if (feature) {
    std::string f(feature);
    if (f.find("固有名詞") != std::string::npos)
      mods |= 0x08; // Proper noun
    if (f.find("動詞") != std::string::npos &&
        f.find("自立") != std::string::npos)
      mods |= 0x10; // Independent verb
  }

  return mods;
}

std::vector<std::string> POSAnalyzer::splitFeature(const std::string &feature) {
  std::vector<std::string> fields;
  size_t pos = 0;

  while (pos < feature.size()) {
    size_t nextComma = feature.find(',', pos);
    if (nextComma == std::string::npos) {
      fields.push_back(feature.substr(pos));
      break;
    }
    fields.push_back(feature.substr(pos, nextComma - pos));
    pos = nextComma + 1;
  }

  return fields;
}

void POSAnalyzer::analyzeCharacterTypes(const std::string &text, size_t start,
                                        size_t length, bool &hasKana,
                                        bool &hasKanji, bool &hasNumber) {
  hasKana = false;
  hasKanji = false;
  hasNumber = false;

  size_t end = std::min(start + length, text.size());

  for (size_t i = start; i < end; ++i) {
    unsigned char c = static_cast<unsigned char>(text[i]);

    // ASCII numbers
    if (c >= '0' && c <= '9') {
      hasNumber = true;
      continue;
    }

    // Skip ASCII characters for multi-byte analysis
    if (c < 0x80)
      continue;

    // Multi-byte character analysis (simplified)
    if (i + 2 < end) {
      unsigned char c1 = static_cast<unsigned char>(text[i]);
      unsigned char c2 = static_cast<unsigned char>(text[i + 1]);
      unsigned char c3 = static_cast<unsigned char>(text[i + 2]);

      // Hiragana range: U+3040-U+309F (UTF-8: E3 81 80 - E3 82 9F)
      if (c1 == 0xE3 && (c2 == 0x81 || c2 == 0x82)) {
        hasKana = true;
        i += 2; // Skip next 2 bytes
        continue;
      }

      // Katakana range: U+30A0-U+30FF (UTF-8: E3 82 A0 - E3 83 BF)
      if (c1 == 0xE3 && (c2 == 0x82 || c2 == 0x83)) {
        hasKana = true;
        i += 2;
        continue;
      }

      // CJK Unified Ideographs: U+4E00-U+9FFF (simplified detection)
      if (c1 >= 0xE4 && c1 <= 0xE9) {
        hasKanji = true;
        i += 2;
        continue;
      }
    }
  }
}

} // namespace pos
} // namespace MoZuku
