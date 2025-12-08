#pragma once

#include <string>

// Forward declarations
namespace MeCab {
class Tagger;
}
typedef struct cabocha_t cabocha_t;

namespace MoZuku {
namespace mecab {

struct SystemLibInfo {
  std::string libPath;      // Library path
  std::string dicPath;      // Dictionary path
  std::string charset;      // Character encoding
  bool isAvailable = false; // Availability flag
};

class MeCabManager {
public:
  explicit MeCabManager(bool enableCaboCha = false);

  ~MeCabManager();

  MeCabManager(const MeCabManager &) = delete;
  MeCabManager &operator=(const MeCabManager &) = delete;

  bool initialize(const std::string &mecabDicPath = "",
                  const std::string &mecabCharset = "");

  MeCab::Tagger *getMeCabTagger() const { return mecab_tagger_; }

  cabocha_t *getCaboChaParser() const { return cabocha_parser_; }

  bool isCaboChaAvailable() const { return cabocha_available_; }

  std::string getSystemCharset() const { return system_charset_; }

  static SystemLibInfo detectSystemMeCab();

  static SystemLibInfo detectSystemCaboCha();

private:
  std::string testMeCabCharset(MeCab::Tagger *tagger,
                               const std::string &originalCharset);

  // Member variables
  MeCab::Tagger *mecab_tagger_;
  cabocha_t *cabocha_parser_;
  std::string system_charset_;
  bool cabocha_available_;
  bool enable_cabocha_;
};

} // namespace mecab
} // namespace MoZuku
