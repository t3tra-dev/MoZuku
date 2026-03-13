#pragma once

#include <memory>
#include <string>
#include <vector>

#include <tree_sitter/api.h>

namespace MoZuku::treesitter {

const TSLanguage *resolveLanguage(const std::string &languageId);
bool isLanguageSupported(const std::string &languageId);

class ParsedDocument {
public:
  ParsedDocument();
  ParsedDocument(const std::string &languageId, const std::string &text);
  ParsedDocument(const TSLanguage *language, const std::string &text);

  ParsedDocument(ParsedDocument &&other) noexcept = default;
  ParsedDocument &operator=(ParsedDocument &&other) noexcept = default;

  ParsedDocument(const ParsedDocument &) = delete;
  ParsedDocument &operator=(const ParsedDocument &) = delete;

  bool isValid() const;
  TSNode root() const;

private:
  std::unique_ptr<TSTree, void (*)(TSTree *)> tree_;
};

template <typename Visitor>
void walkDepthFirst(TSNode root, Visitor &&visitor) {
  if (ts_node_is_null(root)) {
    return;
  }

  std::vector<TSNode> stack;
  stack.push_back(root);

  while (!stack.empty()) {
    TSNode node = stack.back();
    stack.pop_back();

    if (ts_node_is_null(node)) {
      continue;
    }

    if (!visitor(node)) {
      continue;
    }

    uint32_t childCount = ts_node_child_count(node);
    for (uint32_t i = childCount; i > 0; --i) {
      TSNode child = ts_node_child(node, i - 1);
      if (!ts_node_is_null(child)) {
        stack.push_back(child);
      }
    }
  }
}

} // namespace MoZuku::treesitter
