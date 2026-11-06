// Hand-maintained (tools/port/owned.csv).
#include "Field/RoomEffectsCore.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>

namespace VS::Field {
namespace {
// Field_RoomEffectCommand (2842C.c:34): the header and four operand words,
// then the two extra words the transform command reads.
inline constexpr std::size_t kCommandHalfBytes = 2;
inline constexpr std::size_t kCommandWordBytes = 4;
}  // namespace

std::uint16_t RoomEffectCommand::Half(std::size_t offset) const {
  if (!room_effects::Fits(bytes.size(), offset, kCommandHalfBytes)) {
    return 0;
  }
  return room_effects::Load<std::uint16_t>(std::as_bytes(std::span(bytes)),
                                           offset);
}

std::int16_t RoomEffectCommand::SignedHalf(std::size_t offset) const {
  return static_cast<std::int16_t>(Half(offset));
}

std::int32_t RoomEffectCommand::Word(std::size_t offset) const {
  if (!room_effects::Fits(bytes.size(), offset, kCommandWordBytes)) {
    return 0;
  }
  return room_effects::Load<std::int32_t>(std::as_bytes(std::span(bytes)),
                                          offset);
}
}  // namespace VS::Field

namespace VS::Field::room_effects {
namespace {
// func_80091C6C_t (2842C.c:133).
inline constexpr std::size_t kFrameDelay = 7;
inline constexpr std::size_t kFrameBaseU = 8;
inline constexpr std::size_t kFrameBaseV = 9;
inline constexpr std::size_t kFrameCount = 0xA;
inline constexpr std::size_t kFrameColumns = 0xB;
inline constexpr std::size_t kFrameWidth = 0xC;
inline constexpr std::size_t kFrameHeight = 0xD;
inline constexpr std::size_t kFrameTick = 0xE;
inline constexpr std::size_t kFrameIndex = 0xF;
inline constexpr std::size_t kFrameU = 0x10;
inline constexpr std::size_t kFrameV = 0x14;

// func_80091E10_t (2842C.c:171).
inline constexpr std::size_t kScrollDelay = 7;
inline constexpr std::size_t kScrollBaseU = 8;
inline constexpr std::size_t kScrollBaseV = 9;
inline constexpr std::size_t kScrollWidth = 0xA;
inline constexpr std::size_t kScrollHeight = 0xB;
inline constexpr std::size_t kScrollCurrentU = 0xC;
inline constexpr std::size_t kScrollCurrentV = 0xD;
inline constexpr std::size_t kScrollWindowWidth = 0xE;
inline constexpr std::size_t kScrollWindowHeight = 0xF;
inline constexpr std::size_t kScrollStepU = 0x10;
inline constexpr std::size_t kScrollStepV = 0x11;
inline constexpr std::size_t kScrollTick = 0x12;
inline constexpr std::size_t kScrollU = 0x14;
inline constexpr std::size_t kScrollV = 0x18;

// func_80091FE8_t (2842C.c:182).
inline constexpr std::size_t kImageTick = 0;
inline constexpr std::size_t kImageFrame = 1;
inline constexpr std::size_t kImageFrameCount = 2;
inline constexpr std::size_t kImageColumns = 3;
inline constexpr std::size_t kImageDelay = 7;
inline constexpr std::size_t kImageScrollStep = 8;
inline constexpr std::size_t kImageRectX = 0xC;
inline constexpr std::size_t kImageRectY = 0xE;
inline constexpr std::size_t kImageRectW = 0x10;
inline constexpr std::size_t kImageRectH = 0x12;

// D_800F1DD8_t2 (2842C.c:49).
inline constexpr std::size_t kSwingAxis = 7;
inline constexpr std::size_t kSwingAngularVelocity = 8;
inline constexpr std::size_t kSwingAcceleration = 0xC;
inline constexpr std::size_t kSwingDampingShift = 0x10;
inline constexpr std::size_t kSwingAngle = 0x14;
inline constexpr std::size_t kSwingRestAngle = 0x18;
inline constexpr std::size_t kSwingStartAngle = 0x1C;
inline constexpr std::size_t kSwingInitialVelocity = 0x20;

inline constexpr std::size_t kScriptIdAlignment = 4;

// The operand bytes each handler reads (2842C.c:1070-1315), header
// included: the radial velocity reads to 0xD, the velocity to 0x13, the
// transform to the halfword at 0x18 and the one-operand commands to 7.
inline constexpr std::size_t kRadialVelocityExtent = 14;
inline constexpr std::size_t kVelocityExtent = 20;
inline constexpr std::size_t kTransformExtent = 26;
inline constexpr std::size_t kOneOperandExtent = 8;

std::uint8_t ByteAt(std::span<const std::byte> bytes, std::size_t offset) {
  return Load<std::uint8_t>(bytes, offset);
}

void SetByte(std::span<std::byte> bytes, std::size_t offset,
             std::uint8_t value) {
  Store<std::uint8_t>(bytes, offset, value);
}

std::array<std::uint8_t, kRoomEffectUvCorners> LoadCorners(
    std::span<const std::byte> bytes, std::size_t offset) {
  std::array<std::uint8_t, kRoomEffectUvCorners> corners{};
  std::memcpy(corners.data(), bytes.subspan(offset, corners.size()).data(),
              corners.size());
  return corners;
}

void StoreCorners(
    std::span<std::byte> bytes, std::size_t offset,
    const std::array<std::uint8_t, kRoomEffectUvCorners>& corners) {
  std::memcpy(bytes.subspan(offset, corners.size()).data(), corners.data(),
              corners.size());
}
}  // namespace

UnsignedDivision DivideUnsigned(std::uint32_t dividend, std::uint32_t divisor) {
  if (divisor == 0) {
    return {std::numeric_limits<std::uint32_t>::max(), dividend};
  }
  return {dividend / divisor, dividend % divisor};
}

std::int32_t WrappingAdd(std::int32_t left, std::int32_t right) {
  return static_cast<std::int32_t>(static_cast<std::uint32_t>(left) +
                                   static_cast<std::uint32_t>(right));
}

std::int32_t WrappingNegate(std::int32_t value) {
  return static_cast<std::int32_t>(0U - static_cast<std::uint32_t>(value));
}

std::int32_t WrappingAbs(std::int32_t value) {
  return value < 0 ? WrappingNegate(value) : value;
}

TextureFrameEffect DecodeTextureFrameEffect(
    std::span<const std::byte, kTextureFrameEffectBytes> bytes) {
  TextureFrameEffect effect;
  effect.geometry_offset = Load<std::int32_t>(bytes, 0);
  effect.delay = ByteAt(bytes, kFrameDelay);
  effect.base_u = ByteAt(bytes, kFrameBaseU);
  effect.base_v = ByteAt(bytes, kFrameBaseV);
  effect.frame_count = ByteAt(bytes, kFrameCount);
  effect.columns = ByteAt(bytes, kFrameColumns);
  effect.frame_width = ByteAt(bytes, kFrameWidth);
  effect.frame_height = ByteAt(bytes, kFrameHeight);
  effect.tick = ByteAt(bytes, kFrameTick);
  effect.frame = ByteAt(bytes, kFrameIndex);
  effect.u = LoadCorners(bytes, kFrameU);
  effect.v = LoadCorners(bytes, kFrameV);
  return effect;
}

void EncodeTextureFrameEffect(
    const TextureFrameEffect& effect,
    std::span<std::byte, kTextureFrameEffectBytes> bytes) {
  Store(bytes, 0, effect.geometry_offset);
  SetByte(bytes, kFrameDelay, effect.delay);
  SetByte(bytes, kFrameBaseU, effect.base_u);
  SetByte(bytes, kFrameBaseV, effect.base_v);
  SetByte(bytes, kFrameCount, effect.frame_count);
  SetByte(bytes, kFrameColumns, effect.columns);
  SetByte(bytes, kFrameWidth, effect.frame_width);
  SetByte(bytes, kFrameHeight, effect.frame_height);
  SetByte(bytes, kFrameTick, effect.tick);
  SetByte(bytes, kFrameIndex, effect.frame);
  StoreCorners(bytes, kFrameU, effect.u);
  StoreCorners(bytes, kFrameV, effect.v);
}

TextureScrollEffect DecodeTextureScrollEffect(
    std::span<const std::byte, kTextureScrollEffectBytes> bytes) {
  TextureScrollEffect effect;
  effect.geometry_offset = Load<std::int32_t>(bytes, 0);
  effect.delay = ByteAt(bytes, kScrollDelay);
  effect.base_u = ByteAt(bytes, kScrollBaseU);
  effect.base_v = ByteAt(bytes, kScrollBaseV);
  effect.width = ByteAt(bytes, kScrollWidth);
  effect.height = ByteAt(bytes, kScrollHeight);
  effect.current_u = ByteAt(bytes, kScrollCurrentU);
  effect.current_v = ByteAt(bytes, kScrollCurrentV);
  effect.window_width = ByteAt(bytes, kScrollWindowWidth);
  effect.window_height = ByteAt(bytes, kScrollWindowHeight);
  effect.step_u = Load<std::int8_t>(bytes, kScrollStepU);
  effect.step_v = Load<std::int8_t>(bytes, kScrollStepV);
  effect.tick = ByteAt(bytes, kScrollTick);
  effect.u = LoadCorners(bytes, kScrollU);
  effect.v = LoadCorners(bytes, kScrollV);
  return effect;
}

void EncodeTextureScrollEffect(
    const TextureScrollEffect& effect,
    std::span<std::byte, kTextureScrollEffectBytes> bytes) {
  Store(bytes, 0, effect.geometry_offset);
  SetByte(bytes, kScrollDelay, effect.delay);
  SetByte(bytes, kScrollBaseU, effect.base_u);
  SetByte(bytes, kScrollBaseV, effect.base_v);
  SetByte(bytes, kScrollWidth, effect.width);
  SetByte(bytes, kScrollHeight, effect.height);
  SetByte(bytes, kScrollCurrentU, effect.current_u);
  SetByte(bytes, kScrollCurrentV, effect.current_v);
  SetByte(bytes, kScrollWindowWidth, effect.window_width);
  SetByte(bytes, kScrollWindowHeight, effect.window_height);
  Store(bytes, kScrollStepU, effect.step_u);
  Store(bytes, kScrollStepV, effect.step_v);
  SetByte(bytes, kScrollTick, effect.tick);
  StoreCorners(bytes, kScrollU, effect.u);
  StoreCorners(bytes, kScrollV, effect.v);
}

TextureImageEffect DecodeTextureImageEffect(
    std::span<const std::byte, kTextureImageEffectBytes> bytes) {
  TextureImageEffect effect;
  effect.tick = ByteAt(bytes, kImageTick);
  effect.frame = ByteAt(bytes, kImageFrame);
  effect.frame_count = ByteAt(bytes, kImageFrameCount);
  effect.columns = ByteAt(bytes, kImageColumns);
  effect.delay = ByteAt(bytes, kImageDelay);
  effect.scroll_step = Load<std::int32_t>(bytes, kImageScrollStep);
  effect.rect.x = Load<std::int16_t>(bytes, kImageRectX);
  effect.rect.y = Load<std::int16_t>(bytes, kImageRectY);
  effect.rect.w = Load<std::int16_t>(bytes, kImageRectW);
  effect.rect.h = Load<std::int16_t>(bytes, kImageRectH);
  return effect;
}

void EncodeTextureImageEffect(
    const TextureImageEffect& effect,
    std::span<std::byte, kTextureImageEffectBytes> bytes) {
  SetByte(bytes, kImageTick, effect.tick);
  SetByte(bytes, kImageFrame, effect.frame);
  SetByte(bytes, kImageFrameCount, effect.frame_count);
  SetByte(bytes, kImageColumns, effect.columns);
  SetByte(bytes, kImageDelay, effect.delay);
  Store(bytes, kImageScrollStep, effect.scroll_step);
  Store(bytes, kImageRectX, effect.rect.x);
  Store(bytes, kImageRectY, effect.rect.y);
  Store(bytes, kImageRectW, effect.rect.w);
  Store(bytes, kImageRectH, effect.rect.h);
}

SwingEffect DecodeSwingEffect(
    std::span<const std::byte, kSwingEffectBytes> bytes) {
  SwingEffect effect;
  effect.geometry_offset = Load<std::int32_t>(bytes, 0);
  effect.needs_reset = ByteAt(bytes, kEffectNeedsResetOffset);
  effect.axis = ByteAt(bytes, kSwingAxis);
  effect.angular_velocity = Load<std::int32_t>(bytes, kSwingAngularVelocity);
  effect.acceleration = Load<std::int32_t>(bytes, kSwingAcceleration);
  effect.damping_shift = Load<std::int32_t>(bytes, kSwingDampingShift);
  effect.angle = Load<std::int32_t>(bytes, kSwingAngle);
  effect.rest_angle = Load<std::int32_t>(bytes, kSwingRestAngle);
  effect.start_angle = Load<std::int32_t>(bytes, kSwingStartAngle);
  effect.initial_velocity = Load<std::int32_t>(bytes, kSwingInitialVelocity);
  return effect;
}

void EncodeSwingEffect(const SwingEffect& effect,
                       std::span<std::byte, kSwingEffectBytes> bytes) {
  Store(bytes, 0, effect.geometry_offset);
  SetByte(bytes, kEffectNeedsResetOffset, effect.needs_reset);
  SetByte(bytes, kSwingAxis, effect.axis);
  Store(bytes, kSwingAngularVelocity, effect.angular_velocity);
  Store(bytes, kSwingAcceleration, effect.acceleration);
  Store(bytes, kSwingDampingShift, effect.damping_shift);
  Store(bytes, kSwingAngle, effect.angle);
  Store(bytes, kSwingRestAngle, effect.rest_angle);
  Store(bytes, kSwingStartAngle, effect.start_angle);
  Store(bytes, kSwingInitialVelocity, effect.initial_velocity);
}

// decomp: BATTLE.PRG func_80092548 (src/BATTLE/BATTLE.PRG/2842C.c:924) and
// func_800927AC (2842C.c:963): the group ids are padded to four bytes.
std::size_t ScriptPreambleBytes(std::uint8_t object_count) {
  const std::size_t remainder = object_count % kScriptIdAlignment;
  const std::size_t padding =
      remainder == 0 ? 0 : kScriptIdAlignment - remainder;
  return kScriptHeaderBytes + object_count + padding;
}

std::size_t CommandExtent(RoomEffectOpcode opcode) {
  switch (opcode) {
    case RoomEffectOpcode::kRadialVelocity:
      return kRadialVelocityExtent;
    case RoomEffectOpcode::kVelocity:
      return kVelocityExtent;
    case RoomEffectOpcode::kSetTransform:
      return kTransformExtent;
    case RoomEffectOpcode::kShowGroup:
    case RoomEffectOpcode::kAdvanceMotion:
    case RoomEffectOpcode::kPlaySound:
    case RoomEffectOpcode::kStopSound:
    case RoomEffectOpcode::kStartEffect:
    case RoomEffectOpcode::kHideGroup:
    case RoomEffectOpcode::kStopScript:
      return kOneOperandExtent;
    case RoomEffectOpcode::kSkip:
    case RoomEffectOpcode::kHideObject:
    case RoomEffectOpcode::kJump:
      break;
  }
  return kRoomEffectCommandHeaderBytes;
}

// decomp: BATTLE.PRG func_80092B04 (src/BATTLE/BATTLE.PRG/2842C.c:1048):
// the header test ahead of the handler table call.
DecodedCommand DecodeCommand(std::span<const std::byte> scripts,
                             std::size_t cursor) {
  DecodedCommand decoded;
  if (!Fits(scripts.size(), cursor, kRoomEffectCommandHeaderBytes)) {
    decoded.status = CommandStatus::kHeaderOutOfRange;
    return decoded;
  }
  const auto size = Load<std::int16_t>(scripts, cursor);
  const auto opcode =
      Load<std::int16_t>(scripts, cursor + kCommandOpcodeOffset);
  decoded.command.size = size;
  decoded.command.opcode = static_cast<RoomEffectOpcode>(opcode);
  if (size == 0 || opcode >= kRoomEffectOpcodeCount) {
    decoded.status = CommandStatus::kEnd;
    return decoded;
  }
  if (opcode < 0) {
    decoded.status = CommandStatus::kNegativeOpcode;
    return decoded;
  }
  const auto extent = CommandExtent(decoded.command.opcode);
  if (!Fits(scripts.size(), cursor, extent)) {
    decoded.status = CommandStatus::kTruncated;
    return decoded;
  }
  std::memcpy(decoded.command.bytes.data(),
              scripts.subspan(cursor, extent).data(), extent);
  decoded.status = CommandStatus::kDecoded;
  return decoded;
}
}  // namespace VS::Field::room_effects
