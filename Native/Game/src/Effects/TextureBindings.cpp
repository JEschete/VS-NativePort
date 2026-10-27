// Hand-maintained (tools/port/owned.csv).
#include "Effects/TextureBindings.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <span>

#include "Core/Log.hpp"
#include "Legacy/Psx.h"

extern "C" {
#include "Effects/Package.h"
#include "Effects/Runtime.h"
}

namespace {
constexpr std::size_t kHeaderBytes = 4;
constexpr std::size_t kTableCapacity = 4;
constexpr unsigned kGravityEffect = 84;
using Bytes = std::span<std::byte>;

void Require(bool condition, const char* message) {
  if (!condition) {
    VS_LOG_ERROR("effects.material", "Invalid effect texture block: {}",
                 message);
    std::abort();
  }
}

template <typename T>
T Read(Bytes bytes, std::size_t offset) {
  Require(offset <= bytes.size() && sizeof(T) <= bytes.size() - offset,
          "truncated field");
  T value;
  std::memcpy(&value, bytes.data() + offset, sizeof value);
  return value;
}

struct TextureBinding {
  std::array<Effects_PackageTextureInfo*, kTableCapacity> tables{};
  Effects_PackageTexture* textures = nullptr;
  int count = 0;
};

TextureBinding ValidateTextureBlock(Bytes bytes) {
  const auto count = Read<std::uint16_t>(bytes, 0);
  const auto geometry = Read<std::uint16_t>(bytes, sizeof(std::uint16_t));
  const std::size_t header = kHeaderBytes + count * sizeof(std::uint32_t);
  Require(count <= kTableCapacity, "too many source tables");
  Require(geometry >= header && geometry <= bytes.size(),
          "texture descriptor offset is outside the block");
  Require((bytes.size() - geometry) % sizeof(Effects_PackageTexture) == 0,
          "truncated texture descriptor");
  Require(reinterpret_cast<std::uintptr_t>(bytes.data()) %
                  alignof(Effects_PackageTexture) ==
              0,
          "unaligned texture storage");
  const auto texture_count =
      (bytes.size() - geometry) / sizeof(Effects_PackageTexture);
  TextureBinding result;
  result.count = count;
  result.textures =
      reinterpret_cast<Effects_PackageTexture*>(bytes.data() + geometry);
  for (std::size_t table = 0; table < count; ++table) {
    const auto offset = Read<std::uint32_t>(
        bytes, kHeaderBytes + table * sizeof(std::uint32_t));
    const auto end =
        table + 1 == count
            ? geometry
            : Read<std::uint32_t>(
                  bytes, kHeaderBytes + (table + 1) * sizeof(std::uint32_t));
    Require(offset >= header && end >= offset && end <= geometry,
            "texture table offset is outside the block");
    Require(offset % alignof(Effects_PackageTextureInfo) == 0 &&
                (end - offset) % sizeof(Effects_PackageTextureInfo) == 0,
            "truncated texture frame record");
    for (std::size_t frame = offset; frame < end;
         frame += sizeof(Effects_PackageTextureInfo)) {
      const auto record = Read<Effects_PackageTextureInfo>(bytes, frame);
      Require(record.dataIndex <= texture_count &&
                  record.reserved0 <= texture_count - record.dataIndex,
              "frame references unavailable texture descriptors");
    }
    result.tables[table] =
        reinterpret_cast<Effects_PackageTextureInfo*>(bytes.data() + offset);
  }
  return result;
}
}  // namespace

// decomp: _parsePfileBlock1 (src/BATTLE/BATTLE.PRG/6F014.c:512);
// original binder independently checked in BATTLE.PRG at 0x800D7A14.
void Effects_BindEffectTextureTables(void* data, size_t byte_count) {
  Require(data != nullptr && Effects_package != nullptr,
          "missing package or source block");
  auto binding =
      ValidateTextureBlock(Bytes(static_cast<std::byte*>(data), byte_count));
  if (Effects_package == &Effects_actionPackage &&
      Effects_actionRequest.effectId == kGravityEffect && binding.count == 1) {
    // E084 authors channel 1 for its grid/rings but supplies only table 0.
    // Native material channels explicitly share that same authored table;
    // original assets, frame numbers, UVs and descriptor contents stay intact.
    binding.tables[1] = binding.tables[0];
    binding.count = 2;
    VS_LOG_DEBUG("effects.material",
                 "E084 material channel 1 shares authored source table 0");
  }
  std::copy(binding.tables.begin(), binding.tables.end(),
            Effects_package->textureInfoTable);
  Effects_package->textures = binding.textures;
  Effects_package->block1TableCount = binding.count;
}
