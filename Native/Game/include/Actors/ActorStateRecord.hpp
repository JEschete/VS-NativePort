#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "Actors/ActorMotion.hpp"
#include "Actors/MotionRequests.hpp"
#include "Actors/SequencePlayback.hpp"
#include "Field/PlatformPlacement.hpp"
#include "Render/DepthCue.hpp"

// The actor-record state the actor flag, lifecycle and placement rules
// (BATTLE.PRG 30DB0.c and 3A1A0.c) read and write, and the host that binds
// them to the actor table, the field gimmicks standing in empty actor slots,
// the weapon models and the other actor systems. The rules see records only
// as ActorStateRecord values the host lends them, so they reach no actor
// record layout, PS1 address or bitfield word.
namespace VS::Actors {
// The actor table (Actors_actorRecords): Ashley, the room actors and their
// companion models.
inline constexpr int kActorStateSlots = 17;
// attach_parent of an actor attached to no parent.
inline constexpr std::uint8_t kNoAttachParent = 0xFF;
// Companion slots below this are none (0) or a marker (1), not a record.
inline constexpr int kFirstCompanionRecord = 2;

// Where a companion model draws relative to its owner (Actors_CompanionOffset).
struct CompanionOffset {
  std::int8_t x = 0;
  std::int8_t y = 0;
  std::int8_t z = 0;
  std::uint8_t depth_bias = 0;

  friend bool operator==(const CompanionOffset&,
                         const CompanionOffset&) = default;
};

// A companion's draw offset and colour, copied together.
struct CompanionStyle {
  CompanionOffset offset;
  Render::ColorCode color;

  friend bool operator==(const CompanionStyle&,
                         const CompanionStyle&) = default;
};

// The unnamed request bytes reserved1 to reserved3 (Actors_MovementRequest).
inline constexpr std::size_t kMovementRequestReservedBytes = 3;

// The movement-request bytes ActorMotion does not hold; the movement resets
// clear the whole request.
struct MovementRequestExtras {
  std::array<std::uint8_t, kMovementRequestReservedBytes> reserved{};
  std::int32_t reserved_c = 0;
  MotionVector step{};

  friend bool operator==(const MovementRequestExtras&,
                         const MovementRequestExtras&) = default;
};

// The shadow-blend bytes ShadowBlend does not hold; the motion reset clears
// the whole blend.
struct ShadowBlendExtras {
  std::int32_t reserved0 = 0;
  std::uint8_t reserved6 = 0;
  std::uint8_t reserved7 = 0;

  friend bool operator==(const ShadowBlendExtras&,
                         const ShadowBlendExtras&) = default;
};

// The record fields the actor state rules use. Fields marked "read" are never
// stored back; bitfield widths are noted and every rule keeps the values it
// stores within them. motion.position.w is the position's fourth halfword and
// motion.current_tile.spare the fourth byte of the packed tile word.
struct ActorStateRecord {
  ActorMotion motion;
  // Model instance.
  bool skipped = false;
  bool visible = false;
  bool frozen = false;
  bool sound_enabled = false;
  bool attached = false;
  std::uint8_t reaction_state = 0;   // 4 bits
  std::uint8_t support_layer = 0;    // 4 bits
  std::uint8_t placement_layer = 0;  // 4 bits
  std::uint8_t bone_count = 0;       // read
  std::uint8_t attach_parent = kNoAttachParent;
  std::uint8_t attach_bone = 0;
  std::uint16_t look_yaw = 0;
  std::uint16_t look_pitch = 0;
  // The halfword after rotation z; the rotation word clears zero it.
  std::int16_t rotation_reserved = 0;
  std::uint8_t palette_pulse_kind = 0;
  // Actor record.
  std::uint8_t attached_child_count = 0;  // 2 bits
  bool animation_locked = false;
  bool animation_event = false;         // read
  std::uint8_t support_gimmick = 0;     // 4 bits, read
  bool bypass_actor_collision = false;  // read
  bool palette_fade_active = false;
  bool palette_fade_reverse = false;
  std::uint8_t animation_id = 0;  // read
  std::int16_t collision_state = 0;
  std::uint16_t collision_height = 0;  // read
  std::uint8_t queued_animation = 0;
  std::uint8_t contact_kind = 0;
  std::int16_t ledge_height = 0;
  std::int8_t platform_offset_x = 0;
  std::int8_t platform_offset_z = 0;
  CompanionStyle companion;
  bool holding_last_frame = false;  // read
  bool pose_reached_end = false;    // read
  MotionVector platform_anchor{};
  std::int8_t head_track_target = 0;
  std::uint8_t companion_slot = 0;
  // An unnamed record byte the motion reset clears (0x17FF).
  std::uint8_t reserved_17ff = 0;
  // The first palette fade state byte.
  std::uint8_t palette_fade_state = 0;
  std::array<std::uint8_t, 2> texture_timelines{};  // 4 bits each
  MovementRequestExtras request_extras;
  // An unnamed record byte the motion reset clears (0x1860).
  std::uint8_t reserved_1860 = 0;
  ShadowBlend shadow;
  ShadowBlendExtras shadow_extras;
  std::uint16_t shadow_expand_rate = 0;  // read
  std::uint16_t shadow_shrink_rate = 0;  // read
  PackedTile saved_placement{};          // read
  std::uint16_t pending_target_x = 0;
  std::uint16_t pending_target_z = 0;
  std::uint16_t previous_target_x = 0;
  std::uint16_t previous_target_z = 0;

