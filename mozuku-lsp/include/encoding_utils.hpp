#pragma once

#include <string>

namespace MoZuku {
namespace encoding {

struct ConversionOptions {
  bool skipInvalidInput{false};
};

std::string convertEncoding(const std::string &input,
                            const std::string &fromCharset,
                            const std::string &toCharset = "UTF-8",
                            ConversionOptions options = {});

std::string systemToUtf8(const std::string &input,
                         const std::string &systemCharset);

std::string utf8ToSystem(const std::string &input,
                         const std::string &systemCharset);

std::string sanitizeUtf8(const std::string &input);

size_t utf8SequenceLength(unsigned char c);

} // namespace encoding
} // namespace MoZuku
