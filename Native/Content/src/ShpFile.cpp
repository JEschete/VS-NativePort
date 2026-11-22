// Strict SHP decoding and encoding (ShpFile.hpp).
#include "Content/ShpFile.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <format>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace VS::Content {
namespace {
using Bytes = std::span<const std::byte>;

// "H01" and a zero byte, read as a little-endian word.
constexpr std::uint32_t kMagic = 0x00313048;
constexpr std::size_t kBoneCountAt = 4;
constexpr std::size_t kGroupCountAt = 5;
constexpr std::size_t kTriangleCountAt = 6;
constexpr std::size_t kQuadCountAt = 8;
constexpr std::size_t kMixedCountAt = 10;
constexpr std::size_t kLayoutAt = 0x0C;
constexpr std::size_t kModelSizeAt = 0xF4;
constexpr std::size_t kModelHeaderBytes = 0x40;
constexpr std::size_t kSoundAt = 0x30;
constexpr std::size_t kGroupsAt = 0x34;
constexpr std::size_t kVerticesAt = 0x38;
constexpr std::size_t kPolygonsAt = 0x3C;
constexpr std::size_t kBoneBytes = 16;
constexpr std::size_t kGroupBytes = 4;
constexpr std::size_t kVertexBytes = 8;
constexpr std::size_t kVertexYAt = 2;
constexpr std::size_t kVertexZAt = 4;
constexpr std::size_t kVertexPadAt = 6;
// A polygon names vertex n by its byte offset in the projected vertex table.
constexpr std::uint32_t kIndexScale = 4;
// Indices are halfwords, so vertex 0x3FFF (index 0xFFFC) is the last.
constexpr std::size_t kMaxVertices = 0x10000 / kIndexScale;

constexpr std::size_t kBoneParentAt = 4;
constexpr std::size_t kBoneReserved5At = 5;
constexpr std::size_t kBoneTagAt = 6;
constexpr std::size_t kBoneLimbIdsAt = 7;
constexpr std::size_t kBoneAuxiliaryTagAt = 8;
constexpr std::size_t kBoneReserved9At = 9;
constexpr std::size_t kBoneJointInitAt = 10;
constexpr std::size_t kBonePoseLockAt = 12;
constexpr std::size_t kBoneReserved13At = 13;

constexpr std::uint8_t kFlatTriangle = 0x24;
constexpr std::uint8_t kFlatQuad = 0x2C;
constexpr std::uint8_t kColoredTriangle = 0x34;
constexpr std::uint8_t kColoredQuad = 0x3C;
constexpr std::size_t kFlatTriangleBytes = 16;
constexpr std::size_t kFlatQuadBytes = 20;
constexpr std::size_t kColoredTriangleBytes = 24;
constexpr std::size_t kColoredQuadBytes = 32;
constexpr std::size_t kTriangleCorners = 3;
constexpr std::size_t kQuadCorners = 4;
// Flat: command, size, flags, extra, then the indices and UVs.
constexpr std::size_t kFlatSizeAt = 1;
constexpr std::size_t kFlatFlagsAt = 2;
constexpr std::size_t kFlatExtraAt = 3;
constexpr std::size_t kFlatIndicesAt = 4;
// Coloured: the indices, then colour words whose high bytes hold the command,
// size, flags and (quads) extra; the UVs fill the gaps.
constexpr std::size_t kColorWordsAt = 8;
constexpr std::size_t kColoredCommandAt = 11;
constexpr std::size_t kColoredSizeAt = 15;
constexpr std::size_t kColoredFlagsAt = 19;
constexpr std::size_t kColoredExtraAt = 23;
constexpr std::size_t kColoredFirstUvAt = 6;
constexpr std::size_t kColoredTriangleTailUvAt = 20;
constexpr std::size_t kColoredQuadUvAt = 24;
constexpr std::uint32_t kColorMask = 0xFFFFFF;
constexpr std::size_t kColorWordBytes = 4;

// The texture header: flags, width and height bytes, then the palette words.
constexpr std::size_t kTextureHeaderBytes = 4;
constexpr std::size_t kTextureWidthAt = 1;
constexpr std::size_t kTextureHeightAt = 2;
constexpr std::size_t kTexturePaletteWordsAt = 3;
constexpr std::size_t kSizeFieldBytes = sizeof(std::int32_t);
constexpr std::size_t kPaletteRows = 2;
constexpr std::size_t kPaletteWordBytes = 2;
// The texture size field counts the header, palettes and pixels, less this.
constexpr std::int32_t k8BitTextureSizeBias = 256;
constexpr std::int32_t k4BitTextureSizeBias = 32;

template <typename T>
T ReadLe(Bytes data, std::size_t offset) {
  T value{};
  std::memcpy(&value, data.subspan(offset, sizeof value).data(), sizeof value);
  return value;
}

std::uint8_t ByteAt(Bytes data, std::size_t offset) {
  return std::to_integer<std::uint8_t>(data[offset]);
}

std::unexpected<std::string> Fail(std::size_t offset,
                                  std::string_view message) {
  return std::unexpected(std::format("byte 0x{:X}: {}", offset, message));
}

struct PolygonLayout {
  std::size_t corners = 0;
  std::size_t bytes = 0;
};

std::optional<PolygonLayout> LayoutOfCommand(std::uint8_t command,
                                             bool colored) {
  if (command == (colored ? kColoredTriangle : kFlatTriangle)) {
    return PolygonLayout{
        .corners = kTriangleCorners,
        .bytes = colored ? kColoredTriangleBytes : kFlatTriangleBytes};
  }
  if (command == (colored ? kColoredQuad : kFlatQuad)) {
    return PolygonLayout{.corners = kQuadCorners,
                         .bytes = colored ? kColoredQuadBytes : kFlatQuadBytes};
  }
  return std::nullopt;
}

std::size_t RecordBytes(std::size_t corners, bool colored) {
  if (corners == kTriangleCorners) {
    return colored ? kColoredTriangleBytes : kFlatTriangleBytes;
  }
  return colored ? kColoredQuadBytes : kFlatQuadBytes;
}

std::uint8_t CommandOf(std::size_t corners, bool colored) {
  if (corners == kTriangleCorners) {
    return colored ? kColoredTriangle : kFlatTriangle;
  }
  return colored ? kColoredQuad : kFlatQuad;
}

// The record offset of UV slot `slot`.
std::size_t UvAt(std::size_t slot, std::size_t corners, bool colored) {
  if (!colored) return kFlatIndicesAt + ((corners + slot) * 2);
  if (corners == kQuadCorners) return kColoredQuadUvAt + (slot * 2);
  return slot == 0 ? kColoredFirstUvAt
                   : kColoredTriangleTailUvAt + ((slot - 1) * 2);
}

std::size_t IndexAt(std::size_t corner, bool colored) {
  return (colored ? 0 : kFlatIndicesAt) + (corner * 2);
}

std::expected<void, std::string> CheckPartCorners(std::size_t at, ShpPart part,
                                                  std::size_t corners) {
  if ((part == ShpPart::kTriangles && corners != kTriangleCorners) ||
      (part == ShpPart::kQuads && corners != kQuadCorners)) {
    return Fail(at,
                std::format("a {}-corner polygon in the {} part", corners,
                            part == ShpPart::kTriangles ? "triangle" : "quad"));
  }
  return {};
}

struct DecodedPolygon {
  ShpPolygon polygon;
  std::size_t bytes = 0;
};

std::expected<PolygonLayout, std::string> PolygonLayoutAt(Bytes model,
                                                          std::size_t at,
                                                          bool colored) {
  const std::size_t command_at = at + (colored ? kColoredCommandAt : 0);
  if (command_at >= model.size()) {
    return Fail(at, "the polygon section ends inside a polygon");
  }
  const std::uint8_t command = ByteAt(model, command_at);
  const auto layout = LayoutOfCommand(command, colored);
  if (!layout) {
    return Fail(command_at,
                std::format("0x{:02X} is not a {} polygon command", command,
                            colored ? "coloured" : "flat"));
  }
  if (model.size() - at < layout->bytes) {
    return Fail(at, "the polygon section ends inside a polygon");
  }
  const std::size_t size_at = at + (colored ? kColoredSizeAt : kFlatSizeAt);
  if (ByteAt(model, size_at) != layout->bytes) {
    return Fail(size_at, std::format("the size byte {} is not the record's {}",
                                     ByteAt(model, size_at), layout->bytes));
  }
  return *layout;
}

std::expected<DecodedPolygon, std::string> DecodePolygon(
    Bytes model, std::size_t at, ShpPart part, bool colored,
    std::size_t vertex_count) {
  const auto layout = PolygonLayoutAt(model, at, colored);
  if (!layout) return std::unexpected(layout.error());
  if (auto checked = CheckPartCorners(at, part, layout->corners); !checked) {
    return std::unexpected(checked.error());
  }
  const Bytes record = model.subspan(at, layout->bytes);
  DecodedPolygon decoded{.polygon = {}, .bytes = layout->bytes};
  ShpPolygon& polygon = decoded.polygon;
  polygon.corners = static_cast<std::uint8_t>(layout->corners);
  polygon.flags = ByteAt(record, colored ? kColoredFlagsAt : kFlatFlagsAt);
  if (!colored) {
    polygon.extra = ByteAt(record, kFlatExtraAt);
  } else if (layout->corners == kQuadCorners) {
    polygon.extra = ByteAt(record, kColoredExtraAt);
  }
  for (std::size_t corner = 0; corner < layout->corners; ++corner) {
    const auto index = ReadLe<std::uint16_t>(record, IndexAt(corner, colored));
    if (index % kIndexScale != 0 || index / kIndexScale >= vertex_count) {
      return Fail(
          at + IndexAt(corner, colored),
          std::format("index {} names no vertex of {}", index, vertex_count));
    }
    polygon.vertices.at(corner) =
        static_cast<std::uint16_t>(index / kIndexScale);
    polygon.uvs.at(corner) =
        ReadLe<std::uint16_t>(record, UvAt(corner, layout->corners, colored));
    if (colored) {
      polygon.colors.at(corner) =
          ReadLe<std::uint32_t>(record,
                                kColorWordsAt + (corner * kColorWordBytes)) &
          kColorMask;
    }
  }
  return decoded;
}

struct Sections {
  std::size_t sound = 0;
  std::size_t groups = 0;
  std::size_t vertices = 0;
  std::size_t polygons = 0;
};

std::expected<Sections, std::string> SectionsOf(Bytes model,
                                                const ShpModelCounts& counts) {
  const Sections sections{
      .sound = ReadLe<std::uint32_t>(model, kSoundAt),
      .groups = ReadLe<std::uint32_t>(model, kGroupsAt),
      .vertices = ReadLe<std::uint32_t>(model, kVerticesAt),
      .polygons = ReadLe<std::uint32_t>(model, kPolygonsAt)};
  const std::size_t bones_end = kModelHeaderBytes + (counts.bones * kBoneBytes);
  if (sections.groups != bones_end) {
    return Fail(kGroupsAt, std::format("the groups start at 0x{:X}, not after "
                                       "the bones at 0x{:X}",
                                       sections.groups, bones_end));
  }
  const std::size_t groups_end = bones_end + (counts.groups * kGroupBytes);
  if (sections.vertices != groups_end || groups_end > model.size()) {
    return Fail(kVerticesAt, std::format("the vertices start at 0x{:X}, not "
                                         "after the groups at 0x{:X}",
                                         sections.vertices, groups_end));
  }
  return sections;
}

std::expected<void, std::string> DecodeGroups(Bytes model,
                                              const Sections& sections,
                                              const ShpModelCounts& counts,
                                              ShpModel& shp) {
  std::size_t previous = 0;
  for (std::size_t group = 0; group < counts.groups; ++group) {
    const std::size_t at = sections.groups + (group * kGroupBytes);
    const ShpGroup read{.bone = ReadLe<std::uint16_t>(model, at),
                        .end = ReadLe<std::uint16_t>(model, at + 2)};
    if (read.bone >= counts.bones) {
      return Fail(at, std::format("group {} names bone {} of {}", group,
                                  read.bone, counts.bones));
    }
    if (read.end <= previous) {
      return Fail(at + 2, std::format("group {} ends at vertex {}, not after "
                                      "vertex {}",
                                      group, read.end, previous));
    }
    previous = read.end;
    shp.groups.push_back(read);
  }
  if (previous > kMaxVertices) {
    return Fail(sections.groups, std::format("{} vertices; at most {} can be "
                                             "indexed",
                                             previous, kMaxVertices));
  }
  return {};
}

void DecodeBones(Bytes model, std::size_t count, ShpModel& shp) {
  for (std::size_t bone = 0; bone < count; ++bone) {
    const Bytes record =
        model.subspan(kModelHeaderBytes + (bone * kBoneBytes), kBoneBytes);
    ShpBone& read = shp.bones.emplace_back();
    read.length = ReadLe<std::int32_t>(record, 0);
    read.parent = ByteAt(record, kBoneParentAt);
    read.reserved5 = ByteAt(record, kBoneReserved5At);
    read.tag = ByteAt(record, kBoneTagAt);
    read.limb_ids = ByteAt(record, kBoneLimbIdsAt);
    read.auxiliary_tag = ByteAt(record, kBoneAuxiliaryTagAt);
    read.reserved9 = ByteAt(record, kBoneReserved9At);
    read.joint_init = ReadLe<std::uint16_t>(record, kBoneJointInitAt);
    read.pose_lock_slot = ByteAt(record, kBonePoseLockAt);
    for (std::size_t i = 0; i < read.reserved13.size(); ++i) {
      read.reserved13.at(i) = ByteAt(record, kBoneReserved13At + i);
    }
  }
}

std::expected<void, std::string> DecodeVertices(Bytes model,
                                                const Sections& sections,
                                                ShpModel& shp) {
  const std::size_t count = shp.groups.empty() ? 0 : shp.groups.back().end;
  const std::size_t end = sections.vertices + (count * kVertexBytes);
  if (sections.polygons != end || end > model.size()) {
    return Fail(kPolygonsAt, std::format("the polygons start at 0x{:X}, not "
                                         "after the {} vertices at 0x{:X}",
                                         sections.polygons, count, end));
  }
  for (std::size_t vertex = 0; vertex < count; ++vertex) {
    const std::size_t at = sections.vertices + (vertex * kVertexBytes);
    if (ReadLe<std::int16_t>(model, at + kVertexPadAt) != 0) {
      return Fail(at + kVertexPadAt,
                  std::format("vertex {}'s pad halfword is not zero", vertex));
    }
    shp.vertices.push_back({.x = ReadLe<std::int16_t>(model, at),
                            .y = ReadLe<std::int16_t>(model, at + kVertexYAt),
                            .z = ReadLe<std::int16_t>(model, at + kVertexZAt)});
  }
  return {};
}

// The polygons from `at`; returns where they end.
std::expected<std::size_t, std::string> DecodePolygons(
    Bytes model, std::size_t at, const ShpModelCounts& counts, ShpModel& shp) {
  for (std::size_t part = 0; part < kShpPartCount; ++part) {
    for (std::size_t index = 0; index < counts.parts.at(part); ++index) {
      auto decoded = DecodePolygon(model, at, static_cast<ShpPart>(part),
                                   shp.colored, shp.vertices.size());
      if (!decoded) return std::unexpected(decoded.error());
      shp.parts.at(part).push_back(decoded->polygon);
      at += decoded->bytes;
    }
  }
  return at;
}

std::expected<void, std::string> DecodeModel(Bytes model,
                                             const ShpModelCounts& counts,
                                             ShpModel& shp) {
  if (model.size() < kModelHeaderBytes + (counts.bones * kBoneBytes)) {
    return Fail(0, "the model is too small for its bones");
  }
  for (std::size_t range = 0; range < kShpLimbRangeCount; ++range) {
    shp.limb_ranges.at(range) = ReadLe<std::uint16_t>(model, range * 2);
  }
  const auto sections = SectionsOf(model, counts);
  if (!sections) return std::unexpected(sections.error());
  DecodeBones(model, counts.bones, shp);
  if (auto groups = DecodeGroups(model, *sections, counts, shp); !groups) {
    return groups;
  }
  if (auto vertices = DecodeVertices(model, *sections, shp); !vertices) {
    return vertices;
  }
  const auto end = DecodePolygons(model, sections->polygons, counts, shp);
  if (!end) return std::unexpected(end.error());
  if (sections->sound != *end) {
    return Fail(kSoundAt, std::format("the sound section starts at 0x{:X}, "
                                      "not after the polygons at 0x{:X}",
                                      sections->sound, *end));
  }
  const Bytes sound = model.subspan(*end);
  shp.sound.assign(sound.begin(), sound.end());
  return {};
}

std::int32_t TextureSizeBias(std::uint8_t flags) {
  return flags == kShp8BitTexture ? k8BitTextureSizeBias : k4BitTextureSizeBias;
}

std::size_t TextureBytes(const ShpTexture& texture) {
  return kTextureHeaderBytes +
         (kPaletteRows * texture.palette_words * kPaletteWordBytes) +
         (std::size_t{texture.RowBytes()} * texture.Height());
}

std::expected<ShpTexture, std::string> DecodeTexture(Bytes data, std::size_t at,
                                                     std::int32_t size_field) {
  if (data.size() - at < kTextureHeaderBytes) {
    return Fail(at, "the file ends inside the texture header");
  }
  ShpTexture texture{.flags = ByteAt(data, at),
                     .width_byte = ByteAt(data, at + kTextureWidthAt),
                     .height_byte = ByteAt(data, at + kTextureHeightAt),
                     .palette_words = ByteAt(data, at + kTexturePaletteWordsAt),
                     .palettes = {},
                     .pixels = {}};
  if (texture.flags != kShp8BitTexture && texture.flags != kShp4BitTexture) {
    return Fail(at,
                std::format("texture flags 0x{:02X} are neither 0x{:02X} "
                            "nor 0x{:02X}",
                            texture.flags, kShp8BitTexture, kShp4BitTexture));
  }
  const std::size_t bytes = TextureBytes(texture);
  if (data.size() - at != bytes) {
    return Fail(at, std::format("the texture needs {} bytes to the file's end; "
                                "{} remain",
                                bytes, data.size() - at));
  }
  if (std::cmp_not_equal(size_field, static_cast<std::int64_t>(bytes) -
                                         TextureSizeBias(texture.flags))) {
    return Fail(at - kSizeFieldBytes,
                std::format("the texture size field {} is not {} - {}",
                            size_field, bytes, TextureSizeBias(texture.flags)));
  }
  std::size_t offset = at + kTextureHeaderBytes;
  const std::size_t words = kPaletteRows * texture.palette_words;
  for (std::size_t word = 0; word < words; ++word) {
    texture.palettes.push_back(ReadLe<std::uint16_t>(data, offset));
    offset += kPaletteWordBytes;
  }
  for (const std::byte pixel : data.subspan(offset)) {
    texture.pixels.push_back(std::to_integer<std::uint8_t>(pixel));
  }
  return texture;
}

// The embedded SEQ and the texture or its lender after the model at `at`.
std::expected<void, std::string> DecodeTail(Bytes data, std::size_t at,
                                            ShpFile& shp) {
  if (data.size() - at < 2 * sizeof(std::uint32_t)) {
    return Fail(at, "the file ends before the embedded SEQ");
  }
  shp.sequence_word = ReadLe<std::uint32_t>(data, at);
  const auto sequence_bytes =
      ReadLe<std::int32_t>(data, at + sizeof(std::uint32_t));
  at += 2 * sizeof(std::uint32_t);
  if (sequence_bytes < 0 ||
      data.size() - at <
          static_cast<std::size_t>(sequence_bytes) + kSizeFieldBytes) {
    return Fail(
        at - kSizeFieldBytes,
        std::format("the embedded SEQ's {} bytes do not fit", sequence_bytes));
  }
  const Bytes sequence =
      data.subspan(at, static_cast<std::size_t>(sequence_bytes));
  shp.sequence.assign(sequence.begin(), sequence.end());
  at += sequence.size();
  const auto texture_size = ReadLe<std::int32_t>(data, at);
  at += sizeof texture_size;
  if (texture_size == kShpBorrowedTextureSize) {
    if (data.size() - at != sizeof(std::int32_t)) {
      return Fail(at,
                  "a borrowed texture's lender id is not the file's last "
                  "word");
    }
    shp.lender = ReadLe<std::int32_t>(data, at);
    return {};
  }
  auto texture = DecodeTexture(data, at, texture_size);
  if (!texture) return std::unexpected(texture.error());
  shp.texture = std::move(*texture);
  return {};
}

std::expected<std::size_t, std::string> ModelBytesOf(Bytes data) {
  if (data.size() < kShpModelOffset) {
    return Fail(0, std::format("an SHP header needs {} bytes; {} remain",
                               kShpModelOffset, data.size()));
  }
  if (ReadLe<std::uint32_t>(data, 0) != kMagic) {
    return Fail(0, "the file does not start with the magic \"H01\"");
  }
  const auto model_bytes = ReadLe<std::int32_t>(data, kModelSizeAt);
  if (std::cmp_less(model_bytes, kModelHeaderBytes) ||
      std::cmp_greater(model_bytes, data.size() - kShpModelOffset)) {
    return Fail(kModelSizeAt,
                std::format("the model size {} does not fit", model_bytes));
  }
  return static_cast<std::size_t>(model_bytes);
}

// --- Encoding ---

class Writer {
 public:
  template <typename T>
  void Put(T value) {
    std::array<std::byte, sizeof value> bytes{};
    std::memcpy(bytes.data(), &value, sizeof value);
    bytes_.insert(bytes_.end(), bytes.begin(), bytes.end());
  }
  void PutByte(std::uint8_t value) { bytes_.push_back(std::byte{value}); }
  void Append(Bytes bytes) {
    bytes_.insert(bytes_.end(), bytes.begin(), bytes.end());
  }
  [[nodiscard]] std::size_t Size() const noexcept { return bytes_.size(); }
  std::vector<std::byte> Take() { return std::move(bytes_); }

