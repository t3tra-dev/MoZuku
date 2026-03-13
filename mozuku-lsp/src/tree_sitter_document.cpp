#include "mozuku/treesitter/document.hpp"

#include <algorithm>
#include <cctype>
#include <memory>
#include <string>
#include <unordered_map>

extern "C" {
const TSLanguage *tree_sitter_c();
const TSLanguage *tree_sitter_cpp();
const TSLanguage *tree_sitter_html();
const TSLanguage *tree_sitter_javascript();
const TSLanguage *tree_sitter_python();
const TSLanguage *tree_sitter_rust();
const TSLanguage *tree_sitter_typescript();
const TSLanguage *tree_sitter_tsx();
const TSLanguage *tree_sitter_latex();
}

namespace {

using LanguageFactory = const TSLanguage *(*)();

const std::unordered_map<std::string, LanguageFactory> &languageMap() {
  static const std::unordered_map<std::string, LanguageFactory> map = {
      {"c", tree_sitter_c},
      {"cpp", tree_sitter_cpp},
      {"c++", tree_sitter_cpp},
      {"html", tree_sitter_html},
      {"javascript", tree_sitter_javascript},
      {"javascriptreact", tree_sitter_tsx},
      {"typescript", tree_sitter_typescript},
      {"typescriptreact", tree_sitter_tsx},
      {"tsx", tree_sitter_tsx},
      {"python", tree_sitter_python},
      {"rust", tree_sitter_rust},
      {"latex", tree_sitter_latex}};
  return map;
}

std::string toLower(std::string input) {
  std::transform(
      input.begin(), input.end(), input.begin(),
      [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return input;
}

struct ParserDeleter {
  void operator()(TSParser *parser) const {
    if (parser) {
      ts_parser_delete(parser);
    }
  }
};

} // namespace

namespace MoZuku::treesitter {

const TSLanguage *resolveLanguage(const std::string &languageId) {
  const auto &map = languageMap();
  auto it = map.find(toLower(languageId));
  if (it == map.end()) {
    return nullptr;
  }
  return it->second();
}

bool isLanguageSupported(const std::string &languageId) {
  const auto &map = languageMap();
  return map.find(toLower(languageId)) != map.end();
}

ParsedDocument::ParsedDocument() : tree_(nullptr, &ts_tree_delete) {}

ParsedDocument::ParsedDocument(const std::string &languageId,
                               const std::string &text)
    : ParsedDocument(resolveLanguage(languageId), text) {}

ParsedDocument::ParsedDocument(const TSLanguage *language,
                               const std::string &text)
    : tree_(nullptr, &ts_tree_delete) {
  if (!language) {
    return;
  }

  std::unique_ptr<TSParser, ParserDeleter> parser(ts_parser_new());
  if (!parser) {
    return;
  }

  if (!ts_parser_set_language(parser.get(), language)) {
    return;
  }

  tree_.reset(
      ts_parser_parse_string(parser.get(), nullptr, text.c_str(), text.size()));
}

bool ParsedDocument::isValid() const { return tree_ != nullptr; }

TSNode ParsedDocument::root() const {
  return tree_ ? ts_tree_root_node(tree_.get()) : TSNode{};
}

} // namespace MoZuku::treesitter
