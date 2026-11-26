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
// primitive's extras.vs.polygons lists its polygons in triangle order, each
// by its place in the SHP's polygon order and its corners, with a styled
// polygon's drawing style (ShpPolygonEntryJson). Every value the game reads
// that glTF cannot express exactly is in the payload, which is authoritative
// with the geometry: the reader rebuilds the SHP from them, and ignores the
// materials, textures and images, which only preview the drawing styles. The
// payload names each part's polygon count and how many of each part's
// polygons belong to each body part (ShpBodyPartPolygonsJson), and the
// 0xE8-byte layout block's fields (the actor record's texture overlays
// through reserved6d4, ActorRecord.hpp), so collision, shadow and texture
// animation values edit as numbers. The animation banks are JSON in
// the payload (ActorAnimation.hpp, §6.2.1): "sequence" is the SHP's embedded
// bank in the vs.actor-animation bank form (null when it has none), and
// "sequences" holds each OBJ\*.SEQ file the model owns as an {id, animation}
// entry sorted by source. "sequence_bank_ids" maps the battle and special
// layout slots to those stable IDs or null. Banks with event scripts need the
// game's event stride table (SeqFile.hpp kSeqEventLengthsAt) to read and
// rebuild.
namespace VS::Content {

inline constexpr std::string_view kShpModelConverterName = "shp-gltf";
inline constexpr std::uint32_t kShpModelConverterVersion = 5;
inline constexpr std::string_view kShpModelSchema = "vs.shp-model";
inline constexpr std::uint32_t kShpModelSchemaVersion = 5;
inline constexpr std::string_view kShpModelDirectory = "models/characters";
inline constexpr std::string_view kShpExtension = ".SHP";
inline constexpr std::size_t kShpBattleSequenceCount = 12;
inline constexpr std::size_t kShpSpecialSequenceCount = 4;
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
// The flags byte every triangle- and quad-part polygon on the disc holds; the
// game does not read it.
inline constexpr std::uint8_t kShpPlainPartFlags = 4;
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

struct ShpSequenceBankReferences {
  std::array<std::optional<std::string>, kShpBattleSequenceCount> battle{};
  std::array<std::optional<std::string>, kShpSpecialSequenceCount> special{};

  bool operator==(const ShpSequenceBankReferences&) const = default;
};

struct ShpSequenceBankDocument {
  std::string id;
  ActorAnimationDocument animation;

  bool operator==(const ShpSequenceBankDocument&) const = default;
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
  // model's bones, named by stable ID.
  std::vector<ShpSequenceBankDocument> sequences;
  // Stable references for the original battle and special sequence slots.
  // The on-disc LBAs remain in `shp.layout` for import reconstruction.
  ShpSequenceBankReferences sequence_bank_ids;

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

// The polygons of each part (triangles, quads, styled).
using ShpPolygonCounts = std::array<std::uint16_t, kShpPartCount>;
// Per part, how many polygons from the part's start belong to each body part
// in turn (ShpModel::limb_ranges, part-major); the rest belong to none.
using ShpLimbRanges = std::array<std::uint16_t, kShpLimbRangeCount>;
inline constexpr std::size_t kShpBodyPartCount = 8;
static_assert(kShpBodyPartCount * kShpPartCount == kShpLimbRangeCount);
// The names of the parts' counts in polygon_counts and body_part_polygons.
inline constexpr std::array<std::string_view, kShpPartCount> kShpPartKeys{
    "triangles",
    "quads",
    "styled",
};

// Each part's polygon count, which the file header holds in a halfword.
[[nodiscard]] ShpPolygonCounts ShpPolygonCountsOf(const ShpModel& model);
// The part polygon `order` (its place in the parts' file order) is in, or
// nothing past `counts`.
[[nodiscard]] std::optional<ShpPart> ShpPartOf(std::size_t order,
                                               const ShpPolygonCounts& counts);

// The payload members polygon_counts ({"triangles", "quads", "styled"}),
// body_part_polygons (one {"body_part", "triangles", "quads", "styled"} entry
// per body part with polygons, ascending; a body part not listed has none)
// and bones, which every model payload has.
[[nodiscard]] JsonDocument ShpPolygonCountsJson(const ShpPolygonCounts& counts);
[[nodiscard]] JsonDocument ShpBodyPartPolygonsJson(const ShpLimbRanges& ranges);
[[nodiscard]] JsonDocument ShpBonesJson(const ShpModel& model);
// Reads body_part_polygons and bones into `model` and returns polygon_counts;
// each part's body parts must fit in its count.
[[nodiscard]] std::expected<ShpPolygonCounts, std::string> ReadShpModelHeader(
    const JsonMembers& payload, ShpModel& model);

// --- How the game draws a polygon (ModelGeometry.cpp DrawPolygons).
// Only the styled part's polygons are drawn by their flags byte; the triangle
// and quad parts draw one-sided and opaque at depth offset -1. A styled
// polygon's flags: 0x01 draws both sides; 0x03 on an uncoloured quad splits
// it into two one-sided triangles along a diagonal chosen as it is drawn, at
// depth offset -1 (bits 2-3 are then not read); bits 2-3 add 4 each to depth
// offset -5; 0x10 is semi-transparency and 0x60 its blend mode. The depth
// offset is added to the polygon's sort depth: a larger one sorts it further
// back. Bit 0x80 is not read.

// One polygon's entry in its primitive's extras.vs.polygons.
struct ShpPolygonEntry {
  std::size_t order = 0;  // its place in the parts' file order
  std::uint8_t corners = kShpTriangleCorners;
  std::uint8_t flags = 0;
  std::uint8_t extra = 0;  // ShpPolygon::extra, which the game does not read

  bool operator==(const ShpPolygonEntry&) const = default;
};

// The entry as JSON: "order", "corners", a styled polygon's "style" ("sides":
// "one", "both" or "split"; "depth_offset" -5, -1, 3 or 7, which a split quad
// has not; "semi_transparent"; "blend": "average", "add", "subtract" or
// "add_quarter"), and "unread" for the values the game does not read that
// differ from the disc's: "flags" of a triangle- or quad-part polygon (4),
// "depth_bits" of a split quad (0) and "extra" (0). `counts` places the
// polygon in its part and `colored` is the model's polygon layout. The error
// names flags no style holds: 0x80, 0x02 without 0x01, or 0x03 on a triangle
// or coloured quad, none of which a disc model has.
[[nodiscard]] std::expected<JsonDocument, std::string> ShpPolygonEntryJson(
    const ShpPolygonEntry& entry, const ShpPolygonCounts& counts, bool colored);
// The entry `json` holds; errors name the member at `where`.
[[nodiscard]] std::expected<ShpPolygonEntry, std::string> ShpPolygonEntryOf(
    const JsonDocument& json, const std::string& where,
    const ShpPolygonCounts& counts, bool colored);
// The flags a polygon of `part` is drawn with: its own in the styled part,
// 0 in the others.
[[nodiscard]] std::uint8_t ShpDrawnFlags(std::uint8_t flags,
                                         ShpPart part) noexcept;
// The preview material name of the primitive flags `flags`
// (kShpPrimitiveFlags): one_sided or double_sided, then the blend mode when
// semi-transparent or "opaque", e.g. double_sided_add or one_sided_opaque. A
// blend mode without semi-transparency follows "opaque".
[[nodiscard]] std::string ShpMaterialName(std::uint8_t flags);

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
