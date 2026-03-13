#include "comment_extractor.hpp"
#include "mozuku/treesitter/document.hpp"

#include <cctype>
#include <string>
#include <string_view>
#include <vector>

namespace {

inline bool isNewline(char c) { return c == '\n' || c == '\r'; }

inline void setSpace(char &c) {
  if (!isNewline(c)) {
    c = ' ';
  }
}

void sanitizeLineComment(std::string &segment) {
  const size_t len = segment.size();
  if (len == 0)
    return;

  size_t i = 0;

  if (len >= 2 && segment[0] == '/' && segment[1] == '/') {
    setSpace(segment[0]);
    setSpace(segment[1]);
    i = 2;
    while (i < len && (segment[i] == '/' || segment[i] == '!')) {
      setSpace(segment[i]);
      ++i;
    }
  } else if (segment[0] == '#') {
    while (i < len && segment[i] == '#') {
      setSpace(segment[i]);
      ++i;
    }
    if (i < len && segment[i] == '!') {
      setSpace(segment[i]);
      ++i;
    }
  } else if (segment[0] == '%') {
    setSpace(segment[0]);
    i = 1;
    while (i < len && (segment[i] == ' ' || segment[i] == '\t')) {
      setSpace(segment[i]);
      ++i;
    }
  } else if (len >= 2 && segment[0] == '-' && segment[1] == '-') {
    setSpace(segment[0]);
    setSpace(segment[1]);
    i = 2;
  }

  while (i < len && (segment[i] == ' ' || segment[i] == '\t')) {
    setSpace(segment[i]);
    ++i;
  }
}

void sanitizeBlockComment(std::string &segment) {
  const size_t len = segment.size();
  if (len == 0)
    return;

  if (len >= 4 && segment[0] == '<' && segment[1] == '!' && segment[2] == '-' &&
      segment[3] == '-') {
    setSpace(segment[0]);
    setSpace(segment[1]);
    setSpace(segment[2]);
    setSpace(segment[3]);
    size_t i = 4;
    while (i < len && segment[i] == '-') {
      setSpace(segment[i]);
      ++i;
    }
    while (i < len && (segment[i] == ' ' || segment[i] == '\t')) {
      setSpace(segment[i]);
      ++i;
    }
  } else if (len >= 2 && segment[0] == '/' && segment[1] == '*') {
    setSpace(segment[0]);
    setSpace(segment[1]);
    size_t i = 2;
    while (i < len && segment[i] == '*') {
      setSpace(segment[i]);
      ++i;
    }
    while (i < len && (segment[i] == ' ' || segment[i] == '\t')) {
      setSpace(segment[i]);
      ++i;
    }
  }

  if (len >= 3 && segment[len - 3] == '-' && segment[len - 2] == '-' &&
      segment[len - 1] == '>') {
    setSpace(segment[len - 3]);
    setSpace(segment[len - 2]);
    setSpace(segment[len - 1]);

    size_t idx = len >= 4 ? len - 4 : static_cast<size_t>(-1);
    while (idx < len && (segment[idx] == '-' || segment[idx] == ' ' ||
                         segment[idx] == '\t')) {
      setSpace(segment[idx]);
      if (idx == 0)
        break;
      --idx;
    }
  } else if (len >= 2 && segment[len - 2] == '*' && segment[len - 1] == '/') {
    setSpace(segment[len - 2]);
    setSpace(segment[len - 1]);

    size_t idx = len >= 3 ? len - 3 : static_cast<size_t>(-1);
    while (idx < len && (segment[idx] == '*' || segment[idx] == ' ' ||
                         segment[idx] == '\t')) {
      setSpace(segment[idx]);
      if (idx == 0)
        break;
      --idx;
    }
  }

  size_t pos = 0;
  while (pos < len) {
    size_t lineStart = pos;
    size_t lineEnd = segment.find('\n', pos);
    if (lineEnd == std::string::npos) {
      lineEnd = len;
    }

    size_t idx = lineStart;
    while (idx < lineEnd && (segment[idx] == ' ' || segment[idx] == '\t' ||
                             segment[idx] == '\r')) {
      setSpace(segment[idx]);
      ++idx;
    }

    if (idx < lineEnd && (segment[idx] == '*' || segment[idx] == '-')) {
      setSpace(segment[idx]);
      ++idx;
      if (idx < lineEnd && segment[idx] == ' ') {
        setSpace(segment[idx]);
      }
    }

    pos = (lineEnd < len) ? lineEnd + 1 : len;
  }
}

void sanitizeComment(std::string &segment, const char *nodeType) {
  std::string_view type =
      nodeType ? std::string_view(nodeType) : std::string_view();

  bool isBlock =
      type.find("block") != std::string_view::npos ||
      (segment.size() >= 2 && segment[0] == '/' && segment[1] == '*') ||
      (segment.size() >= 4 && segment[0] == '<' && segment[1] == '!' &&
       segment[2] == '-' && segment[3] == '-');
  bool isLine = type.find("line") != std::string_view::npos ||
                (!segment.empty() && (segment[0] == '#')) ||
                (segment.size() >= 2 && segment[0] == '/' && segment[1] == '/');

  if (isBlock && !isLine) {
    sanitizeBlockComment(segment);
  } else {
    sanitizeLineComment(segment);
  }
}

} // namespace

namespace MoZuku {
namespace comments {

const TSLanguage *resolveLanguage(const std::string &languageId) {
  return treesitter::resolveLanguage(languageId);
}

bool isLanguageSupported(const std::string &languageId) {
  return treesitter::isLanguageSupported(languageId);
}

std::vector<CommentSegment> extractComments(const std::string &languageId,
                                            const std::string &text) {
  std::vector<CommentSegment> segments;
  treesitter::ParsedDocument document(languageId, text);
  if (!document.isValid()) {
    return segments;
  }

  treesitter::walkDepthFirst(document.root(), [&](TSNode node) {
    const char *type = ts_node_type(node);
    if (type) {
      std::string_view nodeType(type);
      if (nodeType.find("comment") != std::string_view::npos) {
        size_t start = ts_node_start_byte(node);
        size_t end = ts_node_end_byte(node);
        if (start < end && end <= text.size()) {
          std::string segmentText = text.substr(start, end - start);
          sanitizeComment(segmentText, type);

          CommentSegment segment;
          segment.startByte = start;
          segment.endByte = end;
          segment.sanitized = std::move(segmentText);
          segments.push_back(std::move(segment));
        }
        return false;
      }
    }
    return true;
  });

  return segments;
}

} // namespace comments
} // namespace MoZuku
