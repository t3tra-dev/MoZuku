#include "wikipedia.hpp"

#include <chrono>
#include <cstring>
#include <curl/curl.h>
#include <future>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>

std::string URLEncode(const std::string &value) {
  CURL *curl = curl_easy_init();
  if (!curl) {
    return value;
  }
  char *output = curl_easy_escape(curl, value.c_str(), value.length());
  if (output) {
    std::string encoded(output);
    curl_free(output);
    curl_easy_cleanup(curl);
    return encoded;
  }
  curl_easy_cleanup(curl);
  return value;
}

size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp) {
  size_t totalSize = size * nmemb;
  std::string *buffer = static_cast<std::string *>(userp);
  buffer->append(static_cast<char *>(contents), totalSize);
  return totalSize;
}

struct AsyncRequest {
  CURL *easy_handle;
  std::string response_buffer;
  std::promise<wikipedia::FetchResult> promise;

  AsyncRequest() : easy_handle(nullptr) {}
  ~AsyncRequest() {
    if (easy_handle) {
      curl_easy_cleanup(easy_handle);
    }
  }
};

std::string parseWikipediaResponse(const std::string &response) {
  try {
    nlohmann::json jsonResponse = nlohmann::json::parse(response);
    if (jsonResponse.contains("query")) {
      const auto &queryData = jsonResponse["query"];
      if (queryData.contains("pages")) {
        const auto &pages = queryData["pages"];
        for (const auto &page : pages) {
          if (page.contains("extract")) {
            return page["extract"];
          }
        }
      }
    }
  } catch (const std::exception &e) {
    return "Error parsing response: " + std::string(e.what());
  }
  return "No summary available.";
}

std::string getErrorMessage(long response_code) {
  switch (response_code) {
  case -1:
    return "Network connection error";
  case 404:
    return "Page not found";
  case 403:
    return "Access forbidden";
  case 500:
    return "Internal server error";
  case 502:
    return "Bad gateway";
  case 503:
    return "Service unavailable";
  case 504:
    return "Gateway timeout";
  default:
    return "HTTP error: " + std::to_string(response_code);
  }
}

