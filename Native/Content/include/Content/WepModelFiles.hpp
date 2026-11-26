#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <string>
#include <string_view>

#include "Content/ImageFiles.hpp"
#include "Content/JsonText.hpp"
#include "Content/NativeImage.hpp"
#include "Content/WepFile.hpp"

// The native files of one weapon model (ASSET_PIPELINE.md §6.2.4, #204),
// converted from OBJ\<NN>.WEP into models/weapons/:
//   <NN>.gltf   glTF 2.0 laid out as a character model's (ShpModelFiles.hpp),
//               with the payload in asset.extras.vs (schema vs.wep-model)
//   <NN>.bin    the buffer: geometry and inverse bind matrices
//   <NN>.png    the texture as an 8-bit native image (§6.4) 32 rows tall,
//               with its .image.json and .palettes.png: one palette row per
//               blade material, each the handle's colours then the
//               material's, in kWepMaterialNames order
//   <NN>.<material>.preview.png  the texture through each other material's
//               row, which the glTF's KHR_materials_variants variants show
//               (the first variant, iron, shows <NN>.png). The game and the
//               reader ignore them: they are derived from .palettes.png and
//               go stale when it is edited until the weapon is converted
//               again.
// Each bone node after the first also turns by the bone's rest rotation, as
// the game draws the weapon (bone 0's rotation is unused), so the glTF shows
// the drawn pose. The payload's bone_rotations hold the rotations the file
// stores, which the reader checks the nodes against. As for a character
// model, the payload is authoritative with the geometry and the reader
// rebuilds the WEP from them.
namespace VS::Content {

inline constexpr std::string_view kWepModelConverterName = "wep-gltf";
inline constexpr std::uint32_t kWepModelConverterVersion = 2;
inline constexpr std::string_view kWepModelSchema = "vs.wep-model";
inline constexpr std::uint32_t kWepModelSchemaVersion = 2;
inline constexpr std::string_view kWepModelDirectory = "models/weapons";
// What a material preview's stem adds after <NN>.<material>.
inline constexpr std::string_view kWepPreviewSuffix = ".preview";
// What a weapon's model and image ids add to the ids its disc path gives,
// which the character model of the same number already has
// (OBJ/01.WEP -> model.obj.01.wep and image.obj.01.wep).
inline constexpr std::string_view kWepIdSuffix = ".wep";

struct WepModelDocument {
  std::string id;      // e.g. model.obj.01.wep
  std::string source;  // the disc path, e.g. OBJ/01.WEP
  std::string source_sha256;
  std::string image_id;  // e.g. image.obj.01.wep
  WepFile wep;

  bool operator==(const WepModelDocument&) const = default;
};

// ShpSkeletonGltf's members with each bone after the first turned by its
// rest rotation: the game's Z, then Y, then X Euler turn of the angles' low
// 12 bits, as a unit quaternion with a non-negative w.
[[nodiscard]] JsonDocument WepSkeletonGltf(const WepFile& wep,
                                           std::size_t inverse_bind_matrices);

// The texture as a native image with one palette row per material. Each row
// is padded with 0x0000 to every index the pixels use: some shields use
// indices past the palette the game uploads, where it reads whatever VRAM
// holds.
[[nodiscard]] NativeImage WepTextureImage(const WepTexture& texture);
// The texture `image` holds with `palette_words` palette words per row; the
// handle's colours must be the same in every row. The error names what a WEP
// texture cannot hold.
[[nodiscard]] std::expected<WepTexture, std::string> WepTextureOf(
    const NativeImage& image, std::uint8_t palette_words);

// The payload asset.extras.vs holds for `document`. The writer and reader
// share it, so the schema has one definition.
[[nodiscard]] JsonDocument WepModelPayload(const WepModelDocument& document);

// The document `stem`'s files hold, checked against everything the payload
// records. Errors name the file and the member or vertex at fault.
[[nodiscard]] std::expected<WepModelDocument, std::string> DecodeWepModelFiles(
    std::string_view stem, const ImageFileReader& read);
// DecodeWepModelFiles over the files in `directory`.
[[nodiscard]] std::expected<WepModelDocument, std::string> LoadWepModelFiles(
    const std::filesystem::path& directory, std::string_view stem);

}  // namespace VS::Content
