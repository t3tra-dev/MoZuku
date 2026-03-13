#include "text_processor.hpp"
#include "encoding_utils.hpp"
#include "mozuku/core/debug.hpp"
#include <algorithm>
#include <iostream>
#include <vector>
#include <cstdint>

namespace MoZuku {
namespace text {

std::string TextProcessor::sanitizeUTF8(const std::string &input) {
  return encoding::sanitizeUtf8(input);
}

std::vector<SentenceBoundary>
TextProcessor::splitIntoSentences(const std::string &text) {
  if (debug::isEnabled()) {
    std::cerr << "[DEBUG] splitIntoSentences called with text length: "
              << text.size() << std::endl;
  }

  std::vector<SentenceBoundary> sentences;
  if (text.empty()) {
    if (debug::isEnabled()) {
      std::cerr << "[DEBUG] Empty text, returning empty sentences" << std::endl;
    }
    return sentences;
  }

  // Multi-stage approach: 1. newlines, 2. tabs, 3. periods
  size_t start = 0;
  int sentenceId = 0;

  while (start < text.size()) {
    size_t end = start;
    bool foundBoundary = false;

    // Find next sentence boundary - limit search to avoid infinite loops
    size_t maxSearch = std::min(text.size(), start + 10000); // Safety limit

    while (end < maxSearch) {
      char c = text[end];

      // Priority 1: Check for newline first
      if (c == '\n') {
        foundBoundary = true;
        end++; // Include the boundary character
        break;
      }

      // Priority 2: Check for tab
      if (c == '\t') {
        foundBoundary = true;
        end++; // Include the boundary character
        break;
      }

      // Priority 3: Check for Japanese period (。)
      if (isJapanesePunctuation(text, end)) {
        foundBoundary = true;
        end += 3; // Japanese punctuation is 3 bytes in UTF-8
        break;
      }

      end++;
    }

    // Safety check - if we hit the limit, just end at text size
    if (end >= maxSearch && end < text.size()) {
      end = text.size();
      foundBoundary = true;
    }

    // Create sentence boundary
    if (end > start) {
      SentenceBoundary sentence;
      sentence.start = start;
      sentence.end = end;
      sentence.sentenceId = sentenceId++;
      sentence.text = text.substr(start, end - start);

      // Trim leading tabs and whitespace from sentence text for analysis
      size_t textStart = 0;
      while (textStart < sentence.text.size() &&
             (sentence.text[textStart] == ' ' ||
              sentence.text[textStart] == '\t' ||
              sentence.text[textStart] == '\r')) {
        textStart++;
      }

      size_t textEnd = sentence.text.size();
      while (textEnd > textStart && (sentence.text[textEnd - 1] == ' ' ||
                                     sentence.text[textEnd - 1] == '\t' ||
                                     sentence.text[textEnd - 1] == '\r' ||
                                     sentence.text[textEnd - 1] == '\n')) {
        textEnd--;
      }

      if (textEnd > textStart) {
        sentence.text = sentence.text.substr(textStart, textEnd - textStart);
        sentences.push_back(sentence);

        if (debug::isEnabled()) {
          std::cerr << "[DEBUG] Created sentence " << sentenceId - 1
                    << ": length=" << sentence.text.size()
                    << ", start=" << sentence.start << ", end=" << sentence.end
                    << std::endl;
        }
      }
    }

    start = end;

    // Skip multiple whitespace after boundaries (with safety limit)
    start = skipWhitespace(text, start);

    // Safety check to prevent infinite loop
    if (start >= text.size()) {
      break;
    }
  }

  if (debug::isEnabled()) {
    std::cerr << "[DEBUG] splitIntoSentences completed: created "
              << sentences.size() << " sentences" << std::endl;
  }

  return sentences;
}

double TextProcessor::calculateJapaneseRatio(const std::string &text) {
  if (text.empty()) {
    return 0.0;
  }

  size_t japaneseCount = 0;
  size_t visibleCount = 0;

  for (size_t i = 0; i < text.size(); ++i) {
    unsigned char c = static_cast<unsigned char>(text[i]);
    size_t seqLen = 1;

    if (c < 0x80) {
      seqLen = 1;
    } else if ((c & 0xE0) == 0xC0) {
      seqLen = 2;
    } else if ((c & 0xF0) == 0xE0) {
      seqLen = 3;
    } else if ((c & 0xF8) == 0xF0) {
      seqLen = 4;
    } else {
      continue;
    }

    if (!isValidUtf8Sequence(text, i, seqLen)) {
      continue;
    }

    uint32_t codepoint = decodeCodepoint(text, i, seqLen);
    if (!isWhitespaceCodepoint(codepoint)) {
      ++visibleCount;
      if (isJapaneseCodepoint(codepoint)) {
        ++japaneseCount;
      }
    }

    i += seqLen - 1;
  }

  if (visibleCount == 0) {
    return 0.0;
  }

  return static_cast<double>(japaneseCount) / static_cast<double>(visibleCount);
}

bool TextProcessor::isJapanesePunctuation(const std::string &text, size_t pos) {
  if (pos + 2 >= text.size())
    return false;

  // Check for Japanese period (。) - UTF-8 encoded as 0xE3 0x80 0x82
  if (static_cast<unsigned char>(text[pos]) == 0xE3 &&
      static_cast<unsigned char>(text[pos + 1]) == 0x80 &&
      static_cast<unsigned char>(text[pos + 2]) == 0x82) {
    return true;
  }

  // Check for Japanese question mark (？) - UTF-8: 0xEF 0xBC 0x9F
  if (static_cast<unsigned char>(text[pos]) == 0xEF &&
      static_cast<unsigned char>(text[pos + 1]) == 0xBC &&
      static_cast<unsigned char>(text[pos + 2]) == 0x9F) {
    return true;
  }

  // Check for Japanese exclamation mark (！) - UTF-8: 0xEF 0xBC 0x81
  if (static_cast<unsigned char>(text[pos]) == 0xEF &&
      static_cast<unsigned char>(text[pos + 1]) == 0xBC &&
      static_cast<unsigned char>(text[pos + 2]) == 0x81) {
    return true;
  }

  return false;
}

size_t TextProcessor::skipWhitespace(const std::string &text, size_t pos) {
  size_t skipCount = 0;
  while (pos < text.size() && skipCount < 100 &&
         (text[pos] == ' ' || text[pos] == '\t' || text[pos] == '\r')) {
    pos++;
    skipCount++;
  }
  return pos;
}

bool TextProcessor::isValidUtf8Sequence(const std::string &input, size_t pos,
                                        size_t seqLen) {
  if (pos + seqLen > input.size())
    return false;

  for (size_t j = 1; j < seqLen; ++j) {
    if ((static_cast<unsigned char>(input[pos + j]) & 0xC0) != 0x80) {
      return false; // Invalid continuation byte
    }
  }
  return true;
}

uint32_t TextProcessor::decodeCodepoint(const std::string &text, size_t pos,
                                        size_t seqLen) {
  const unsigned char c0 = static_cast<unsigned char>(text[pos]);
  if (seqLen == 1) {
    return c0;
  }

  const unsigned char c1 = static_cast<unsigned char>(text[pos + 1]);
  if (seqLen == 2) {
    return (static_cast<uint32_t>(c0 & 0x1F) << 6) |
           static_cast<uint32_t>(c1 & 0x3F);
  }

  const unsigned char c2 = static_cast<unsigned char>(text[pos + 2]);
  if (seqLen == 3) {
    return (static_cast<uint32_t>(c0 & 0x0F) << 12) |
           (static_cast<uint32_t>(c1 & 0x3F) << 6) |
           static_cast<uint32_t>(c2 & 0x3F);
  }

  const unsigned char c3 = static_cast<unsigned char>(text[pos + 3]);
  return (static_cast<uint32_t>(c0 & 0x07) << 18) |
         (static_cast<uint32_t>(c1 & 0x3F) << 12) |
         (static_cast<uint32_t>(c2 & 0x3F) << 6) |
         static_cast<uint32_t>(c3 & 0x3F);
}

bool TextProcessor::isWhitespaceCodepoint(uint32_t codepoint) {
  return codepoint == 0x09 || codepoint == 0x0A || codepoint == 0x0D ||
         codepoint == 0x20 || codepoint == 0x3000;
}

bool TextProcessor::isJapaneseCodepoint(uint32_t codepoint) {
  return (codepoint >= 0x3040 && codepoint <= 0x309F) ||
         (codepoint >= 0x30A0 && codepoint <= 0x30FF) ||
         (codepoint >= 0x31F0 && codepoint <= 0x31FF) ||
         (codepoint >= 0x3400 && codepoint <= 0x4DBF) ||
         (codepoint >= 0x4E00 && codepoint <= 0x9FFF) ||
         (codepoint >= 0x3000 && codepoint <= 0x303F) ||
         (codepoint >= 0xFF66 && codepoint <= 0xFF9F);
}

} // namespace text
} // namespace MoZuku
