#pragma once

#include <string>

namespace MoZuku {
namespace encoding {

std::string convertEncoding(const std::string &input,
                            const std::string &fromCharset,
                            const std::string &toCharset = "UTF-8");

std::string systemToUtf8(const std::string &input,
                         const std::string &systemCharset);

std::string utf8ToSystem(const std::string &input,
                         const std::string &systemCharset);

} // namespace encoding
} // namespace MoZuku
