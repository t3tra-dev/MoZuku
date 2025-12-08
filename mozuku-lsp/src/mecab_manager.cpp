#include "mecab_manager.hpp"
#include <cabocha.h>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <mecab.h>

namespace MoZuku {
namespace mecab {

static bool isDebugEnabled() {
  static bool initialized = false;
  static bool debug = false;
  if (!initialized) {
    debug = (std::getenv("MOZUKU_DEBUG") != nullptr);
    initialized = true;
  }
  return debug;
}

MeCabManager::MeCabManager(bool enableCaboCha)
    : mecab_tagger_(nullptr), cabocha_parser_(nullptr),
      system_charset_("UTF-8"), cabocha_available_(false),
      enable_cabocha_(enableCaboCha) {

  if (isDebugEnabled()) {
    std::cerr << "[DEBUG] MeCabManager created with CaboCha "
              << (enableCaboCha ? "enabled" : "disabled") << std::endl;
  }
}

MeCabManager::~MeCabManager() {
  if (cabocha_parser_) {
    cabocha_destroy(cabocha_parser_);
    cabocha_parser_ = nullptr;
  }
  if (mecab_tagger_) {
    delete mecab_tagger_;
    mecab_tagger_ = nullptr;
  }
}

bool MeCabManager::initialize(const std::string &mecabDicPath,
                              const std::string &mecabCharset) {
  SystemLibInfo systemMeCab = detectSystemMeCab();
  if (!systemMeCab.isAvailable) {
    if (isDebugEnabled()) {
      std::cerr << "[ERROR] System MeCab not detected" << std::endl;
    }
    return false;
  }

  if (!mecabCharset.empty()) {
    system_charset_ = mecabCharset;
  } else {
    system_charset_ = systemMeCab.charset;
  }

  std::string mecab_args;
  if (!mecabDicPath.empty()) {
    mecab_args = "-d " + mecabDicPath;
  } else if (!systemMeCab.dicPath.empty()) {
    mecab_args = "-d " + systemMeCab.dicPath + "/ipadic";
    if (isDebugEnabled()) {
      std::cerr << "[DEBUG] Using detected MeCab dicdir: "
                << systemMeCab.dicPath << "/ipadic" << std::endl;
    }
  }

  if (isDebugEnabled() && !mecab_args.empty()) {
    std::cerr << "[DEBUG] MeCab args: " << mecab_args << std::endl;
  }

  mecab_tagger_ = MeCab::createTagger(mecab_args.c_str());
  if (!mecab_tagger_) {
    std::string error = MeCab::getTaggerError() ? MeCab::getTaggerError()
                                                : "Unknown MeCab error";
    if (isDebugEnabled()) {
      std::cerr << "[ERROR] MeCab initialization failed with args '"
                << mecab_args << "': " << error << std::endl;
    }

    if (!mecab_args.empty()) {
      if (isDebugEnabled()) {
        std::cerr << "[DEBUG] Trying MeCab without explicit dictionary path..."
                  << std::endl;
      }
      mecab_tagger_ = MeCab::createTagger("");
      if (!mecab_tagger_) {
        error = MeCab::getTaggerError() ? MeCab::getTaggerError()
                                        : "Unknown MeCab error";
        if (isDebugEnabled()) {
          std::cerr << "[ERROR] MeCab fallback initialization also failed: "
                    << error << std::endl;
        }
        return false;
      }
    } else {
      return false;
    }
  }

  system_charset_ = testMeCabCharset(mecab_tagger_, system_charset_);

  if (isDebugEnabled()) {
    std::cerr << "[DEBUG] MeCab successfully initialized with charset: "
              << system_charset_ << std::endl;
  }

  if (enable_cabocha_) {
    SystemLibInfo systemCabocha = detectSystemCaboCha();
    if (systemCabocha.isAvailable) {
      cabocha_parser_ = cabocha_new2("");
      if (cabocha_parser_) {
        cabocha_available_ = true;
        if (isDebugEnabled()) {
          std::cerr << "[DEBUG] CaboCha successfully initialized" << std::endl;
        }
      } else {
        const char *error = cabocha_strerror(nullptr);
        if (isDebugEnabled()) {
          std::cerr << "[DEBUG] CaboCha initialization failed: "
                    << (error ? error : "Unknown error") << std::endl;
        }
      }
    } else if (isDebugEnabled()) {
      std::cerr << "[DEBUG] CaboCha not available on system" << std::endl;
    }
  }

  if (isDebugEnabled()) {
    std::cerr << "[DEBUG] MeCabManager initialized - MeCab: "
              << (mecab_tagger_ ? "OK" : "FAIL")
              << ", CaboCha: " << (cabocha_available_ ? "OK" : "N/A")
              << std::endl;
  }

  return mecab_tagger_ != nullptr;
}

SystemLibInfo MeCabManager::detectSystemMeCab() {
  SystemLibInfo info;

  if (isDebugEnabled()) {
    std::cerr << "[DEBUG] Detecting system MeCab installation..." << std::endl;
  }

  // Try mecab-config first
  const char *cmd = "mecab-config --dicdir 2>/dev/null";
  FILE *pipe = popen(cmd, "r");
  if (pipe) {
    char buffer[1024];
    if (fgets(buffer, sizeof(buffer), pipe)) {
      std::string dicdir(buffer);
      // Remove trailing newline
      if (!dicdir.empty() && dicdir.back() == '\n') {
        dicdir.pop_back();
      }
      info.dicPath = dicdir;

      if (isDebugEnabled()) {
        std::cerr << "[DEBUG] mecab-config --dicdir: " << dicdir << std::endl;
      }
    }
    pclose(pipe);
  }

  // Try to detect charset from dicrc file
  if (!info.dicPath.empty()) {
    std::string dicrcPath = info.dicPath + "/ipadic/dicrc";
    std::ifstream dicrc(dicrcPath);
    if (dicrc.is_open()) {
      std::string line;
      while (std::getline(dicrc, line)) {
        if (line.find("config-charset") != std::string::npos) {
          size_t equalPos = line.find('=');
          if (equalPos != std::string::npos) {
            std::string charset = line.substr(equalPos + 1);
            // Trim whitespace
            charset.erase(0, charset.find_first_not_of(" \t"));
            charset.erase(charset.find_last_not_of(" \t") + 1);
            info.charset = charset;

            if (isDebugEnabled()) {
              std::cerr << "[DEBUG] Found charset in dicrc: " << charset
                        << std::endl;
            }
            break;
          }
        }
      }
    }
  }

  if (info.charset.empty()) {
    info.charset = "UTF-8";
    if (isDebugEnabled()) {
      std::cerr << "[DEBUG] Using default charset: UTF-8" << std::endl;
    }
  } else if (info.charset != "UTF-8") {
    // Test if MeCab actually works with UTF-8 despite dicrc settings
    if (isDebugEnabled()) {
      std::cerr << "[DEBUG] dicrc says charset: " << info.charset
                << ", testing actual behavior..." << std::endl;
    }

    // Quick test: try to parse UTF-8 text directly
    MeCab::Tagger *testTagger = MeCab::createTagger("");
    if (testTagger) {
      const char *testUtf8 = "誤解"; // Known UTF-8 text
      const MeCab::Node *testNode = testTagger->parseToNode(testUtf8);

      bool utf8Works = false;
      for (const MeCab::Node *n = testNode; n; n = n->next) {
        if (n->stat == MECAB_BOS_NODE || n->stat == MECAB_EOS_NODE)
          continue;

        size_t len = static_cast<size_t>(n->length);
        std::string surface = std::string(n->surface, len);

        // Check if the surface contains valid UTF-8 Japanese characters
        if (surface == testUtf8 &&
            surface.size() == 6) { // "誤解" is 6 bytes in UTF-8
          utf8Works = true;
          if (isDebugEnabled()) {
            std::cerr << "[DEBUG] MeCab actually works with UTF-8 input, "
                         "overriding dicrc charset from "
                      << info.charset << " to UTF-8" << std::endl;
          }
          break;
        }
      }
      delete testTagger;

      if (utf8Works) {
        info.charset = "UTF-8"; // Override dicrc setting
      }
    }
  }

  info.isAvailable = !info.dicPath.empty();

  if (isDebugEnabled()) {
    std::cerr << "[DEBUG] System MeCab detection result - Available: "
              << (info.isAvailable ? "yes" : "no")
              << ", DicPath: " << info.dicPath << ", Charset: " << info.charset
              << std::endl;
  }

  return info;
}

SystemLibInfo MeCabManager::detectSystemCaboCha() {
  SystemLibInfo info;

  if (isDebugEnabled()) {
    std::cerr << "[DEBUG] Detecting system CaboCha installation..."
              << std::endl;
  }

  // Try cabocha-config
  const char *cmd = "cabocha-config --version 2>/dev/null";
  FILE *pipe = popen(cmd, "r");
  if (pipe) {
    char buffer[256];
    if (fgets(buffer, sizeof(buffer), pipe)) {
      info.isAvailable = true;
      if (isDebugEnabled()) {
        std::cerr << "[DEBUG] cabocha-config found, system CaboCha available"
                  << std::endl;
      }
    }
    pclose(pipe);
  }

  SystemLibInfo mecabInfo = detectSystemMeCab();
  info.charset = mecabInfo.charset;

  if (isDebugEnabled()) {
    std::cerr << "[DEBUG] System CaboCha detection result - Available: "
              << (info.isAvailable ? "yes" : "no")
              << ", Charset: " << info.charset << std::endl;
  }

  return info;
}

std::string MeCabManager::testMeCabCharset(MeCab::Tagger *tagger,
                                           const std::string &originalCharset) {
  if (!tagger || originalCharset == "UTF-8") {
    return originalCharset;
  }

  // Test with known UTF-8 text
  const char *testUtf8 = "誤解";
  const MeCab::Node *testNode = tagger->parseToNode(testUtf8);

  for (const MeCab::Node *n = testNode; n; n = n->next) {
    if (n->stat == MECAB_BOS_NODE || n->stat == MECAB_EOS_NODE)
      continue;

    size_t len = static_cast<size_t>(n->length);
    std::string surface = std::string(n->surface, len);

    // If we get back the same UTF-8 text, MeCab is working in UTF-8 mode
    if (surface == testUtf8 && surface.size() == 6) {
      if (isDebugEnabled()) {
        std::cerr << "[DEBUG] MeCab accepts UTF-8 input directly, using UTF-8"
                  << std::endl;
      }
      return "UTF-8";
    }
  }

  if (isDebugEnabled()) {
    std::cerr << "[DEBUG] MeCab requires " << originalCharset << " encoding"
              << std::endl;
  }
  return originalCharset;
}

} // namespace mecab
} // namespace MoZuku
