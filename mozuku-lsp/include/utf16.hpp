#pragma once

#include "lsp.hpp"

std::vector<size_t> computeLineStarts(const std::string &text);

Position byteOffsetToPosition(const std::string &text,
                              const std::vector<size_t> &lineStarts,
                              size_t offset);

size_t utf8ToUtf16Length(const std::string &utf8Str);
