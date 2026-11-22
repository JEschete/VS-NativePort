#pragma once

#include <cstdint>
#include <expected>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "Content/ConversionManifest.hpp"
#include "Content/DataStore.hpp"
#include "Content/ImageFiles.hpp"
#include "Content/NativeImage.hpp"
#include "Extract/ImageConversion.hpp"

// The fbt-texture-png converter (ASSET_PIPELINE.md §6.4, #214): each effect
// texture page (EFFECT\*.FBT, 64 x 256 VRAM halfwords) becomes an indexed
// native image under textures\effect. EFFPURGE.BIN names the one effect that
// loads a page and its slot (VRAM texture page 24 + slot); the descriptors of
// that effect's package fix the depth, and the row of the effect's FBC they
// sample most gives the PNG its preview colours. The runtime uploads the
// indices and draws them with the FBC rows, so the preview palette is not
// authoritative. Pages with the same bytes and depth convert once: the first
// by disc path holds the files and the others become manifest aliases of it,
// so editing it changes every copy.
namespace VS::Extract {

inline constexpr std::string_view kFbtTextureConverterName = "fbt-texture-png";
inline constexpr std::uint32_t kFbtTextureConverterVersion = 1;

// One effect texture page and what fixes its image.
struct FbtConversion {
  SourceFile texture;
  std::uint32_t effect = 0;
  std::uint32_t slot = 0;
  // The depth its effect's descriptors sample it at; nothing when none does,
  // and then it converts at 8 bits.
  std::optional<Content::ImageDepth> depth;
  SourceFile palette;  // the effect's FBC
  std::uint32_t palette_row = 0;
  std::uint32_t palette_entry = 0;
  std::vector<Content::ImageDependency> dependencies;  // sorted by source
};

// The manifest entry of `conversion` without its files.
[[nodiscard]] std::expected<Content::ConversionOutput, std::string> FbtOutputOf(
    const FbtConversion& conversion);

// The image files of one page and its manifest entry. The page must be 32 KB,
// the preview palette row must lie in the FBC, and the indices must rebuild
// every source halfword.
[[nodiscard]] std::expected<ConvertedImage, std::string> ConvertFbtTexture(
    const FbtConversion& conversion);

// Reads one disc file by its '/'-separated path.
using SourceRead = std::expected<SourceFile, std::string>;
using SourceReader = std::function<SourceRead(std::string_view)>;

struct FbtConversionPlan {
  std::vector<FbtConversion> conversions;  // the first page of each content
  std::vector<Content::ConversionOutput> aliases;  // every other page
};

// The conversions of every FBT `entries` lists, from EFFECT/EFFPURGE.BIN and
// the packages and FBCs it binds them to. Nothing when no FBT is listed. Every
// FBT must be loaded by exactly one effect, sampled at one indexed depth, and
// sampled only on rows its effect's FBC holds.
[[nodiscard]] std::expected<FbtConversionPlan, std::string> PlanFbtConversions(
    std::span<const Content::DataStore::Entry> entries,
    const SourceReader& read);

}  // namespace VS::Extract