 private:
  std::vector<std::byte> bytes_;
};

std::unexpected<std::string> Refuse(std::string message) {
  return std::unexpected(std::move(message));
}

std::expected<void, std::string> CheckPolygon(const ShpPolygon& polygon,
                                              ShpPart part, bool colored,
                                              std::size_t vertex_count) {
  const std::size_t corners = polygon.corners;
  if (corners != kTriangleCorners && corners != kQuadCorners) {
    return Refuse(std::format("a polygon has {} corners", corners));
  }
  if ((part == ShpPart::kTriangles && corners != kTriangleCorners) ||
      (part == ShpPart::kQuads && corners != kQuadCorners)) {
    return Refuse(
        std::format("a {}-corner polygon is in the wrong part", corners));
  }
  if (colored && corners == kTriangleCorners && polygon.extra != 0) {
    return Refuse("a coloured triangle has no extra byte");
  }
  for (std::size_t corner = 0; corner < kShpMaxCorners; ++corner) {
    const bool used = corner < corners;
    if (used && polygon.vertices.at(corner) >= vertex_count) {
      return Refuse(std::format("a polygon names vertex {} of {}",
                                polygon.vertices.at(corner), vertex_count));
    }
    if (polygon.colors.at(corner) > (used && colored ? kColorMask : 0) ||
        (!used &&
         (polygon.vertices.at(corner) != 0 || polygon.uvs.at(corner) != 0))) {
      return Refuse("a polygon sets a value its record cannot hold");
    }
  }
  return {};
}

void PutPolygon(std::span<std::byte> record, const ShpPolygon& polygon,
                bool colored) {
  const std::size_t corners = polygon.corners;
  const auto put = [&](std::size_t at, auto value) {
    std::memcpy(&record[at], &value, sizeof value);
  };
  for (std::size_t corner = 0; corner < corners; ++corner) {
    put(IndexAt(corner, colored),
        static_cast<std::uint16_t>(polygon.vertices.at(corner) * kIndexScale));
    put(UvAt(corner, corners, colored), polygon.uvs.at(corner));
    if (colored) {
      put(kColorWordsAt + (corner * kColorWordBytes),
          polygon.colors.at(corner));
    }
  }
  const auto command = CommandOf(corners, colored);
  const auto size = static_cast<std::uint8_t>(RecordBytes(corners, colored));
  record[colored ? kColoredCommandAt : 0] = std::byte{command};
  record[colored ? kColoredSizeAt : kFlatSizeAt] = std::byte{size};
  record[colored ? kColoredFlagsAt : kFlatFlagsAt] = std::byte{polygon.flags};
  if (!colored) {
    record[kFlatExtraAt] = std::byte{polygon.extra};
  } else if (corners == kQuadCorners) {
    record[kColoredExtraAt] = std::byte{polygon.extra};
  }
}

std::expected<void, std::string> CheckGroups(const ShpModel& shp) {
  std::size_t previous = 0;
  for (const ShpGroup& group : shp.groups) {
    if (group.bone >= shp.bones.size() || group.end <= previous) {
      return Refuse(
          std::format("the group of bone {} ending at vertex {} "
                      "does not follow vertex {} or names no bone",
                      group.bone, group.end, previous));
    }
    previous = group.end;
  }
  if (previous != shp.vertices.size() || previous > kMaxVertices) {
    return Refuse(std::format("the groups cover {} of {} vertices", previous,
                              shp.vertices.size()));
  }
  return {};
}

std::expected<void, std::string> CheckTexture(const ShpFile& shp) {
  if (shp.texture.has_value() == shp.lender.has_value()) {
    return Refuse("a model needs either its own texture or a lender");
  }
  if (!shp.texture) return {};
  const ShpTexture& texture = *shp.texture;
  if (texture.flags != kShp8BitTexture && texture.flags != kShp4BitTexture) {
    return Refuse(std::format("texture flags 0x{:02X}", texture.flags));
  }
  if (shp.colored != (texture.flags == kShp4BitTexture)) {
    return Refuse("the polygon layout disagrees with the texture flags");
  }
  if (texture.palettes.size() != kPaletteRows * texture.palette_words ||
      texture.pixels.size() !=
          std::size_t{texture.RowBytes()} * texture.Height()) {
    return Refuse("the texture's palettes or pixels do not match its header");
  }
  // A size field of 4 would read back as a borrowed texture.
  if (std::cmp_equal(static_cast<std::int64_t>(TextureBytes(texture)) -
                         TextureSizeBias(texture.flags),
                     kShpBorrowedTextureSize)) {
    return Refuse("the texture's size field would mark it borrowed");
  }
  return {};
}

std::expected<void, std::string> CheckModel(const ShpModel& shp) {
  constexpr std::size_t kByteMax = std::numeric_limits<std::uint8_t>::max();
  constexpr std::size_t kHalfMax = std::numeric_limits<std::uint16_t>::max();
  if (shp.bones.size() > kByteMax || shp.groups.size() > kByteMax ||
      std::ranges::any_of(
          shp.parts, [](const auto& part) { return part.size() > kHalfMax; })) {
    return Refuse("a count does not fit the SHP header");
  }
  if (auto groups = CheckGroups(shp); !groups) {
    return groups;
  }
  for (std::size_t part = 0; part < kShpPartCount; ++part) {
    for (const ShpPolygon& polygon : shp.parts.at(part)) {
      if (auto checked = CheckPolygon(polygon, static_cast<ShpPart>(part),
                                      shp.colored, shp.vertices.size());
          !checked) {
        return checked;
      }
    }
  }
  return {};
}

void PutBones(Writer& out, const ShpModel& shp) {
  for (const ShpBone& bone : shp.bones) {
    out.Put(bone.length);
    out.PutByte(bone.parent);
    out.PutByte(bone.reserved5);
    out.PutByte(bone.tag);
    out.PutByte(bone.limb_ids);
    out.PutByte(bone.auxiliary_tag);
    out.PutByte(bone.reserved9);
    out.Put(bone.joint_init);
    out.PutByte(bone.pose_lock_slot);
    for (const std::uint8_t reserved : bone.reserved13) out.PutByte(reserved);
  }
}

std::vector<std::byte> EncodeModel(const ShpModel& shp) {
  std::size_t polygon_bytes = 0;
  for (const auto& part : shp.parts) {
    for (const ShpPolygon& polygon : part) {
      polygon_bytes += RecordBytes(polygon.corners, shp.colored);
    }
  }
  const std::size_t groups_at =
      kModelHeaderBytes + (shp.bones.size() * kBoneBytes);
  const std::size_t vertices_at = groups_at + (shp.groups.size() * kGroupBytes);
  const std::size_t polygons_at =
      vertices_at + (shp.vertices.size() * kVertexBytes);
  Writer out;
  for (const std::uint16_t range : shp.limb_ranges) out.Put(range);
  for (const std::size_t at :
       {polygons_at + polygon_bytes, groups_at, vertices_at, polygons_at}) {
    out.Put(static_cast<std::uint32_t>(at));
  }
  PutBones(out, shp);
  for (const ShpGroup& group : shp.groups) {
    out.Put(group.bone);
    out.Put(group.end);
  }
  for (const ShpVertex& vertex : shp.vertices) {
    out.Put(vertex.x);
    out.Put(vertex.y);
    out.Put(vertex.z);
    out.Put(std::int16_t{0});
  }
  std::vector<std::byte> model = out.Take();
  model.resize(polygons_at + polygon_bytes);
  std::size_t at = polygons_at;
  for (const auto& part : shp.parts) {
    for (const ShpPolygon& polygon : part) {
      const std::size_t bytes = RecordBytes(polygon.corners, shp.colored);
      PutPolygon(std::span(model).subspan(at, bytes), polygon, shp.colored);
      at += bytes;
    }
  }
  model.insert(model.end(), shp.sound.begin(), shp.sound.end());
  return model;
}

void PutTail(Writer& out, const ShpFile& shp) {
  out.Put(shp.sequence_word);
  out.Put(static_cast<std::int32_t>(shp.sequence.size()));
  out.Append(shp.sequence);
  if (shp.lender) {
    out.Put(kShpBorrowedTextureSize);
    out.Put(*shp.lender);
    return;
  }
  const ShpTexture& texture = *shp.texture;
  out.Put(static_cast<std::int32_t>(
      static_cast<std::int64_t>(TextureBytes(texture)) -
      TextureSizeBias(texture.flags)));
  out.PutByte(texture.flags);
  out.PutByte(texture.width_byte);
  out.PutByte(texture.height_byte);
  out.PutByte(texture.palette_words);
  for (const std::uint16_t word : texture.palettes) out.Put(word);
  out.Append(std::as_bytes(std::span(texture.pixels)));
}

}  // namespace