  friend bool operator==(const ActorStateRecord&,
                         const ActorStateRecord&) = default;
};

// A field gimmick standing in an actor slot with no record.
struct GimmickView {
  bool skipped = false;
  bool visible = false;
  PackedTile tile{};
  MotionVector position{};
  std::uint16_t facing = 0;

  friend bool operator==(const GimmickView&, const GimmickView&) = default;
};

// The actor table, gimmicks, weapon models and actor systems the state rules
// use. Records are lent by Record and stay valid for the whole operation;
// every other call may read or write the records, so the host stores the
// lent records before it and reloads them after it.
class ActorStateHost {
 public:
  ActorStateHost() = default;
  ActorStateHost(const ActorStateHost&) = delete;
  ActorStateHost& operator=(const ActorStateHost&) = delete;
  ActorStateHost(ActorStateHost&&) = delete;
  ActorStateHost& operator=(ActorStateHost&&) = delete;
  virtual ~ActorStateHost() = default;

  // The record in actor slot `id`; null for an empty slot or an id outside
  // the actor table.
  [[nodiscard]] virtual ActorStateRecord* Record(int id) = 0;
  // The gimmick bound to actor slot `id`.
  [[nodiscard]] virtual std::optional<GimmickView> Gimmick(int id) = 0;
  virtual void SetGimmickDisplay(int id, bool skipped, bool visible) = 0;
  virtual void SetGimmickMode(int id, std::int16_t mode) = 0;
  virtual void ClearGimmickSupport(int id) = 0;
  virtual void PlayGimmickSound(int id, int sound, int variant) = 0;

  // Whether weapon model `index` (two per actor slot) is shown, or nothing
  // when there is no such model; setting an absent model does nothing.
  [[nodiscard]] virtual std::optional<bool> WeaponModelShown(int index) = 0;
  virtual void SetWeaponModelShown(int index, bool shown) = 0;

  // Whether the current shared sequence bank has texture pages.
  [[nodiscard]] virtual bool SharedBankHasTexturePages() = 0;
  virtual void ResetHeadTrackingSpeed(int id) = 0;
  virtual void UploadTextureFrame(int id, int channel, int frame) = 0;
  // Returns the actor to its own colour.
  virtual void ClearTint(int id) = 0;
  // Applies a silent sound override.
  virtual void ClearSoundOverrides(int id) = 0;
  virtual void PlayAnimation(int id, int animation, int blend_frames) = 0;
  [[nodiscard]] virtual bool HasAnimation(int id, int animation) = 0;
  // Frees moving platform `platform` from its rider.
  virtual void ReleaseMovingPlatform(int platform) = 0;
  [[nodiscard]] virtual int SupportHeight(int x, int z) = 0;
  // The room's navigation ceiling height; 0 for an open room.
  [[nodiscard]] virtual int NavigationCeilingHeight() = 0;
  [[nodiscard]] virtual std::optional<Field::PlatformContact>
  MovingPlatformContact(int support_layer) = 0;
  virtual void SaveGroundedTransform(int id) = 0;
};
}  // namespace VS::Actors
