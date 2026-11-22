// Native character model files (ShpModelFiles.hpp).
#include "Content/ShpModelFiles.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Content/ConversionManifest.hpp"
#include "Platform/Paths.hpp"
#include "nlohmann/json.hpp"

namespace VS::Content {
namespace {
namespace fs = std::filesystem;
using Json = JsonDocument;
using Bytes = std::vector<std::byte>;
using Layout = std::array<std::byte, kShpLayoutBlockBytes>;
using Check = std::expected<void, std::string>;
template <typename T>
using Result = std::expected<T, std::string>;

constexpr std::uint32_t kByteBits = 8;
constexpr std::uint64_t kByteMask = 0xFF;
constexpr std::uint32_t kU32Max = std::numeric_limits<std::uint32_t>::max();
// Polygon records hold a vertex number times 4 in a halfword.
constexpr std::uint32_t kMaxVertices = 0x4000;
// Bone origins stay where a float holds every integer, with room for a
// vertex's 16-bit offset.
constexpr std::int64_t kMaxOrigin = std::int64_t{1} << 23;
constexpr std::size_t kPaletteRows = 2;
constexpr unsigned kNibbleBits = 4;
constexpr std::uint8_t kNibbleMask = 0x0F;
// Pixels per width_byte: two 8-bit indices or four 4-bit ones per halfword.
constexpr std::uint32_t k8BitPixelsPerUnit = 2;
constexpr std::uint32_t k4BitPixelsPerUnit = 4;
constexpr std::uint32_t kRowsPerUnit = 2;
constexpr std::uint32_t kUnitMax = std::numeric_limits<std::uint8_t>::max();
constexpr std::uint64_t kMaxBufferBytes = std::uint64_t{1} << 30;
constexpr std::uint64_t kMaxBufferPadding = 3;
constexpr std::uint64_t kStrideAlignment = 4;
constexpr std::uint64_t kMaxStride = 252;
constexpr std::uint64_t kMaxElements = std::uint64_t{1} << 24;
constexpr std::size_t kAxes = 3;
constexpr std::size_t kColorChannels = 3;
constexpr std::size_t kSkinSlots = 4;  // JOINTS_0 and WEIGHTS_0 are VEC4
constexpr std::size_t kMatrixColumns = 4;
constexpr double kByteMaximum = 255.0;
constexpr double kShortMaximum = 65535.0;
// A weight this close to 1 or 0 is 1 or 0 (an editor's u8 weights).
constexpr double kWeightTolerance = 1.0 / 512.0;
// Positions past this are not near any SHP vertex.
constexpr double kMaxPosition = 2147483647.0;
constexpr std::uint32_t kUvMaximum = 255;

std::string NameOf(std::string_view stem, std::string_view suffix) {
  return std::string(stem) + std::string(suffix);
}

Check CheckStem(std::string_view stem) {
  if (stem.empty() || stem == "." || stem == ".." ||
      stem.find_first_of("/\\:") != std::string_view::npos) {
    return std::unexpected(
        std::format("'{}' is not a model file name stem", stem));
  }
  return {};
}

Result<Bytes> ReadFile(const ImageFileReader& read, const std::string& name) {
  std::optional<Bytes> bytes = read(name);
  if (!bytes) return std::unexpected(std::format("{} is missing", name));
  return std::move(*bytes);
}

// The first failure among `checks`, which are all evaluated.
Check FirstError(std::initializer_list<Check> checks) {
  for (const Check& check : checks) {
    if (!check) return check;
  }
  return {};
}

std::uint32_t LittleEndian(std::span<const std::byte> bytes) {
  std::uint32_t value = 0;
  for (std::size_t index = 0; index < bytes.size(); ++index) {
    value |= std::to_integer<std::uint32_t>(bytes[index])
             << (index * kByteBits);
  }
  return value;
}

struct IntegerRange {
  std::int64_t low = 0;
  std::int64_t high = 0;
};
using Int32Limits = std::numeric_limits<std::int32_t>;
constexpr IntegerRange kInt32Range{Int32Limits::min(), Int32Limits::max()};

// A whole JSON number within `range`; floating-point values are rejected.
Result<std::int64_t> IntegerIn(const Json& value, IntegerRange range,
                               std::string_view where) {
  std::optional<std::int64_t> number;
  if (value.is_number_unsigned()) {
    const auto whole = value.get<std::uint64_t>();
    if (whole <=
        static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
      number = static_cast<std::int64_t>(whole);
    }
  } else if (value.is_number_integer()) {
    number = value.get<std::int64_t>();
  }
  if (!number || *number < range.low || *number > range.high) {
    return std::unexpected(
        std::format("{} must be a whole number from {} to {}", where, range.low,
                    range.high));
  }
  return *number;
}

Result<std::uint64_t> OptionalUnsigned(const JsonMembers& members,
                                       std::string_view key,
                                       std::uint64_t max) {
  if (members.Find(key) == nullptr) return 0;
  return members.Unsigned(key, max);
}

template <typename T>
Check ReadUnsigned(const JsonMembers& members, std::string_view key, T& out) {
  auto value = members.Unsigned(key, std::numeric_limits<T>::max());
  if (!value) return std::unexpected(value.error());
  out = static_cast<T>(*value);
  return {};
}

template <typename T, std::size_t N>
Check ReadUnsignedArray(const JsonMembers& members, std::string_view key,
                        std::array<T, N>& out) {
  auto value = members.Get(key);
  if (!value) return std::unexpected(value.error());
  const Json& array = **value;
  if (!array.is_array() || array.size() != N) {
    return std::unexpected(std::format("{} must be an array of {} numbers",
                                       members.Where(key), N));
  }
  for (std::size_t index = 0; index < N; ++index) {
    const Json& element = array[index];
    if (!element.is_number_unsigned() ||
        element.get<std::uint64_t>() > std::numeric_limits<T>::max()) {
      return std::unexpected(std::format(
          "{}[{}] must be a whole number from 0 to {}", members.Where(key),
          index, std::uint64_t{std::numeric_limits<T>::max()}));
    }
    out[index] = static_cast<T>(element.get<std::uint64_t>());
  }
  return {};
}

// Fails unless `object` has exactly the members `items` name.
template <typename Item, typename NameOfItem>
Check CheckExactMembers(const Json& object, std::span<const Item> items,
                        NameOfItem name_of, std::string_view where) {
  if (!object.is_object()) {
    return std::unexpected(std::format("{} must be an object", where));
  }
  for (const auto& member : object.items()) {
    const std::string& key = member.key();
    if (std::ranges::none_of(
            items, [&](const Item& item) { return name_of(item) == key; })) {
      return std::unexpected(
          std::format("{} has an unknown member '{}'", where, key));
    }
  }
  for (const Item& item : items) {
    if (!object.contains(std::string(name_of(item)))) {
      return std::unexpected(
          std::format("{}.{} is missing", where, name_of(item)));
    }
  }
  return {};
}

Json IndexJson(std::optional<std::size_t> index) {
  return index ? Json(*index) : Json(nullptr);
}

Result<std::optional<std::size_t>> OptionalIndex(const JsonMembers& members,
                                                 std::string_view key) {
  auto value = members.Get(key);
  if (!value) return std::unexpected(value.error());
  if ((*value)->is_null()) return std::optional<std::size_t>{};
  auto index = members.Unsigned(key, kU32Max);
  if (!index) {
    return std::unexpected(std::format(
        "{} must be a buffer view number or null", members.Where(key)));
  }
  return std::optional<std::size_t>(*index);
}

// --- The layout block.

// The little-endian unit a field's bits are in.
enum class Unit : std::uint8_t { kByte = 1, kHalfword = 2, kWord = 4 };

struct LayoutField {
  std::string_view name;
  std::uint32_t offset = 0;  // from the record or the block
  Unit unit = Unit::kByte;
  bool is_signed = false;
  bool flag = false;  // one bit, shown as true or false
  std::uint32_t shift = 0;
  std::uint32_t bits = 0;  // 0: the whole unit
};

struct BitRange {
  std::uint32_t shift = 0;
  std::uint32_t bits = 0;
};

constexpr LayoutField Whole(std::string_view name, std::uint32_t offset,
                            Unit unit) {
  LayoutField field;
  field.name = name;
  field.offset = offset;
  field.unit = unit;
  return field;
}

constexpr LayoutField SignedWhole(std::string_view name, std::uint32_t offset,
                                  Unit unit) {
  LayoutField field = Whole(name, offset, unit);
  field.is_signed = true;
  return field;
}

constexpr LayoutField Bits(std::string_view name, std::uint32_t offset,
                           Unit unit, BitRange range) {
  LayoutField field = Whole(name, offset, unit);
  field.shift = range.shift;
  field.bits = range.bits;
  return field;
}

constexpr LayoutField Flag(std::string_view name, std::uint32_t offset,
                           Unit unit, std::uint32_t bit) {
  LayoutField field =
      Bits(name, offset, unit, BitRange{.shift = bit, .bits = 1});
  field.flag = true;
  return field;
}

constexpr std::uint32_t FieldBits(const LayoutField& field) {
  return field.bits != 0 ? field.bits
                         : static_cast<std::uint32_t>(field.unit) * kByteBits;
}

constexpr std::uint64_t FieldMask(std::uint32_t bits) {
  return (std::uint64_t{1} << bits) - 1;
}

struct LayoutEntry {
  // A value, each element of an array, or a record array's name and offset.
  LayoutField field;
  std::uint32_t count = 0;  // 0: one value
  std::uint32_t stride = 0;
  std::span<const LayoutField> members;  // each record's fields
};

struct Repeat {
  std::uint32_t count = 0;
  std::uint32_t stride = 0;
};

constexpr LayoutEntry Value(LayoutField field) {
  LayoutEntry entry;
  entry.field = field;
  return entry;
}

constexpr LayoutEntry Values(LayoutField field, std::uint32_t count) {
  LayoutEntry entry = Value(field);
  entry.count = count;
  entry.stride = static_cast<std::uint32_t>(field.unit);
  return entry;
}

constexpr LayoutEntry Records(std::string_view name, std::uint32_t offset,
                              Repeat repeat,
                              std::span<const LayoutField> members) {
  LayoutEntry entry = Value(Whole(name, offset, Unit::kByte));
  entry.count = repeat.count;
  entry.stride = repeat.stride;
  entry.members = members;
  return entry;
}

// decomp: TexMapOverlay (src/BATTLE/BATTLE.PRG/entity.h:259)
constexpr std::array kOverlayFields{
    Whole("x", 0, Unit::kByte),
    Whole("y", 1, Unit::kByte),
    Whole("width", 2, Unit::kByte),
    Whole("height", 3, Unit::kByte),
};
// decomp: D_800F4538_unk614 (src/BATTLE/BATTLE.PRG/entity.h:266)
constexpr std::array kCellOriginFields{
    Whole("x", 0, Unit::kByte),
    Whole("y", 1, Unit::kByte),
    Whole("columns", 2, Unit::kByte),
    Whole("reserved3", 3, Unit::kByte),
};
// decomp: D_800F4538_unk61C (src/BATTLE/BATTLE.PRG/entity.h:280)
constexpr std::array kScrollBoundsFields{
    Whole("x", 0, Unit::kByte),
    Whole("y", 1, Unit::kByte),
    Whole("width", 2, Unit::kByte),
    Whole("height", 3, Unit::kByte),
    Whole("initial_step_x", 4, Unit::kByte),
    Whole("initial_step_y", 5, Unit::kByte),
    Flag("bounces", 6, Unit::kByte, 0),
    Bits("reserved_bits1", 6, Unit::kByte, BitRange{.shift = 1, .bits = 7}),
    Whole("interval", 7, Unit::kByte),
};

// The block's fields as the actor record names them (ActorRecord.hpp,
// texture_overlays through reserved6d4).
constexpr std::array kLayout{
    Records("texture_overlays", 0x00, Repeat{.count = 8, .stride = 4},
            kOverlayFields),
    Records("texture_cell_origins", 0x20, Repeat{.count = 2, .stride = 4},
            kCellOriginFields),
    Records("texture_scroll_bounds", 0x28, Repeat{.count = 4, .stride = 8},
            kScrollBoundsFields),
    Value(Whole("collision_radius", 0x48, Unit::kHalfword)),
    Value(Whole("collision_height", 0x4A, Unit::kHalfword)),
    Value(Whole("occupancy_radius", 0x4C, Unit::kHalfword)),
    Value(Whole("occupancy_height", 0x4E, Unit::kHalfword)),
    Value(Whole("reserved644", 0x50, Unit::kHalfword)),
    Value(Whole("target_point_height", 0x52, Unit::kHalfword)),
    Value(Bits("reserved648", 0x54, Unit::kWord,
               BitRange{.shift = 0, .bits = 16})),
    Value(Flag("floor_lit", 0x54, Unit::kWord, 16)),
    Value(Bits("reserved648_bits17", 0x54, Unit::kWord,
               BitRange{.shift = 17, .bits = 15})),
    Value(Whole("shadow_radius", 0x58, Unit::kHalfword)),
    Value(Whole("shadow_radius_expand_rate", 0x5A, Unit::kHalfword)),
    Value(Whole("shadow_radius_shrink_rate", 0x5C, Unit::kHalfword)),
    Value(Whole("reserved652", 0x5E, Unit::kHalfword)),
    Value(Whole("reserved654", 0x60, Unit::kHalfword)),
    Value(Whole("menu_camera_distance", 0x62, Unit::kHalfword)),
    Value(Whole("reserved658", 0x64, Unit::kHalfword)),
    Value(SignedWhole("anchor_height", 0x66, Unit::kHalfword)),
    Value(SignedWhole("reserved65c", 0x68, Unit::kWord)),
    Value(SignedWhole("reserved660", 0x6C, Unit::kWord)),
    Values(Whole("battle_sequence_lbas", 0x70, Unit::kWord), 12),
    Values(Whole("battle_sequence_sizes", 0xA0, Unit::kHalfword), 12),
    Values(Whole("special_sequence_lbas", 0xB8, Unit::kWord), 4),
    Values(SignedWhole("reserved6bc", 0xC8, Unit::kWord), 4),
    Values(Whole("special_sequence_sizes", 0xD8, Unit::kHalfword), 4),
    Values(SignedWhole("reserved6d4", 0xE0, Unit::kWord), 2),
};

consteval bool LayoutCoversEveryBitOnce() {
  std::array<std::uint8_t, kShpLayoutBlockBytes * kByteBits> uses{};
  const auto mark = [&uses](const LayoutField& field, std::size_t offset) {
    for (std::uint32_t bit = 0; bit < FieldBits(field); ++bit) {
      ++uses[(offset * kByteBits) + field.shift + bit];
    }
  };
  for (const LayoutEntry& entry : kLayout) {
    const std::uint32_t count = entry.count == 0 ? 1 : entry.count;
    for (std::uint32_t index = 0; index < count; ++index) {
      const std::size_t base =
          entry.field.offset + (std::size_t{index} * entry.stride);
      if (entry.members.empty()) {
        mark(entry.field, base);
        continue;
      }
      for (const LayoutField& member : entry.members) {
        mark(member, base + member.offset);
      }
    }
  }
  return std::ranges::all_of(uses, [](std::uint8_t used) { return used == 1; });
}
static_assert(LayoutCoversEveryBitOnce(),
              "every bit of the layout block must be in exactly one field");

Json FieldJson(const LayoutField& field, const Layout& layout,
               std::size_t offset) {
  const std::uint32_t bits = FieldBits(field);
  const std::uint64_t unit = LittleEndian(
      std::span(layout).subspan(offset, static_cast<std::size_t>(field.unit)));
  const std::uint64_t raw = (unit >> field.shift) & FieldMask(bits);
  if (field.flag) return Json(raw != 0);
  if (field.is_signed) {
    const std::uint64_t sign = std::uint64_t{1} << (bits - 1);
    return Json(static_cast<std::int64_t>(raw ^ sign) -
                static_cast<std::int64_t>(sign));
  }
  return Json(raw);
}

Check WriteField(const LayoutField& field, const Json& value,
                 const std::string& where, std::size_t offset, Layout& layout) {
  const std::uint32_t bits = FieldBits(field);
  std::uint64_t raw = 0;
  if (field.flag) {
    if (!value.is_boolean()) {
      return std::unexpected(std::format("{} must be true or false", where));
    }
    raw = value.get<bool>() ? 1 : 0;
  } else {
    const IntegerRange range =
        field.is_signed
            ? IntegerRange{.low =
                               -static_cast<std::int64_t>(FieldMask(bits - 1)) -
                               1,
                           .high =
                               static_cast<std::int64_t>(FieldMask(bits - 1))}
            : IntegerRange{.low = 0,
                           .high = static_cast<std::int64_t>(FieldMask(bits))};
    auto number = IntegerIn(value, range, where);
    if (!number) return std::unexpected(number.error());
    raw = static_cast<std::uint64_t>(*number) & FieldMask(bits);
  }
  const std::uint64_t unit = raw << field.shift;
  for (std::size_t index = 0; index < static_cast<std::size_t>(field.unit);
       ++index) {
    layout[offset + index] |=
        static_cast<std::byte>((unit >> (index * kByteBits)) & kByteMask);
  }
  return {};
}

Json EntryJson(const LayoutEntry& entry, const Layout& layout) {
  if (entry.count == 0) {
    return FieldJson(entry.field, layout, entry.field.offset);
  }
  Json values = Json::array();
  for (std::uint32_t index = 0; index < entry.count; ++index) {
    const std::size_t base =
        entry.field.offset + (std::size_t{index} * entry.stride);
    if (entry.members.empty()) {
      values.push_back(FieldJson(entry.field, layout, base));
      continue;
    }
    Json record = Json::object();
    for (const LayoutField& member : entry.members) {
      record[std::string(member.name)] =
          FieldJson(member, layout, base + member.offset);
    }
    values.push_back(std::move(record));
  }
  return values;
}

Check RecordOf(const LayoutEntry& entry, const Json& value,
               const std::string& where, std::size_t base, Layout& layout) {
  if (auto members = CheckExactMembers(
          value, entry.members,
          [](const LayoutField& field) { return field.name; }, where);
      !members) {
    return members;
  }
  for (const LayoutField& member : entry.members) {
    const std::string name(member.name);
    if (auto written = WriteField(member, value.at(name),
                                  std::format("{}.{}", where, name),
                                  base + member.offset, layout);
        !written) {
      return written;
    }
  }
  return {};
}

Check EntryOf(const LayoutEntry& entry, const Json& value,
              const std::string& where, Layout& layout) {
  if (entry.count == 0) {
    return WriteField(entry.field, value, where, entry.field.offset, layout);
  }
  if (!value.is_array() || value.size() != entry.count) {
    return std::unexpected(
        std::format("{} must be an array of {}", where, entry.count));
  }
  for (std::uint32_t index = 0; index < entry.count; ++index) {
    const std::size_t base =
        entry.field.offset + (std::size_t{index} * entry.stride);
    const std::string element = std::format("{}[{}]", where, index);
    const Check written =
        entry.members.empty()
            ? WriteField(entry.field, value[index], element, base, layout)
            : RecordOf(entry, value[index], element, base, layout);
    if (!written) return written;
  }
  return {};
}

Result<Layout> LayoutOf(const Json& json, const std::string& where) {
  if (auto members = CheckExactMembers(
          json, std::span<const LayoutEntry>(kLayout),
          [](const LayoutEntry& entry) { return entry.field.name; }, where);
      !members) {
    return std::unexpected(members.error());
  }
  Layout layout{};
  for (const LayoutEntry& entry : kLayout) {
    const std::string name(entry.field.name);
    if (auto value = EntryOf(entry, json.at(name),
                             std::format("{}.{}", where, name), layout);
        !value) {
      return std::unexpected(value.error());
    }
  }
  return layout;
}

// --- Bones and the payload.

Json BoneJson(const ShpBone& bone) {
  return Json{
      {"length", bone.length},
      {"parent", bone.parent},
      {"reserved5", bone.reserved5},
      {"tag", bone.tag},
      {"limb_ids", bone.limb_ids},
      {"auxiliary_tag", bone.auxiliary_tag},
      {"reserved9", bone.reserved9},
      {"joint_init", bone.joint_init},
      {"pose_lock_slot", bone.pose_lock_slot},
      {"reserved13", bone.reserved13},
  };
}

Result<ShpBone> BoneOf(const Json& json, const std::string& where) {
  const JsonMembers members(json, where);
  if (auto only = members.Only({"length", "parent", "reserved5", "tag",
                                "limb_ids", "auxiliary_tag", "reserved9",
                                "joint_init", "pose_lock_slot", "reserved13"});
      !only) {
    return std::unexpected(only.error());
  }
  auto length = members.Get("length").and_then([&](const Json* value) {
    return IntegerIn(*value, kInt32Range, members.Where("length"));
  });
  if (!length) return std::unexpected(length.error());
  ShpBone bone;
  bone.length = static_cast<std::int32_t>(*length);
  if (auto read = FirstError({
          ReadUnsigned(members, "parent", bone.parent),
          ReadUnsigned(members, "reserved5", bone.reserved5),
          ReadUnsigned(members, "tag", bone.tag),
          ReadUnsigned(members, "limb_ids", bone.limb_ids),
          ReadUnsigned(members, "auxiliary_tag", bone.auxiliary_tag),
          ReadUnsigned(members, "reserved9", bone.reserved9),
          ReadUnsigned(members, "joint_init", bone.joint_init),
          ReadUnsigned(members, "pose_lock_slot", bone.pose_lock_slot),
          ReadUnsignedArray(members, "reserved13", bone.reserved13),
      });
      !read) {
    return std::unexpected(read.error());
  }
  return bone;
}

Json TexturePayload(const ShpModelDocument& document) {
  const ShpFile& shp = document.shp;
  if (shp.texture) {
    return Json{{"image", document.image_id},
                {"palette_words", shp.texture->palette_words}};
  }
  const ShpTextureSize size =
      document.lender_texture.value_or(ShpTextureSize{});
  return Json{{"lender", shp.lender.value_or(0)},
              {"uv_scale", Json::array({size.width, size.height})}};
}

// What the payload holds beyond the document: the parts' sizes, the texture's
// palette words and the buffer views of the sound and the embedded SEQ.
struct Payload {
  ShpModelDocument document;
  std::array<std::uint16_t, kShpPartCount> counts{};
  std::uint8_t palette_words = 0;
  std::optional<std::size_t> sound_view;
  std::optional<std::size_t> sequence_view;
};

Check ReadIdentity(const JsonMembers& members, ShpModelDocument& document) {
  auto schema = members.String("schema");
  if (!schema) return std::unexpected(schema.error());
  if (*schema != kShpModelSchema) {
    return std::unexpected(
        std::format("{} is \"{}\"; a character model is \"{}\"",
                    members.Where("schema"), *schema, kShpModelSchema));
  }
  auto version = members.Unsigned("schema_version", kU32Max);
  if (!version) return std::unexpected(version.error());
  if (*version != kShpModelSchemaVersion) {
    return std::unexpected(std::format("{} is {}; this reader takes {}",
                                       members.Where("schema_version"),
                                       *version, kShpModelSchemaVersion));
  }
  auto id = members.String("id");
  auto source = members.String("source");
  auto sha256 = members.String("source_sha256");
  if (!id || !IsContentId(*id)) {
    return std::unexpected(
        std::format("{} must be lowercase letters, digits, '.', '_' or '-'",
                    members.Where("id")));
  }
  if (!source || source->empty()) {
    return std::unexpected(
        std::format("{} must name the disc file", members.Where("source")));
  }
  if (!sha256 || !IsLowercaseSha256(*sha256)) {
    return std::unexpected(std::format("{} must be a lowercase SHA-256",
                                       members.Where("source_sha256")));
  }
  document.id = std::move(*id);
  document.source = std::move(*source);
  document.source_sha256 = std::move(*sha256);
  return {};
}

Check ReadGeometryHeader(const JsonMembers& members, Payload& payload) {
  ShpFile& shp = payload.document.shp;
  auto colored = members.Boolean("colored");
  if (!colored) return std::unexpected(colored.error());
  shp.colored = *colored;
  auto sound = OptionalIndex(members, "sound");
  if (!sound) return std::unexpected(sound.error());
  auto sequence = OptionalIndex(members, "sequence");
  if (!sequence) return std::unexpected(sequence.error());
  payload.sound_view = *sound;
  payload.sequence_view = *sequence;
  return FirstError({
      ReadUnsignedArray(members, "polygon_counts", payload.counts),
      ReadUnsignedArray(members, "limb_ranges", shp.limb_ranges),
      ReadUnsigned(members, "sequence_word", shp.sequence_word),
  });
}

Check ReadBones(const JsonMembers& members, ShpFile& shp) {
  auto bones = members.Get("bones");
  if (!bones) return std::unexpected(bones.error());
  const Json& list = **bones;
  if (!list.is_array() || list.empty() || list.size() > kUnitMax) {
    return std::unexpected(std::format("{} must list 1 to {} bones",
                                       members.Where("bones"), kUnitMax));
  }
  for (std::size_t index = 0; index < list.size(); ++index) {
    auto bone = BoneOf(list[index],
                       std::format("{}[{}]", members.Where("bones"), index));
    if (!bone) return std::unexpected(bone.error());
    shp.bones.push_back(*bone);
  }
  return {};
}

Check ReadLayout(const JsonMembers& members, ShpFile& shp) {
  auto value = members.Get("layout");
  if (!value) return std::unexpected(value.error());
  auto layout = LayoutOf(**value, members.Where("layout"));
  if (!layout) return std::unexpected(layout.error());
  shp.layout = *layout;
  return {};
}

Check ReadOwnTexturePayload(const JsonMembers& texture, Payload& payload) {
  if (auto only = texture.Only({"image", "palette_words"}); !only) {
    return only;
  }
  auto image = texture.String("image");
  if (!image || !IsContentId(*image)) {
    return std::unexpected(std::format("{} must be the texture's image id",
                                       texture.Where("image")));
  }
  payload.document.image_id = std::move(*image);
  return ReadUnsigned(texture, "palette_words", payload.palette_words);
}

Check ReadBorrowedTexturePayload(const JsonMembers& texture, Payload& payload) {
  if (auto only = texture.Only({"lender", "uv_scale"}); !only) {
    return only;
  }
  auto lender = texture.Get("lender").and_then([&](const Json* value) {
    return IntegerIn(*value, kInt32Range, texture.Where("lender"));
  });
  if (!lender) return std::unexpected(lender.error());
  std::array<std::uint32_t, 2> scale{};
  if (auto read = ReadUnsignedArray(texture, "uv_scale", scale); !read) {
    return read;
  }
  if (scale[0] == 0 || scale[1] == 0) {
    return std::unexpected(
        std::format("{} must be the lender's texture width and height",
                    texture.Where("uv_scale")));
  }
  payload.document.shp.lender = static_cast<std::int32_t>(*lender);
  payload.document.lender_texture =
      ShpTextureSize{.width = scale[0], .height = scale[1]};
  return {};
}

Check ReadTexturePayload(const JsonMembers& members, Payload& payload) {
  auto value = members.Get("texture");
  if (!value) return std::unexpected(value.error());
  const JsonMembers texture(**value, members.Where("texture"));
  return texture.Find("image") != nullptr
             ? ReadOwnTexturePayload(texture, payload)
             : ReadBorrowedTexturePayload(texture, payload);
}

// --- The glTF file.

struct BufferView {
  std::size_t offset = 0;
  std::size_t length = 0;
  std::size_t stride = 0;  // 0: tightly packed
};

struct GltfFile {
  std::string name;
  Json json;
  Bytes buffer;
  std::vector<BufferView> views;
};

Check CheckAsset(const GltfFile& file) {
  const JsonMembers top(file.json, file.name);
  if (auto only =
          top.Only({"asset", "extensionsUsed", "scene", "scenes", "nodes",
                    "skins", "meshes", "materials", "textures", "samplers",
                    "images", "accessors", "bufferViews", "buffers"});
      !only) {
    return only;
  }
  auto asset = top.Get("asset");
  if (!asset) return std::unexpected(asset.error());
  const JsonMembers members(**asset, top.Where("asset"));
  if (auto only = members.Only({"version", "generator", "copyright", "extras"});
      !only) {
    return only;
  }
  auto version = members.String("version");
  if (!version) return std::unexpected(version.error());
  if (*version != "2.0") {
    return std::unexpected(
        std::format("{} is \"{}\"; the reader takes glTF \"2.0\"",
                    members.Where("version"), *version));
  }
  return {};
}

Check ReadBuffer(std::string_view stem, const ImageFileReader& read,
                 GltfFile& file) {
  const JsonMembers top(file.json, file.name);
  auto buffers = top.Get("buffers");
  if (!buffers) return std::unexpected(buffers.error());
  if (!(*buffers)->is_array() || (*buffers)->size() != 1) {
    return std::unexpected(
        std::format("{} must hold exactly one buffer", top.Where("buffers")));
  }
  const JsonMembers buffer((**buffers)[0], top.Where("buffers") + "[0]");
  if (auto only = buffer.Only({"uri", "byteLength", "name"}); !only) {
    return only;
  }
  const std::string bin = NameOf(stem, kGltfBufferSuffix);
  auto uri = buffer.String("uri");
  if (!uri || *uri != bin) {
    return std::unexpected(
        std::format("{} must be \"{}\"", buffer.Where("uri"), bin));
  }
  auto length = buffer.Unsigned("byteLength", kMaxBufferBytes);
  if (!length) return std::unexpected(length.error());
  auto bytes = ReadFile(read, bin);
  if (!bytes) return std::unexpected(bytes.error());
  if (bytes->size() < *length || bytes->size() - *length > kMaxBufferPadding) {
    return std::unexpected(std::format("{} has {} bytes; {} says {}", bin,
                                       bytes->size(),
                                       buffer.Where("byteLength"), *length));
  }
  bytes->resize(*length);
  file.buffer = std::move(*bytes);
  return {};
}

Result<BufferView> ReadView(const Json& json, const std::string& where,
                            std::size_t buffer_bytes) {
  const JsonMembers members(json, where);
  if (auto only = members.Only({"buffer", "byteOffset", "byteLength",
                                "byteStride", "target", "name"});
      !only) {
    return std::unexpected(only.error());
  }
  auto buffer = members.Unsigned("buffer", 0);
  auto offset = OptionalUnsigned(members, "byteOffset", kMaxBufferBytes);
  auto length = members.Unsigned("byteLength", kMaxBufferBytes);
  auto stride = OptionalUnsigned(members, "byteStride", kMaxStride);
  auto target = OptionalUnsigned(members, "target", kU32Max);
  for (const auto* failed : {&buffer, &offset, &length, &stride, &target}) {
    if (!*failed) return std::unexpected(failed->error());
  }
  if (*offset + *length > buffer_bytes) {
    return std::unexpected(
        std::format("{} runs past the buffer's {} bytes", where, buffer_bytes));
  }
  if (*stride != 0 &&
      (*stride < kStrideAlignment || *stride % kStrideAlignment != 0)) {
    return std::unexpected(
        std::format("{} must be a multiple of {} from {} to {}",
                    members.Where("byteStride"), kStrideAlignment,
                    kStrideAlignment, kMaxStride));
  }
  return BufferView{.offset = *offset, .length = *length, .stride = *stride};
}

Check ReadViews(GltfFile& file) {
  const JsonMembers top(file.json, file.name);
  auto views = top.Get("bufferViews");
  if (!views) return std::unexpected(views.error());
  if (!(*views)->is_array()) {
    return std::unexpected(
        std::format("{} must be an array", top.Where("bufferViews")));
  }
  for (std::size_t index = 0; index < (*views)->size(); ++index) {
    auto view = ReadView((**views)[index],
                         std::format("{}[{}]", top.Where("bufferViews"), index),
                         file.buffer.size());
    if (!view) return std::unexpected(view.error());
    file.views.push_back(*view);
  }
  return {};
}

Result<GltfFile> ReadGltf(std::string_view stem, const ImageFileReader& read) {
  GltfFile file;
  file.name = NameOf(stem, kGltfSuffix);
  auto text = ReadFile(read, file.name);
  if (!text) return std::unexpected(text.error());
  auto json = ParseJsonText(*text, file.name);
  if (!json) return std::unexpected(json.error());
  file.json = std::move(*json);
  if (auto asset = CheckAsset(file); !asset) {
    return std::unexpected(asset.error());
  }
  if (auto buffer = ReadBuffer(stem, read, file); !buffer) {
    return std::unexpected(buffer.error());
  }
  if (auto views = ReadViews(file); !views) {
    return std::unexpected(views.error());
  }
  return file;
}

Result<Payload> ReadPayload(const GltfFile& file) {
  const JsonMembers top(file.json, file.name);
  const JsonMembers asset(file.json.at("asset"), top.Where("asset"));
  auto extras = asset.Get("extras");
  if (!extras) return std::unexpected(extras.error());
  const JsonMembers extra(**extras, asset.Where("extras"));
  if (auto only = extra.Only({kShpExtrasKey}); !only) {
    return std::unexpected(only.error());
  }
  auto value = extra.Get(kShpExtrasKey);
  if (!value) return std::unexpected(value.error());
  const JsonMembers members(**value, extra.Where(kShpExtrasKey));
  if (auto only = members.Only({"schema", "schema_version", "id", "source",
                                "source_sha256", "colored", "polygon_counts",
                                "limb_ranges", "layout", "bones", "sound",
                                "sequence_word", "sequence", "texture"});
      !only) {
    return std::unexpected(only.error());
  }
  Payload payload;
  if (auto read = FirstError({
          ReadIdentity(members, payload.document),
          ReadGeometryHeader(members, payload),
          ReadBones(members, payload.document.shp),
          ReadLayout(members, payload.document.shp),
          ReadTexturePayload(members, payload),
      });
      !read) {
    return std::unexpected(read.error());
  }
  return payload;
}

// --- Accessors.

// The component encodings an accessor rule allows.
constexpr std::uint32_t kFloatBit = 1U << 0U;
constexpr std::uint32_t kByteBit = 1U << 1U;
constexpr std::uint32_t kShortBit = 1U << 2U;
constexpr std::uint32_t kIntBit = 1U << 3U;
constexpr std::uint32_t kUnitByteBit = 1U << 4U;
constexpr std::uint32_t kUnitShortBit = 1U << 5U;
constexpr std::uint32_t kUnitIntegerBits = kUnitByteBit | kUnitShortBit;
constexpr std::uint32_t kUnitEncodings = kFloatBit | kUnitIntegerBits;
constexpr std::uint32_t kIntegerBits = kByteBit | kShortBit | kIntBit;

struct AccessorRule {
  std::string_view type;
  std::string_view other_type;  // a second type it may have, or empty
  std::uint32_t encodings = 0;
};

constexpr AccessorRule kPositionRule{"VEC3", {}, kFloatBit};
constexpr AccessorRule kTexcoordRule{"VEC2", {}, kUnitEncodings};
constexpr AccessorRule kColorRule{"VEC3", "VEC4", kUnitEncodings};
constexpr AccessorRule kJointsRule{"VEC4", {}, kByteBit | kShortBit};
constexpr AccessorRule kWeightsRule{"VEC4", {}, kUnitEncodings};
constexpr AccessorRule kVertexRule{"SCALAR", {}, kFloatBit | kIntegerBits};
constexpr AccessorRule kIndicesRule{"SCALAR", {}, kIntegerBits};
constexpr AccessorRule kMatrixRule{"MAT4", {}, kFloatBit};

struct AccessorType {
  std::string_view name;
  std::size_t components = 0;
};
constexpr std::array kAccessorTypes{
    AccessorType{.name = "SCALAR", .components = 1},
    AccessorType{.name = "VEC2", .components = 2},
    AccessorType{.name = "VEC3", .components = 3},
    AccessorType{.name = "VEC4", .components = 4},
    AccessorType{.name = "MAT4", .components = kShpMatrixElements},
};

std::size_t ComponentsOf(std::string_view type) {
  for (const AccessorType& entry : kAccessorTypes) {
    if (entry.name == type) return entry.components;
  }
  return 0;
}

std::uint32_t EncodingOf(std::uint64_t component_type, bool normalized) {
  switch (component_type) {
    case kGltfFloat:
      return normalized ? 0 : kFloatBit;
    case kGltfUnsignedByte:
      return normalized ? kUnitByteBit : kByteBit;
    case kGltfUnsignedShort:
      return normalized ? kUnitShortBit : kShortBit;
    case kGltfUnsignedInt:
      return normalized ? 0 : kIntBit;
    default:
      return 0;
  }
}

std::size_t ComponentBytes(std::uint32_t component_type) {
  switch (component_type) {
    case kGltfUnsignedByte:
      return sizeof(std::uint8_t);
    case kGltfUnsignedShort:
      return sizeof(std::uint16_t);
    default:
      return sizeof(std::uint32_t);
  }
}

struct AccessorLayout {
  std::string where;
  std::size_t start = 0;  // the first element's first byte in the buffer
  std::size_t count = 0;
  std::size_t components = 0;
  std::size_t stride = 0;
  std::uint32_t component_type = 0;
  bool normalized = false;
};

Check ReadAccessorFormat(const JsonMembers& members, const AccessorRule& rule,
                         AccessorLayout& layout) {
  auto component_type = members.Unsigned("componentType", kU32Max);
  if (!component_type) return std::unexpected(component_type.error());
  bool normalized = false;
  if (members.Find("normalized") != nullptr) {
    auto value = members.Boolean("normalized");
    if (!value) return std::unexpected(value.error());
    normalized = *value;
  }
  if ((EncodingOf(*component_type, normalized) & rule.encodings) == 0) {
    return std::unexpected(std::format(
        "{} has componentType {}{}, which this attribute cannot use",
        layout.where, *component_type, normalized ? " (normalized)" : ""));
  }
  auto type = members.String("type");
  if (!type) return std::unexpected(type.error());
  if (*type != rule.type &&
      (rule.other_type.empty() || *type != rule.other_type)) {
    return std::unexpected(
        std::format("{} must be \"{}\"", members.Where("type"), rule.type));
  }
  auto count = members.Unsigned("count", kMaxElements);
  if (!count || *count == 0) {
    return std::unexpected(std::format("{} must be from 1 to {}",
                                       members.Where("count"), kMaxElements));
  }
  layout.component_type = static_cast<std::uint32_t>(*component_type);
  layout.normalized = normalized;
  layout.components = ComponentsOf(*type);
  layout.count = *count;
  return {};
}

Check PlaceAccessor(const GltfFile& file, const JsonMembers& members,
                    AccessorLayout& layout) {
  auto view_index = members.Unsigned("bufferView", kU32Max);
  if (!view_index) return std::unexpected(view_index.error());
  if (*view_index >= file.views.size()) {
    return std::unexpected(
        std::format("{} names buffer view {}, which does not "
                    "exist",
                    layout.where, *view_index));
  }
  const BufferView& view = file.views[*view_index];
  auto offset = OptionalUnsigned(members, "byteOffset", kMaxBufferBytes);
  if (!offset) return std::unexpected(offset.error());
  const std::size_t component = ComponentBytes(layout.component_type);
  const std::size_t element = component * layout.components;
  layout.stride = view.stride != 0 ? view.stride : element;
  layout.start = view.offset + *offset;
  if (layout.start % component != 0 || layout.stride % component != 0 ||
      layout.stride < element) {
    return std::unexpected(
        std::format("{} is not aligned to its {}-byte components", layout.where,
                    component));
  }
  if (*offset + ((layout.count - 1) * layout.stride) + element > view.length) {
    return std::unexpected(std::format(
        "{} runs past its buffer view's {} bytes", layout.where, view.length));
  }
  return {};
}

Result<AccessorLayout> AccessorLayoutOf(const GltfFile& file, std::size_t index,
                                        const AccessorRule& rule) {
  const JsonMembers top(file.json, file.name);
  auto accessors = top.Get("accessors");
  if (!accessors) return std::unexpected(accessors.error());
  if (!(*accessors)->is_array() || index >= (*accessors)->size()) {
    return std::unexpected(
        std::format("{} has no accessor {}", top.Where("accessors"), index));
  }
  AccessorLayout layout;
  layout.where = std::format("{}[{}]", top.Where("accessors"), index);
  const JsonMembers members((**accessors)[index], layout.where);
  if (auto only =
          members.Only({"bufferView", "byteOffset", "componentType",
                        "normalized", "count", "type", "max", "min", "name"});
      !only) {
    return std::unexpected(only.error());
  }
  if (auto format = ReadAccessorFormat(members, rule, layout); !format) {
    return std::unexpected(format.error());
  }
  if (auto placed = PlaceAccessor(file, members, layout); !placed) {
    return std::unexpected(placed.error());
  }
  return layout;
}

Result<double> ComponentValue(std::span<const std::byte> buffer, std::size_t at,
                              const AccessorLayout& layout) {
  const std::uint32_t raw =
      LittleEndian(buffer.subspan(at, ComponentBytes(layout.component_type)));
  switch (layout.component_type) {
    case kGltfFloat: {
      const auto value = std::bit_cast<float>(raw);
      if (!std::isfinite(value)) {
        return std::unexpected(std::string("is not a finite number"));
      }
      return double{value};
    }
    case kGltfUnsignedByte:
      return layout.normalized ? raw / kByteMaximum : static_cast<double>(raw);
    case kGltfUnsignedShort:
      return layout.normalized ? raw / kShortMaximum : static_cast<double>(raw);
    default:
      return static_cast<double>(raw);
  }
}

// An accessor's values, normalized ones scaled to 0 to 1.
struct AccessorValues {
  std::size_t components = 0;
  std::vector<double> values;

