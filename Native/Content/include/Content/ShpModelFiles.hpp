#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "Content/ActorAnimation.hpp"
#include "Content/ImageFiles.hpp"
#include "Content/JsonText.hpp"
#include "Content/NativeImage.hpp"
#include "Content/ShpFile.hpp"

// The native files of one character model (ASSET_PIPELINE.md §6.2, #203),
// converted from OBJ\<NN>.SHP into models/characters/:
//   <NN>.gltf   glTF 2.0: the skinned mesh, one node per bone and the VS
//               payload in asset.extras.vs (schema vs.shp-model)
//   <NN>.bin    the buffer: geometry, inverse bind matrices, and the sound
//               bytes the payload names by buffer view
//   <NN>.png    the model's own texture as a native image (§6.4), with its
//               .image.json and .palettes.png; a model that borrows a texture
//               shows its lender's <LL>.png instead
// Positions are the SHP's integer units with each bone's rest origin added,
// under a root node turned 180 degrees about X (the PlayStation's Y points
// down). Each polygon corner is a glTF vertex whose _VS_VERTEX attribute is
// its SHP vertex number and whose JOINTS_0 is the vertex's bone; each
// primitive's extras.vs.polygons lists its polygons in triangle order as
// [ordinal, corners, flags, extra]. Every value the game reads that glTF
// cannot express exactly is in the payload, which is authoritative with the
// geometry: the reader rebuilds the SHP from them, and ignores the materials,
// textures and images, which only preview the polygon flags. The payload
// names the 0xE8-byte layout block's fields (the actor record's texture
// overlays through reserved6d4, ActorRecord.hpp), so collision, shadow and
// texture animation values edit as numbers. The animation banks are JSON in
// the payload (ActorAnimation.hpp, §6.2.1): "sequence" is the SHP's embedded
// bank in the vs.actor-animation bank form (null when it has none), and
// "sequences" holds each OBJ\*.SEQ file the model owns as a whole
// vs.actor-animation document, sorted by source. Banks with event scripts need
// the game's event stride table (SeqFile.hpp kSeqEventLengthsAt) to read and
// rebuild.
namespace VS::Content {

inline constexpr std::string_view kShpModelConverterName = "shp-gltf";
inline constexpr std::uint32_t kShpModelConverterVersion = 3;
inline constexpr std::string_view kShpModelSchema = "vs.shp-model";
inline constexpr std::uint32_t kShpModelSchemaVersion = 3;
inline constexpr std::string_view kShpModelDirectory = "models/characters";
inline constexpr std::string_view kShpExtension = ".SHP";
// Each OBJ\*.SEQ bank's own output, which lists its owner's glTF and buffer.
inline constexpr std::string_view kSeqAnimationConverterName = "seq-gltf";
inline constexpr std::uint32_t kSeqAnimationConverterVersion = 2;
inline constexpr std::string_view kSeqExtension = ".SEQ";
inline constexpr std::string_view kGltfSuffix = ".gltf";
inline constexpr std::string_view kGltfBufferSuffix = ".bin";
// The custom attribute holding each glTF vertex's SHP vertex number.
inline constexpr std::string_view kShpVertexAttribute = "_VS_VERTEX";
// The extras member holding the payload (asset.extras.vs) and each
// primitive's polygons (extras.vs.polygons).
inline constexpr std::string_view kShpExtrasKey = "vs";
inline constexpr std::string_view kShpPolygonsKey = "polygons";
// The polygon flags a primitive groups by: no backface culling (0x01),
// semi-transparency (0x10) and its blend mode (0x60).
inline constexpr std::uint8_t kShpPrimitiveFlags = 0x71;
inline constexpr std::uint8_t kShpDoubleSidedFlag = 0x01;
inline constexpr std::uint8_t kShpSemiTransparentFlag = 0x10;
// glTF accessor component types and the triangle primitive mode.
inline constexpr std::uint32_t kGltfUnsignedByte = 5121;
inline constexpr std::uint32_t kGltfUnsignedShort = 5123;
inline constexpr std::uint32_t kGltfUnsignedInt = 5125;
inline constexpr std::uint32_t kGltfFloat = 5126;
inline constexpr std::uint32_t kGltfTriangles = 4;
// The corner each index of a polygon's triangles holds: a triangle is written
// (c0, c2, c1) and a quad (c0, c2, c1), (c1, c2, c3).
inline constexpr std::array kShpIndexCorners{
    std::size_t{0}, std::size_t{2}, std::size_t{1},
    std::size_t{1}, std::size_t{2}, std::size_t{3},
};
// A 4 x 4 matrix, column-major, and where its X translation is.
inline constexpr std::size_t kShpMatrixElements = 16;
inline constexpr std::size_t kShpMatrixTranslationX = 12;

// The UV slot of a polygon's corner `corner` (ShpPolygon::uvs).
[[nodiscard]] constexpr std::size_t ShpUvSlot(std::uint8_t corners,
                                              std::size_t corner) noexcept {
  return corners == kShpTriangleCorners ? (corner + 1) % kShpTriangleCorners
                                        : corner;
}

struct ShpTextureSize {
  std::uint32_t width = 0;  // pixels
  std::uint32_t height = 0;

