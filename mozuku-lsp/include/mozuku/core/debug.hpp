#pragma once

#include <cstdlib>

namespace MoZuku::debug {

inline bool isEnabled() {
  static const bool enabled = std::getenv("MOZUKU_DEBUG") != nullptr;
  return enabled;
}

} // namespace MoZuku::debug
