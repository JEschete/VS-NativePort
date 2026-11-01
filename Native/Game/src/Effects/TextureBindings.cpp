// Hand-maintained (tools/port/owned.csv).
#include "Effects/TextureBindings.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <span>

#include "Core/Log.hpp"

namespace VS::Effects {
namespace {
constexpr std::size_t kHeaderBytes = 4;
constexpr std::size_t kTableOffsetBytes = sizeof(std::uint32_t);
using Bytes = std::span<const std::byte>;

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

std::size_t TableOffset(Bytes bytes, std::size_t table) {
  return Read<std::uint32_t>(bytes, kHeaderBytes + table * kTableOffsetBytes);
}

// A frame record: frame count, then the first descriptor index.
void ValidateFrames(Bytes bytes, std::size_t begin, std::size_t end,
                    std::size_t descriptor_count) {
  for (std::size_t frame = begin; frame < end;
       frame += kTextureFrameRecordBytes) {
    const auto frames = Read<std::uint16_t>(bytes, frame);
    const auto first =
        Read<std::uint16_t>(bytes, frame + sizeof(std::uint16_t));
    Require(first <= descriptor_count && frames <= descriptor_count - first,
            "frame references unavailable texture descriptors");
  }
}
}  // namespace

// decomp: _parsePfileBlock1 (src/BATTLE/BATTLE.PRG/6F014.c:512);
// original binder independently checked in BATTLE.PRG at 0x800D7A14.
TextureBlockLayout BindEffectTextureTables(Bytes block,
                                           const EffectTextureOwner& owner) {
  const std::size_t count = Read<std::uint16_t>(block, 0);
  const std::size_t descriptors =
      Read<std::uint16_t>(block, sizeof(std::uint16_t));
  const std::size_t header = kHeaderBytes + count * kTableOffsetBytes;
  Require(count <= kTextureTableCapacity, "too many source tables");
  Require(descriptors >= header && descriptors <= block.size(),
          "texture descriptor offset is outside the block");
  Require((block.size() - descriptors) % kTextureDescriptorBytes == 0,
          "truncated texture descriptor");
  const std::size_t descriptor_count =
      (block.size() - descriptors) / kTextureDescriptorBytes;
  TextureBlockLayout layout;
  layout.table_count = count;
  layout.descriptor_offset = descriptors;
  for (std::size_t table = 0; table < count; ++table) {
    const std::size_t begin = TableOffset(block, table);
    const std::size_t end =
        table + 1 == count ? descriptors : TableOffset(block, table + 1);
    Require(begin >= header && end >= begin && end <= descriptors,
            "texture table offset is outside the block");
    Require(begin % kTextureFrameRecordAlignment == 0 &&
                (end - begin) % kTextureFrameRecordBytes == 0,
            "truncated texture frame record");
    ValidateFrames(block, begin, end, descriptor_count);
    layout.table_offsets.at(table) = begin;
  }
  if (owner.is_action_package && owner.action_effect_id == kGravityEffectId &&
      count == 1) {
    // E084 authors channel 1 for its grid/rings but supplies only table 0.
    // Native material channels explicitly share that same authored table;
    // original assets, frame numbers, UVs and descriptor contents stay intact.
    layout.table_offsets[1] = layout.table_offsets[0];
    layout.table_count = 2;
    VS_LOG_DEBUG("effects.material",
                 "E084 material channel 1 shares authored source table 0");
  }
  return layout;
}
}  // namespace VS::Effects
