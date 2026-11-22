// Hand-maintained (tools/port/owned.csv).
#include "Effects/TextureBindings.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <span>
#include <string_view>

#include "Core/Log.hpp"
#include "Effects/EffectResources.hpp"

namespace VS::Effects {
namespace {
constexpr std::size_t kHeaderBytes = 4;
constexpr std::size_t kTableOffsetBytes = sizeof(std::uint32_t);
// A descriptor's CLUT word, then its texture page word.
constexpr std::size_t kDescriptorPageOffset = sizeof(std::uint16_t);
constexpr std::uint32_t kClutColumnMask = 0x3F;
constexpr std::uint32_t kClutColumnEntries = 16;
constexpr std::uint32_t kClutRowShift = 6;
constexpr std::uint32_t kClutRowMask = 0x1FF;
constexpr std::uint32_t kPageDepthShift = 7;
constexpr std::uint32_t kPageDepthMask = 3;
constexpr std::uint32_t kIndexed8Depth = 1;
constexpr std::uint32_t kIndexed4Entries = 16;
constexpr std::uint32_t kIndexed8Entries = 256;
using Bytes = std::span<const std::byte>;

void Require(bool condition, const char* message) {
  if (!condition) {
    VS::Core::LogError("effects.material", "Invalid effect texture block: {}",
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

PaletteReferenceFault PaletteFault(std::uint16_t clut, std::uint16_t page,
                                   std::uint8_t bound_rows) {
  const std::uint32_t depth = (page >> kPageDepthShift) & kPageDepthMask;
  if (depth > kIndexed8Depth) return PaletteReferenceFault::kNone;
  const std::uint32_t entries =
      depth == kIndexed8Depth ? kIndexed8Entries : kIndexed4Entries;
  const std::uint32_t first = (clut & kClutColumnMask) * kClutColumnEntries;
  const auto area_x = static_cast<std::uint32_t>(kEffectClutX);
  const auto area_end = area_x + static_cast<std::uint32_t>(kEffectClutWidth);
  if (first < area_x || first + entries > area_end) {
    return PaletteReferenceFault::kColumn;
  }
  const std::uint32_t row = (clut >> kClutRowShift) & kClutRowMask;
  const auto first_row = static_cast<std::uint32_t>(kEffectClutFirstRow);
  if (row < first_row ||
      row - first_row >= static_cast<std::uint32_t>(kEffectClutRows)) {
    return PaletteReferenceFault::kRow;
  }
  if (((static_cast<std::uint32_t>(bound_rows) >> (row - first_row)) & 1U) ==
      0) {
    return PaletteReferenceFault::kUnbound;
  }
  return PaletteReferenceFault::kNone;
}
}  // namespace

// decomp: parsePfileBlock1 (src/BATTLE/BATTLE.PRG/6E4CC.c:141; was
// _parsePfileBlock1); original binder independently checked in BATTLE.PRG at
// 0x800D7A14.
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
    VS::Core::LogDebug(
        "effects.material",
        "E084 material channel 1 shares authored source table 0");
  }
  return layout;
}

PaletteReferenceCheck CheckEffectPaletteReferences(
    Bytes block, const TextureBlockLayout& layout, std::uint8_t bound_rows) {
  for (std::size_t offset = layout.descriptor_offset, index = 0;
       offset <= block.size() &&
       kTextureDescriptorBytes <= block.size() - offset;
       offset += kTextureDescriptorBytes, ++index) {
    const auto clut = Read<std::uint16_t>(block, offset);
    const auto page =
        Read<std::uint16_t>(block, offset + kDescriptorPageOffset);
    const PaletteReferenceFault fault = PaletteFault(clut, page, bound_rows);
    if (fault != PaletteReferenceFault::kNone) {
      return {.fault = fault, .descriptor = index, .clut = clut};
    }
  }
  return {};
}

std::string_view DescribePaletteFault(PaletteReferenceFault fault) noexcept {
  switch (fault) {
    case PaletteReferenceFault::kColumn:
      return "samples palette entries outside the effect CLUT's x 768-1023";
    case PaletteReferenceFault::kRow:
      return "names a CLUT row outside the effect rows 240-244";
    case PaletteReferenceFault::kUnbound:
      return "names an effect CLUT row that neither its effect's palette nor "
             "the system palette fills";
    case PaletteReferenceFault::kNone:
      break;
  }
  return "samples a loaded effect palette";
}
}  // namespace VS::Effects
