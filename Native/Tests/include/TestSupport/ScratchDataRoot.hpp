#pragma once

#include <chrono>
#include <filesystem>
#include <format>
#include <fstream>
#include <string_view>
#include <system_error>

#include <catch2/catch_test_macros.hpp>

#include "Content/DataStore.hpp"
#include "Platform/Paths.hpp"

namespace VS::test_support {
// A throwaway data root under build\<family>\ that is the active DataStore
// until it is destroyed, then restores the previous store and deletes itself.
class ScratchDataRoot {
 public:
  explicit ScratchDataRoot(std::string_view family)
      : root_(std::format(
            "build\\{}\\{}-{}", family,
            std::chrono::steady_clock::now().time_since_epoch().count(),
            sequence++)) {}
  // NOLINTNEXTLINE(bugprone-exception-escape): error_code cleanup.
  ~ScratchDataRoot() {
    restore_.Restore();
    std::error_code error;
    std::filesystem::remove_all(root_, error);
  }
  ScratchDataRoot(const ScratchDataRoot&) = delete;
  ScratchDataRoot& operator=(const ScratchDataRoot&) = delete;
  ScratchDataRoot(ScratchDataRoot&&) = delete;
  ScratchDataRoot& operator=(ScratchDataRoot&&) = delete;

  [[nodiscard]] const std::filesystem::path& Root() const { return root_; }
  void Write(std::string_view path, std::string_view text) const {
    const auto file = root_ / Platform::Utf8Path(path);
    std::filesystem::create_directories(file.parent_path());
    std::ofstream stream(file, std::ios::binary | std::ios::trunc);
    stream << text;
    REQUIRE(stream.good());
  }
  void Open() const {
    auto opened = Content::DataStore::Open(root_);
    if (!opened) FAIL(opened.error());
  }

 private:
  Content::DataStore::ScopedOverride restore_;
  std::filesystem::path root_;
  static inline unsigned sequence = 0;
};
}  // namespace VS::test_support
