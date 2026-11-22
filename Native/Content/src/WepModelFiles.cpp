// Native weapon model files (WepModelFiles.hpp).
#include "Content/WepModelFiles.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <format>
#include <limits>
#include <numbers>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Content/ConversionManifest.hpp"
#include "Content/ShpModelFiles.hpp"

namespace VS::Content {
namespace {
namespace fs = std::filesystem;
using Json = JsonDocument;
using Check = std::expected<void, std::string>;
template <typename T>
using Result = std::expected<T, std::string>;

// A turn is 4096 angle units, of which the game uses the low 12 bits.
constexpr std::uint32_t kAngleMask = 0xFFF;
constexpr double kTurnUnits = 4096.0;
constexpr std::size_t kAxisX = 0;
constexpr std::size_t kAxisY = 1;
constexpr std::size_t kAxisZ = 2;
constexpr std::size_t kQuaternionW = 3;
constexpr std::size_t kQuaternionSize = kQuaternionW + 1;
using Int16Limits = std::numeric_limits<std::int16_t>;

// x, y, z and w, the order glTF writes them in.
using Quaternion = std::array<double, kQuaternionSize>;

std::uint32_t MaskedAngle(std::int16_t angle) {
  return static_cast<std::uint32_t>(static_cast<std::uint16_t>(angle)) &
         kAngleMask;
}

Quaternion AxisTurn(std::size_t axis, std::int16_t angle) {
  // Half the angle: units x 2 pi / 4096 / 2.
  const double half =
      static_cast<double>(MaskedAngle(angle)) * std::numbers::pi / kTurnUnits;
  Quaternion turn{0.0, 0.0, 0.0, std::cos(half)};
  turn.at(axis) = std::sin(half);
  return turn;
}

// The Hamilton product: turning by `right`, then by `left`.
Quaternion Multiply(const Quaternion& left, const Quaternion& right) {
  const auto [lx, ly, lz, lw] = left;
  const auto [rx, ry, rz, rw] = right;
  return {(lw * rx) + (lx * rw) + (ly * rz) - (lz * ry),
          (lw * ry) - (lx * rz) + (ly * rw) + (lz * rx),
          (lw * rz) + (lx * ry) - (ly * rx) + (lz * rw),
          (lw * rw) - (lx * rx) - (ly * ry) - (lz * rz)};
}

bool Turns(const WepBoneRotation& rotation) {
  return MaskedAngle(rotation.x) != 0 || MaskedAngle(rotation.y) != 0 ||
         MaskedAngle(rotation.z) != 0;
}

// The game's bone turn, Rz x Ry x Rx: about X, then Y, then Z
// (ScalarMath.hpp BuildEulerRotationMatrixQuadrant).
// decomp: BATTLE.PRG func_800B17F0 (src/BATTLE/BATTLE.PRG/48C7C.s:245)
Json RotationJson(const WepBoneRotation& rotation) {
  const Quaternion turn = Multiply(
      Multiply(AxisTurn(kAxisZ, rotation.z), AxisTurn(kAxisY, rotation.y)),
      AxisTurn(kAxisX, rotation.x));
  const double sign = turn[kQuaternionW] < 0.0 ? -1.0 : 1.0;
  Json json = Json::array();
  // Adding 0.0 writes a negative zero as 0.0.
  for (const double component : turn) json.push_back((component * sign) + 0.0);
  return json;
}

Json RotationsJson(const WepFile& wep) {
  Json list = Json::array();
  for (const WepBoneRotation& rotation : wep.rotations) {
    list.push_back(Json{{"x", rotation.x},
                        {"y", rotation.y},
                        {"z", rotation.z},
                        {"reserved6", rotation.reserved6}});
  }
  return list;
}

// The first entry past `kept` that is not 0x0000.
std::optional<std::size_t> ColorPast(std::span<const std::uint16_t> palette,
                                     std::size_t kept) {
  const auto extra = palette.subspan(std::min(kept, palette.size()));
  const auto color = std::ranges::find_if(
      extra, [](std::uint16_t word) { return word != kTransparentWord; });
  if (color == extra.end()) return std::nullopt;
  return kept + static_cast<std::size_t>(color - extra.begin());
}

Check CheckTextureShape(const NativeImage& image, std::uint8_t palette_words) {
  if (image.depth != ImageDepth::kIndexed8) {
    return std::unexpected(
        std::string("a WEP texture is an 8-bit indexed image"));
  }
  if (auto valid = ValidateImage(image); !valid) {
    return valid;
  }
  if (image.height != kWepTextureRows || image.width == 0 ||
      image.width % 2 != 0 || image.width > 2U * kWepMaxWidthByte) {
    return std::unexpected(std::format(
        "a WEP texture is an even number of pixels wide, up to {}, and {} "
        "rows tall; the image is {} x {}",
        2U * kWepMaxWidthByte, kWepTextureRows, image.width, image.height));
  }
  if (palette_words != kWepSinglePaletteWords &&
      palette_words != kWepDoublePaletteWords) {
    return std::unexpected(std::format(
        "texture.palette_words is {}; a WEP has {} or {}", palette_words,
        kWepSinglePaletteWords, kWepDoublePaletteWords));
  }
  if (image.palettes.size() != kWepMaterialCount ||
      image.PaletteEntries() < palette_words) {
    return std::unexpected(std::format(
        "a WEP texture has {} palette rows, one per material, of at least "
        "texture.palette_words ({}) entries; the image has {} of {}",
        kWepMaterialCount, palette_words, image.palettes.size(),
        image.PaletteEntries()));
  }
  return {};
}

Check ReadRotation(const JsonMembers& members, WepBoneRotation& rotation) {
  if (auto only = members.Only({"x", "y", "z", "reserved6"}); !only) {
    return only;
  }
  const std::array<std::pair<std::string_view, std::int16_t*>, 4> fields{{
      {"x", &rotation.x},
      {"y", &rotation.y},
      {"z", &rotation.z},
      {"reserved6", &rotation.reserved6},
  }};
  for (const auto& [key, out] : fields) {
    auto value = members.Integer(key, Int16Limits::min(), Int16Limits::max());
    if (!value) return std::unexpected(value.error());
    *out = static_cast<std::int16_t>(*value);
  }
  return {};
}

Check ReadRotations(const JsonMembers& payload, WepFile& wep) {
  auto value = payload.Get("bone_rotations");
  if (!value) return std::unexpected(value.error());
  const Json& list = **value;
  const std::string where = payload.Where("bone_rotations");
  if (!list.is_array() || list.size() != wep.bones.size()) {
    return std::unexpected(std::format("{} must hold one rotation per bone, {}",
                                       where, wep.bones.size()));
  }
  wep.rotations.assign(list.size(), WepBoneRotation{});
  for (std::size_t bone = 0; bone < list.size(); ++bone) {
    const JsonMembers members(list[bone], std::format("{}[{}]", where, bone));
    if (auto read = ReadRotation(members, wep.rotations[bone]); !read) {
      return read;
    }
  }
  return {};
}

struct TextureMembers {
  std::string image;
  std::uint8_t palette_words = 0;
};

Result<TextureMembers> ReadTextureMembers(const JsonMembers& payload) {
  auto value = payload.Get("texture");
  if (!value) return std::unexpected(value.error());
  const JsonMembers texture(**value, payload.Where("texture"));
  if (auto only = texture.Only({"image", "palette_words"}); !only) {
    return std::unexpected(only.error());
  }
  auto image = texture.String("image");
  if (!image || !IsContentId(*image)) {
    return std::unexpected(std::format("{} must be the texture's image id",
                                       texture.Where("image")));
  }
  auto words = texture.Unsigned("palette_words",
                                std::numeric_limits<std::uint8_t>::max());
  if (!words) return std::unexpected(words.error());
  return TextureMembers{.image = std::move(*image),
                        .palette_words = static_cast<std::uint8_t>(*words)};
}

// The payload's members other than the mesh's and the texture's.
struct Payload {
  WepModelDocument document;
  ShpPolygonCounts counts{};
  std::uint8_t palette_words = 0;
};

Result<Payload> ReadPayload(const ModelGltfFile& file) {
  auto members = ModelGltfPayload(file);
  if (!members) return std::unexpected(members.error());
  if (auto only =
          members->Only({"schema", "schema_version", "id", "source",
                         "source_sha256", "polygon_counts", "limb_ranges",
                         "bones", "bone_rotations", "texture"});
      !only) {
    return std::unexpected(only.error());
  }
  auto identity = ReadModelIdentity(*members, kWepModelSchema,
                                    kWepModelSchemaVersion, "a weapon model");
  if (!identity) return std::unexpected(identity.error());
  Payload payload{
      .document = {.id = std::move(identity->id),
                   .source = std::move(identity->source),
                   .source_sha256 = std::move(identity->source_sha256),
                   .image_id = {},
                   .wep = {}},
      .counts = {},
      .palette_words = 0};
  auto counts = ReadShpModelHeader(*members, payload.document.wep);
  if (!counts) return std::unexpected(counts.error());
  payload.counts = *counts;
  if (auto rotations = ReadRotations(*members, payload.document.wep);
      !rotations) {
    return std::unexpected(rotations.error());
  }
  auto texture = ReadTextureMembers(*members);
  if (!texture) return std::unexpected(texture.error());
  payload.document.image_id = std::move(texture->image);
  payload.palette_words = texture->palette_words;
  return payload;
}

Check ReadTexture(std::string_view stem, const ImageFileReader& read,
                  const Payload& payload, WepFile& wep) {
  const WepModelDocument& document = payload.document;
  auto image =
      ReadModelImage(stem, read,
                     ModelIdentity{.id = document.id,
                                   .source = document.source,
                                   .source_sha256 = document.source_sha256},
                     document.image_id);
  if (!image) return std::unexpected(image.error());
  auto texture = WepTextureOf(image->image, payload.palette_words);
  if (!texture) {
    return std::unexpected(
        std::format("{}{}: {}", stem, kImagePngSuffix, texture.error()));
  }
  wep.texture = std::move(*texture);
  return {};
}

}  // namespace

JsonDocument WepSkeletonGltf(const WepFile& wep,
                             std::size_t inverse_bind_matrices) {
  Json skeleton = ShpSkeletonGltf(wep, inverse_bind_matrices);
  Json& nodes = skeleton["nodes"];
  const std::size_t bones = std::min(wep.bones.size(), wep.rotations.size());
  for (std::size_t bone = 1; bone < bones; ++bone) {
    const WepBoneRotation& rotation = wep.rotations[bone];
    if (wep.bones[bone].parent >= bone || !Turns(rotation)) continue;
    Json& node = nodes[kShpFirstBoneNode + bone];
    Json turned = Json::object();
    for (const auto& member : node.items()) {
      turned[member.key()] = member.value();
      if (member.key() == "translation") {
        turned["rotation"] = RotationJson(rotation);
      }
    }
    node = std::move(turned);
  }
  return skeleton;
}

NativeImage WepTextureImage(const WepTexture& texture) {
  NativeImage image;
  image.depth = ImageDepth::kIndexed8;
  image.width = texture.Width();
  image.height = kWepTextureRows;
  image.indices = texture.pixels;
  const std::size_t used =
      image.indices.empty()
          ? 0
          : std::size_t{*std::ranges::max_element(image.indices)} + 1;
  const std::size_t entries =
      std::max(std::size_t{texture.palette_words}, used);
  for (const std::vector<std::uint16_t>& material : texture.materials) {
    std::vector<std::uint16_t> row = texture.handle;
    row.insert(row.end(), material.begin(), material.end());
    row.resize(entries, kTransparentWord);
    image.palettes.push_back(std::move(row));
  }
  return image;
}

std::expected<WepTexture, std::string> WepTextureOf(
    const NativeImage& image, std::uint8_t palette_words) {
  if (auto shape = CheckTextureShape(image, palette_words); !shape) {
    return std::unexpected(shape.error());
  }
  WepTexture texture{.width_byte = static_cast<std::uint8_t>(image.width / 2),
                     .palette_words = palette_words,
                     .handle = {},
                     .materials = {},
                     .pixels = image.indices};
  const std::span<const std::uint16_t> first(image.palettes.front());
  texture.handle.assign(
      first.begin(),
      first.begin() + static_cast<std::ptrdiff_t>(texture.HandleWords()));
  for (std::size_t row = 0; row < kWepMaterialCount; ++row) {
    const std::span<const std::uint16_t> palette(image.palettes[row]);
    if (!std::ranges::equal(palette.first(texture.HandleWords()),
                            texture.handle)) {
      return std::unexpected(std::format(
          "palette row {} starts with other colours than row 0; the WEP has "
          "one handle palette, the first {} entries of every row",
          row, texture.HandleWords()));
    }
    if (const auto extra = ColorPast(palette, palette_words)) {
      return std::unexpected(std::format(
          "palette row {} entry {} is 0x{:04X}; the WEP keeps the first {} "
          "entries of each row (texture.palette_words)",
          row, *extra, palette[*extra], palette_words));
    }
    const auto colors =
        palette.subspan(texture.HandleWords(), texture.MaterialWords());
    texture.materials.at(row).assign(colors.begin(), colors.end());
  }
  return texture;
}

JsonDocument WepModelPayload(const WepModelDocument& document) {
  const WepFile& wep = document.wep;
  Json payload = Json::object();
  payload["schema"] = std::string(kWepModelSchema);
  payload["schema_version"] = kWepModelSchemaVersion;
  payload["id"] = document.id;
  payload["source"] = document.source;
  payload["source_sha256"] = document.source_sha256;
  payload["polygon_counts"] = ShpPolygonCountsJson(wep);
  payload["limb_ranges"] = wep.limb_ranges;
  payload["bones"] = ShpBonesJson(wep);
  payload["bone_rotations"] = RotationsJson(wep);
  payload["texture"] = Json{{"image", document.image_id},
                            {"palette_words", wep.texture.palette_words}};
  return payload;
}

std::expected<WepModelDocument, std::string> DecodeWepModelFiles(
    std::string_view stem, const ImageFileReader& read) {
  auto file = ReadModelGltf(stem, read);
  if (!file) return std::unexpected(file.error());
  auto payload = ReadPayload(*file);
  if (!payload) return std::unexpected(payload.error());
  WepFile& wep = payload->document.wep;
  if (auto texture = ReadTexture(stem, read, *payload, wep); !texture) {
    return std::unexpected(texture.error());
  }
  const ModelMeshSpec spec{
      .counts = payload->counts,
      .uv_scale = {.width = wep.texture.Width(), .height = kWepTextureRows},
      .skeleton = [&wep](std::size_t matrices) {
        return WepSkeletonGltf(wep, matrices);
      }};
  if (auto mesh = ReadModelMesh(*file, spec, wep); !mesh) {
    return std::unexpected(mesh.error());
  }
  if (auto encoded = EncodeWep(wep); !encoded) {
    return std::unexpected(std::format("{}: the model is not a valid WEP: {}",
                                       file->name, encoded.error()));
  }
  return std::move(payload->document);
}

std::expected<WepModelDocument, std::string> LoadWepModelFiles(
    const fs::path& directory, std::string_view stem) {
  return DecodeWepModelFiles(stem, DirectoryFileReader(directory));
}

}  // namespace VS::Content
