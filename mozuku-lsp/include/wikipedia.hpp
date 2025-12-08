#pragma once

#include <curl/curl.h>
#include <future>
#include <mutex>
#include <string>
#include <unordered_map>

namespace wikipedia {
struct FetchResult {
  long response_code;
  std::string content;
  bool success;

  FetchResult(long code, const std::string &data)
      : response_code(code), content(data), success(code == 200) {}
};

struct CacheEntry {
  long response_code;
  std::string content;
  bool is_error;

  CacheEntry() : response_code(0), content(""), is_error(false) {}
  CacheEntry(long code, const std::string &data, bool error = false)
      : response_code(code), content(data), is_error(error) {}
};

class WikipediaCache {
public:
  static WikipediaCache &getInstance();

  std::unique_ptr<CacheEntry> getEntry(const std::string &query);
  void setEntry(const std::string &query, long response_code,
                const std::string &content, bool is_error = false);
  void clear();
  size_t size() const;

private:
  WikipediaCache() = default;
  mutable std::mutex cache_mutex_;
  std::unordered_map<std::string, CacheEntry> cache_;
};

std::future<FetchResult> fetchSummary(const std::string &query);

std::string getJapaneseErrorMessage(long response_code);

} // namespace wikipedia
