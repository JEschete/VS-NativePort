#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

#include "Field/RoomEffects.hpp"
#include "Render/RoomDraw.hpp"

// The byte layouts of the room's geometry, texture effect and effect script
// sections as the room effects read them (decomp 2842C.c:23-194 and
// Field_MpdPolygonUvView/Field_MpdTriangleColorView), with bounds-checked
// little-endian codecs. RoomEffects.cpp decodes every record through these;
// RoomEffectsLive.cpp pins the offsets legacy code still shares.
namespace VS::Field::room_effects {
static_assert(std::endian::native == std::endian::little,
              "the MPD sections are little-endian");

// Room geometry: the group count, then 64-byte group records whose words
// are the flags (the depth bias in the upper halfword), the position, the
// rotation and the polygon block's offset.
inline constexpr std::size_t kGeometryCountBytes = 4;
inline constexpr std::size_t kGroupRecordBytes = Render::kRoomGroupRecordBytes;
inline constexpr std::size_t kGroupWordBytes = 4;
inline constexpr std::size_t kGroupDepthBiasOffset = 2;
inline constexpr std::size_t kGroupPositionWord = 1;
inline constexpr std::size_t kGroupRotationWord = 4;
inline constexpr std::size_t kGroupPolygonBlockWord = 7;
inline constexpr std::size_t kGroupVectorWords = 3;
inline constexpr std::uint32_t kGroupHiddenFlag = 0x100;
inline constexpr std::uint32_t kGroupDisabledFlag = 0x1000;
inline constexpr std::uint32_t kGroupSwingRotationFlags = 0x6000;

// Field_MpdPolygonUvView: a triangle when (type & 0x3C) == 0x34.
inline constexpr std::size_t kPolygonTypeOffset = 0xF;
inline constexpr std::uint8_t kPolygonShapeMask = 0x3C;
inline constexpr std::uint8_t kPolygonTriangleShape = 0x34;
inline constexpr std::size_t kTriangleCorners = 3;
using CornerOffsets = std::array<std::size_t, kRoomEffectUvCorners>;
inline constexpr CornerOffsets kPolygonUOffsets{0x18, 0x1C, 0x13, 0x23};
inline constexpr CornerOffsets kPolygonVOffsets{0x19, 0x1D, 0x17, 0x27};
// The bytes a triangle's and a quad's UV writes reach.
inline constexpr std::size_t kTriangleUvExtent = 0x1E;
inline constexpr std::size_t kQuadUvExtent = 0x28;

// Field_MpdTriangleColorView and Field_MpdQuadColorView: each red byte
// with its green byte after it. Triangles use the first three pairs. The
// views are Render::kRoomTriangleRecordBytes and kRoomQuadRecordBytes long.
inline constexpr CornerOffsets kPolygonRedOffsets{0xC, 0x10, 0x14, 0x24};
// The triangle and quad counts ahead of a polygon block.
inline constexpr std::size_t kPolygonBlockCountsBytes = 8;

// Texture effect records: the flag bytes shared by every kind, each kind's
// section stride (zero for the kinds the walk stops at) and the bytes each
// decoded record covers.
inline constexpr std::size_t kEffectNeedsResetOffset = 4;
inline constexpr std::size_t kEffectStartsInactiveOffset = 5;
inline constexpr std::size_t kEffectKindOffset = 6;
using KindBytes = std::array<std::size_t, kRoomTextureEffectKindCount>;
inline constexpr KindBytes kTextureEffectStrides{24, 20, 32, 0, 0, 0, 36};
inline constexpr std::size_t kTextureFrameEffectBytes = 0x18;
inline constexpr std::size_t kTextureImageEffectBytes = 0x14;
inline constexpr std::size_t kTextureScrollEffectBytes = 0x1C;
inline constexpr std::size_t kSwingEffectBytes = 0x24;
inline constexpr KindBytes kTextureEffectRecordBytes{
    kTextureFrameEffectBytes,
    kTextureImageEffectBytes,
    kTextureScrollEffectBytes,
    0,
    0,
    0,
    kSwingEffectBytes,
};

// Effect scripts: a header (the object count and the starts-active byte),
// one geometry group id per object padded to four bytes, then the commands
// up to a zero-size terminator.
inline constexpr std::size_t kScriptHeaderBytes = 4;
inline constexpr std::size_t kScriptActiveOffset = 2;
inline constexpr std::size_t kCommandOpcodeOffset = 2;

// Section E (Field_MpdSectionE): a header, then the room's entry point code.
inline constexpr std::size_t kEntryPointHeaderBytes = 0x14;

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): a range check over
// sizes
[[nodiscard]] constexpr bool Fits(std::size_t size, std::size_t offset,
                                  std::size_t bytes) {
  return offset <= size && bytes <= size - offset;
}