  [[nodiscard]] std::size_t Count() const {
    return components == 0 ? 0 : values.size() / components;
  }
  [[nodiscard]] double At(std::size_t element, std::size_t component) const {
    return values[(element * components) + component];
  }
};

Result<AccessorValues> ReadAccessor(const GltfFile& file, std::size_t index,
                                    const AccessorRule& rule) {
  auto layout = AccessorLayoutOf(file, index, rule);
  if (!layout) return std::unexpected(layout.error());
  AccessorValues result;
  result.components = layout->components;
  result.values.reserve(layout->count * layout->components);
  const std::size_t component = ComponentBytes(layout->component_type);
  for (std::size_t element = 0; element < layout->count; ++element) {
    for (std::size_t part = 0; part < layout->components; ++part) {
      const std::size_t at =
          layout->start + (element * layout->stride) + (part * component);
      auto value = ComponentValue(file.buffer, at, *layout);
      if (!value) {
        return std::unexpected(std::format("{} element {} {}", layout->where,
                                           element, value.error()));
      }
      result.values.push_back(*value);
    }
  }
  return result;
}

// The whole number `value` is, when it is one below `limit`.
std::optional<std::uint32_t> WholeBelow(double value, std::uint32_t limit) {
  if (!(value >= 0) || value >= limit || value != std::floor(value)) {
    return std::nullopt;
  }
  return static_cast<std::uint32_t>(value);
}

// --- The skeleton.

Result<std::size_t> SkinMatrices(const JsonMembers& top) {
  auto skins = top.Get("skins");
  if (!skins) return std::unexpected(skins.error());
  if (!(*skins)->is_array() || (*skins)->size() != 1) {
    return std::unexpected(
        std::format("{} must hold exactly one skin", top.Where("skins")));
  }
  const JsonMembers skin((**skins)[0], top.Where("skins") + "[0]");
  auto matrices = skin.Unsigned("inverseBindMatrices", kU32Max);
  if (!matrices) return std::unexpected(matrices.error());
  return static_cast<std::size_t>(*matrices);
}

// Whether two values are the same JSON, whatever their member order and
// whether their numbers are written as integers or floats.
bool SameJson(const Json& left, const Json& right) {
  return nlohmann::json::parse(left.dump()) ==
         nlohmann::json::parse(right.dump());
}

Check CheckInverseBindMatrices(const GltfFile& file, std::size_t accessor,
                               const std::vector<std::int64_t>& origins) {
  auto matrices = ReadAccessor(file, accessor, kMatrixRule);
  if (!matrices) return std::unexpected(matrices.error());
  if (matrices->Count() != origins.size()) {
    return std::unexpected(std::format(
        "{}.accessors[{}] holds {} inverse bind matrices for {} bones",
        file.name, accessor, matrices->Count(), origins.size()));
  }
  for (std::size_t bone = 0; bone < origins.size(); ++bone) {
    const auto matrix = ShpInverseBindMatrix(origins[bone]);
    for (std::size_t element = 0; element < kShpMatrixElements; ++element) {
      if (matrices->At(bone, element) != double{matrix[element]}) {
        return std::unexpected(std::format(
            "{}.accessors[{}] matrix {} is not bone {}'s inverse bind matrix "
            "(a move by {} along X)",
            file.name, accessor, bone, bone, -origins[bone]));
      }
    }
  }
  return {};
}

Check CheckSkeleton(const GltfFile& file, const ShpFile& shp,
                    const std::vector<std::int64_t>& origins) {
  const JsonMembers top(file.json, file.name);
  auto matrices = SkinMatrices(top);
  if (!matrices) return std::unexpected(matrices.error());
  const Json skeleton = ShpSkeletonGltf(shp, *matrices);
  for (const auto& member : skeleton.items()) {
    const Json* actual = top.Find(member.key());
    if (actual == nullptr || !SameJson(*actual, member.value())) {
      return std::unexpected(std::format(
          "{} is not the skeleton asset.extras.vs.bones gives; edit the bones "
          "there",
          top.Where(member.key())));
    }
  }
  return CheckInverseBindMatrices(file, *matrices, origins);
}

// --- The mesh.

// One glTF vertex: a polygon corner.
struct Corner {
  std::uint16_t vertex = 0;
  std::uint16_t uv = 0;
  std::uint32_t color = 0;

