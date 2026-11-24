#include "Content/BackgroundAssets.hpp"

#include <array>
#include <format>
#include <string>

namespace VS::Content {
namespace {

constexpr auto kAssets = std::to_array<BackgroundAssetIdentity>({
    {"001OP01A", "castle-silhouette", "Castle silhouette"},
    {"002OP01A", "dark-clouds", "Dark cloud sheet"},
    {"007OP01A", "sun-on-horizon", "Sun on the clouded horizon"},
    {"008OP01A", "grey-clouds", "Grey cloud sheet"},
});

std::string Lower(std::string_view text) {
  std::string result(text);
  for (char& letter : result) {
    if (letter >= 'A' && letter <= 'Z') {
      letter = static_cast<char>(letter - 'A' + 'a');
    }
  }
  return result;
}

}  // namespace

std::span<const BackgroundAssetIdentity> BackgroundAssets() noexcept {
  return kAssets;
}

const BackgroundAssetIdentity* BackgroundAssetForSource(
    std::string_view source) noexcept {
  constexpr std::string_view kDirectory = "BG/";
  constexpr std::string_view kLayoutExtension = ".FAR";
  constexpr std::string_view kImageExtension = ".TIM";
  if (!source.starts_with(kDirectory) ||
      (!source.ends_with(kLayoutExtension) &&
       !source.ends_with(kImageExtension)) ||
      source.size() < kDirectory.size() + kLayoutExtension.size()) {
    return nullptr;
  }
  const auto stem =
      source.substr(kDirectory.size(), source.size() - kDirectory.size() -
                                           kLayoutExtension.size());
  for (const auto& asset : kAssets) {
    if (stem == asset.source_stem) return &asset;
  }
  return nullptr;
}

std::string BackgroundLayoutId(const BackgroundAssetIdentity& asset) {
  return std::format("background.{}", asset.slug);
}

std::string BackgroundImageId(const BackgroundAssetIdentity& asset) {
  return std::format("image.bg.{}", asset.slug);
}

std::string LegacyBackgroundLayoutId(const BackgroundAssetIdentity& asset) {
  return std::format("background.import.bg.{}", Lower(asset.source_stem));
}

std::string LegacyBackgroundImageId(const BackgroundAssetIdentity& asset) {
  return std::format("image.bg.{}", Lower(asset.source_stem));
}

std::string BackgroundLayoutPath(const BackgroundAssetIdentity& asset) {
  return std::format("backgrounds/{}.background.json", asset.slug);
}

std::string BackgroundImageStemPath(const BackgroundAssetIdentity& asset) {
  return std::format("textures/bg/{}", asset.slug);
}

std::string CanonicalBackgroundReference(std::string_view id) {
  for (const auto& asset : kAssets) {
    if (id == LegacyBackgroundLayoutId(asset)) return BackgroundLayoutId(asset);
    if (id == LegacyBackgroundImageId(asset)) return BackgroundImageId(asset);
  }
  return std::string(id);
}

std::optional<std::string> BackgroundSourceAssetId(std::string_view source) {
  const auto* asset = BackgroundAssetForSource(source);
  if (asset == nullptr) return std::nullopt;
  return std::format("asset.background-source.{}.{}", asset->slug,
                     source.ends_with(".FAR") ? "layout" : "image");
}

}  // namespace VS::Content
