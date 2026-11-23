#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "Content/ImageFiles.hpp"

// The OBJ\E*.ETM actor effect texture sheets, decoded strictly for the asset
// pipeline (ASSET_PIPELINE.md §6.2.3, #207). The game loads the sheet whose
// table entry is the model's set number (ActorAssets.cpp UploadEffectTexture)
// into the 64-halfword-wide column of the actor's texture slot, from VRAM row
// 256, and each model the paired ESQ set gives a texture row samples its
// region from that row at its own texture depth.
//
// A file is a marker halfword ("E0", which the game never reads), the height
// in lines, then the lines of 64 halfwords each, with nothing after them.
namespace VS::Content {

inline constexpr std::string_view kEtmExtension = ".ETM";
inline constexpr std::string_view kEtmTextureConverterName = "etm-png";
inline constexpr std::uint32_t kEtmTextureConverterVersion = 1;
inline constexpr std::string_view kActorTextureDirectory = "textures/actors";
// The role of the dependency that names the model drawing a region (its
// OBJ\*.SHP), whose texture depth the region must keep.
inline constexpr std::string_view kEtmRegionModelRole = "texture";
inline constexpr std::uint16_t kEtmMarker = 0x3045;
inline constexpr std::size_t kEtmHeaderBytes = 4;
inline constexpr std::size_t kEtmLineWords = 64;
inline constexpr std::size_t kEtmLineBytes = kEtmLineWords * 2;
// The sheet uploads from VRAM row 256, so it holds at most the 256 rows below
// it.
inline constexpr std::size_t kEtmMaxHeight = 256;

struct EtmSheet {
  std::vector<std::uint16_t> words;  // the lines, kEtmLineWords each

  [[nodiscard]] std::size_t Height() const noexcept {
    return words.size() / kEtmLineWords;
  }
  bool operator==(const EtmSheet&) const = default;
};

// Decodes a whole ETM file: the marker, a height of 1 to kEtmMaxHeight lines
// and exactly that many lines.
[[nodiscard]] std::expected<EtmSheet, std::string> DecodeEtmFile(
    std::span<const std::byte> data);
// The file `sheet` is, with the marker. A sheet DecodeEtmFile returned
// rebuilds byte for byte. Errors name what the file cannot hold.
[[nodiscard]] std::expected<std::vector<std::byte>, std::string> EncodeEtmFile(
    const EtmSheet& sheet);

// The sheet the region images of `source` (an ETM's disc path) make together:
// each an indexed image one sheet line wide at its depth, whose provenance
// offset is where its first line starts in the file, the regions following
// one another from the first line. Errors name the image at fault.
[[nodiscard]] std::expected<EtmSheet, std::string> EtmSheetOfImages(
    std::span<const ImageDocument> images, std::string_view source);

}  // namespace VS::Content
