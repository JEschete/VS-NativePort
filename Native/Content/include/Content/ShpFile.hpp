#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <vector>

// The OBJ\*.SHP character model file, decoded strictly for the asset pipeline
// (ASSET_PIPELINE.md §6.2, #203). A file is the magic "H01\0", the counts
// (bones and groups as bytes, then triangles, quads and mixed polygons as
// halfwords), a 0xE8-byte texture layout block, the model's size and the
// model, then a word, the embedded SEQ's size and bytes, and the texture or,
// when its size is 4, the id of the model whose texture it uses.
//
// The model is a 0x40-byte header (24 limb palette ranges and the sound,
// group, vertex and polygon section offsets), the bones (16 bytes each), the
// groups (bone, end vertex), the vertices (x, y, z and a zero pad) and the
// polygons, followed by the sound section, with no gaps. A polygon's record
// layout is flat or coloured (Gouraud) as the texture's flag 0x10 says; a
// model that borrows a texture uses its lender's flag, which DecodeShp takes.
// The decoder keeps every byte it cannot derive, so EncodeShp rebuilds the
// exact file; it rejects anything else with a message naming the offset. The
// game's own parse (ActorAssets.cpp) keeps the original's lenient rules.
namespace VS::Content {

inline constexpr std::size_t kShpModelOffset = 0xF8;
inline constexpr std::size_t kShpLayoutBlockBytes = 0xE8;
inline constexpr std::size_t kShpLimbRangeCount = 24;
inline constexpr std::size_t kShpPartCount = 3;
inline constexpr std::size_t kShpMaxCorners = 4;
inline constexpr std::size_t kShpTriangleCorners = 3;
inline constexpr std::size_t kShpBoneTailBytes = 3;
// The texture header's flags: 8-bit indices, or 4-bit indices with the
// coloured polygon layout.
inline constexpr std::uint8_t kShp8BitTexture = 0x01;
inline constexpr std::uint8_t kShp4BitTexture = 0x10;
// The texture size field that marks a borrowed texture.
inline constexpr std::int32_t kShpBorrowedTextureSize = 4;

// The polygon parts in file order: triangles, quads, then the styled part,
// which holds either kind and is the only one drawn by its polygons' flags.
enum class ShpPart : std::uint8_t { kTriangles, kQuads, kStyled };

// decomp: Armature (src/BATTLE/BATTLE.PRG/entity.h:62)
struct ShpBone {
  std::int32_t length = 0;
  std::uint8_t parent = 0;
  std::uint8_t reserved5 = 0;
  std::uint8_t tag = 0;
  std::uint8_t limb_ids = 0;
  std::uint8_t auxiliary_tag = 0;
  std::uint8_t reserved9 = 0;
  std::uint16_t joint_init = 0;
  std::uint8_t pose_lock_slot = 0;
  std::array<std::uint8_t, kShpBoneTailBytes> reserved13{};

  bool operator==(const ShpBone&) const = default;
};

// The vertices from the previous group's end to `end` belong to `bone`.
struct ShpGroup {
  std::uint16_t bone = 0;
  std::uint16_t end = 0;

  bool operator==(const ShpGroup&) const = default;
};

struct ShpVertex {
  std::int16_t x = 0;
  std::int16_t y = 0;
  std::int16_t z = 0;

  bool operator==(const ShpVertex&) const = default;
};

struct ShpPolygon {
  std::uint8_t corners = kShpTriangleCorners;
  std::uint8_t flags = 0;
  // The byte after a flat polygon's flags, or a coloured quad's fourth colour
  // word's high byte; a coloured triangle has none and keeps 0.
  std::uint8_t extra = 0;
  std::array<std::uint16_t, kShpMaxCorners> vertices{};  // vertex numbers
  // u | v << 8 in record slot order: a triangle's corner c uses slot
  // (c + 1) % 3 (ModelGeometry.cpp UvOffset), a quad's corner c slot c.
  std::array<std::uint16_t, kShpMaxCorners> uvs{};
  // Coloured layout only: each corner's 0xBBGGRR.
  std::array<std::uint32_t, kShpMaxCorners> colors{};

