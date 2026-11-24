#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "Content/ConvertedImages.hpp"
#include "Content/ImageFiles.hpp"

namespace VS::Content {

inline constexpr std::string_view kFarConverterName = "far-layout-json";
inline constexpr std::uint32_t kFarConverterVersion = 1;
inline constexpr auto kBackgroundLayoutSchema = "background-layout.schema.json";
inline constexpr std::uint32_t kBackgroundLayoutVersion = 1;
inline constexpr std::uint32_t kNeutralBackgroundColor = 128;
inline constexpr std::size_t kBackgroundCornerCount = 4;
inline constexpr std::size_t kUnusedBackgroundCornerBytes = 3;

struct BackgroundColor {
  std::uint32_t red = kNeutralBackgroundColor;
  std::uint32_t green = kNeutralBackgroundColor;
  std::uint32_t blue = kNeutralBackgroundColor;
  bool operator==(const BackgroundColor&) const = default;
};

struct BackgroundCell {
  // Inclusive sampled endpoints, not a positive pixel extent. Reversed
  // endpoints mirror the image; equal endpoints on both axes disable a cell.
  std::int32_t left = 0;
  std::int32_t top = 0;
  std::int32_t right = 0;
  std::int32_t bottom = 0;
  // Top-left, top-right, bottom-left, bottom-right; 128 is neutral modulation.
  std::array<BackgroundColor, kBackgroundCornerCount> colors{};
  // CVECTOR.cd for corners 0, 2, 3 is not read by the original renderer.
  std::array<std::uint8_t, kUnusedBackgroundCornerBytes> unused_corner_bytes{};
  bool operator==(const BackgroundCell&) const = default;
};

struct BackgroundLayoutDocument {
  std::string id;
  std::string image_id;
  std::string source;
  std::string source_sha256;
  std::string image_source;
  std::string image_source_sha256;
  bool unreferenced = false;
  std::uint32_t columns = 0;
  std::uint32_t rows = 0;
  std::uint32_t unused_header_word = 0;
  std::vector<BackgroundCell> cells{};  // row-major
  bool operator==(const BackgroundLayoutDocument&) const = default;
};

// Bounded 8-bit FAR import. Other consumed texture-page modes are refused.
[[nodiscard]] std::expected<BackgroundLayoutDocument, std::string> DecodeFar(
    std::span<const std::byte> bytes, BackgroundLayoutDocument identity);
[[nodiscard]] std::expected<std::vector<std::byte>, std::string> EncodeFar(
    const BackgroundLayoutDocument& layout);
[[nodiscard]] std::expected<void, std::string> ValidateBackgroundLayout(
    const BackgroundLayoutDocument& layout);
// Sampling/material checks; LoadBackgroundLayoutImage also validates identity
// and source dependencies against the manifest.
[[nodiscard]] std::expected<void, std::string> ValidateBackgroundImage(
    const BackgroundLayoutDocument& layout, const ImageDocument& image);
// Resolves the authored image_id, then checks its manifest/source dependencies,
// material, dimensions and sampled endpoints. Import image_source is not a
// veto.
[[nodiscard]] std::expected<ImageDocument, std::string>
LoadBackgroundLayoutImage(const std::filesystem::path& root,
                          const ConversionManifest& manifest,
                          const BackgroundLayoutDocument& layout);
[[nodiscard]] std::expected<std::string, std::string> WriteBackgroundLayout(
    const BackgroundLayoutDocument& layout);
[[nodiscard]] std::expected<BackgroundLayoutDocument, std::string>
ReadBackgroundLayout(std::span<const std::byte> bytes, std::string_view name);
// Resolves the room's compatibility source once; all drawing uses native cells.
[[nodiscard]] std::expected<BackgroundLayoutDocument, std::string>
LoadConvertedBackground(const DataStore& store, const ConvertedSource& source);
}  // namespace VS::Content
