#pragma once
// The room gimmicks: the pushable, breakable and animated objects a room
// places, their animation queue and the moving-platform frame state, plus the
// active room's section pointers (roomData). Gimmicks.cpp defines the globals.

#include <array>
#include <cstddef>
#include <cstdint>

#include "Actors/ActorRecord.h"
#include "Actors/Animation.hpp"
#include "Actors/ModelInstance.h"
#include "Actors/Shadow.hpp"
#include "Core/Gte.h"
#include "Field/Mpd.hpp"
#include "Field/Room.hpp"
#include "Items/ItemRecords.hpp"
#include "Render/Palette.hpp"

namespace VS::Actors {
struct ShpModel;
}  // namespace VS::Actors

namespace VS::Field {

inline constexpr std::size_t kGimmickBoneMatrixCount = 5;
inline constexpr std::size_t kGimmickInstanceReserved114Bytes = 0x18;
inline constexpr std::size_t kGimmickPaletteColorCount = 16;
inline constexpr std::size_t kGimmickInstanceReserved178Bytes = 0x434;
inline constexpr std::size_t kGimmickInstanceReserved5ECBytes = 0x50;
inline constexpr std::size_t kGimmickInstanceReserved640Bytes = 8;
inline constexpr std::size_t kGimmickInstanceReserved658Bytes = 0x88;
inline constexpr std::size_t kGimmickInstanceReserved11A4Bytes = 0x280;
inline constexpr std::size_t kGimmickPaletteRowColors = 0xA0;
inline constexpr std::size_t kGimmickInstanceReserved16A4Bytes = 0x158;
inline constexpr std::size_t kGimmickInstanceReserved1800Bytes = 0x3C;
inline constexpr std::size_t kGimmickInstanceReserved1854Bytes = 0x10;
inline constexpr std::size_t kGimmickPhysicsReserved0Bytes = 8;
inline constexpr std::size_t kGimmickPhysicsReservedCBytes = 3;
inline constexpr std::size_t kGimmickPhysicsReserved42Bytes = 0x1A;
inline constexpr std::size_t kGimmickPhysicsReserved60Bytes = 8;
inline constexpr std::size_t kGimmickPhysicsReserved6CBytes = 256;

// decomp: D_800E8FB4_t (src/BATTLE/BATTLE.PRG/30DB0.c:32-37)
struct GimmickTemplate {
  std::int32_t instance_flags;
  std::int32_t instance_word4;
  std::int32_t recolor_count;
  std::int32_t model_data_size;
};
inline constexpr std::size_t kGimmickTemplateBytes = 0x10;
static_assert(sizeof(GimmickTemplate) == kGimmickTemplateBytes);

// decomp: D_800F1D28_t (src/BATTLE/BATTLE.PRG/146C.c:123-126)
struct GimmickAnimationJob {
  std::int32_t job_kind;
  VS::Field::MpdLockTrigger* trigger;
};

// decomp: D_800F4538_t2 (src/BATTLE/BATTLE.PRG/entity.h:7-20)
struct GimmickObjectInfo {
  std::uint8_t object_id : 3;
  std::uint8_t is_falling : 1;
  std::uint8_t interaction : 2;
  std::uint8_t reserved_bits7 : 2;
  std::uint8_t subtype;
  std::uint8_t material;
  std::uint8_t carrier;
  std::uint8_t support_index;
  std::uint8_t reserved5;
  std::uint8_t reserved6;
  std::uint8_t reserved7;
  std::int32_t reserved8;
};
inline constexpr std::size_t kGimmickObjectInfoBytes = 0xC;
static_assert(sizeof(GimmickObjectInfo) == kGimmickObjectInfoBytes);

// decomp: D_800F45E0_t (src/BATTLE/BATTLE.PRG/entity.h:514-522 declares
// the first 0x178 bytes; the rest of the 0x1880-byte record was named from
// its readers when copied, 3A1A0.h:633).
struct GimmickInstance {
  char bone_count;
  char reserved1;
  char reserved2;
  char reserved3;
  std::int32_t reserved4;
  char is_skipped : 1;
  char is_visible : 1;
  char reserved8_bits2 : 1;
  char reserved8_bits3 : 1;
  char reserved8_bits4 : 1;
  char reserved8_bits5 : 1;
  char reserved8_bits6 : 1;
  char transition_flag : 1;
  std::uint8_t motion_state : 4;
  std::uint8_t reserved9_bits4 : 4;
  std::uint8_t transition_mode : 3;
  std::uint8_t reserved_a_bits3 : 2;
  std::uint8_t reserved_a_bits5 : 2;
  std::uint8_t reserved_a_bits7 : 1;
  std::uint8_t reserved_b_bits0 : 4;
  std::uint8_t reserved_b_bits4 : 3;
  std::uint8_t reserved_b_bits7 : 1;
  std::uint8_t support_layer : 4;
  std::uint8_t reserved_c_bits4 : 4;
  std::uint8_t reserved_d;
  std::uint8_t reserved_e;
  std::uint8_t slot;
  std::uint8_t reserved10;
  std::uint8_t delay;
  std::uint8_t parent;
  std::uint8_t linked_gimmick;
  std::int32_t reserved14;
  std::int16_t rotation_ticks;
  std::int16_t mode;
  std::int16_t position_x;
  std::int16_t position_y;
  std::int16_t position_z;
  std::int16_t reserved22;
  std::int16_t rotation_x;
  std::uint16_t rotation_y;
  std::uint16_t rotation_z;
  std::int16_t reserved2a;
  std::int16_t scale_x;
  std::int16_t scale_y;
  std::int16_t scale_z;
  std::int16_t reserved32;
  std::int16_t slide_x;
  std::int16_t velocity_y;
  std::int16_t slide_z;
  std::int16_t orientation;
  std::int16_t rotation_delta_x;
  std::int16_t rotation_delta_y;
  std::int16_t rotation_delta_z;
  std::int16_t reserved42;
  std::int32_t reserved44;
  std::int32_t reserved48;
  std::int32_t saved_position;
  std::int16_t reserved50;
  std::int16_t saved_rotation_y;
  std::int32_t base_color;
  VS::Render::ColorCode tint;
  char tile_x;
  // Volatile as declared by the original.
  volatile char tile_layer;
  char tile_z;
  char reserved5f;
  std::int32_t saved_tile;
  Actors_ModelEffectSlot palette_pulse;
  VS::Actors::ShpModel* model;
  std::array<MATRIX, kGimmickBoneMatrixCount> matrices;
  SVECTOR rotation_offset;
  std::array<std::uint8_t, kGimmickInstanceReserved114Bytes> reserved114;
  std::array<std::uint16_t, kGimmickPaletteColorCount> base_palette;
  std::array<std::uint16_t, kGimmickPaletteColorCount> pulsed_palette;
  GimmickObjectInfo object_info;
  std::array<std::uint8_t, kGimmickInstanceReserved178Bytes> reserved178;
  std::uint32_t reserved5ac_bits0 : 3;
  std::uint32_t action_pose_active : 1;
  std::uint32_t reserved5ac_bits4 : 2;
  std::uint32_t reserved5ac_bits6 : 2;
  std::uint32_t reserved5ac_bits8 : 1;
  std::uint32_t bypass_actor_collision : 2;
  std::uint32_t reserved5ac_bits11 : 1;
  std::uint32_t reserved5ac_bits12 : 1;
  std::uint32_t reserved5ac_bits13 : 1;
  std::uint32_t reserved5ac_bits14 : 1;
  std::uint32_t reserved5ac_bits15 : 1;
  std::uint32_t reserved5ac_bits16 : 2;
  std::uint32_t reserved5ac_bits18 : 1;
  std::uint32_t reserved5ac_bits19 : 1;
  std::uint32_t reserved5ac_bits20 : 1;
  std::uint32_t reserved5ac_bits21 : 1;
  std::uint32_t reserved5ac_bits22 : 1;
  std::uint32_t reserved5ac_bits23 : 1;
  std::uint32_t animation_locked : 1;
  std::uint32_t reserved5ac_bits25 : 7;
  std::uint8_t reserved5b0;
  std::uint8_t reserved5b1;
  std::uint8_t reserved5b2;
  std::uint8_t reserved5b3;
  std::uint8_t reserved5b4;
  std::uint8_t reserved5b5;
  std::uint8_t reserved5b6;
  std::uint8_t reserved5b7;
  std::uint8_t reserved5b8;
  std::uint8_t reserved5b9;
  std::uint8_t reserved5ba;
  std::uint8_t reserved5bb;
  std::int16_t reserved5bc;
  std::int16_t reserved5be;
  std::uint8_t reserved5c0;
  std::uint8_t reserved5c1;
  std::uint16_t reserved5c2;
  std::int32_t reserved5c4;
  std::int32_t reserved5c8;
  char reserved5cc;
  char reserved5cd;
  std::int16_t reserved5ce;
  std::int32_t reserved5d0;
  std::int32_t reserved5d4;
  void* reserved5d8;
  std::int32_t reserved5dc;
  std::int32_t reserved5e0;
  std::int32_t reserved5e4;
  std::int32_t reserved5e8;
  std::array<std::uint8_t, kGimmickInstanceReserved5ECBytes> reserved5ec;
  std::uint16_t collision_radius;
  std::uint16_t reserved63e;
  std::array<std::uint8_t, kGimmickInstanceReserved640Bytes> reserved640;
  std::int32_t reserved648;
  std::uint8_t reserved64c;
  std::uint8_t reserved64d;
  std::uint16_t reserved64e;
  std::uint16_t reserved650;
  std::uint16_t reserved652;
  std::uint16_t reserved654;
  std::uint16_t reserved656;
  std::array<std::uint8_t, kGimmickInstanceReserved658Bytes> reserved658;
  std::uint8_t reserved6e0;
  std::uint8_t reserved6e1;
  std::uint8_t reserved6e2;
  std::uint8_t reserved6e3;
  std::uint8_t reserved6e4;
  std::uint8_t reserved6e5;
  std::uint16_t reserved6e6;
  std::int16_t reserved6e8;
  std::int16_t reserved6ea;
  std::int32_t reserved6ec;
  std::uint8_t reserved6f0;
  std::uint8_t fade_green;
  std::uint8_t fade_red;
  std::uint8_t reserved6f3;
  Actors_CompanionOffset reserved6f4;
  VS::Render::ColorCode reserved6f8;
  std::int32_t reserved6fc;
  std::int32_t reserved700;
  VS::Actors::AnimationPose reserved704;
  VS::Actors::AnimationPose reserved_c54;
  std::array<std::uint8_t, kGimmickInstanceReserved11A4Bytes> reserved11a4;
  std::array<std::uint16_t, kGimmickPaletteRowColors> base_palette_rows;
  std::array<std::uint16_t, kGimmickPaletteRowColors> transition_palette;
  std::array<std::uint8_t, kGimmickInstanceReserved16A4Bytes> reserved16a4;
  std::uint8_t reserved17fc;
  std::uint8_t reserved17fd;
  std::uint8_t reserved17fe;
  std::uint8_t reserved17ff;
  std::array<std::uint8_t, kGimmickInstanceReserved1800Bytes> reserved1800;
  std::int16_t reserved183c;
  std::int16_t reserved183e;
  std::int16_t reserved1840;
  std::int16_t reserved1842;
  std::int16_t reserved1844;
  std::uint8_t reserved1846;
  std::uint8_t reserved1847;
  std::uint8_t reserved1848;
  std::uint8_t reserved1849;
  std::uint8_t reserved184a;
  std::uint8_t reserved184b;
  std::uint16_t reserved184c;
  std::uint16_t reserved184e;
  std::uint32_t reserved1850;
  std::array<std::uint8_t, kGimmickInstanceReserved1854Bytes> reserved1854;
  VS::Actors::ShadowBlendRecord shadow_blend;
  std::int32_t saved_placement;
  std::int32_t reserved187c;
};

// decomp: func_800A0024_t (BATTLE.PRG 30DB0.c:24 when copied; no longer
// declared upstream)
struct GimmickSnapshot {
  std::uint8_t placement_byte0;
  std::uint8_t height_above_ground;
  std::uint8_t placement_byte2;
  std::uint8_t orientation;
  std::int32_t position_xy;
  std::int32_t position_z;
};
inline constexpr std::size_t kGimmickSnapshotBytes = 0xC;
static_assert(sizeof(GimmickSnapshot) == kGimmickSnapshotBytes);

// decomp: objectAnimationState (BATTLE.PRG 44F14.c:148 when copied; no
// longer declared upstream). A view of GimmickInstance (D_800F45E0_t).
struct GimmickPhysicsView {
  std::array<char, kGimmickPhysicsReserved0Bytes> reserved0;
  std::int32_t flags;
  std::array<char, kGimmickPhysicsReservedCBytes> reserved_c;
  std::uint8_t id;
  std::uint8_t reserved10;
  std::int8_t delay;
  std::uint8_t parent;
  std::uint8_t reserved13;
  std::int32_t reserved14;
  std::int16_t rotation_ticks;
  std::int16_t mode;
  SVECTOR position;
  std::int16_t rotation_x;
  std::int16_t rotation_y;
  std::int16_t rotation_z;
  std::int16_t reserved2a;
  std::int16_t scale_x;
  std::int16_t scale_y;
  std::int16_t scale_z;
  std::int16_t reserved32;
  std::int16_t slide_x;
  std::int16_t velocity_y;
  std::int16_t slide_z;
  std::int16_t orientation;
  std::int16_t rotation_remaining_x;
  std::int16_t rotation_remaining_y;
  std::int16_t rotation_remaining_z;
  std::array<char, kGimmickPhysicsReserved42Bytes> reserved42;
  std::uint8_t tile_x;
  std::uint8_t reserved5d;
  std::uint8_t tile_z;
  std::uint8_t reserved5f;
  std::array<char, kGimmickPhysicsReserved60Bytes> reserved60;
  // PS1 pointer slot 0x68.
  void* native_pointer68;
  std::array<char, kGimmickPhysicsReserved6CBytes> reserved6c;
  std::uint8_t material;
  std::uint8_t durability;
  std::uint8_t reserved16e;
  std::uint8_t carrier;
  std::uint8_t frame;
};

inline constexpr std::size_t kTriggerMarkerRecordTailBytes = 192;
inline constexpr std::size_t kGimmickPaletteParametersBytes = 32;
inline constexpr std::size_t kGimmickModelTemplateCount = 5;
inline constexpr std::size_t kGimmickRollOrientationsBytes = 3424;
inline constexpr std::size_t kOrientationRotationsBytes = 3328;
inline constexpr std::size_t kGimmickTemplateIndicesBytes = 2964;
inline constexpr std::size_t kSpinHeightStepsBytes = 2948;
inline constexpr std::size_t kSpinPitchStepsBytes = 2932;
inline constexpr std::size_t kSpinYawStepsBytes = 2916;
inline constexpr std::size_t kSpinRollStepsBytes = 2900;
inline constexpr std::size_t kBounceHeightStepsBytes = 2884;
inline constexpr std::size_t kTiltDepthStepsBytes = 2880;
inline constexpr std::size_t kTiltAngleStepsBytes = 2868;
inline constexpr std::size_t kRollHeightStepsBytes = 2856;
inline constexpr std::size_t kRollDisplacementStepsBytes = 2840;
inline constexpr std::size_t kRollAngleStepsBytes = 2824;
inline constexpr std::size_t kSquashHeightStepsBytes = 2808;
inline constexpr std::size_t kSquashScaleXBytes = 2792;
inline constexpr std::size_t kSquashScaleYBytes = 2776;
inline constexpr std::size_t kSquashScaleZBytes = 2760;
inline constexpr std::size_t kGimmickJobCount = 8;
inline constexpr std::size_t kPlatformBounceFramesBytes = 1304;
inline constexpr std::size_t kGimmickPaletteColorsBytes = 240;
inline constexpr std::size_t kGimmickInstanceCount = 16;
inline constexpr std::size_t kPlatformBounceOffsetCount = 7;

// A module-image byte table, sized to the extent the original reserved.
template <std::size_t Size>
using GimmickBytes = std::array<std::uint8_t, Size>;
using BounceOffsetTable = std::array<std::int16_t, kPlatformBounceOffsetCount>;

extern std::array<char, kTriggerMarkerRecordTailBytes> triggerMarkerRecordTail;
extern GimmickBytes<kGimmickPaletteParametersBytes> gimmickPaletteParameters;
extern std::array<void*, kGimmickModelTemplateCount> gimmickModelTemplates;
extern GimmickBytes<kGimmickRollOrientationsBytes> kGimmickRollOrientations;
extern GimmickBytes<kOrientationRotationsBytes> kGimmickOrientationRotations;
extern std::int32_t interactingGimmickId;
extern GimmickBytes<kGimmickTemplateIndicesBytes> gimmickTemplateIndices;
extern GimmickBytes<kSpinHeightStepsBytes> kGimmickSpinHeightSteps;
extern GimmickBytes<kSpinPitchStepsBytes> kGimmickSpinPitchSteps;
extern GimmickBytes<kSpinYawStepsBytes> kGimmickSpinYawSteps;
extern GimmickBytes<kSpinRollStepsBytes> kGimmickSpinRollSteps;
extern GimmickBytes<kBounceHeightStepsBytes> kGimmickBounceHeightSteps;
extern GimmickBytes<kTiltDepthStepsBytes> kGimmickTiltDepthSteps;
extern GimmickBytes<kTiltAngleStepsBytes> kGimmickTiltAngleSteps;
extern GimmickBytes<kRollHeightStepsBytes> kGimmickRollHeightSteps;
extern GimmickBytes<kRollDisplacementStepsBytes> kGimmickRollDisplacementSteps;
extern GimmickBytes<kRollAngleStepsBytes> kGimmickRollAngleSteps;
extern GimmickBytes<kSquashHeightStepsBytes> kGimmickSquashHeightSteps;
extern GimmickBytes<kSquashScaleXBytes> kGimmickSquashScaleX;
extern GimmickBytes<kSquashScaleYBytes> kGimmickSquashScaleY;
extern GimmickBytes<kSquashScaleZBytes> kGimmickSquashScaleZ;
extern RoomDataRecord roomData;
extern std::uint16_t roomAnimationTicks;
extern std::array<GimmickAnimationJob, kGimmickJobCount> gimmickAnimationQueue;
extern std::int16_t platformFreezeFrames;
extern std::int16_t pendingGimmickAnimationCount;
extern std::uint16_t triggerMarkerFrameTicks;
extern std::uint8_t areMovingPlatformsPaused;
extern char isPlatformFreezeTintActive;
extern GimmickBytes<kPlatformBounceFramesBytes> platformBounceFrames;
extern std::array<char, kGimmickPaletteColorsBytes> gimmickPaletteColors;
extern std::array<GimmickInstance*, kGimmickInstanceCount> gimmickInstances;
extern std::int32_t gimmickInteractionDirection;
extern const BounceOffsetTable kPlatformBounceOffsets;

}  // namespace VS::Field
