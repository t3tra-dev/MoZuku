#include "encoding_utils.hpp"
#include <cstring>
#include <iconv.h>

namespace MoZuku {
namespace encoding {

std::string convertEncoding(const std::string &input,
                            const std::string &fromCharset,
                            const std::string &toCharset) {
  if (input.empty())
    return input;

  iconv_t cd = iconv_open(toCharset.c_str(), fromCharset.c_str());
  if (cd == (iconv_t)-1) {
    return input;
  }

  size_t inBytesLeft = input.size();
  size_t outBytesLeft = input.size() * 4; // Conservative estimate

  std::string result(outBytesLeft, '\0');

  char *inBuf = const_cast<char *>(input.data());
  char *outBuf = &result[0];

  if (iconv(cd, &inBuf, &inBytesLeft, &outBuf, &outBytesLeft) == (size_t)-1) {
    iconv_close(cd);
    return input;
  }

  iconv_close(cd);

  // Resize result to actual converted size
  result.resize(result.size() - outBytesLeft);
  return result;
}

std::string systemToUtf8(const std::string &input,
                         const std::string &systemCharset) {
  if (systemCharset == "UTF-8" || systemCharset.empty()) {
    return input;
  }
  return convertEncoding(input, systemCharset, "UTF-8");
}

std::string utf8ToSystem(const std::string &input,
                         const std::string &systemCharset) {
  if (systemCharset == "UTF-8" || systemCharset.empty()) {
    return input;
  }
  return convertEncoding(input, "UTF-8", systemCharset);
}

} // namespace encoding
} // namespace MoZuku
