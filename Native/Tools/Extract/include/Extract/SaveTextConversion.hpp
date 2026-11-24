#pragma once

#include <expected>
#include <filesystem>
#include <string>

namespace VS::Extract {
// Two MCMAN choices plus native-owned save status messages; only the first
// English font bank from TITLE, not the full module/SYSTEM image corpus.
[[nodiscard]] std::expected<void, std::string> ConvertSaveText(
    const std::filesystem::path& root, bool force);
}  // namespace VS::Extract
