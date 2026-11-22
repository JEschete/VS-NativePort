// Strict WEP decoding and encoding (WepFile.hpp).
#include "Content/WepFile.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <format>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Content/ActorAssets.hpp"
#include "Content/ShpFile.hpp"

namespace VS::Content {
namespace {
using Bytes = std::span<const std::byte>;

static_assert(kWepMaxBones == kWeaponModelBones);
static_assert(kWepMaxModelBytes == kWeaponGeometryBytes);

constexpr std::size_t kBoneCountAt = 4;
constexpr std::size_t kModelSizeAt = kShpCountsBytes;
constexpr std::size_t kModelAt = kModelSizeAt + sizeof(std::int32_t);
constexpr std::uint8_t kTextureFlags = 0x01;
constexpr std::uint8_t kHeightByte = 16;
// The size field, flags, width, height and palette words bytes.
constexpr std::size_t kTextureHeaderBytes = sizeof(std::int32_t) + 4;
constexpr std::size_t kWidthAt = sizeof(std::int32_t) + 1;
constexpr std::size_t kHeightAt = kWidthAt + 1;
constexpr std::size_t kPaletteWordsAt = kHeightAt + 1;
constexpr std::size_t kColorBytes = sizeof(std::uint16_t);
// A bone rotation's halfwords: x, y, z, then the pad.
constexpr std::size_t kRotationYAt = sizeof(std::int16_t);
constexpr std::size_t kRotationZAt = 2 * sizeof(std::int16_t);
constexpr std::size_t kRotationPadAt = 3 * sizeof(std::int16_t);
constexpr std::size_t kRotationBytes = 4 * sizeof(std::int16_t);
// What the size field adds to the pixel bytes and the palette words.
constexpr std::int64_t kSizeFieldBias = 4;

template <typename T>
T ReadLe(Bytes data, std::size_t offset) {
  T value{};
  std::memcpy(&value, data.subspan(offset, sizeof value).data(), sizeof value);
  return value;
}

template <typename T>
void PutLe(std::vector<std::byte>& out, T value) {
  const auto bytes = std::as_bytes(std::span(&value, 1));
  out.insert(out.end(), bytes.begin(), bytes.end());
}

std::unexpected<std::string> Fail(std::size_t offset,
                                  std::string_view message) {
  return std::unexpected(std::format("byte 0x{:X}: {}", offset, message));
}

std::size_t PixelBytes(const WepTexture& texture) {
  return std::size_t{texture.Width()} * kWepTextureRows;
}

std::size_t PaletteBytes(const WepTexture& texture) {
  return (texture.HandleWords() +
          (kWepMaterialCount * texture.MaterialWords())) *
         kColorBytes;
}

std::int64_t SizeField(const WepTexture& texture) {
  return kSizeFieldBias + static_cast<std::int64_t>(PixelBytes(texture)) +
         texture.palette_words;
}

bool KnownPaletteWords(std::uint8_t words) {
  return words == kWepSinglePaletteWords || words == kWepDoublePaletteWords;
}

std::expected<std::size_t, std::string> ModelBytesOf(Bytes data) {
  if (data.size() < kModelAt) {
    return Fail(0, std::format("a WEP header needs {} bytes; {} remain",
                               kModelAt, data.size()));
  }
  const auto model_bytes = ReadLe<std::int32_t>(data, kModelSizeAt);
  if (model_bytes < 0 ||
      std::cmp_greater(model_bytes, data.size() - kModelAt)) {
    return Fail(kModelSizeAt,
                std::format("the model size {} does not fit", model_bytes));
  }
  if (std::cmp_greater(model_bytes, kWepMaxModelBytes)) {
    return Fail(kModelSizeAt,
                std::format("the model size {} exceeds the game's {}-byte "
                            "weapon geometry buffer",
                            model_bytes, kWepMaxModelBytes));
  }
  return static_cast<std::size_t>(model_bytes);
}

std::vector<std::uint16_t> ReadColors(Bytes data, std::size_t& at,
                                      std::size_t count) {
  std::vector<std::uint16_t> colors;
  colors.reserve(count);
  for (std::size_t i = 0; i < count; ++i) {
    colors.push_back(ReadLe<std::uint16_t>(data, at));
    at += kColorBytes;
  }
  return colors;
}

std::expected<void, std::string> CheckTextureHeader(Bytes data, std::size_t at,
                                                    const WepTexture& texture) {
  if (const std::uint8_t flags =
          std::to_integer<std::uint8_t>(data[at + sizeof(std::int32_t)]);
      flags != kTextureFlags) {
    return Fail(at + sizeof(std::int32_t),
                std::format("texture flags 0x{:02X} are not 0x{:02X}", flags,
                            kTextureFlags));
  }
  if (texture.width_byte == 0 || texture.width_byte > kWepMaxWidthByte) {
    return Fail(at + kWidthAt,
                std::format("the width byte {} is not 1 to {}",
                            texture.width_byte, kWepMaxWidthByte));
  }
  if (const std::uint8_t height =
          std::to_integer<std::uint8_t>(data[at + kHeightAt]);
      height != kHeightByte) {
    return Fail(at + kHeightAt, std::format("the height byte {} is not {}",
                                            height, kHeightByte));
  }
  if (!KnownPaletteWords(texture.palette_words)) {
    return Fail(at + kPaletteWordsAt,
                std::format("{} palette words are neither {} nor {}",
                            texture.palette_words, kWepSinglePaletteWords,
                            kWepDoublePaletteWords));
  }
  if (const auto size_field = ReadLe<std::int32_t>(data, at);
      size_field != SizeField(texture)) {
    return Fail(at, std::format("the texture size field {} is not {}",
                                size_field, SizeField(texture)));
  }
  return {};
}

// The texture and the bone rotations from `at` to the file's end.
std::expected<void, std::string> DecodeTail(Bytes data, std::size_t at,
                                            WepFile& wep) {
  if (data.size() - at < kTextureHeaderBytes) {
    return Fail(at, "the file ends inside the texture header");
  }
  WepTexture& texture = wep.texture;
  texture.width_byte = std::to_integer<std::uint8_t>(data[at + kWidthAt]);
  texture.palette_words =
      std::to_integer<std::uint8_t>(data[at + kPaletteWordsAt]);
  if (auto header = CheckTextureHeader(data, at, texture); !header) {
    return header;
  }
  const std::size_t bytes = kTextureHeaderBytes + PaletteBytes(texture) +
                            PixelBytes(texture) +
                            (wep.bones.size() * kRotationBytes);
  if (data.size() - at != bytes) {
    return Fail(at, std::format("the texture and {} bone rotations need {} "
                                "bytes to the file's end; {} remain",
                                wep.bones.size(), bytes, data.size() - at));
  }
  at += kTextureHeaderBytes;
  texture.handle = ReadColors(data, at, texture.HandleWords());
  for (auto& material : texture.materials) {
    material = ReadColors(data, at, texture.MaterialWords());
  }
  const Bytes pixels = data.subspan(at, PixelBytes(texture));
  texture.pixels.clear();
  for (const std::byte pixel : pixels) {
    texture.pixels.push_back(std::to_integer<std::uint8_t>(pixel));
  }
  at += pixels.size();
  for (std::size_t bone = 0; bone < wep.bones.size(); ++bone) {
    wep.rotations.push_back(
        {.x = ReadLe<std::int16_t>(data, at),
         .y = ReadLe<std::int16_t>(data, at + kRotationYAt),
         .z = ReadLe<std::int16_t>(data, at + kRotationZAt),
         .reserved6 = ReadLe<std::int16_t>(data, at + kRotationPadAt)});
    at += kRotationBytes;
  }
  return {};
}

std::unexpected<std::string> Refuse(std::string message) {
  return std::unexpected(std::move(message));
}

std::expected<void, std::string> CheckTexture(const WepTexture& texture) {
  if (texture.width_byte == 0 || texture.width_byte > kWepMaxWidthByte) {
    return Refuse(std::format("the texture width {} is not 2 to {}",
                              texture.Width(), 2U * kWepMaxWidthByte));
  }
  if (!KnownPaletteWords(texture.palette_words)) {
    return Refuse(std::format("{} palette words are neither {} nor {}",
                              texture.palette_words, kWepSinglePaletteWords,
                              kWepDoublePaletteWords));
  }
  if (texture.handle.size() != texture.HandleWords()) {
    return Refuse(std::format("the handle palette has {} colours, not {}",
                              texture.handle.size(), texture.HandleWords()));
  }
  for (std::size_t material = 0; material < kWepMaterialCount; ++material) {
    if (const std::size_t colors = texture.materials.at(material).size();
        colors != texture.MaterialWords()) {
      return Refuse(std::format("material palette {} has {} colours, not {}",
                                material, colors, texture.MaterialWords()));
    }
  }
  if (texture.pixels.size() != PixelBytes(texture)) {
    return Refuse(std::format("the texture has {} pixels, not {} x {}",
                              texture.pixels.size(), texture.Width(),
                              kWepTextureRows));
  }
  return {};
}

std::expected<void, std::string> CheckWep(const WepFile& wep) {
  if (wep.colored) {
    return Refuse("weapon polygons are flat; the model is coloured");
  }
  if (!wep.sound.empty()) {
    return Refuse("a weapon model has no sound section");
  }
  if (wep.bones.size() > kWepMaxBones) {
    return Refuse(std::format("{} bones exceed the game's {}", wep.bones.size(),
                              kWepMaxBones));
  }
  if (wep.rotations.size() != wep.bones.size()) {
    return Refuse(std::format("{} bone rotations for {} bones",
                              wep.rotations.size(), wep.bones.size()));
  }
  return CheckTexture(wep.texture);
}

void PutTail(std::vector<std::byte>& out, const WepFile& wep) {
  const WepTexture& texture = wep.texture;
  PutLe(out, static_cast<std::int32_t>(SizeField(texture)));
  for (const std::uint8_t byte : {kTextureFlags, texture.width_byte,
                                  kHeightByte, texture.palette_words}) {
    out.push_back(std::byte{byte});
  }
  for (const std::uint16_t color : texture.handle) PutLe(out, color);
  for (const auto& material : texture.materials) {
    for (const std::uint16_t color : material) PutLe(out, color);
  }
  const auto pixels = std::as_bytes(std::span(texture.pixels));
  out.insert(out.end(), pixels.begin(), pixels.end());
  for (const WepBoneRotation& rotation : wep.rotations) {
    for (const std::int16_t angle :
         {rotation.x, rotation.y, rotation.z, rotation.reserved6}) {
      PutLe(out, angle);
    }
  }
}

}  // namespace

std::expected<WepFile, std::string> DecodeWep(Bytes data) {
  const auto counts = DecodeShpCounts(data);
  if (!counts) return std::unexpected(counts.error());
  if (counts->bones > kWepMaxBones) {
    return Fail(kBoneCountAt, std::format("{} bones exceed the game's {}",
                                          counts->bones, kWepMaxBones));
  }
  const auto model_bytes = ModelBytesOf(data);
  if (!model_bytes) return std::unexpected(model_bytes.error());
  auto model = DecodeShpModel(data.subspan(kModelAt, *model_bytes), *counts,
                              /*colored=*/false);
  if (!model) return std::unexpected(std::format("model {}", model.error()));
  if (!model->sound.empty()) {
    return Fail(kModelAt + *model_bytes - model->sound.size(),
                "a weapon model has no sound section");
  }
  WepFile wep;
  static_cast<ShpModel&>(wep) = std::move(*model);
  if (auto tail = DecodeTail(data, kModelAt + *model_bytes, wep); !tail) {
    return std::unexpected(tail.error());
  }
  return wep;
}

std::expected<std::vector<std::byte>, std::string> EncodeWep(
    const WepFile& wep) {
  if (auto checked = CheckWep(wep); !checked) {
    return std::unexpected(checked.error());
  }
  auto model = EncodeShpModel(wep);
  if (!model) return std::unexpected(model.error());
  if (model->model.size() > kWepMaxModelBytes) {
    return Refuse(
        std::format("the {}-byte model exceeds the game's {}-byte "
                    "weapon geometry buffer",
                    model->model.size(), kWepMaxModelBytes));
  }
  std::vector<std::byte> out(model->counts.begin(), model->counts.end());
  PutLe(out, static_cast<std::int32_t>(model->model.size()));
  out.insert(out.end(), model->model.begin(), model->model.end());
  PutTail(out, wep);
  return out;
}

}  // namespace VS::Content
