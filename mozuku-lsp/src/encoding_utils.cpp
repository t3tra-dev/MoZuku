#include "encoding_utils.hpp"

#include <array>
#include <cctype>
#include <cerrno>
#include <cstring>
#include <iconv.h>

namespace {

std::string normalizeCharsetName(const std::string &charset) {
  std::string normalized;
  normalized.reserve(charset.size());
  for (unsigned char c : charset) {
    if (std::isalnum(c)) {
      normalized.push_back(static_cast<char>(std::toupper(c)));
    }
  }
  return normalized;
}

bool isUtf8Charset(const std::string &charset) {
  return normalizeCharsetName(charset) == "UTF8";
}

bool isSameCharset(const std::string &lhs, const std::string &rhs) {
  return normalizeCharsetName(lhs) == normalizeCharsetName(rhs);
}

struct IconvCloser {
  explicit IconvCloser(iconv_t handle) : handle_(handle) {}
  ~IconvCloser() {
    if (handle_ != (iconv_t)-1) {
      iconv_close(handle_);
    }
  }

  iconv_t get() const { return handle_; }

private:
  iconv_t handle_;
};

void appendBuffer(std::string &result, const std::array<char, 256> &buffer,
                  size_t remaining) {
  result.append(buffer.data(), buffer.size() - remaining);
}

void stripUnsupportedControlChars(std::string &text) {
  std::string filtered;
  filtered.reserve(text.size());
  for (unsigned char c : text) {
    if (c >= 0x20 || c == 0x09 || c == 0x0A || c == 0x0D || c >= 0x80) {
      filtered.push_back(static_cast<char>(c));
    }
  }
  text.swap(filtered);
}

} // namespace

namespace MoZuku {
namespace encoding {

std::string convertEncoding(const std::string &input,
                            const std::string &fromCharset,
                            const std::string &toCharset,
                            ConversionOptions options) {
  if (input.empty())
    return input;

  if (!options.skipInvalidInput && isSameCharset(fromCharset, toCharset)) {
    return input;
  }

  iconv_t cd = iconv_open(toCharset.c_str(), fromCharset.c_str());
  if (cd == (iconv_t)-1) {
    return input;
  }
  IconvCloser guard(cd);

  char *inBuf = const_cast<char *>(input.data());
  size_t inBytesLeft = input.size();
  std::string result;
  result.reserve(input.size() * 2 + 16);
  std::array<char, 256> outputBuffer{};

  while (true) {
    char *outBuf = outputBuffer.data();
    size_t outBytesLeft = outputBuffer.size();
    size_t status =
        iconv(guard.get(), &inBuf, &inBytesLeft, &outBuf, &outBytesLeft);
    appendBuffer(result, outputBuffer, outBytesLeft);

    if (status != static_cast<size_t>(-1)) {
      break;
    }

    if (errno == E2BIG) {
      continue;
    }

    if (options.skipInvalidInput && (errno == EILSEQ || errno == EINVAL)) {
      if (inBytesLeft == 0) {
        break;
      }
      ++inBuf;
      --inBytesLeft;
      continue;
    }

    return input;
  }

  while (true) {
    char *outBuf = outputBuffer.data();
    size_t outBytesLeft = outputBuffer.size();
    size_t status =
        iconv(guard.get(), nullptr, nullptr, &outBuf, &outBytesLeft);
    appendBuffer(result, outputBuffer, outBytesLeft);

    if (status != static_cast<size_t>(-1)) {
      break;
    }

    if (errno == E2BIG) {
      continue;
    }

    return input;
  }

  return result;
}

std::string systemToUtf8(const std::string &input,
                         const std::string &systemCharset) {
  if (systemCharset.empty() || isUtf8Charset(systemCharset)) {
    return input;
  }
  return convertEncoding(input, systemCharset, "UTF-8");
}

std::string utf8ToSystem(const std::string &input,
                         const std::string &systemCharset) {
  if (systemCharset.empty() || isUtf8Charset(systemCharset)) {
    return input;
  }
  return convertEncoding(input, "UTF-8", systemCharset);
}

std::string sanitizeUtf8(const std::string &input) {
  std::string sanitized =
      convertEncoding(input, "UTF-8", "UTF-8", ConversionOptions{true});
  stripUnsupportedControlChars(sanitized);
  return sanitized;
}

size_t utf8SequenceLength(unsigned char c) {
  if (c < 0x80) {
    return 1;
  }
  if ((c & 0xE0) == 0xC0) {
    return 2;
  }
  if ((c & 0xF0) == 0xE0) {
    return 3;
  }
  if ((c & 0xF8) == 0xF0) {
    return 4;
  }
  return 1;
}

} // namespace encoding
} // namespace MoZuku
