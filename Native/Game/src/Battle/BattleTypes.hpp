#pragma once
// Battle record views: the hit status view, the targeting sphere's volumes,
// label lines and shape vertices, and a limb's effect pair.

#include <array>
#include <cstddef>
#include <cstdint>

#include "Core/Gte.h"

namespace VS::Battle {

inline constexpr std::size_t kHitStatusViewReservedWords = 8;

// The hit record as the hit-status finalizer reads it.
// decomp: HitEntity (src/BATTLE/BATTLE.PRG/hitEntity.h:19-56), as read by
// func_80085A34 (src/BATTLE/BATTLE.PRG/146C.c:13024)
struct HitStatusView {
  std::int8_t reserved0;
  std::int8_t reserved1;
  std::int8_t opaque2;
  std::int8_t opaque3;
  std::int32_t reserved4;
  std::int32_t reserved8;
  std::int32_t reserved_c;
  std::int32_t reserved10;
  std::int32_t opaque14;
  std::int32_t opaque18;
  std::int32_t opaque1c;
  std::array<std::int32_t, kHitStatusViewReservedWords> reserved20;
  std::int32_t opaque40;
};
inline constexpr std::size_t kHitStatusViewBytes = 0x44;
static_assert(sizeof(HitStatusView) == kHitStatusViewBytes,
              "HitStatusView must keep its PS1 layout");

// TargetVolume::scale read as four flag bytes.
// decomp: func_800C1564_flags (src/BATTLE/BATTLE.PRG/57B1C.h:4-9)
struct TargetVolumeFlags {
  std::uint32_t reserved0_bit0 : 8;
  std::uint32_t opaque1_bit0 : 8;
  std::uint32_t reserved2_bit0 : 8;
  std::uint32_t opaque3_bit0 : 8;
};
inline constexpr std::size_t kTargetVolumeFlagsBytes = 0x4;
static_assert(sizeof(TargetVolumeFlags) == kTargetVolumeFlagsBytes,
              "TargetVolumeFlags must keep its PS1 layout");

inline constexpr std::size_t kTargetVolumeExtentCount = 4;
inline constexpr std::size_t kTargetVolumePositionCount = 4;

// TargetVolume::scale, read as flags or as four extents.
// decomp: func_800C1564_t.unk4 (src/BATTLE/BATTLE.PRG/57B1C.h:15-18)
union TargetShapeExtent {
  TargetVolumeFlags flags;
  std::array<std::uint8_t, kTargetVolumeExtentCount> extents;
};

// One volume of the targeting sphere: its kind, facing angle, extents and
// position.
// decomp: func_800C1564_t (src/BATTLE/BATTLE.PRG/57B1C.h:11-23)
struct TargetVolume {
  std::uint8_t kind;
  std::uint8_t opaque1;
  std::int16_t angle;
  TargetShapeExtent scale;
  std::array<std::int16_t, kTargetVolumePositionCount> position;
};
inline constexpr std::size_t kTargetVolumeRecordScaleOffset = 0x4;
inline constexpr std::size_t kTargetVolumeRecordBytes = 0x10;
static_assert(offsetof(TargetVolume, scale) == kTargetVolumeRecordScaleOffset,
              "TargetVolume::scale must keep its PS1 offset");
static_assert(sizeof(TargetVolume) == kTargetVolumeRecordBytes,
              "TargetVolume must keep its PS1 layout");

inline constexpr std::size_t kTargetLabelLinePositionCount = 3;
inline constexpr std::size_t kTargetLabelLineReservedBytes = 0xD;

// A targeting list line as the sphere's target highlights read it.
// decomp: D_800F19CC_t5 (src/BATTLE/BATTLE.PRG/57B1C.h:25-38), as read by
// func_800C4650 (src/BATTLE/BATTLE.PRG/58578.c:1071)
struct TargetLabelLine {
  std::array<std::uint16_t, kTargetLabelLinePositionCount> opaque0;
  std::uint8_t reserved6;
  std::uint8_t reserved7;
  std::uint8_t reserved8;
  std::uint8_t opaque9;
  std::int8_t opaque_a;
  std::array<std::int8_t, kTargetLabelLineReservedBytes> reserved_b;
};
inline constexpr std::size_t kTargetLabelLineBytes = 0x18;
static_assert(sizeof(TargetLabelLine) == kTargetLabelLineBytes,
              "TargetLabelLine must keep its PS1 layout");

// A targeting list line as the sphere's label updater reads it.
// decomp: D_800F19CC_t5 (src/BATTLE/BATTLE.PRG/57B1C.h:25-38), as returned
// by func_800C5798 (src/BATTLE/BATTLE.PRG/5BF94.c:839)
struct TargetInfoLine {
  std::int32_t opaque0;
  std::int32_t opaque4;
  std::uint8_t opaque8;
  std::uint8_t opaque9;
  std::int8_t opaque_a;
  std::uint8_t opaque_b;
  std::uint8_t opaque_c;
  std::uint8_t reserved_d;
  std::int16_t opaque_e;
  std::int32_t opaque10;
  std::uint8_t opaque14;
  std::uint8_t opaque15;
  std::uint8_t opaque16;
  std::uint8_t opaque17;
};
inline constexpr std::size_t kTargetInfoLineBytes = 0x18;
static_assert(sizeof(TargetInfoLine) == kTargetInfoLineBytes,
              "TargetInfoLine must keep its PS1 layout");

inline constexpr std::size_t kTargetSphereLabelEntryOpaqueBytes = 3;

// One limb label of the targeting sphere.
// decomp: D_800EB9B8_unk990 (src/BATTLE/BATTLE.PRG/58578.h:6-17)
struct TargetSphereLabelEntry {
  std::int8_t opaque0;
  std::array<std::int8_t, kTargetSphereLabelEntryOpaqueBytes> opaque1;
  std::int16_t opaque4;
  std::uint8_t opaque6;
  std::int8_t opaque7;
  std::int32_t opaque8;
  std::int32_t opaque_c;
  char* limb_name;
};

inline constexpr std::size_t kTargetSphereVolumeCount = 2;
inline constexpr std::size_t kTargetSphereReserved48Bytes = 0x948;
inline constexpr std::size_t kTargetSphereLabelCount = 24;
inline constexpr std::size_t kTargetSphereReservedB70Bytes = 0x3EE4;
inline constexpr std::size_t kTargetSphereTextBufferBytes = 200;

// The targeting sphere's state as its label code reads it: the two volumes,
// the info lines, the limb labels and the label text buffer.
// decomp: D_800EB9B8_t (src/BATTLE/BATTLE.PRG/58578.h:19-41), as read by
// func_800C56C0 (src/BATTLE/BATTLE.PRG/5BF94.c:824)
struct TargetSphereLabelView {
  std::array<TargetVolume, kTargetSphereVolumeCount> opaque0;
  std::int16_t reserved20;
  std::int16_t opaque22;
  std::int32_t reserved24;
  std::int16_t reserved28;
  std::int16_t opaque2a;
  std::uint8_t opaque2c;
  std::uint8_t opaque2d;
  std::uint8_t opaque2e;
  std::uint8_t opaque2f;
  std::int8_t opaque30;
  std::uint8_t opaque31;
  std::array<std::int8_t, 2> reserved32;
  std::int32_t opaque34;
  SVECTOR opaque38;
  TargetInfoLine* opaque40;
  TargetInfoLine* opaque44;
  std::array<std::int8_t, kTargetSphereReserved48Bytes> reserved48;
  std::array<TargetSphereLabelEntry, kTargetSphereLabelCount> opaque990;
  std::array<std::int8_t, kTargetSphereReservedB70Bytes> reserved_b70;
  std::array<char, kTargetSphereTextBufferBytes> text_buffer;
};

// A limb's pair of effect values inside a hit result.
// decomp: LimbEffects (src/BATTLE/BATTLE.PRG/hitEntity.h:14-17)
struct LimbEffects {
  std::int16_t opaque0;
  std::int16_t opaque2;
};
inline constexpr std::size_t kLimbEffectsBytes = 0x4;
static_assert(sizeof(LimbEffects) == kLimbEffectsBytes,
              "LimbEffects must keep its PS1 layout");

inline constexpr std::size_t kShapeVertexAxisCount = 3;

// One vertex of a targeting sphere's wireframe shape.
// decomp: MenuShapeVertex (src/BATTLE/BATTLE.PRG/57B1C.h:40-43)
struct ShapeVertex {
  std::array<std::int16_t, kShapeVertexAxisCount> xyz;
  std::int16_t opaque6;
};
inline constexpr std::size_t kShapeVertexBytes = 0x8;
static_assert(sizeof(ShapeVertex) == kShapeVertexBytes,
              "ShapeVertex must keep its PS1 layout");

}  // namespace VS::Battle
