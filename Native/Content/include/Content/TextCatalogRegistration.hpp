#pragma once

#include <expected>
#include <string>
#include <string_view>

namespace VS::Content {
inline constexpr std::string_view kNativeTextCatalogId{"text.catalog.en"};
inline constexpr std::string_view kNativeTextCatalogConverter{
    "text-catalog-json",
};
struct ConversionManifest;
[[nodiscard]] std::expected<void, std::string> CheckNativeTextCatalogOwnership(
    const ConversionManifest& manifest, std::string_view owner);
}  // namespace VS::Content
