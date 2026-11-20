#pragma once
// The effect instance's transform slots and the package's block-10
// transform definitions in their PS1 layouts. TransformsLive.cpp binds them
// to TransformState and TransformDefinition (Effects/Transforms.hpp), their
// typed values.

#include <array>
#include <cstddef>
#include <cstdint>

#include "Core/Math/Matrix.hpp"
#include "Legacy/Psx.hpp"

namespace VS::Effects {
// decomp: func_800D0548_arg1 (src/BATTLE/BATTLE.PRG/battleEffect.h:281):
// one of an effect instance's 16 transforms (D_800F53B8_t.unk1C,
// battleEffect.h:350).
struct LiveTransformSlot {
  // Accumulated rotation of the animated local transform.
  std::array<std::int16_t, Core::Math::kMatrixOrder> rotation;
  // Bit 0 attachment resolved, bit 1 target resolved, bit 2 frame advances,
  // bit 3 spline controls built (5BF94.c:5658, 5817, 5822 and 5841).
  std::uint16_t flags;
  // Cleared when the instance starts (5BF94.c:4659); never read.
  std::int32_t reserved_8;
  // Cleared when the instance starts (5BF94.c:4658); never read.
  std::int16_t reserved_c;
  // Motion frame; advances while the definition's limit allows.
  std::int16_t frame;
  // Cleared when the instance starts (5BF94.c:4660-4662); never read.
  std::array<std::int32_t, Core::Math::kMatrixOrder> reserved_10;
  // Position along the motion; scripts set vy to raise or lower the effect
  // (5BF94.c:6602-6606). The instance start clears only pad.
  VS::Core::Gte::VECTOR position;
  // Cleared when the instance starts (5BF94.c:4663-4664); never read.
  std::array<std::int32_t, 2> reserved_2c;
  // Actor and bone the transform attaches to.
  std::uint8_t actor;
  std::uint8_t bone;
  // Actor and bone the transform moves towards.
  std::uint8_t target_actor;
  std::uint8_t target_bone;
  // Effect-space matrix the renderers draw through.
  VS::Core::Gte::MATRIX world;
  // world composed with the camera's view matrix.
  VS::Core::Gte::MATRIX view;
  // Matrix of the attached actor bone, the camera or the action origin.
  VS::Core::Gte::MATRIX attachment;
  // Rotation that turns the transform towards its target.
  VS::Core::Gte::MATRIX facing;
  // Resolved target point; pad keeps the attached actor's facing angle.
  VS::Core::Gte::SVECTOR saved;
  // Spline control points between the world translation and saved.
  VS::Core::Gte::SVECTOR control0;
  VS::Core::Gte::SVECTOR control1;
};

// Values in each transform range table: (start, end) for x, y and z.
inline constexpr auto kRangeValues = std::size_t{2} * Core::Math::kMatrixOrder;

// decomp: func_800D0B30_t (src/BATTLE/BATTLE.PRG/battleEffect.h:57): one
// block-10 transform definition.
struct LiveTransformDefinition {
  std::uint16_t flags;
  std::uint8_t attach_actor;
  std::uint8_t attach_bone;
  std::uint8_t translation_curve;
  std::uint8_t curve;
  std::uint8_t rotation_curve;
  std::uint8_t scale_curve;
  std::uint16_t rotation_x;
  std::uint16_t mode;
  std::uint16_t rotation_y;
  std::uint16_t limit : 8;
  std::uint16_t radius : 4;
  std::uint16_t radius_jitter : 4;
  std::uint16_t rotation_z;
  std::uint16_t shape;
  std::array<std::int16_t, kRangeValues> rotation_range;
  std::array<std::int16_t, kRangeValues> translation_range;
  std::array<std::int16_t, kRangeValues> scale_range;
};

namespace layout {
inline constexpr std::size_t kTransformSlotBytes = 0xD0;
static_assert(sizeof(LiveTransformSlot) == kTransformSlotBytes);
inline constexpr std::size_t kSlotFrameOffset = 0x0E;
static_assert(offsetof(LiveTransformSlot, frame) == kSlotFrameOffset);
inline constexpr std::size_t kSlotPositionOffset = 0x1C;
static_assert(offsetof(LiveTransformSlot, position) == kSlotPositionOffset);
inline constexpr std::size_t kSlotActorOffset = 0x34;
static_assert(offsetof(LiveTransformSlot, actor) == kSlotActorOffset);
inline constexpr std::size_t kSlotWorldOffset = 0x38;
static_assert(offsetof(LiveTransformSlot, world) == kSlotWorldOffset);
inline constexpr std::size_t kSlotAttachmentOffset = 0x78;
static_assert(offsetof(LiveTransformSlot, attachment) == kSlotAttachmentOffset);
inline constexpr std::size_t kSlotSavedOffset = 0xB8;
static_assert(offsetof(LiveTransformSlot, saved) == kSlotSavedOffset);
inline constexpr std::size_t kSlotControl1Offset = 0xC8;
static_assert(offsetof(LiveTransformSlot, control1) == kSlotControl1Offset);
inline constexpr std::size_t kTransformDefinitionRecordBytes = 0x38;
static_assert(sizeof(LiveTransformDefinition) ==
              kTransformDefinitionRecordBytes);
inline constexpr std::size_t kDefinitionRotationZOffset = 0x10;
static_assert(offsetof(LiveTransformDefinition, rotation_z) ==
              kDefinitionRotationZOffset);
inline constexpr std::size_t kDefinitionRotationRangeOffset = 0x14;
static_assert(offsetof(LiveTransformDefinition, rotation_range) ==
              kDefinitionRotationRangeOffset);
inline constexpr std::size_t kDefinitionScaleRangeOffset = 0x2C;
static_assert(offsetof(LiveTransformDefinition, scale_range) ==
              kDefinitionScaleRangeOffset);
}  // namespace layout
}  // namespace VS::Effects