  bool operator==(const ShpTextureSize&) const = default;
};

struct ShpModelDocument {
  std::string id;      // e.g. model.obj.4f
  std::string source;  // the disc path, e.g. OBJ/4F.SHP
  std::string source_sha256;
  std::string image_id;  // the own texture's image id; empty when borrowed
  // A borrowed texture's size, which scales the UVs (the lender's file has
  // the texture itself).
  std::optional<ShpTextureSize> lender_texture;
  ShpFile shp;
  // The OBJ\*.SEQ banks the model owns, sorted by source, each for the
  // model's bones.
  std::vector<ActorAnimationDocument> sequences;

  bool operator==(const ShpModelDocument&) const = default;
};

// The size the texture coordinates are divided by: the own texture's, or the
// lender's.
[[nodiscard]] std::optional<ShpTextureSize> ShpUvScale(
    const ShpModelDocument& document);

// The glTF nodes: the root (turned 180 degrees about X), the mesh, then one
// node per bone in bone order.
inline constexpr std::size_t kShpRootNode = 0;
inline constexpr std::size_t kShpMeshNode = 1;
inline constexpr std::size_t kShpFirstBoneNode = 2;

// Each bone's rest origin along X: bone 0 at 0, every other bone at its
// parent's origin moved by -length[parent]. Every bone after the first must
// have a lower-numbered parent (bone 0's parent byte is not used).
[[nodiscard]] std::expected<std::vector<std::int64_t>, std::string>
ShpBoneOrigins(const ShpModel& model);

// The glTF members that hold the skeleton, "scene", "scenes", "nodes" and
// "skins", with the inverse bind matrices in accessor
// `inverse_bind_matrices`. The bones must have valid origins (ShpBoneOrigins).
[[nodiscard]] JsonDocument ShpSkeletonGltf(const ShpModel& model,
                                           std::size_t inverse_bind_matrices);
// The inverse bind matrix of a bone at `origin`: a move by -origin along X.
// The root's turn applies on top, so it is not inverted here.
[[nodiscard]] std::array<float, kShpMatrixElements> ShpInverseBindMatrix(
    std::int64_t origin);

// The texture as a native image: 8-bit indices are 2 x width_byte pixels
// wide, 4-bit ones 4 x width_byte. Its two palette rows are padded with 0x0000
// to every index the pixels use, which the game reads past the palette.
[[nodiscard]] NativeImage ShpTextureImage(const ShpTexture& texture);
// The texture `image` holds with `palette_words` palette words per row; the
// error names what an SHP texture cannot hold.
[[nodiscard]] std::expected<ShpTexture, std::string> ShpTextureOf(
    const NativeImage& image, std::uint8_t palette_words);

// The layout block as named fields (texture_overlays through reserved6d4),
// and back; every bit of the block is in exactly one field. The error names
// the member at fault.
using ShpLayoutBlock = std::array<std::byte, kShpLayoutBlockBytes>;
[[nodiscard]] JsonDocument ShpLayoutJson(const ShpLayoutBlock& layout);
[[nodiscard]] std::expected<ShpLayoutBlock, std::string> ShpLayoutOf(
    const JsonDocument& layout);

// The embedded SEQ bank in the vs.actor-animation bank form, or null when the
// model has none. `event_lengths` is the game's event stride table; the error
// names what the bank cannot hold.
[[nodiscard]] std::expected<JsonDocument, std::string> ShpSequenceJson(
    const ShpFile& shp, std::span<const std::uint8_t> event_lengths);

// The payload asset.extras.vs holds for `document`, naming the buffer view
// that holds its sound (nothing when it is empty) and holding `sequence`
// (ShpSequenceJson) and the owned banks. The writer and reader share it, so
// the schema has one definition.
[[nodiscard]] JsonDocument ShpModelPayload(
    const ShpModelDocument& document, std::optional<std::size_t> sound_view,
    JsonDocument sequence);

// --- Shared by every model format whose glTF holds an ShpModel (SHP, WEP).

// A buffer view of a model's glTF buffer.
struct ModelGltfView {
  std::size_t offset = 0;
  std::size_t length = 0;
  std::size_t stride = 0;  // 0: tightly packed
};

// A model's <stem>.gltf and the <stem>.bin buffer it names.
struct ModelGltfFile {
  std::string name;  // <stem>.gltf
  JsonDocument json;
  std::vector<std::byte> buffer;
  std::vector<ModelGltfView> views;
};

// `stem`'s glTF and buffer: glTF 2.0 with only the members a model writes
// and exactly one buffer, <stem>.bin.
[[nodiscard]] std::expected<ModelGltfFile, std::string> ReadModelGltf(
    std::string_view stem, const ImageFileReader& read);
// The payload, asset.extras.vs, which must be asset.extras' only member.
[[nodiscard]] std::expected<JsonMembers, std::string> ModelGltfPayload(
    const ModelGltfFile& file);

// The members every model payload starts with.
struct ModelIdentity {
  std::string id;      // e.g. model.obj.4f
  std::string source;  // the disc path
  std::string source_sha256;

