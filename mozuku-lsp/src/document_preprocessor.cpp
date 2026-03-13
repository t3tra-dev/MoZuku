#include "mozuku/analysis/document_preprocessor.hpp"
#include "encoding_utils.hpp"
#include "mozuku/treesitter/document.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>

namespace {

struct LocalByteRange {
  size_t startByte{0};
  size_t endByte{0};
};

bool isEscaped(const std::string &text, size_t pos) {
  size_t count = 0;
  while (pos > count && text[pos - count - 1] == '\\') {
    ++count;
  }
  return (count % 2) == 1;
}

size_t findClosingDollar(const std::string &text, size_t pos) {
  for (size_t i = pos; i < text.size(); ++i) {
    if (text[i] == '$' && !isEscaped(text, i)) {
      return i;
    }
  }
  return std::string::npos;
}

size_t findClosingDoubleDollar(const std::string &text, size_t pos) {
  for (size_t i = pos; i + 1 < text.size(); ++i) {
    if (text[i] == '$' && text[i + 1] == '$' && !isEscaped(text, i)) {
      return i;
    }
  }
  return std::string::npos;
}

std::string sanitizeLatexCommentText(const std::string &raw) {
  if (raw.empty()) {
    return raw;
  }

  std::string sanitized = raw;
  sanitized[0] = ' ';
  size_t idx = 1;
  while (idx < sanitized.size() && sanitized[idx] == '%') {
    sanitized[idx] = ' ';
    ++idx;
  }
  while (idx < sanitized.size() &&
         (sanitized[idx] == ' ' || sanitized[idx] == '\t')) {
    sanitized[idx] = ' ';
    ++idx;
  }
  return sanitized;
}

std::vector<MoZuku::comments::CommentSegment>
collectLatexComments(const std::string &text) {
  std::vector<MoZuku::comments::CommentSegment> segments;
  size_t pos = 0;
  while (pos < text.size()) {
    size_t lineStart = pos;
    size_t lineEnd = text.find('\n', pos);
    if (lineEnd == std::string::npos) {
      lineEnd = text.size();
    }

    size_t current = lineStart;
    bool found = false;
    while (current < lineEnd) {
      if (text[current] == '%' && !isEscaped(text, current)) {
        found = true;
        break;
      }
      ++current;
    }

    if (found) {
      MoZuku::comments::CommentSegment segment;
      segment.startByte = current;
      segment.endByte = lineEnd;
      segment.sanitized =
          sanitizeLatexCommentText(text.substr(current, lineEnd - current));
      segments.push_back(std::move(segment));
    }

    if (lineEnd >= text.size()) {
      break;
    }
    pos = lineEnd + 1;
  }

  return segments;
}

std::vector<LocalByteRange> collectHtmlContentRanges(const std::string &text) {
  std::vector<LocalByteRange> ranges;
  MoZuku::treesitter::ParsedDocument document("html", text);
  if (!document.isValid()) {
    return ranges;
  }

  MoZuku::treesitter::walkDepthFirst(document.root(), [&](TSNode node) {
    const char *type = ts_node_type(node);
    if (type && std::strcmp(type, "text") == 0) {
      size_t start = ts_node_start_byte(node);
      size_t end = ts_node_end_byte(node);
      if (start >= end || end > text.size()) {
        return false;
      }

      size_t trimmedStart = start;
      while (trimmedStart < end &&
             std::isspace(static_cast<unsigned char>(text[trimmedStart]))) {
        ++trimmedStart;
      }
      size_t trimmedEnd = end;
      while (trimmedEnd > trimmedStart &&
             std::isspace(static_cast<unsigned char>(text[trimmedEnd - 1]))) {
        --trimmedEnd;
      }
      if (trimmedEnd > trimmedStart) {
        ranges.push_back({trimmedStart, trimmedEnd});
      }
      return false;
    }
    return true;
  });

  return ranges;
}

std::vector<LocalByteRange> collectLatexContentRanges(const std::string &text) {
  std::vector<LocalByteRange> ranges;
  size_t i = 0;
  while (i < text.size()) {
    unsigned char c = static_cast<unsigned char>(text[i]);
    if (c == '%' && !isEscaped(text, i)) {
      size_t lineEnd = text.find('\n', i);
      if (lineEnd == std::string::npos) {
        break;
      }
      i = lineEnd + 1;
      continue;
    }
    if (c == '$' && !isEscaped(text, i)) {
      if (i + 1 < text.size() && text[i + 1] == '$') {
        size_t closing = findClosingDoubleDollar(text, i + 2);
        if (closing == std::string::npos) {
          break;
        }
        i = closing + 2;
        continue;
      }

      size_t closing = findClosingDollar(text, i + 1);
      if (closing == std::string::npos) {
        break;
      }
      i = closing + 1;
      continue;
    }
    if (c == '\\') {
      ++i;
      while (i < text.size()) {
        unsigned char ch = static_cast<unsigned char>(text[i]);
        if (!std::isalpha(ch) && ch != '@') {
          break;
        }
        ++i;
      }
      if (i < text.size() && text[i] == '*') {
        ++i;
      }
      continue;
    }
    if (c == '{' || c == '}') {
      ++i;
      continue;
    }
    if (std::isspace(c)) {
      ++i;
      continue;
    }

    size_t start = i;
    bool advanced = false;
    while (i < text.size()) {
      unsigned char d = static_cast<unsigned char>(text[i]);
      if (d == '\\' || d == '$' || d == '{' || d == '}' ||
          (d == '%' && !isEscaped(text, i))) {
        break;
      }
      if (d < 0x80 && (std::isspace(d) || std::ispunct(d))) {
        break;
      }
      i += MoZuku::encoding::utf8SequenceLength(d);
      advanced = true;
    }
    if (advanced) {
      ranges.push_back({start, i});
      continue;
    }
    ++i;
  }

  return ranges;
}

std::vector<ByteRange> toByteRanges(const std::vector<LocalByteRange> &ranges) {
  std::vector<ByteRange> converted;
  converted.reserve(ranges.size());
  for (const auto &range : ranges) {
    converted.push_back(ByteRange{range.startByte, range.endByte});
  }
  return converted;
}

void appendCommentRanges(
    std::vector<ByteRange> &ranges,
    const std::vector<MoZuku::comments::CommentSegment> &segments) {
  ranges.reserve(ranges.size() + segments.size());
  for (const auto &segment : segments) {
    ranges.push_back(ByteRange{segment.startByte, segment.endByte});
  }
}

std::string buildMaskWithContentRanges(
    const std::string &text, const std::vector<LocalByteRange> &contentRanges,
    const std::vector<MoZuku::comments::CommentSegment> &commentSegments) {
  std::string masked = text;
  for (char &ch : masked) {
    if (ch != '\n' && ch != '\r') {
      ch = ' ';
    }
  }

  for (const auto &range : contentRanges) {
    if (range.startByte >= masked.size()) {
      continue;
    }
    size_t len = std::min(range.endByte - range.startByte,
                          masked.size() - range.startByte);
    for (size_t i = 0; i < len; ++i) {
      masked[range.startByte + i] = text[range.startByte + i];
    }
  }

  for (const auto &segment : commentSegments) {
    if (segment.startByte >= masked.size()) {
      continue;
    }
    size_t len =
        std::min(segment.sanitized.size(), masked.size() - segment.startByte);
    for (size_t i = 0; i < len; ++i) {
      masked[segment.startByte + i] = segment.sanitized[i];
    }
  }

  return masked;
}

std::string buildCommentOnlyMask(
    const std::string &text,
    const std::vector<MoZuku::comments::CommentSegment> &segments) {
  std::string masked = text;
  for (char &ch : masked) {
    if (ch != '\n' && ch != '\r') {
      ch = ' ';
    }
  }

  const size_t docSize = masked.size();
  for (const auto &segment : segments) {
    if (segment.startByte >= docSize) {
      continue;
    }
    size_t maxCopy =
        std::min(docSize - segment.startByte, segment.sanitized.size());
    for (size_t i = 0; i < maxCopy; ++i) {
      masked[segment.startByte + i] = segment.sanitized[i];
    }
  }

  return masked;
}

} // namespace