// Little-endian loads and stores; the caller checks Fits first.
template <class Value>
[[nodiscard]] Value Load(std::span<const std::byte> bytes, std::size_t offset) {
  Value value{};
  std::memcpy(&value, bytes.subspan(offset, sizeof(Value)).data(),
              sizeof(Value));
  return value;
}

template <class Value>
void Store(std::span<std::byte> bytes, std::size_t offset, Value value) {
  std::memcpy(bytes.subspan(offset, sizeof(Value)).data(), &value,
              sizeof(Value));
}

// The R3000's divu: a zero divisor leaves an all-ones quotient and the
// dividend as the remainder.
struct UnsignedDivision {
  std::uint32_t quotient = 0;
  std::uint32_t remainder = 0;
};
[[nodiscard]] UnsignedDivision DivideUnsigned(std::uint32_t dividend,
                                              std::uint32_t divisor);

// Wrapping 32-bit arithmetic as the R3000 does it.
[[nodiscard]] std::int32_t WrappingAdd(std::int32_t left, std::int32_t right);
[[nodiscard]] std::int32_t WrappingNegate(std::int32_t value);
// ABS(): the most negative value stays itself.
[[nodiscard]] std::int32_t WrappingAbs(std::int32_t value);

// The texture effect records.
[[nodiscard]] TextureFrameEffect DecodeTextureFrameEffect(
    std::span<const std::byte, kTextureFrameEffectBytes> bytes);
void EncodeTextureFrameEffect(
    const TextureFrameEffect& effect,
    std::span<std::byte, kTextureFrameEffectBytes> bytes);
[[nodiscard]] TextureScrollEffect DecodeTextureScrollEffect(
    std::span<const std::byte, kTextureScrollEffectBytes> bytes);
void EncodeTextureScrollEffect(
    const TextureScrollEffect& effect,
    std::span<std::byte, kTextureScrollEffectBytes> bytes);
[[nodiscard]] TextureImageEffect DecodeTextureImageEffect(
    std::span<const std::byte, kTextureImageEffectBytes> bytes);
void EncodeTextureImageEffect(
    const TextureImageEffect& effect,
    std::span<std::byte, kTextureImageEffectBytes> bytes);
[[nodiscard]] SwingEffect DecodeSwingEffect(
    std::span<const std::byte, kSwingEffectBytes> bytes);
void EncodeSwingEffect(const SwingEffect& effect,
                       std::span<std::byte, kSwingEffectBytes> bytes);

// The bytes an effect script's header and padded group ids take.
[[nodiscard]] std::size_t ScriptPreambleBytes(std::uint8_t object_count);

// Decoding one command at a script cursor.
enum class CommandStatus : std::uint8_t {
  kDecoded,
  // A zero size or an opcode past the table ends the object's script.
  kEnd,
  kHeaderOutOfRange,
  kNegativeOpcode,
  kTruncated,
};
struct DecodedCommand {
  CommandStatus status = CommandStatus::kEnd;
  RoomEffectCommand command;
};
// The bytes each opcode's handler reads, header included.
[[nodiscard]] std::size_t CommandExtent(RoomEffectOpcode opcode);
[[nodiscard]] DecodedCommand DecodeCommand(std::span<const std::byte> scripts,
                                           std::size_t cursor);
}  // namespace VS::Field::room_effects