  bool operator==(const ShpPolygon&) const = default;
};

struct ShpTexture {
  std::uint8_t flags = kShp8BitTexture;
  std::uint8_t width_byte = 0;   // width / 2 in bytes of pixel data
  std::uint8_t height_byte = 0;  // height / 2
  std::uint8_t palette_words = 0;
  // Two palette rows of `palette_words` colour words each.
  std::vector<std::uint16_t> palettes;
  std::vector<std::uint8_t> pixels;  // RowBytes() x Height(), row-major

  [[nodiscard]] bool Is8Bit() const noexcept {
    return flags == kShp8BitTexture;
  }
  [[nodiscard]] std::uint32_t RowBytes() const noexcept {
    return std::uint32_t{width_byte} * 2U;
  }
  // Pixels: 4-bit rows hold two per byte, low nibble first.
  [[nodiscard]] std::uint32_t Width() const noexcept {
    return Is8Bit() ? RowBytes() : RowBytes() * 2U;
  }
  [[nodiscard]] std::uint32_t Height() const noexcept {
    return std::uint32_t{height_byte} * 2U;
  }
  bool operator==(const ShpTexture&) const = default;
};

// The model section SHP and WEP files share, with the counts the file header
// gives for it.
struct ShpModel {
  std::array<std::uint16_t, kShpLimbRangeCount> limb_ranges{};
  std::vector<ShpBone> bones;
  std::vector<ShpGroup> groups;
  std::vector<ShpVertex> vertices;
  std::array<std::vector<ShpPolygon>, kShpPartCount> parts;
  std::vector<std::byte> sound;
  bool colored = false;  // the polygon record layout

  bool operator==(const ShpModel&) const = default;
};

struct ShpFile : ShpModel {
  std::array<std::byte, kShpLayoutBlockBytes> layout{};
  std::uint32_t sequence_word = 1;  // the word before the embedded SEQ
  std::vector<std::byte> sequence;
  // Exactly one: the model's own texture, or the model whose texture it uses.
  std::optional<ShpTexture> texture;
  std::optional<std::int32_t> lender;

  bool operator==(const ShpFile&) const = default;
};

// The magic and the counts that start an SHP or WEP file: bones and groups as
// bytes, then the polygons of each part as halfwords.
inline constexpr std::size_t kShpCountsBytes = 0x0C;

struct ShpModelCounts {
  std::size_t bones = 0;
  std::size_t groups = 0;
  std::array<std::size_t, kShpPartCount> parts{};

  bool operator==(const ShpModelCounts&) const = default;
};

// A model section and the file header that counts it.
struct ShpModelBytes {
  std::array<std::byte, kShpCountsBytes> counts{};
  std::vector<std::byte> model;
};

// Where a file's texture comes from, read without decoding the model.
struct ShpTextureSource {
  std::optional<std::uint8_t> flags;  // its own texture's flags
  std::optional<std::int32_t> lender;
};

[[nodiscard]] std::expected<ShpTextureSource, std::string> PeekShpTexture(
    std::span<const std::byte> data);
// Decodes a whole SHP file. `lender_colored` is the lender's coloured flag,
// needed only when the file borrows a texture.
[[nodiscard]] std::expected<ShpFile, std::string> DecodeShp(
    std::span<const std::byte> data,
    std::optional<bool> lender_colored = std::nullopt);
// The file `shp` is; DecodeShp reads it back to `shp`. Errors name what the
// file cannot hold.
[[nodiscard]] std::expected<std::vector<std::byte>, std::string> EncodeShp(
    const ShpFile& shp);

// The magic and counts at the start of `data`.
[[nodiscard]] std::expected<ShpModelCounts, std::string> DecodeShpCounts(
    std::span<const std::byte> data);
// The model section `model` (exactly its bytes) that `counts` describe, in the
// polygon layout `colored`. Error offsets count from the section's start.
[[nodiscard]] std::expected<ShpModel, std::string> DecodeShpModel(
    std::span<const std::byte> model, const ShpModelCounts& counts,
    bool colored);
// The model section `model` is and the header that counts it, which
// DecodeShpCounts and DecodeShpModel read back to `model`.
[[nodiscard]] std::expected<ShpModelBytes, std::string> EncodeShpModel(
    const ShpModel& model);

}  // namespace VS::Content
