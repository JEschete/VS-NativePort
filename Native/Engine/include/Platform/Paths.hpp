#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace VS::Platform {

inline std::string PathText(const std::filesystem::path& path) {
  const auto text = path.u8string();
  return {text.begin(), text.end()};
}

inline std::filesystem::path Utf8Path(std::string_view text) {
  return std::filesystem::path(std::u8string(text.begin(), text.end()));
}

}  // namespace VS::Platform