std::expected<ShpTextureSource, std::string> PeekShpTexture(Bytes data) {
  const auto model_bytes = ModelBytesOf(data);
  if (!model_bytes) return std::unexpected(model_bytes.error());
  ShpFile tail;
  if (auto read = DecodeTail(data, kShpModelOffset + *model_bytes, tail);
      !read) {
    return std::unexpected(read.error());
  }
  return ShpTextureSource{
      .flags = tail.texture ? std::optional(tail.texture->flags) : std::nullopt,
      .lender = tail.lender};
}

std::expected<ShpFile, std::string> DecodeShp(
    Bytes data, std::optional<bool> lender_colored) {
  const auto model_bytes = ModelBytesOf(data);
  if (!model_bytes) return std::unexpected(model_bytes.error());
  ShpFile shp;
  if (auto tail = DecodeTail(data, kShpModelOffset + *model_bytes, shp);
      !tail) {
    return std::unexpected(tail.error());
  }
  if (shp.texture) {
    shp.colored = shp.texture->flags == kShp4BitTexture;
  } else if (lender_colored) {
    shp.colored = *lender_colored;
  } else {
    return std::unexpected(std::format(
        "the model uses model {}'s texture, whose flags set its polygon "
        "layout",
        *shp.lender));
  }
  const auto counts = DecodeShpCounts(data);
  if (!counts) return std::unexpected(counts.error());
  std::ranges::copy(data.subspan(kLayoutAt, kShpLayoutBlockBytes),
                    shp.layout.begin());
  const Bytes model = data.subspan(kShpModelOffset, *model_bytes);
  if (auto decoded = DecodeModel(model, *counts, shp); !decoded) {
    return std::unexpected(std::format("model {}", decoded.error()));
  }
  return shp;
}