std::future<wikipedia::FetchResult>
performAsyncRequest(const std::string &url) {
  auto request = std::make_shared<AsyncRequest>();
  auto future = request->promise.get_future();

  // 別スレッドで非同期リクエストを実行
  std::thread([request, url]() {
    CURLM *multi_handle = curl_multi_init();
    if (!multi_handle) {
      request->promise.set_value(
          wikipedia::FetchResult(-1, "Failed to initialize curl multi handle"));
      return;
    }

    request->easy_handle = curl_easy_init();
    if (!request->easy_handle) {
      curl_multi_cleanup(multi_handle);
      request->promise.set_value(
          wikipedia::FetchResult(-1, "Failed to initialize curl easy handle"));
      return;
    }

    // cURLオプションの設定
    curl_easy_setopt(request->easy_handle, CURLOPT_URL, url.c_str());
    curl_easy_setopt(request->easy_handle, CURLOPT_WRITEFUNCTION,
                     WriteCallback);
    curl_easy_setopt(request->easy_handle, CURLOPT_WRITEDATA,
                     &request->response_buffer);
    curl_easy_setopt(request->easy_handle, CURLOPT_TIMEOUT, 5L);
    curl_easy_setopt(request->easy_handle, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(request->easy_handle, CURLOPT_CONNECTTIMEOUT, 3L);
    curl_easy_setopt(
        request->easy_handle, CURLOPT_USERAGENT,
        "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 "
        "(KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36");

    curl_multi_add_handle(multi_handle, request->easy_handle);

    int still_running = 0;
    do {
      CURLMcode mc = curl_multi_perform(multi_handle, &still_running);
      if (mc != CURLM_OK) {
        request->promise.set_value(
            wikipedia::FetchResult(-1, "curl_multi_perform failed"));
        break;
      }

      if (still_running) {
        // 100msの短い待機
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }
    } while (still_running > 0);

    // レスポンスコードをチェック
    long response_code;
    curl_easy_getinfo(request->easy_handle, CURLINFO_RESPONSE_CODE,
                      &response_code);

    // ネットワーク接続エラーやタイムアウトをチェック
    CURLcode curl_code;
    curl_code = curl_easy_getinfo(request->easy_handle, CURLINFO_RESPONSE_CODE,
                                  &response_code);

    // ネットワーク接続に関するエラーをチェック
    if (response_code == 0) {
      // レスポンスコードが0の場合はネットワーク接続エラーの可能性
      response_code = -1;
    }

    curl_multi_remove_handle(multi_handle, request->easy_handle);
    curl_multi_cleanup(multi_handle);

    if (response_code == 200) {
      std::string summary = parseWikipediaResponse(request->response_buffer);
      request->promise.set_value(
          wikipedia::FetchResult(response_code, summary));
    } else if (response_code == -1) {
      request->promise.set_value(
          wikipedia::FetchResult(response_code, "Network connection error"));
    } else {
      std::string errorMsg = getErrorMessage(response_code);
      request->promise.set_value(
          wikipedia::FetchResult(response_code, errorMsg));
    }
  }).detach();

  return future;
}

namespace wikipedia {

// WikipediaCache implementation
WikipediaCache &WikipediaCache::getInstance() {
  static WikipediaCache instance;
  return instance;
}

std::unique_ptr<CacheEntry> WikipediaCache::getEntry(const std::string &query) {
  std::lock_guard<std::mutex> lock(cache_mutex_);
  auto it = cache_.find(query);
  if (it != cache_.end()) {
    return std::make_unique<CacheEntry>(it->second);
  }
  return nullptr;
}

void WikipediaCache::setEntry(const std::string &query, long response_code,
                              const std::string &content, bool is_error) {
  std::lock_guard<std::mutex> lock(cache_mutex_);
  cache_[query] = CacheEntry(response_code, content, is_error);
}

void WikipediaCache::clear() {
  std::lock_guard<std::mutex> lock(cache_mutex_);
  cache_.clear();
}

size_t WikipediaCache::size() const {
  std::lock_guard<std::mutex> lock(cache_mutex_);
  return cache_.size();
}

std::string getJapaneseErrorMessage(long response_code) {
  switch (response_code) {
  case -1:
    return "Wikipediaからのサマリ取得に失敗しました";
  case 404:
    return "該当するサマリは存在しません";
  case 403:
    return "Wikipediaからのサマリ取得に失敗しました";
  case 500:
  case 502:
  case 503:
  case 504:
    return "Wikipediaからのサマリ取得に失敗しました";
  default:
    if (response_code >= 500) {
      return "Wikipediaからのサマリ取得に失敗しました";
    }
    return "該当するサマリは存在しません";
  }
}

std::future<FetchResult> fetchSummary(const std::string &query) {
  auto &cache = WikipediaCache::getInstance();
  auto cached_entry = cache.getEntry(query);

  if (cached_entry) {
    std::promise<FetchResult> promise;
    auto future = promise.get_future();
    promise.set_value(
        FetchResult(cached_entry->response_code, cached_entry->content));
    return future;
  }

  std::string encodedQuery = URLEncode(query);
  std::string url = "https://ja.wikipedia.org/w/"
                    "api.php?format=json&action=query&prop=extracts&exintro&"
                    "explaintext&redirects=1&titles=" +
                    encodedQuery;

  auto future = performAsyncRequest(url);

  return std::async(
      std::launch::async, [query, future = std::move(future)]() mutable {
        auto result = future.get();

        auto &cache = WikipediaCache::getInstance();
        bool is_error = (result.response_code != 200);
        cache.setEntry(query, result.response_code, result.content, is_error);

        return result;
      });
}

} // namespace wikipedia