namespace MoZuku::analysis {

ProcessedDocument DocumentPreprocessor::prepare(const std::string &languageId,
                                                const std::string &text) const {
  ProcessedDocument result;
  result.analysisText = text;

  if (languageId.empty() || languageId == "japanese") {
    return result;
  }

  if (languageId == "html") {
    result.commentSegments = comments::extractComments(languageId, text);
    std::vector<LocalByteRange> contentRanges = collectHtmlContentRanges(text);
    result.contentHighlightRanges = toByteRanges(contentRanges);
    appendCommentRanges(result.contentHighlightRanges, result.commentSegments);
    result.analysisText =
        buildMaskWithContentRanges(text, contentRanges, result.commentSegments);
    return result;
  }

  if (languageId == "latex") {
    result.commentSegments = collectLatexComments(text);
    std::vector<LocalByteRange> contentRanges = collectLatexContentRanges(text);
    result.contentHighlightRanges = toByteRanges(contentRanges);
    appendCommentRanges(result.contentHighlightRanges, result.commentSegments);
    result.analysisText =
        buildMaskWithContentRanges(text, contentRanges, result.commentSegments);
    return result;
  }

  if (!comments::isLanguageSupported(languageId)) {
    return result;
  }

  result.commentSegments = comments::extractComments(languageId, text);
  result.analysisText = buildCommentOnlyMask(text, result.commentSegments);
  return result;
}

} // namespace MoZuku::analysis