std::expected<std::vector<std::byte>, std::string> EncodeShp(
    const ShpFile& shp) {
  if (auto checked = CheckTexture(shp); !checked) {
    return std::unexpected(checked.error());
  }
  auto model = EncodeShpModel(shp);
  if (!model) return std::unexpected(model.error());
  Writer out;
  out.Append(model->counts);
  out.Append(shp.layout);
  out.Put(static_cast<std::int32_t>(model->model.size()));
  out.Append(model->model);
  PutTail(out, shp);
  return out.Take();
}

std::expected<ShpModelCounts, std::string> DecodeShpCounts(Bytes data) {
  if (data.size() < kShpCountsBytes) {
    return Fail(0, std::format("the counts need {} bytes; {} remain",
                               kShpCountsBytes, data.size()));
  }
  if (ReadLe<std::uint32_t>(data, 0) != kMagic) {
    return Fail(0, "the file does not start with the magic \"H01\"");
  }
  return ShpModelCounts{.bones = ByteAt(data, kBoneCountAt),
                        .groups = ByteAt(data, kGroupCountAt),
                        .parts = {ReadLe<std::uint16_t>(data, kTriangleCountAt),
                                  ReadLe<std::uint16_t>(data, kQuadCountAt),
                                  ReadLe<std::uint16_t>(data, kMixedCountAt)}};
}

std::expected<ShpModel, std::string> DecodeShpModel(
    Bytes model, const ShpModelCounts& counts, bool colored) {
  ShpModel decoded;
  decoded.colored = colored;
  if (auto read = DecodeModel(model, counts, decoded); !read) {
    return std::unexpected(read.error());
  }
  return decoded;
}

std::expected<ShpModelBytes, std::string> EncodeShpModel(
    const ShpModel& model) {
  if (auto checked = CheckModel(model); !checked) {
    return std::unexpected(checked.error());
  }
  ShpModelBytes encoded{.counts = {}, .model = EncodeModel(model)};
  if (encoded.model.size() >
      static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())) {
    return Refuse("the model is too large");
  }
  Writer out;
  out.Put(kMagic);
  out.PutByte(static_cast<std::uint8_t>(model.bones.size()));
  out.PutByte(static_cast<std::uint8_t>(model.groups.size()));
  for (const auto& part : model.parts) {
    out.Put(static_cast<std::uint16_t>(part.size()));
  }
  const std::vector<std::byte> counts = out.Take();
  std::ranges::copy(counts, encoded.counts.begin());
  return encoded;
}

}  // namespace VS::Content
