#pragma once

#include <cstddef>
#include <string>
#include <vector>

struct TSLanguage;

namespace MoZuku {
namespace comments {

struct CommentSegment {
  size_t startByte{0};
  size_t endByte{0};
  std::string sanitized;
};

// サポート対象の言語IDか確認
bool isLanguageSupported(const std::string &languageId);

// 指定言語のコメントを抽出し、コメント記号を除去したテキストとバイト範囲を返す
std::vector<CommentSegment> extractComments(const std::string &languageId,
                                            const std::string &text);

// tree-sitter言語ハンドルを取得 (未対応の場合はnullptr)
const TSLanguage *resolveLanguage(const std::string &languageId);

} // namespace comments
} // namespace MoZuku
