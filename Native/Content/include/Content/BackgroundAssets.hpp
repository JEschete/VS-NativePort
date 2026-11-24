#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace VS::Content {

inline constexpr std::uint32_t kBackgroundGridSideBudget = 64;
inline constexpr std::uint32_t kBackgroundCellBudget = 1024;
inline constexpr std::uint32_t kBackgroundSmallCellUnits = 16;
inline constexpr std::uint32_t kBackgroundDefaultCellUnits = 32;
inline constexpr std::uint32_t kBackgroundLargeCellUnits = 64;
[[nodiscard]] constexpr bool IsBackgroundCellSize(
    std::uint32_t units) noexcept {
  return units == kBackgroundSmallCellUnits ||
         units == kBackgroundDefaultCellUnits ||
         units == kBackgroundLargeCellUnits;
}

// Artwork descriptions verified from the four imported images (#210).
// No exact room, narrative, or sunrise/sunset identity is implied.
struct BackgroundAssetIdentity {
  std::string_view source_stem;
  std::string_view slug;
  std::string_view display_name;
};

[[nodiscard]] std::span<const BackgroundAssetIdentity>
BackgroundAssets() noexcept;
[[nodiscard]] const BackgroundAssetIdentity* BackgroundAssetForSource(
    std::string_view source) noexcept;
[[nodiscard]] std::string BackgroundLayoutId(
    const BackgroundAssetIdentity& asset);
[[nodiscard]] std::string BackgroundImageId(
    const BackgroundAssetIdentity& asset);
[[nodiscard]] std::string LegacyBackgroundLayoutId(
    const BackgroundAssetIdentity& asset);
[[nodiscard]] std::string LegacyBackgroundImageId(
    const BackgroundAssetIdentity& asset);
[[nodiscard]] std::string BackgroundLayoutPath(
    const BackgroundAssetIdentity& asset);
[[nodiscard]] std::string BackgroundImageStemPath(
    const BackgroundAssetIdentity& asset);
// Only the four known legacy references change; custom identities stay intact.
[[nodiscard]] std::string CanonicalBackgroundReference(std::string_view id);
// The catalog entries still describe import-source files, not native resources.
[[nodiscard]] std::optional<std::string> BackgroundSourceAssetId(
    std::string_view source);

}  // namespace VS::Content