  bool operator==(const ModelIdentity&) const = default;
};

// The payload's schema, schema_version, id, source and source_sha256; the
// schema must be `schema` at `version`. `kind` names the model in the error
// ("a character model").
[[nodiscard]] std::expected<ModelIdentity, std::string> ReadModelIdentity(
    const JsonMembers& payload, std::string_view schema, std::uint32_t version,
    std::string_view kind);

// The polygons of each part (triangles, quads, mixed).
using ShpPolygonCounts = std::array<std::uint16_t, kShpPartCount>;

// The payload members polygon_counts, limb_ranges and bones, which every
// model payload has, and back.
[[nodiscard]] JsonDocument ShpPolygonCountsJson(const ShpModel& model);
[[nodiscard]] JsonDocument ShpBonesJson(const ShpModel& model);
// Reads limb_ranges and bones into `model` and returns polygon_counts.
[[nodiscard]] std::expected<ShpPolygonCounts, std::string> ReadShpModelHeader(
    const JsonMembers& payload, ShpModel& model);

// What a model's mesh is read against.
struct ModelMeshSpec {
  ShpPolygonCounts counts{};
  ShpTextureSize uv_scale;  // what the texture coordinates are divided by
  // The skeleton members the payload gives (ShpSkeletonGltf), with the
  // inverse bind matrices in the accessor it is passed.
  std::function<JsonDocument(std::size_t)> skeleton;
};

// Reads the glTF's mesh into `model`'s groups, vertices and parts; its bones
// and polygon layout must already be read. The skeleton members must be the
// ones `spec` gives, numbers within a float's precision, and the inverse bind
// matrices exactly ShpInverseBindMatrix's.
[[nodiscard]] std::expected<void, std::string> ReadModelMesh(
    const ModelGltfFile& file, const ModelMeshSpec& spec, ShpModel& model);

// `stem`'s image files, which must be image `image_id` converted from
// `model`'s source.
[[nodiscard]] std::expected<ImageDocument, std::string> ReadModelImage(
    std::string_view stem, const ImageFileReader& read,
    const ModelIdentity& model, std::string_view image_id);

// The document `stem`'s files hold, checked against everything the payload
// records; every bank must rebuild with `event_lengths` (the game's event
// stride table). Errors name the file and the member or vertex at fault.
[[nodiscard]] std::expected<ShpModelDocument, std::string> DecodeShpModelFiles(
    std::string_view stem, const ImageFileReader& read,
    std::span<const std::uint8_t> event_lengths);
// DecodeShpModelFiles over the files in `directory`.
[[nodiscard]] std::expected<ShpModelDocument, std::string> LoadShpModelFiles(
    const std::filesystem::path& directory, std::string_view stem,
    std::span<const std::uint8_t> event_lengths);

}  // namespace VS::Content
