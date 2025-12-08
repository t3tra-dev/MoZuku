#include "utf16.hpp"

namespace {
static inline int utf8SeqLen(unsigned char c) {
  if (c < 0x80)
    return 1;
  if (c < 0xE0)
    return 2;
  if (c < 0xF0)
    return 3;
  return 4;
}

static inline unsigned int decodeCodePoint(const std::string &s, size_t &i) {
  unsigned char c = static_cast<unsigned char>(s[i]);
  if (c < 0x80) {
    return s[i++];
  }
  if ((c >> 5) == 0x6) {
    unsigned int cp =
        ((c & 0x1F) << 6) | (static_cast<unsigned char>(s[i + 1]) & 0x3F);
    i += 2;
    return cp;
  }
  if ((c >> 4) == 0xE) {
    unsigned int cp = ((c & 0x0F) << 12) |
                      ((static_cast<unsigned char>(s[i + 1]) & 0x3F) << 6) |
                      (static_cast<unsigned char>(s[i + 2]) & 0x3F);
    i += 3;
    return cp;
  }
  unsigned int cp = ((c & 0x07) << 18) |
                    ((static_cast<unsigned char>(s[i + 1]) & 0x3F) << 12) |
                    ((static_cast<unsigned char>(s[i + 2]) & 0x3F) << 6) |
                    (static_cast<unsigned char>(s[i + 3]) & 0x3F);
  i += 4;
  return cp;
}
} // namespace

std::vector<size_t> computeLineStarts(const std::string &text) {
  std::vector<size_t> lineStarts;
  lineStarts.reserve(64);
  lineStarts.push_back(0);
  for (size_t i = 0; i < text.size(); ++i)
    if (text[i] == '\n')
      lineStarts.push_back(i + 1);
  return lineStarts;
}

Position byteOffsetToPosition(const std::string &text,
                              const std::vector<size_t> &lineStarts,
                              size_t offset) {
  // オフセットをテキストサイズに制限
  if (offset > text.size())
    offset = text.size();

  // オフセット以下の最後の開始位置を二分探索で検索
  size_t lo = 0, hi = lineStarts.size();
  while (lo + 1 < hi) {
    size_t mid = (lo + hi) / 2;
    if (lineStarts[mid] <= offset)
      lo = mid;
    else
      hi = mid;
  }

  size_t lineStart = lineStarts[lo];

  // 行開始からオフセットまでのUTF-16コードユニット数をカウント
  size_t i = lineStart;
  unsigned int col16 = 0;

  while (i < offset && i < text.size() && text[i] != '\n') {
    unsigned char c = static_cast<unsigned char>(text[i]);

    // 効率性と正確性のためASCII文字を直接処理
    if (c < 0x80) {
      // ASCII文字 (タブ、スペースを含む) は常に1つのUTF-16コードユニット
      col16 += 1;
      i += 1;
    } else {
      // マルチバイトUTF-8文字
      size_t prev = i;
      unsigned int cp = decodeCodePoint(text, i);

      // UTF-16エンコーディング:
      // BMP文字は1コードユニット、その他は2コードユニット (サロゲートペア) 
      if (cp <= 0xFFFF) {
        col16 += 1; // BMP文字: 1 UTF-16コードユニット
      } else {
        col16 += 2; // 非BMP文字: 2 UTF-16コードユニット (サロゲートペア) 
      }

      // 無限ループを防ぐ安全性チェック
      if (i == prev) {
        i++;     // 無効なバイトをスキップ
        col16++; // 1コードユニットとしてカウント
      }
    }
  }

  return Position{static_cast<int>(lo), static_cast<int>(col16)};
}

size_t utf8ToUtf16Length(const std::string &utf8Str) {
  size_t i = 0;
  size_t utf16Length = 0;

  while (i < utf8Str.size()) {
    unsigned char c = static_cast<unsigned char>(utf8Str[i]);

    // 効率性と正確性のためASCII文字を直接処理
    if (c < 0x80) {
      // ASCII文字 (タブ、スペースを含む) は常に1つのUTF-16コードユニット
      utf16Length += 1;
      i += 1;
    } else {
      // マルチバイトUTF-8文字
      size_t prev = i;
      unsigned int cp = decodeCodePoint(utf8Str, i);

      // UTF-16エンコーディング:
      // BMP文字は1コードユニット、その他は2コードユニット (サロゲートペア) 
      if (cp <= 0xFFFF) {
        utf16Length += 1; // BMP character
      } else {
        utf16Length += 2; // Non-BMP character (surrogate pair)
      }

      // 無限ループを防ぐ安全性チェック
      if (i == prev) {
        i++;           // 無効なバイトをスキップ
        utf16Length++; // 1コードユニットとしてカウント
      }
    }
  }

  return utf16Length;
}
