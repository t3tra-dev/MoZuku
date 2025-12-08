#include "text_processor.hpp"
#include <algorithm>
#include <cstdlib>
#include <iostream>

namespace MoZuku {
namespace text {

static bool isDebugEnabled() {
  static bool initialized = false;
  static bool debug = false;
  if (!initialized) {
    debug = (std::getenv("MOZUKU_DEBUG") != nullptr);
    initialized = true;
  }
  return debug;
}

std::string TextProcessor::sanitizeUTF8(const std::string &input) {
  if (input.empty())
    return input;

  std::string result;
  result.reserve(input.size());

  for (size_t i = 0; i < input.size(); ++i) {
    unsigned char c = static_cast<unsigned char>(input[i]);

    // ASCII characters (0x00-0x7F) are safe
    if (c < 0x80) {
      // Skip control characters except tab, newline, carriage return
      if (c >= 0x20 || c == 0x09 || c == 0x0A || c == 0x0D) {
        result += static_cast<char>(c);
      }
      continue;
    }

    // Handle multi-byte UTF-8 sequences
    size_t seqLen = 0;
    if ((c & 0xE0) == 0xC0)
      seqLen = 2; // 110xxxxx (2-byte)
    else if ((c & 0xF0) == 0xE0)
      seqLen = 3; // 1110xxxx (3-byte)
    else if ((c & 0xF8) == 0xF0)
      seqLen = 4; // 11110xxx (4-byte)
    else {
      // Invalid UTF-8 start byte, skip it
      continue;
    }

    // Check if we have enough bytes for the sequence
    if (i + seqLen > input.size()) {
      break; // Incomplete sequence at end of string
    }

    // Validate all continuation bytes
    if (isValidUtf8Sequence(input, i, seqLen)) {
      // Valid sequence, copy it
      for (size_t j = 0; j < seqLen; ++j) {
        result += input[i + j];
      }
      i += seqLen - 1; // -1 because loop will increment i
    } else {
      // Invalid sequence, skip start byte (continuation bytes will be handled
      // in next iterations)
      continue;
    }
  }

  return result;
}

std::vector<SentenceBoundary>
TextProcessor::splitIntoSentences(const std::string &text) {
  if (isDebugEnabled()) {
    std::cerr << "[DEBUG] splitIntoSentences called with text length: "
              << text.size() << std::endl;
  }

  std::vector<SentenceBoundary> sentences;
  if (text.empty()) {
    if (isDebugEnabled()) {
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

        if (isDebugEnabled()) {
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

  if (isDebugEnabled()) {
    std::cerr << "[DEBUG] splitIntoSentences completed: created "
              << sentences.size() << " sentences" << std::endl;
  }

  return sentences;
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

} // namespace text
} // namespace MoZuku
