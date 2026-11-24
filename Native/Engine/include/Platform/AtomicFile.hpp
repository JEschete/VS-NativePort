#pragma once

#include <expected>
#include <filesystem>
#include <string>
#include <string_view>

namespace VS::Platform {

// Writes a private sibling temporary file, closes it, then replaces the target
// with one rename. Failure leaves the existing target intact.
[[nodiscard]] std::expected<void, std::string> WriteFileAtomically(
    const std::filesystem::path& path, std::string_view contents);

}  // namespace VS::Platform