  bool operator==(const Corner&) const = default;
};

// What every corner of one SHP vertex agrees on.
struct VertexRecord {
  bool seen = false;
  std::uint16_t bone = 0;
  ShpVertex position;
};

struct MeshState {
  const std::vector<std::int64_t>* origins = nullptr;
  ShpTextureSize uv_scale;
  bool colored = false;
  std::vector<VertexRecord> vertices;               // by SHP vertex number
  std::vector<std::optional<ShpPolygon>> polygons;  // by ordinal
};

struct Attributes {
  AccessorValues position;
  AccessorValues joints;
  AccessorValues weights;
  AccessorValues vertex;
  AccessorValues uv;
  std::optional<AccessorValues> color;
};

Result<AccessorValues> ReadAttribute(const GltfFile& file,
                                     const JsonMembers& attributes,
                                     std::string_view name,
                                     const AccessorRule& rule) {
  auto index = attributes.Unsigned(name, kU32Max);
  if (!index) return std::unexpected(index.error());
  return ReadAccessor(file, *index, rule);
}

Check CheckSkinSets(const Json& attributes, const std::string& where) {
  if (!attributes.is_object()) {
    return std::unexpected(std::format("{} must be an object", where));
  }
  for (const auto& member : attributes.items()) {
    const std::string& key = member.key();
    if ((key.starts_with("JOINTS_") && key != "JOINTS_0") ||
        (key.starts_with("WEIGHTS_") && key != "WEIGHTS_0")) {
      return std::unexpected(std::format(
          "{} has {}; each vertex has one bone, in JOINTS_0 and WEIGHTS_0",
          where, key));
    }
  }
  return {};
}

Result<Attributes> ReadAttributes(const GltfFile& file, const Json& json,
                                  const std::string& where, bool colored) {
  if (auto sets = CheckSkinSets(json, where); !sets) {
    return std::unexpected(sets.error());
  }
  const JsonMembers members(json, where);
  auto position = ReadAttribute(file, members, "POSITION", kPositionRule);
  auto joints = ReadAttribute(file, members, "JOINTS_0", kJointsRule);
  auto weights = ReadAttribute(file, members, "WEIGHTS_0", kWeightsRule);
  auto vertex = ReadAttribute(file, members, kShpVertexAttribute, kVertexRule);
  auto uv = ReadAttribute(file, members, "TEXCOORD_0", kTexcoordRule);
  for (const auto* read : {&position, &joints, &weights, &vertex, &uv}) {
    if (!*read) return std::unexpected(read->error());
  }
  Attributes attributes{.position = std::move(*position),
                        .joints = std::move(*joints),
                        .weights = std::move(*weights),
                        .vertex = std::move(*vertex),
                        .uv = std::move(*uv),
                        .color = std::nullopt};
  if (colored) {
    auto color = ReadAttribute(file, members, "COLOR_0", kColorRule);
    if (!color) return std::unexpected(color.error());
    attributes.color = std::move(*color);
  }
  const std::size_t count = attributes.position.Count();
  if (attributes.joints.Count() != count ||
      attributes.weights.Count() != count ||
      attributes.vertex.Count() != count || attributes.uv.Count() != count ||
      (attributes.color && attributes.color->Count() != count)) {
    return std::unexpected(
        std::format("{}' accessors hold different numbers of vertices", where));
  }
  return attributes;
}

Result<std::uint16_t> BoneAt(const Attributes& attributes, std::size_t index,
                             std::size_t bones) {
  std::optional<std::size_t> heavy;
  for (std::size_t slot = 0; slot < kSkinSlots; ++slot) {
    const double weight = attributes.weights.At(index, slot);
    if (!heavy && std::abs(weight - 1.0) <= kWeightTolerance) {
      heavy = slot;
      continue;
    }
    if (std::abs(weight) > kWeightTolerance) {
      return std::unexpected(std::format(
          "WEIGHTS_0 slot {} is {}; each vertex has one bone, weighted 1", slot,
          weight));
    }
  }
  if (!heavy) {
    return std::unexpected(
        std::string("WEIGHTS_0 has no weight of 1; each vertex has one bone"));
  }
  const double joint = attributes.joints.At(index, *heavy);
  const std::optional<std::uint32_t> bone =
      WholeBelow(joint, static_cast<std::uint32_t>(bones));
  if (!bone) {
    return std::unexpected(
        std::format("JOINTS_0 {} is not a bone below {}", joint, bones));
  }
  return static_cast<std::uint16_t>(*bone);
}

Result<ShpVertex> PositionAt(const AccessorValues& positions, std::size_t index,
                             std::int64_t origin) {
  std::array<std::int64_t, kAxes> local{};
  for (std::size_t axis = 0; axis < kAxes; ++axis) {
    const double value = positions.At(index, axis);
    if (std::abs(value) > kMaxPosition) {
      return std::unexpected(std::format("POSITION {} is out of range", value));
    }
    local[axis] = std::llround(value) - (axis == 0 ? origin : 0);
    if (local[axis] < std::numeric_limits<std::int16_t>::min() ||
        local[axis] > std::numeric_limits<std::int16_t>::max()) {
      return std::unexpected(std::format(
          "POSITION is {} from its bone's origin; an SHP vertex holds {} to {}",
          local[axis], std::numeric_limits<std::int16_t>::min(),
          std::numeric_limits<std::int16_t>::max()));
    }
  }
  return ShpVertex{.x = static_cast<std::int16_t>(local[0]),
                   .y = static_cast<std::int16_t>(local[1]),
                   .z = static_cast<std::int16_t>(local[2])};
}

Result<std::uint16_t> UvAt(const AccessorValues& uvs, std::size_t index,
                           ShpTextureSize scale) {
  const double u = std::round(uvs.At(index, 0) * scale.width);
  const double v = std::round(uvs.At(index, 1) * scale.height);
  if (u < 0 || u > kUvMaximum || v < 0 || v > kUvMaximum) {
    return std::unexpected(std::format(
        "TEXCOORD_0 is texel ({}, {}) of a {} x {} texture; an SHP texel is 0 "
        "to {} each way",
        u, v, scale.width, scale.height, kUvMaximum));
  }
  return static_cast<std::uint16_t>(
      static_cast<std::uint32_t>(u) |
      (static_cast<std::uint32_t>(v) << kByteBits));
}

Result<std::uint32_t> ColorAt(const AccessorValues& colors, std::size_t index) {
  std::uint32_t color = 0;
  for (std::size_t channel = 0; channel < kColorChannels; ++channel) {
    const double value = std::round(colors.At(index, channel) * kByteMaximum);
    if (value < 0 || value > kByteMaximum) {
      return std::unexpected(std::string("COLOR_0 is outside 0 to 1"));
    }
    color |= static_cast<std::uint32_t>(value) << (channel * kByteBits);
  }
  return color;
}

Check Agree(MeshState& mesh, std::uint16_t vertex, std::uint16_t bone,
            const ShpVertex& position) {
  if (vertex >= mesh.vertices.size()) {
    mesh.vertices.resize(std::size_t{vertex} + 1);
  }
  VertexRecord& record = mesh.vertices[vertex];
  if (!record.seen) {
    record = VertexRecord{.seen = true, .bone = bone, .position = position};
    return {};
  }
  if (record.bone != bone || record.position != position) {
    return std::unexpected(std::format(
        "SHP vertex {} is at ({}, {}, {}) on bone {} here and ({}, {}, {}) on "
        "bone {} at another corner",
        vertex, position.x, position.y, position.z, bone, record.position.x,
        record.position.y, record.position.z, record.bone));
  }
  return {};
}

Result<Corner> CornerAt(const Attributes& attributes, std::size_t index,
                        MeshState& mesh) {
  const double number = attributes.vertex.At(index, 0);
  const std::optional<std::uint32_t> vertex = WholeBelow(number, kMaxVertices);
  if (!vertex) {
    return std::unexpected(
        std::format("{} {} is not an SHP vertex number below {}",
                    kShpVertexAttribute, number, kMaxVertices));
  }
  auto bone = BoneAt(attributes, index, mesh.origins->size());
  if (!bone) return std::unexpected(bone.error());
  auto position =
      PositionAt(attributes.position, index, (*mesh.origins)[*bone]);
  if (!position) return std::unexpected(position.error());
  auto uv = UvAt(attributes.uv, index, mesh.uv_scale);
  if (!uv) return std::unexpected(uv.error());
  Corner corner{.vertex = static_cast<std::uint16_t>(*vertex), .uv = *uv};
  if (attributes.color) {
    auto color = ColorAt(*attributes.color, index);
    if (!color) return std::unexpected(color.error());
    corner.color = *color;
  }
  if (auto agreed = Agree(mesh, corner.vertex, *bone, *position); !agreed) {
    return std::unexpected(agreed.error());
  }
  return corner;
}

Result<std::vector<Corner>> DecodeCorners(const Attributes& attributes,
                                          const std::string& where,
                                          MeshState& mesh) {
  std::vector<Corner> corners;
  const std::size_t count = attributes.position.Count();
  corners.reserve(count);
  for (std::size_t index = 0; index < count; ++index) {
    auto corner = CornerAt(attributes, index, mesh);
    if (!corner) {
      return std::unexpected(
          std::format("{} vertex {}: {}", where, index, corner.error()));
    }
    corners.push_back(*corner);
  }
  return corners;
}

Result<std::vector<std::size_t>> ReadIndices(const GltfFile& file,
                                             const JsonMembers& primitive,
                                             std::size_t corners) {
  auto accessor = primitive.Unsigned("indices", kU32Max);
  if (!accessor) return std::unexpected(accessor.error());
  auto values = ReadAccessor(file, *accessor, kIndicesRule);
  if (!values) return std::unexpected(values.error());
  std::vector<std::size_t> indices;
  indices.reserve(values->values.size());
  for (const double value : values->values) {
    const std::optional<std::uint32_t> corner =
        WholeBelow(value, static_cast<std::uint32_t>(corners));
    if (!corner) {
      return std::unexpected(
          std::format("{} holds index {}, past the primitive's {} vertices",
                      primitive.Where("indices"), value, corners));
    }
    indices.push_back(*corner);
  }
  return indices;
}

struct PolygonEntry {
  std::size_t ordinal = 0;
  std::uint8_t corners = 0;
  std::uint8_t flags = 0;
  std::uint8_t extra = 0;
};

// [ordinal, corners, flags, extra]
constexpr std::size_t kPolygonEntryValues = 4;

Result<PolygonEntry> PolygonEntryOf(const Json& json, const std::string& where,
                                    std::size_t total) {
  std::array<std::uint64_t, kPolygonEntryValues> values{};
  const bool numbers = json.is_array() && json.size() == values.size() &&
                       std::ranges::all_of(json, [](const Json& value) {
                         return value.is_number_unsigned();
                       });
  if (!numbers) {
    return std::unexpected(
        std::format("{} must be [ordinal, corners, flags, extra]", where));
  }
  for (std::size_t index = 0; index < values.size(); ++index) {
    values[index] = json[index].get<std::uint64_t>();
  }
  const auto [ordinal, corners, flags, extra] = values;
  if (ordinal >= total) {
    return std::unexpected(
        std::format("{} names polygon {}; polygon_counts adds up to {}", where,
                    ordinal, total));
  }
  if ((corners != kShpTriangleCorners && corners != kShpMaxCorners) ||
      flags > kUnitMax || extra > kUnitMax) {
    return std::unexpected(std::format(
        "{} must have 3 or 4 corners and flags and extra from 0 to {}", where,
        kUnitMax));
  }
  return PolygonEntry{.ordinal = static_cast<std::size_t>(ordinal),
                      .corners = static_cast<std::uint8_t>(corners),
                      .flags = static_cast<std::uint8_t>(flags),
                      .extra = static_cast<std::uint8_t>(extra)};
}

// The polygon `entry` describes from the corners of its triangles' indices.
Result<ShpPolygon> PolygonOf(const PolygonEntry& entry,
                             std::span<const std::size_t> indices,
                             std::span<const Corner> corners) {
  std::array<std::optional<Corner>, kShpMaxCorners> ordered;
  for (std::size_t slot = 0; slot < indices.size(); ++slot) {
    const Corner& corner = corners[indices[slot]];
    std::optional<Corner>& target = ordered[kShpIndexCorners[slot]];
    if (target && *target != corner) {
      return std::unexpected(std::string(
          "a quad's second triangle must start with its first triangle's last "
          "two corners"));
    }
    target = corner;
  }
  ShpPolygon polygon{
      .corners = entry.corners, .flags = entry.flags, .extra = entry.extra};
  for (std::size_t index = 0; index < entry.corners; ++index) {
    const Corner corner = ordered[index].value_or(Corner{});
    polygon.vertices[index] = corner.vertex;
    polygon.uvs[ShpUvSlot(entry.corners, index)] = corner.uv;
    polygon.colors[index] = corner.color;
  }
  return polygon;
}

struct PolygonList {
  const Json* list = nullptr;
  std::string where;
};

Result<PolygonList> PolygonListOf(const JsonMembers& primitive) {
  auto extras = primitive.Get("extras");
  if (!extras) return std::unexpected(extras.error());
  const JsonMembers extra(**extras, primitive.Where("extras"));
  if (auto only = extra.Only({kShpExtrasKey}); !only) {
    return std::unexpected(only.error());
  }
  auto value = extra.Get(kShpExtrasKey);
  if (!value) return std::unexpected(value.error());
  const JsonMembers members(**value, extra.Where(kShpExtrasKey));
  if (auto only = members.Only({kShpPolygonsKey}); !only) {
    return std::unexpected(only.error());
  }
  auto list = members.Get(kShpPolygonsKey);
  if (!list) return std::unexpected(list.error());
  if (!(*list)->is_array()) {
    return std::unexpected(
        std::format("{} must be an array", members.Where(kShpPolygonsKey)));
  }
  return PolygonList{.list = *list, .where = members.Where(kShpPolygonsKey)};
}

Check WalkPolygons(const PolygonList& polygons,
                   std::span<const std::size_t> indices,
                   std::span<const Corner> corners, MeshState& mesh) {
  std::size_t cursor = 0;
  for (std::size_t index = 0; index < polygons.list->size(); ++index) {
    const std::string where = std::format("{}[{}]", polygons.where, index);
    auto entry =
        PolygonEntryOf((*polygons.list)[index], where, mesh.polygons.size());
    if (!entry) return std::unexpected(entry.error());
    const std::size_t needed = entry->corners == kShpTriangleCorners
                                   ? kShpTriangleCorners
                                   : kShpIndexCorners.size();
    if (indices.size() - cursor < needed) {
      return std::unexpected(std::format("{} needs {} more indices; {} remain",
                                         where, needed,
                                         indices.size() - cursor));
    }
    auto polygon = PolygonOf(*entry, indices.subspan(cursor, needed), corners);
    if (!polygon) {
      return std::unexpected(std::format("{}: {}", where, polygon.error()));
    }
    cursor += needed;
    if (mesh.polygons[entry->ordinal]) {
      return std::unexpected(
          std::format("{} repeats polygon {}", where, entry->ordinal));
    }
    mesh.polygons[entry->ordinal] = std::move(*polygon);
  }
  if (cursor != indices.size()) {
    return std::unexpected(std::format("{} leaves {} indices in no polygon",
                                       polygons.where,
                                       indices.size() - cursor));
  }
  return {};
}

Check ReadPrimitive(const GltfFile& file, const Json& json,
                    const std::string& where, MeshState& mesh) {
  const JsonMembers members(json, where);
  if (auto only =
          members.Only({"attributes", "indices", "material", "mode", "extras"});
      !only) {
    return only;
  }
  if (members.Find("mode") != nullptr) {
    auto mode = members.Unsigned("mode", kU32Max);
    if (!mode || *mode != kGltfTriangles) {
      return std::unexpected(std::format(
          "{} must be {} (triangles)", members.Where("mode"), kGltfTriangles));
    }
  }
  auto attributes_json = members.Get("attributes");
  if (!attributes_json) return std::unexpected(attributes_json.error());
  auto attributes = ReadAttributes(file, **attributes_json,
                                   members.Where("attributes"), mesh.colored);
  if (!attributes) return std::unexpected(attributes.error());
  auto corners = DecodeCorners(*attributes, where, mesh);
  if (!corners) return std::unexpected(corners.error());
  auto indices = ReadIndices(file, members, corners->size());
  if (!indices) return std::unexpected(indices.error());
  auto polygons = PolygonListOf(members);
  if (!polygons) return std::unexpected(polygons.error());
  return WalkPolygons(*polygons, *indices, *corners, mesh);
}

Result<MeshState> ReadMesh(const GltfFile& file, const Payload& payload,
                           const std::vector<std::int64_t>& origins) {
  MeshState mesh;
  mesh.origins = &origins;
  mesh.uv_scale = ShpUvScale(payload.document).value_or(ShpTextureSize{});
  mesh.colored = payload.document.shp.colored;
  std::size_t total = 0;
  for (const std::uint16_t count : payload.counts) total += count;
  mesh.polygons.resize(total);
  const JsonMembers top(file.json, file.name);
  auto meshes = top.Get("meshes");
  if (!meshes) return std::unexpected(meshes.error());
  if (!(*meshes)->is_array() || (*meshes)->size() != 1) {
    return std::unexpected(
        std::format("{} must hold exactly one mesh", top.Where("meshes")));
  }
  const JsonMembers members((**meshes)[0], top.Where("meshes") + "[0]");
  if (auto only = members.Only({"primitives", "name"}); !only) {
    return std::unexpected(only.error());
  }
  auto primitives = members.Get("primitives");
  if (!primitives) return std::unexpected(primitives.error());
  if (!(*primitives)->is_array()) {
    return std::unexpected(
        std::format("{} must be an array", members.Where("primitives")));
  }
  for (std::size_t index = 0; index < (*primitives)->size(); ++index) {
    const std::string where =
        std::format("{}[{}]", members.Where("primitives"), index);
    if (auto read = ReadPrimitive(file, (**primitives)[index], where, mesh);
        !read) {
      return std::unexpected(read.error());
    }
  }
  return mesh;
}

// --- The SHP from the parts.

Check AssembleVertices(const MeshState& mesh, ShpFile& shp) {
  for (std::size_t vertex = 0; vertex < mesh.vertices.size(); ++vertex) {
    const VertexRecord& record = mesh.vertices[vertex];
    if (!record.seen) {
      return std::unexpected(std::format(
          "SHP vertex {} is in no polygon; {} numbers run from 0 to {} with "
          "no gaps",
          vertex, kShpVertexAttribute, mesh.vertices.size() - 1));
    }
    shp.vertices.push_back(record.position);
    const auto end = static_cast<std::uint16_t>(vertex + 1);
    if (shp.groups.empty() || shp.groups.back().bone != record.bone) {
      shp.groups.push_back(ShpGroup{.bone = record.bone, .end = end});
    } else {
      shp.groups.back().end = end;
    }
  }
  return {};
}

Check AssembleParts(MeshState& mesh,
                    const std::array<std::uint16_t, kShpPartCount>& counts,
                    ShpFile& shp) {
  std::size_t ordinal = 0;
  for (std::size_t part = 0; part < kShpPartCount; ++part) {
    for (std::size_t index = 0; index < counts[part]; ++index, ++ordinal) {
      std::optional<ShpPolygon>& polygon = mesh.polygons[ordinal];
      if (!polygon) {
        return std::unexpected(std::format(
            "polygon {} is in no primitive; polygon_counts adds up to {}",
            ordinal, mesh.polygons.size()));
      }
      shp.parts[part].push_back(std::move(*polygon));
    }
  }
  return {};
}

Result<Bytes> ViewBytes(const GltfFile& file, std::optional<std::size_t> view,
                        std::string_view what) {
  if (!view) return Bytes{};
  if (*view >= file.views.size() || file.views[*view].stride != 0) {
    return std::unexpected(std::format(
        "{}: asset.extras.vs.{} must name a buffer view without a byteStride",
        file.name, what));
  }
  const BufferView& bytes = file.views[*view];
  const auto data = std::span(file.buffer).subspan(bytes.offset, bytes.length);
  return Bytes(data.begin(), data.end());
}

Check ReadOwnTexture(std::string_view stem, const ImageFileReader& read,
                     Payload& payload) {
  ShpModelDocument& document = payload.document;
  if (document.image_id.empty()) return {};
  auto image = DecodeImageFiles(stem, read);
  if (!image) return std::unexpected(image.error());
  const std::string sidecar = NameOf(stem, kImageSidecarSuffix);
  if (image->id != document.image_id) {
    return std::unexpected(
        std::format("{} has id '{}'; asset.extras.vs.texture.image says '{}'",
                    sidecar, image->id, document.image_id));
  }
  if (image->provenance.source != document.source ||
      image->provenance.source_sha256 != document.source_sha256) {
    return std::unexpected(std::format("{} comes from {}; the model from {}",
                                       sidecar, image->provenance.source,
                                       document.source));
  }
  auto texture = ShpTextureOf(image->image, payload.palette_words);
  if (!texture) {
    return std::unexpected(
        std::format("{}: {}", NameOf(stem, kImagePngSuffix), texture.error()));
  }
  document.shp.texture = std::move(*texture);
  return {};
}

Result<ShpModelDocument> AssembleModel(const GltfFile& file, Payload& payload) {
  ShpFile& shp = payload.document.shp;
  auto origins = ShpBoneOrigins(shp);
  if (!origins) {
    return std::unexpected(std::format("{}: asset.extras.vs.bones: {}",
                                       file.name, origins.error()));
  }
  if (auto skeleton = CheckSkeleton(file, shp, *origins); !skeleton) {
    return std::unexpected(skeleton.error());
  }
  auto mesh = ReadMesh(file, payload, *origins);
  if (!mesh) return std::unexpected(mesh.error());
  auto sound = ViewBytes(file, payload.sound_view, "sound");
  if (!sound) return std::unexpected(sound.error());
  auto sequence = ViewBytes(file, payload.sequence_view, "sequence");
  if (!sequence) return std::unexpected(sequence.error());
  if (auto assembled = FirstError({AssembleVertices(*mesh, shp),
                                   AssembleParts(*mesh, payload.counts, shp)});
      !assembled) {
    return std::unexpected(std::format("{}: {}", file.name, assembled.error()));
  }
  shp.sound = std::move(*sound);
  shp.sequence = std::move(*sequence);
  if (auto encoded = EncodeShp(shp); !encoded) {
    return std::unexpected(std::format("{}: the model is not a valid SHP: {}",
                                       file.name, encoded.error()));
  }
  return std::move(payload.document);
}

}  // namespace

std::optional<ShpTextureSize> ShpUvScale(const ShpModelDocument& document) {
  if (document.shp.texture) {
    return ShpTextureSize{.width = document.shp.texture->Width(),
                          .height = document.shp.texture->Height()};
  }
  return document.lender_texture;
}

std::expected<std::vector<std::int64_t>, std::string> ShpBoneOrigins(
    const ShpFile& shp) {
  if (shp.bones.empty()) {
    return std::unexpected(std::string("the model has no bones"));
  }
  std::vector<std::int64_t> origins(shp.bones.size());
  for (std::size_t bone = 1; bone < shp.bones.size(); ++bone) {
    const std::size_t parent = shp.bones[bone].parent;
    if (parent >= bone) {
      return std::unexpected(std::format(
          "bone {}'s parent {} must be a lower-numbered bone", bone, parent));
    }
    origins[bone] = origins[parent] - shp.bones[parent].length;
    if (origins[bone] < -kMaxOrigin || origins[bone] > kMaxOrigin) {
      return std::unexpected(
          std::format("bone {}'s origin {} is beyond {} either way", bone,
                      origins[bone], kMaxOrigin));
    }
  }
  return origins;
}

JsonDocument ShpSkeletonGltf(const ShpFile& shp,
                             std::size_t inverse_bind_matrices) {
  std::vector<Json> children(shp.bones.size(), Json::array());
  for (std::size_t bone = 1; bone < shp.bones.size(); ++bone) {
    const std::size_t parent = shp.bones[bone].parent;
    if (parent < bone) children[parent].push_back(kShpFirstBoneNode + bone);
  }
  Json nodes = Json::array();
  nodes.push_back(Json{{"name", "root"},
                       {"rotation", Json::array({1.0, 0.0, 0.0, 0.0})},
                       {"children", Json::array({kShpFirstBoneNode})}});
  nodes.push_back(Json{{"name", "mesh"}, {"mesh", 0}, {"skin", 0}});
  Json joints = Json::array();
  for (std::size_t bone = 0; bone < shp.bones.size(); ++bone) {
    Json node = Json::object();
    node["name"] = std::format("bone{}", bone);
    const std::size_t parent = shp.bones[bone].parent;
    if (bone != 0 && parent < bone) {
      const std::int64_t length = shp.bones[parent].length;
      node["translation"] =
          Json::array({static_cast<double>(-length), 0.0, 0.0});
    }
    if (!children[bone].empty()) node["children"] = std::move(children[bone]);
    nodes.push_back(std::move(node));
    joints.push_back(kShpFirstBoneNode + bone);
  }
  Json skin = Json{{"inverseBindMatrices", inverse_bind_matrices},
                   {"skeleton", kShpFirstBoneNode},
                   {"joints", std::move(joints)}};
  return Json{
      {"scene", 0},
      {"scenes", Json::array({Json{
                     {"nodes", Json::array({kShpRootNode, kShpMeshNode})}}})},
      {"nodes", std::move(nodes)},
      {"skins", Json::array({std::move(skin)})},
  };
}

std::array<float, kShpMatrixElements> ShpInverseBindMatrix(
    std::int64_t origin) {
  std::array<float, kShpMatrixElements> matrix{};
  for (std::size_t diagonal = 0; diagonal < kShpMatrixElements;
       diagonal += kMatrixColumns + 1) {
    matrix[diagonal] = 1.0F;
  }
  matrix[kShpMatrixTranslationX] = static_cast<float>(-origin);
  return matrix;
}

NativeImage ShpTextureImage(const ShpTexture& texture) {
  NativeImage image;
  image.depth =
      texture.Is8Bit() ? ImageDepth::kIndexed8 : ImageDepth::kIndexed4;
  image.width = texture.Width();
  image.height = texture.Height();
  if (texture.Is8Bit()) {
    image.indices = texture.pixels;
  } else {
    image.indices.reserve(texture.pixels.size() * 2);
    for (const std::uint8_t pair : texture.pixels) {
      image.indices.push_back(static_cast<std::uint8_t>(pair & kNibbleMask));
      image.indices.push_back(static_cast<std::uint8_t>(pair >> kNibbleBits));
    }
  }
  const std::size_t used =
      image.indices.empty()
          ? 0
          : std::size_t{*std::ranges::max_element(image.indices)} + 1;
  const std::size_t entries =
      std::max(std::size_t{texture.palette_words}, used);
  const std::span<const std::uint16_t> words(texture.palettes);
  for (std::size_t row = 0; row < kPaletteRows; ++row) {
    const std::size_t first =
        std::min(row * texture.palette_words, words.size());
    const std::size_t count =
        std::min(std::size_t{texture.palette_words}, words.size() - first);
    const auto source = words.subspan(first, count);
    std::vector<std::uint16_t> palette(source.begin(), source.end());
    palette.resize(entries, kTransparentWord);
    image.palettes.push_back(std::move(palette));
  }
  return image;
}

std::expected<ShpTexture, std::string> ShpTextureOf(
    const NativeImage& image, std::uint8_t palette_words) {
  if (!IsIndexed(image.depth)) {
    return std::unexpected(
        std::string("an SHP texture is a 4-bit or 8-bit indexed image"));
  }
  if (auto valid = ValidateImage(image); !valid) {
    return std::unexpected(valid.error());
  }
  const bool eight = image.depth == ImageDepth::kIndexed8;
  const std::uint32_t per_unit =
      eight ? k8BitPixelsPerUnit : k4BitPixelsPerUnit;
  if (image.width % per_unit != 0 || image.height % kRowsPerUnit != 0 ||
      image.width / per_unit > kUnitMax ||
      image.height / kRowsPerUnit > kUnitMax) {
    return std::unexpected(std::format(
        "a {}-bit SHP texture is a multiple of {} pixels wide, up to {}, and "
        "an even number of rows, up to {}; the image is {} x {}",
        BitsPerPixel(image.depth), per_unit, per_unit * kUnitMax,
        kRowsPerUnit * kUnitMax, image.width, image.height));
  }
  if (image.palettes.size() != kPaletteRows ||
      image.PaletteEntries() < palette_words) {
    return std::unexpected(std::format(
        "an SHP texture has {} palette rows of at least texture.palette_words "
        "({}) entries; the image has {} of {}",
        kPaletteRows, palette_words, image.palettes.size(),
        image.PaletteEntries()));
  }
  ShpTexture texture{
      .flags = eight ? kShp8BitTexture : kShp4BitTexture,
      .width_byte = static_cast<std::uint8_t>(image.width / per_unit),
      .height_byte = static_cast<std::uint8_t>(image.height / kRowsPerUnit),
      .palette_words = palette_words,
      .palettes = {},
      .pixels = {}};
  for (std::size_t row = 0; row < kPaletteRows; ++row) {
    const std::vector<std::uint16_t>& palette = image.palettes[row];
    for (std::size_t entry = palette_words; entry < palette.size(); ++entry) {
      if (palette[entry] != kTransparentWord) {
        return std::unexpected(std::format(
            "palette row {} entry {} is 0x{:04X}; the SHP keeps the first {} "
            "entries of each row (texture.palette_words), so raise it to keep "
            "this colour",
            row, entry, palette[entry], palette_words));
      }
    }
    texture.palettes.insert(texture.palettes.end(), palette.begin(),
                            palette.begin() + palette_words);
  }
  if (eight) {
    texture.pixels = image.indices;
    return texture;
  }
  texture.pixels.reserve(image.indices.size() / 2);
  for (std::size_t index = 0; index + 1 < image.indices.size(); index += 2) {
    texture.pixels.push_back(static_cast<std::uint8_t>(
        image.indices[index] | (image.indices[index + 1] << kNibbleBits)));
  }
  return texture;
}

JsonDocument ShpLayoutJson(const Layout& layout) {
  Json json = Json::object();
  for (const LayoutEntry& entry : kLayout) {
    json[std::string(entry.field.name)] = EntryJson(entry, layout);
  }
  return json;
}

std::expected<Layout, std::string> ShpLayoutOf(const JsonDocument& layout) {
  return LayoutOf(layout, "layout");
}

JsonDocument ShpModelPayload(const ShpModelDocument& document,
                             std::optional<std::size_t> sound_view,
                             std::optional<std::size_t> sequence_view) {
  const ShpFile& shp = document.shp;
  Json counts = Json::array();
  for (const std::vector<ShpPolygon>& part : shp.parts) {
    counts.push_back(part.size());
  }
  Json bones = Json::array();
  for (const ShpBone& bone : shp.bones) bones.push_back(BoneJson(bone));
  Json payload = Json::object();
  payload["schema"] = std::string(kShpModelSchema);
  payload["schema_version"] = kShpModelSchemaVersion;
  payload["id"] = document.id;
  payload["source"] = document.source;
  payload["source_sha256"] = document.source_sha256;
  payload["colored"] = shp.colored;
  payload["polygon_counts"] = std::move(counts);
  payload["limb_ranges"] = shp.limb_ranges;
  payload["layout"] = ShpLayoutJson(shp.layout);
  payload["bones"] = std::move(bones);
  payload["sound"] = IndexJson(sound_view);
  payload["sequence_word"] = shp.sequence_word;
  payload["sequence"] = IndexJson(sequence_view);
  payload["texture"] = TexturePayload(document);
  return payload;
}

std::expected<ShpModelDocument, std::string> DecodeShpModelFiles(
    std::string_view stem, const ImageFileReader& read) {
  if (auto valid = CheckStem(stem); !valid) {
    return std::unexpected(valid.error());
  }
  auto file = ReadGltf(stem, read);
  if (!file) return std::unexpected(file.error());
  auto payload = ReadPayload(*file);
  if (!payload) return std::unexpected(payload.error());
  if (auto texture = ReadOwnTexture(stem, read, *payload); !texture) {
    return std::unexpected(texture.error());
  }
  return AssembleModel(*file, *payload);
}

std::expected<ShpModelDocument, std::string> LoadShpModelFiles(
    const fs::path& directory, std::string_view stem) {
  return DecodeShpModelFiles(
      stem, [&directory](const std::string& name) -> std::optional<Bytes> {
        std::ifstream input(directory / Platform::Utf8Path(name),
                            std::ios::binary);
        if (!input) return std::nullopt;
        const std::vector<char> data{std::istreambuf_iterator<char>(input),
                                     std::istreambuf_iterator<char>()};
        if (input.bad()) return std::nullopt;
        Bytes bytes(data.size());
        std::ranges::transform(data, bytes.begin(), [](char value) {
          return static_cast<std::byte>(value);
        });
        return bytes;
      });
}

}  // namespace VS::Content
