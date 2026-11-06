#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Actors/ActorRoster.hpp"
#include "Actors/BattleActorState.hpp"
#include "Content/ActorAssets.hpp"
#include "Content/CdDrive.hpp"

// Actor spawning (BATTLE.PRG 146C.c:6064-6770 and 8924-9256, INITBTL.PRG
// 18.c:304-460): gimmick actors, equipped actors and scripted actors, room
// enemies from the zone's ZUD files, the initial stats of a character
// template, the equipment model requests and Ashley's spawn from the
// inventory and the saved snapshot. The rules see actors as SpawnedActor
// handles and typed records the host lends them, so they reach no
// Battle_Actor, Actors_CharacterInit or ZUD layout and no PS1 address.
// ActorSpawningLive binds Battle_actors, the actor allocations, the zone
// context, the room enemy memory, Ashley's snapshot and the inventory.
namespace VS::Actors {
// An actor allocation the host owns, before and after it joins its slot.
enum class SpawnedActor : std::uintptr_t { kNone = 0 };

// The host's handle to a template's equipment parts, which the Items
// builders read (Actors_CharacterInit.weapon, shield, accessory and armor).
enum class TemplateEquipment : std::uintptr_t { kNone = 0 };

// The slots room actors, scripted actors and gimmicks use.
inline constexpr std::size_t kSpawnActorSlots = 16;
// Gimmick actors take the first free slot from here when asked for any.
inline constexpr std::size_t kFirstAuxiliarySlot = 2;
inline constexpr int kAnyAuxiliarySlot = 0xFF;
// AllocateActorWithEquipment only fills these slots (146C.c:6504).
inline constexpr std::size_t kEquippedActorSlots = 10;
// The equipment model slot of an actor and hand: actor * 2 + hand.
inline constexpr int kEquipmentModelSlots = Content::kWeaponModelSlots;
// An equipment model id that leaves the hand's model as it is.
inline constexpr int kUnchangedEquipment = 0xFF;
// A model id that loads no sequences, weapon or shield (146C.c:6513, 6642).
inline constexpr int kBareModel = 0x7F;

inline constexpr std::uint16_t kLeaderActorKind = 1;
// Actor 1 and spawns whose flags & 3 is 0.
inline constexpr std::uint16_t kNeutralActorKind = 2;
inline constexpr std::uint16_t kAuxiliaryActorKind = 0x10;
// The kinds that join the room actor list after Ashley.
inline constexpr std::uint16_t kListedActorKinds = 0x7;
inline constexpr std::uint16_t kAuxiliaryModelId = 0xFE;
// Battle_Ai_CreateActorAiState's flags for a fresh AI state.
inline constexpr int kFreshAiFlags = -1;

// The display flags a spawned actor gets (SetLiveActorDisplayFlags).
inline constexpr std::uint32_t kLoadingDisplayFlags = 0x1847;
inline constexpr std::uint32_t kWeaponDrawnDisplayFlags = 0x1846;
inline constexpr std::uint32_t kSheathedDisplayFlags = 0x46;

// The diagnostics the original raised and then ignored.
inline constexpr int kNoAuxiliarySlotDiagnostic = 0x86;
inline constexpr int kZudReadPendingDiagnostic = 0xA7;

// The movement speeds of a registered actor before its template applies
// (146C.c:6545), in 20.12 fixed point.
inline constexpr int kRegisteredWalkSpeed = 0x9000;
inline constexpr int kRegisteredRunSpeed = 0x12000;
inline constexpr int kSpeedFractionBits = 12;

// The equipped weapon category of an actor without a blade.
inline constexpr std::uint8_t kUnarmedWeaponCategory = 10;
// The categories the blade-model bounds cover (Items_weaponCategoryUpperIds).
inline constexpr std::size_t kWeaponCategoryBounds = 11;

// The texture slots of the equipment and second-body requests.
inline constexpr std::uint8_t kWeaponTextureSlot = 0xF0;
inline constexpr std::uint8_t kShieldTextureSlot = 0xF1;
inline constexpr std::uint8_t kSecondBodyTextureSlot = 0xFC;
// The material of a model load request (146C.c:6450).
inline constexpr std::uint8_t kModelLoadMaterial = 2;
// The sub-kind of a model load without a texture override.
inline constexpr std::uint8_t kDefaultSubKind = 0xFF;
// The second body model takes actor record slot actor + 4.
inline constexpr std::size_t kSecondBodySlotOffset = 4;

// Ashley's inventory extents (vs_main_inventory_t).
inline constexpr std::size_t kInventoryWeapons = 8;
inline constexpr std::size_t kInventoryBlades = 16;
inline constexpr std::size_t kInventoryShields = 8;
inline constexpr std::size_t kInventoryArmor = 16;
// The armor limb byte that marks an accessory.
inline constexpr std::uint8_t kAccessoryLimb = 7;
// The actor slot Ashley takes.
inline constexpr std::size_t kAshleySlot = 0;

inline constexpr std::size_t kLimbActions = 4;
inline constexpr std::size_t kLimbTypes = 4;
inline constexpr std::size_t kLimbAffinities = 8;
// ApplyActorInitialStats copies the first seven affinities of a limb; the
// eighth keeps its value (146C.c:6229).
inline constexpr std::size_t kInitialLimbAffinities = 7;
inline constexpr std::size_t kDpSplashTargets = 6;
inline constexpr std::size_t kStatusTimers = 8;
// ApplyActorInitialStats clears the first five status timers (146C.c:6286).
inline constexpr std::size_t kInitialStatusTimers = 5;
inline constexpr std::int32_t kMaxInitialStat = 999;
// A randomized stat moves by -10 to +10 (146C.c:6143).
inline constexpr int kStatRollRange = 21;
inline constexpr int kStatRollBias = 10;

// Field_ActorPlacement: four bytes the asset queue carries.
struct SpawnPlacement {
  std::uint8_t tile_x = 0;
  std::uint8_t direction = 0;
  std::uint8_t tile_z = 0;
  std::uint8_t height = 0;

  friend bool operator==(const SpawnPlacement&,
                         const SpawnPlacement&) = default;
};

// The placement word of an asset request: tile X, direction, tile Z and
// height, low byte first.
[[nodiscard]] constexpr std::uint32_t PackPlacement(SpawnPlacement placement) {
  constexpr unsigned kDirectionShift = 8;
  constexpr unsigned kTileZShift = 16;
  constexpr unsigned kHeightShift = 24;
  return static_cast<std::uint32_t>(placement.tile_x) |
         (static_cast<std::uint32_t>(placement.direction) << kDirectionShift) |
         (static_cast<std::uint32_t>(placement.tile_z) << kTileZShift) |
         (static_cast<std::uint32_t>(placement.height) << kHeightShift);
}

enum class ActorShape : std::uint8_t {
  // A gimmick actor: the actor, its stats and its model (allocation kind 0).
  kAuxiliary,
  // An actor with weapon and shield model slots (allocation kind 1).
  kEquipped,
  // An enemy whose second body takes the weapon model slot (kind 2).
  kSecondaryBody,
};

enum class EquipmentHand : std::uint8_t { kWeapon = 0, kShield = 1 };

enum class SpawnRefusal : std::uint8_t {
  // An actor id outside Battle_actors.
  kActorSlot,
  // No actor where the original dereferenced one.
  kNoActor,
  // An equipment model slot outside the weapon model table.
  kEquipmentSlot,
  // The heap refused the actor.
  kAllocation,
  // A template's ZUD index outside the zone's enemy files.
  kZudIndex,
  // The heap refused the ZUD file.
  kZudAllocation,
  // The disc read of the ZUD file failed.
  kZudRead,
  // A ZUD file too short for its header, or a section outside the file.
  kZudSection,
  // A listed actor would join the list after Ashley, but her slot is empty.
  // Refused before anything is allocated.
  kNoLeader,
  // A room enemy in Ashley's slot would join the list after itself.
  kSelfLeader,
  // Ashley's equipped weapon names no blade, or one past the inventory.
  kAshleyBlade,
  // Ashley's actor could not be created.
  kAshleyActor,
  // Ashley's runtime state is restored without Ashley's actor.
  kNoCharacter,
};

[[nodiscard]] const char* SpawnRefusalText(SpawnRefusal refusal);

// --- Character templates (Actors_CharacterInit) -----------------------------

struct LimbActionTemplate {
  std::uint16_t action_id = 0;
  std::uint8_t effect_model = 0;
  std::uint8_t index = 0;

  friend bool operator==(const LimbActionTemplate&,
                         const LimbActionTemplate&) = default;
};

struct LimbTemplate {
  std::uint16_t hp = 0;
  std::uint8_t agility_defense_bonus = 0;
  std::uint8_t chain_evasion = 0;
  std::array<std::int8_t, kLimbTypes> types{};
  std::array<std::int8_t, kLimbAffinities> affinities{};
  std::array<LimbActionTemplate, kLimbActions> actions{};
  std::array<std::uint8_t, kDpSplashTargets> dp_splash{};

  friend bool operator==(const LimbTemplate&, const LimbTemplate&) = default;
};

struct CharacterTemplate {
  std::uint16_t sub_type = 0;
  // The actor's model id (characterKind).
  std::uint8_t model_id = 0;
  std::uint8_t reach = 0;
  std::uint8_t enemy_class = 0;
  std::array<std::uint8_t, kActorNameBytes> name{};
  std::int16_t hp = 0;
  std::int16_t mp = 0;
  std::uint8_t strength = 0;
  std::uint8_t intelligence = 0;
  std::uint8_t agility = 0;
  std::uint8_t recovery_delay_bonus = 0;
  // The model material, which is also the movement class.
  std::uint8_t material = 0;
  std::uint8_t walk_speed = 0;
  std::uint8_t gait_class = 0;
  std::uint8_t run_speed = 0;
  std::int32_t status_immunities = 0;
  // Copied to the battle actor's templateWord2C.
  std::int32_t template_word = 0;
  std::array<std::uint8_t, 2> mobility_limbs{};
  std::uint8_t default_target_limb = 0;
  std::uint8_t blade_model = 0;
  std::uint8_t blade_category = 0;
  std::uint8_t weapon_material = 0;
  std::uint8_t shield_model = 0;
  std::uint8_t shield_material = 0;
  std::array<LimbTemplate, kActorLimbs> limbs{};
  // The zone enemy (ZND record and ZUD file) of a room enemy.
  std::int32_t zud_index = 0;
  TemplateEquipment equipment = TemplateEquipment::kNone;

  friend bool operator==(const CharacterTemplate&,
                         const CharacterTemplate&) = default;
};

// --- Records the rules write ------------------------------------------------

struct AuxiliaryIdentity {
  std::int32_t id = 0;
  std::uint16_t model_id = kAuxiliaryModelId;
  std::uint16_t kind = kAuxiliaryActorKind;

  friend bool operator==(const AuxiliaryIdentity&,
                         const AuxiliaryIdentity&) = default;
};

struct MovementGait {
  std::uint8_t movement_class = 0;
  std::uint8_t gait_class = 0;

  friend bool operator==(const MovementGait&, const MovementGait&) = default;
};

// The kind of a spawned actor and whether it spawns with its weapon drawn.
struct SpawnDisposition {
  std::uint16_t kind = 0;
  bool weapon_drawn = false;

  friend bool operator==(const SpawnDisposition&,
                         const SpawnDisposition&) = default;
};

// func_80076D50 (146C.c:6292): the battle actor and stats of an actor
// without a template.
struct ActorDefaults {
  std::int32_t id = 0;
  std::uint16_t model_id = 0;
  std::uint8_t blade_id = 0;
  std::uint8_t shield_id = 0;
  SpawnDisposition disposition;
  std::uint8_t interaction_flags = 0;
  MovementGait gait;

  friend bool operator==(const ActorDefaults&, const ActorDefaults&) = default;
};

// The default stats ActorDefaults implies (146C.c:6334-6363).
inline constexpr std::int16_t kDefaultVitals = 100;
inline constexpr std::uint8_t kDefaultWalkSpeed = 9;
inline constexpr std::uint8_t kDefaultRunSpeed = 18;
inline constexpr std::uint8_t kDefaultRecoveryDelay = 20;
inline constexpr std::array<std::uint8_t, 3> kDefaultWeaponRange{5, 4, 5};

// func_80076F24 (146C.c:6367): the battle actor fields a template sets.
struct TemplateIdentity {
  std::int32_t id = 0;
  std::uint16_t model_id = 0;
  std::uint16_t sub_type = 0;
  std::uint8_t blade_id = 0;
  std::uint8_t shield_id = 0;
  SpawnDisposition disposition;
  bool retain_after_defeat = false;
  std::uint8_t interaction_flags = 0;

  friend bool operator==(const TemplateIdentity&,
                         const TemplateIdentity&) = default;
};

struct LimbInitialStats {
  // Both the current and the maximum HP.
  std::uint16_t hp = 0;
  std::uint8_t agility_defense_bonus = 0;
  std::uint8_t anchor_bone = 0;
  std::uint8_t name_index = 0;
  std::uint8_t chain_evasion = 0;
  std::array<std::int16_t, kLimbTypes> types{};
  std::array<std::int16_t, kInitialLimbAffinities> affinities{};

  friend bool operator==(const LimbInitialStats&,
                         const LimbInitialStats&) = default;
};

struct LimbActionStats {
  std::uint16_t action_id = 0;
  // The low four bits of each are kept.
  std::uint8_t index = 0;
  std::uint8_t effect_model = 0;

  friend bool operator==(const LimbActionStats&,
                         const LimbActionStats&) = default;
};

// func_80076784 (146C.c:6128): the stats a template sets. The weapon,
// shield, accessory and armor come from the template's equipment parts.
struct InitialStats {
  std::uint8_t actor_id = 0;
  std::array<std::uint8_t, kActorNameBytes> name{};
  // Each is both the current and the maximum (or total) value.
  std::int16_t hp = 0;
  std::int16_t mp = 0;
  std::int16_t strength = 0;
  std::int16_t intelligence = 0;
  std::int16_t agility = 0;
  MovementGait gait;
  // Each is both the base and the current speed.
  std::uint8_t walk_speed = 0;
  std::uint8_t run_speed = 0;
  std::uint8_t enemy_class = 0;
  std::uint8_t reach = 0;
  std::uint8_t recovery_delay_bonus = 0;
  std::array<std::uint8_t, 2> mobility_limbs{};
  std::uint8_t default_target_limb = 0;
  std::array<LimbInitialStats, kActorLimbs> limbs{};
  std::array<std::array<LimbActionStats, kLimbActions>, kActorLimbs>
      limb_actions{};
  std::array<std::array<std::uint8_t, kDpSplashTargets>, kActorLimbs>
      dp_splash{};
  std::int32_t status_immunities = 0;
  std::int32_t statuses = 0;

  friend bool operator==(const InitialStats&, const InitialStats&) = default;
};

// The weapon's reach as built (Items_WeaponRange).
struct WeaponRange {
  std::uint8_t range = 0;
  std::uint8_t second_range = 0;
  std::uint8_t third_range = 0;
  std::uint8_t attack_shape_angle = 0;

  friend bool operator==(const WeaponRange&, const WeaponRange&) = default;
};

// The current attack reach: the weapon's ranges plus the actor's reach.
struct AttackReach {
  std::uint8_t range = 0;
  std::uint8_t range_y = 0;
  std::uint8_t range_z = 0;
  std::uint8_t shape_angle = 0;

  friend bool operator==(const AttackReach&, const AttackReach&) = default;
};
inline constexpr std::uint8_t kInitialAttackShape = 1;
inline constexpr std::uint8_t kReachMask = 0x1F;
inline constexpr std::uint8_t kAttackShapeAngleMask = 0x1F;

// The battle actor fields the rules read back.
struct SpawnedState {
  std::uint16_t kind = 0;
  std::int32_t weapon_drawn = 0;
  std::uint8_t walk_speed = 0;
  std::uint8_t run_speed = 0;

  friend bool operator==(const SpawnedState&, const SpawnedState&) = default;
};

// The eight settings bytes of a room spawn (Field_MpdEnemySpawn.settings):
// the AI group word and the word after it, little-endian.
struct SpawnSettings {
  std::uint8_t ai_group = 0;
  std::uint8_t group_reserved = 0;
  std::uint8_t ai_linked_group = 0;
  std::uint32_t linked_reserved = 0;
  std::int32_t extra = 0;

  friend bool operator==(const SpawnSettings&, const SpawnSettings&) = default;
};
inline constexpr std::size_t kSpawnSettingsBytes = 8;
[[nodiscard]] SpawnSettings DecodeSpawnSettings(
    std::span<const std::uint8_t, kSpawnSettingsBytes> bytes);

struct ItemDrop {
  std::uint16_t id = 0;
  std::uint8_t count = 0;

  friend bool operator==(const ItemDrop&, const ItemDrop&) = default;
};

// Field_MpdEnemySpawn: the room's spawn record of an enemy.
struct RoomSpawn {
  std::uint8_t index = 0;
  SpawnPlacement placement;
  std::array<std::uint8_t, kSpawnSettingsBytes> settings{};
  ItemDrop drop;
  ItemDrop second_drop;
  // The random drop: the item and its drop rate.
  ItemDrop random_drop;
  std::uint8_t facing = 0;
  std::uint8_t variant = 0;
  // reserved26: the combat pose of an actor without an AI script, also
  // kept as the actor's spawn byte.
  std::uint8_t combat_pose = 0;

  friend bool operator==(const RoomSpawn&, const RoomSpawn&) = default;
};

// The battle actor and stats fields a room spawn sets after the template.
struct SpawnDetails {
  std::uint8_t facing = 0;
  std::uint8_t spawn_byte = 0;
  std::uint8_t room_spawn_index = 0;
  ItemDrop drop;
  ItemDrop second_drop;
  ItemDrop random_drop;
  SpawnSettings settings;
  std::int32_t template_word = 0;

  friend bool operator==(const SpawnDetails&, const SpawnDetails&) = default;
};

// A room enemy's remembered state (Actors_RoomActorRecord).
struct RememberedSpawn {
  std::int16_t ai_flags = 0;
  std::int16_t current_hp = 0;
  std::int16_t max_hp = 0;
  std::int16_t current_mp = 0;
  std::int16_t max_mp = 0;
  std::int16_t strength = 0;
  std::int16_t intelligence = 0;
  std::int16_t agility = 0;
  std::array<std::int16_t, kActorLimbs> limb_hp{};
  std::int32_t statuses = 0;
  SpawnPlacement placement;

  friend bool operator==(const RememberedSpawn&,
                         const RememberedSpawn&) = default;
};

// --- ZUD files (Actors_ZudHeader) -------------------------------------------

struct ZudSection {
  std::int32_t offset = 0;
  std::int32_t size = 0;

  friend bool operator==(const ZudSection&, const ZudSection&) = default;
};

struct ZudLayout {
  ZudSection shape;
  ZudSection weapon;
  ZudSection shield;
  ZudSection common_animation;
  ZudSection battle_animation;

  friend bool operator==(const ZudLayout&, const ZudLayout&) = default;
};
inline constexpr std::size_t kZudHeaderBytes = 0x30;

// The section table of a ZUD file, or nothing when it is shorter than its
// header.
[[nodiscard]] std::optional<ZudLayout> ParseZudHeader(
    std::span<const std::byte> file);
// The section lies inside a file of `file_bytes` bytes.
[[nodiscard]] bool ZudSectionFits(ZudSection section, std::size_t file_bytes);

// The loaded ZUD file of the zone and its native address.
struct ZudBlock {
  Content::AssetAddress address = Content::AssetAddress::kNone;
  std::span<const std::byte> bytes;
};

enum class ZudReadState : std::uint8_t { kPending, kLoaded, kFailed };

// --- Ashley ------------------------------------------------------------------

struct AshleyLimbSnapshot {
  std::uint16_t hp = 0;
  std::uint16_t max_hp = 0;
  std::array<std::uint16_t, kLimbTypes> types{};
  std::array<std::uint16_t, kLimbAffinities> affinities{};

  friend bool operator==(const AshleyLimbSnapshot&,
                         const AshleyLimbSnapshot&) = default;
};

// State_AshleyStatsSnapshot: Ashley's state carried between rooms.
struct AshleySnapshot {
  bool pending = false;
  std::int16_t current_hp = 0;
  std::int16_t max_hp = 0;
  std::int16_t current_mp = 0;
  std::int16_t max_mp = 0;
  std::int16_t strength = 0;
  std::int16_t total_strength = 0;
  std::int16_t intelligence = 0;
  std::int16_t total_intelligence = 0;
  std::int16_t agility = 0;
  std::int16_t total_agility = 0;
  std::int16_t risk = 0;
  // Restored as the action recovery timer.
  std::uint16_t recovery_timer = 0;
  std::int32_t statuses = 0;
  std::array<std::uint8_t, kStatusTimers> status_timers{};
  std::array<AshleyLimbSnapshot, kActorLimbs> limbs{};
  std::int32_t weapon_drawn = 0;
  std::int32_t camera_pitch = 0;
  std::int32_t camera_distance = 0;

  friend bool operator==(const AshleySnapshot&,
                         const AshleySnapshot&) = default;
};

struct InventoryWeaponView {
  bool equipped = false;
  // The weapon's blade, one-based into the blades.
  std::uint8_t blade = 0;

  friend bool operator==(const InventoryWeaponView&,
                         const InventoryWeaponView&) = default;
};

struct InventoryModelView {
  bool equipped = false;
  std::uint8_t model = 0;
  std::uint16_t material = 0;

  friend bool operator==(const InventoryModelView&,
                         const InventoryModelView&) = default;
};

// The parts of Ashley's inventory her spawn reads.
struct AshleyInventory {
  std::array<InventoryWeaponView, kInventoryWeapons> weapons{};
  // `equipped` is unused for blades.
  std::array<InventoryModelView, kInventoryBlades> blades{};
  std::array<InventoryModelView, kInventoryShields> shields{};
  // The limb each armor piece is worn on (1-6), or kAccessoryLimb.
  std::array<std::uint8_t, kInventoryArmor> armor_limbs{};

  friend bool operator==(const AshleyInventory&,
                         const AshleyInventory&) = default;
};

// The equipment Ashley spawns with: the last equipped weapon and shield.
struct AshleyEquipment {
  std::optional<std::size_t> weapon;
  std::uint8_t blade_model = 0;
  std::uint16_t blade_material = 0;
  std::optional<std::size_t> shield;
  std::uint8_t shield_model = 0;
  std::uint16_t shield_material = 0;

  friend bool operator==(const AshleyEquipment&,
                         const AshleyEquipment&) = default;
};

// Ashley's spawn placement (18.c:373).
inline constexpr SpawnPlacement kAshleyPlacement{
    .tile_x = 4,
    .direction = 0,
    .tile_z = 2,
    .height = 128,
};
// The blade and spawn flags Ashley's template is applied with (18.c:410).
inline constexpr int kAshleyTemplateBlade = 0x10;
inline constexpr int kAshleyTemplateFlags = 0x80;

// The equipment models Refresh reads from an actor's stats.
struct EquippedModels {
  bool has_blade = false;
  std::uint8_t weapon_model = 0;
  std::uint8_t blade_material = 0;
  bool has_shield = false;
  std::uint8_t shield_model = 0;
  std::uint8_t shield_material = 0;

  friend bool operator==(const EquippedModels&,
                         const EquippedModels&) = default;
};

// --- Pure rules
// ----------------------------------------------------------------

// The kind of the actor in `slot` and whether it draws its weapon
// (146C.c:6305): Ashley, actor 1, then by the low two spawn flags.
[[nodiscard]] SpawnDisposition DispositionFor(std::size_t slot, int flags);
// The movement and gait class of an actor without a template (146C.c:6341).
[[nodiscard]] MovementGait DefaultMovementGait(int model_id);
// The weapon category of a blade model: the first category whose upper
// model id reaches it, or kUnarmedWeaponCategory without a blade
// (146C.c:6510).
[[nodiscard]] std::uint8_t EquippedWeaponCategory(
    int blade_model,
    std::span<const std::uint8_t, kWeaponCategoryBounds> bounds);
// A template stat, randomized by -10 to +10 when asked and clamped to
// 0-999. The original summed in a short; this sums in an int, so a stat
// near the short limit clamps to 999 instead of wrapping to 0.
[[nodiscard]] std::int16_t ClampInitialStat(int value);
// A template speed scaled to 90% (146C.c:6186).
[[nodiscard]] std::uint8_t ScaledSpeed(std::uint8_t speed);
// The statuses of an actor with these immunities: the sign bit when the
// immunity word is negative.
[[nodiscard]] std::int32_t InitialStatuses(std::int32_t immunities);
// The current attack reach of a weapon range plus the actor's reach.
[[nodiscard]] AttackReach AttackReachFor(WeaponRange range, std::uint8_t reach);
// The texture slot byte of a gimmick request: the actor id's second byte,
// rounded toward zero (146C.c:6100).
[[nodiscard]] std::uint8_t AuxiliaryTextureSlot(int actor_id);
// The placement a room enemy is loaded at: the remembered one when the room
// remembers it, else the spawn's with its height in 64-unit steps, as the
// original's 8-bit height field kept them. Only the low direction bit is
// kept.
[[nodiscard]] SpawnPlacement RoomEnemyPlacement(
    const RoomSpawn& spawn, const std::optional<RememberedSpawn>& remembered);
// The last equipped weapon and shield in Ashley's inventory; nothing when
// the weapon names blade 0 or one past the blades (kAshleyBlade).
[[nodiscard]] std::optional<AshleyEquipment> ChooseAshleyEquipment(
    const AshleyInventory& inventory);

// --- Host
// ------------------------------------------------------------------------

// The live actor table, asset queue, zone and Ashley state the spawn rules
// edit. Slots are Battle_actors indices; the rules check them first.
class ActorSpawnHost {
 public:
  ActorSpawnHost() = default;
  ActorSpawnHost(const ActorSpawnHost&) = delete;
  ActorSpawnHost& operator=(const ActorSpawnHost&) = delete;
  ActorSpawnHost(ActorSpawnHost&&) = delete;
  ActorSpawnHost& operator=(ActorSpawnHost&&) = delete;
  virtual ~ActorSpawnHost() = default;

  virtual void Refuse(SpawnRefusal refusal) = 0;
  // A diagnostic the original raised and ignored (vs_main_nop9).
  virtual void Diagnostic(int code) = 0;
  // vs_main_getRandSmoothed: 0 to range - 1.
  [[nodiscard]] virtual int RandomAveraged(int range) = 0;

  // --- The actor table and allocations
  [[nodiscard]] virtual SpawnedActor ActorAt(std::size_t slot) const = 0;
  // A zeroed allocation of `shape` with its stats, model and equipment
  // model links and allocation kind set; kNone when the heap refuses.
  [[nodiscard]] virtual SpawnedActor Allocate(ActorShape shape) = 0;
  virtual void Install(std::size_t slot, SpawnedActor actor) = 0;
  [[nodiscard]] virtual Content::AssetAddress ModelAddress(
      SpawnedActor actor) const = 0;
  [[nodiscard]] virtual Content::AssetAddress EquipmentAddress(
      SpawnedActor actor, EquipmentHand hand) const = 0;
  virtual void SetEquippedWeaponCategory(SpawnedActor actor,
                                         std::uint8_t category) = 0;
  // Sets the identity of a new gimmick actor, unlinked and not retained.
  virtual void WriteAuxiliaryIdentity(SpawnedActor actor,
                                      const AuxiliaryIdentity& identity) = 0;
  // Items_weaponCategoryUpperIds.
  [[nodiscard]] virtual std::array<std::uint8_t, kWeaponCategoryBounds>
  WeaponCategoryBounds() const = 0;

  // --- The installed actor in `slot`
  // Also clears the trigger tile, facing and command state, sets the
  // default stats and marks the actor as without a room spawn.
  virtual void WriteDefaults(std::size_t slot,
                             const ActorDefaults& defaults) = 0;
  // Also clears the command state and trigger tile.
  virtual void WriteTemplateIdentity(std::size_t slot,
                                     const TemplateIdentity& identity) = 0;
  // Also clears the risk, the hit-result and analyzed flags, the first
  // five status timers and the action recovery timer.
  virtual void WriteInitialStats(std::size_t slot,
                                 const InitialStats& stats) = 0;
  // Builds the weapon, shield, accessory and limb armor from the template's
  // parts and returns the weapon's range.
  [[nodiscard]] virtual WeaponRange BuildTemplateEquipment(
      std::size_t slot, TemplateEquipment equipment) = 0;
  // Also sets the current attack shape to kInitialAttackShape.
  virtual void WriteAttackReach(std::size_t slot, const AttackReach& reach) = 0;
  [[nodiscard]] virtual SpawnedState State(std::size_t slot) const = 0;
  // Also clears the killed and defeated marks.
  virtual void WriteSpawnDetails(std::size_t slot,
                                 const SpawnDetails& details) = 0;
  virtual void SetTemplateWord(std::size_t slot, std::int32_t word) = 0;
  virtual void RestoreRemembered(std::size_t slot,
                                 const RememberedSpawn& remembered) = 0;
  virtual void InflictStatuses(std::size_t slot, std::int32_t statuses) = 0;
  // Chains the actor right after Ashley's, or ends its chain.
  virtual void LinkAfterLeader(std::size_t slot) = 0;
  virtual void ClearNext(std::size_t slot) = 0;
  // The skeleton bone carrying `tag`, or a negative value.
  [[nodiscard]] virtual int FindBoneTag(std::size_t slot, int tag,
                                        int kind) const = 0;
  [[nodiscard]] virtual int BoneTag(std::size_t slot, int bone,
                                    int kind) const = 0;
  // The weapon and shield models in the actor's stats; nothing without
  // stats.
  [[nodiscard]] virtual std::optional<EquippedModels> Equipped(
      std::size_t slot) const = 0;

  // --- Animation, display, AI and combat
  virtual void DrawWeapon(std::size_t slot, int mode) = 0;
  virtual void SetDisplayFlags(std::size_t slot, std::uint32_t flags) = 0;
  virtual void PlayAnimation(std::size_t slot, int animation) = 0;
  virtual void SetWalkRunSpeeds(std::size_t slot, int walk, int run) = 0;
  virtual void SetCombatPose(std::size_t slot, int pose) = 0;
  // Battle_Ai_CreateActorAiState: 0 when the actor has no AI script.
  [[nodiscard]] virtual int CreateAiState(std::size_t slot, int flags) = 0;
  virtual void InitializePlaceholder(std::size_t slot, int actor_id) = 0;
  virtual void QueueBaseSequence(SpawnedActor actor, std::size_t slot) = 0;
  virtual void QueueWeaponSequence(SpawnedActor actor, std::size_t slot,
                                   std::uint8_t category, int flags) = 0;

  // --- The asset queue
  // The queue index of the request, or a negative value when it is full.
  [[nodiscard]] virtual int QueueAsset(
      const Content::ActorAssetRequest& request) = 0;
  // vs_battle_getEmptyObjectDataSlot() == 0: every request is done.
  [[nodiscard]] virtual bool AssetQueueIdle() const = 0;
  virtual void UpdateAssetQueue() = 0;
  // vs_main_updatePlayTimeAndLoading(0): the frame-paced loading pump.
  virtual void PumpLoading() = 0;
  virtual void ReleaseWeaponModel(int slot) = 0;

  // --- The zone
  [[nodiscard]] virtual bool UsesSecondaryBodyModel(
      std::uint8_t model_id) const = 0;
  // The zone's enemy records and ZUD files.
  [[nodiscard]] virtual int ZoneEnemyCount() const = 0;
  // The body model of zone enemy `zud`.
  [[nodiscard]] virtual std::uint8_t ZoneEnemyModel(int zud) const = 0;
  // The room's remembered state of the enemy from `spawn_index`.
  [[nodiscard]] virtual std::optional<RememberedSpawn> Remembered(
      std::uint8_t spawn_index) const = 0;
  // The index of the cached ZUD file, when one is cached.
  [[nodiscard]] virtual std::optional<int> CachedZud() const = 0;
  [[nodiscard]] virtual ZudBlock CachedZudBlock() const = 0;
  virtual void FreeZud() = 0;
  [[nodiscard]] virtual Content::DiscFileExtent ZudFile(int zud) const = 0;
  // A ZUD read is still pending (Field_zoneContext.zudDiscRequest).
  [[nodiscard]] virtual bool ZudReadPending() const = 0;
  // Allocates the ZUD file's block; false when the heap refuses.
  [[nodiscard]] virtual bool AllocateZud(Content::DiscFileExtent file) = 0;
  virtual void BeginZudRead(Content::DiscFileExtent file) = 0;
  [[nodiscard]] virtual ZudReadState PollZudRead() const = 0;
  // Releases the read; caches the block as `zud` when it loaded, frees it
  // otherwise.
  virtual void FinishZudRead(int zud, bool loaded) = 0;

  // --- Ashley
  [[nodiscard]] virtual AshleySnapshot ReadAshleySnapshot() const = 0;
  [[nodiscard]] virtual SpawnedActor Character() const = 0;
  virtual void SetCharacter(SpawnedActor actor) = 0;
  // The vitals, attributes, risk and action recovery timer.
  virtual void RestoreAshleyVitals(const AshleySnapshot& snapshot) = 0;
  virtual void InflictCharacterStatuses(std::int32_t statuses) = 0;
  // The status timers and the limbs' HP, types and affinities.
  virtual void RestoreAshleyConditions(const AshleySnapshot& snapshot) = 0;
  virtual void SetCharacterWeaponDrawn(std::int32_t weapon_drawn) = 0;
  virtual void SetHudBattleMode(int mode) = 0;
  virtual void ClearAshleySnapshotPending() = 0;
  virtual void RestoreCamera(std::int32_t pitch, std::int32_t distance) = 0;
  [[nodiscard]] virtual AshleyInventory ReadAshleyInventory() const = 0;
  [[nodiscard]] virtual CharacterTemplate AshleyTemplate() const = 0;
  // Each loads the character's equipment from the inventory entry, or
  // clears it without one.
  [[nodiscard]] virtual WeaponRange LoadAshleyWeapon(
      std::optional<std::size_t> weapon) = 0;
  virtual void LoadAshleyShield(std::optional<std::size_t> shield) = 0;
  virtual void LoadAshleyArmor(std::size_t limb,
                               std::optional<std::size_t> armor) = 0;
  virtual void LoadAshleyAccessory(std::optional<std::size_t> armor) = 0;
};

// --- Spawning
// -------------------------------------------------------------------

// func_800765B0 (146C.c:6064): a gimmick actor in `index`, or in the first
// free slot from kFirstAuxiliarySlot for kAnyAuxiliarySlot. kNone for an
// occupied slot or when no slot is free.
SpawnedActor CreateAuxiliaryActor(ActorSpawnHost& host, int index, int actor_id,
                                  SpawnPlacement placement, int material);
// func_80076784 (146C.c:6128).
void ApplyActorInitialStats(ActorSpawnHost& host, std::size_t slot,
                            const CharacterTemplate& character, bool randomize);
// func_80076D50 (146C.c:6292).
void InitializeActorDefaults(ActorSpawnHost& host, std::size_t slot,
                             int model_id, int blade_id, int shield_id,
                             int flags);
// func_80076F24 (146C.c:6367).
void InitializeActorFromTemplate(ActorSpawnHost& host, std::size_t slot,
                                 const CharacterTemplate& character,
                                 int blade_id, int shield_id, int flags,
                                 bool randomize);
// func_80077078 (146C.c:6423): the queue index of the request.
int QueueActorModelLoad(ActorSpawnHost& host, SpawnedActor actor, int index,
                        int model_id, SpawnPlacement placement, int flags);
// func_80077130 (146C.c:6455).
void QueueEquipmentModelLoad(ActorSpawnHost& host, SpawnedActor actor,
                             int actor_id, int model_id, EquipmentHand hand,
                             int material);
// func_80077240 (146C.c:6493): an equipped actor whose models are queued,
// not yet in its slot.
SpawnedActor AllocateActorWithEquipment(ActorSpawnHost& host, int actor_id,
                                        int model_id, int blade_model,
                                        int blade_material, int shield_model,
                                        int shield_material,
                                        SpawnPlacement placement, int flags);
// func_800773BC (146C.c:6536).
void RegisterLoadedActor(ActorSpawnHost& host, SpawnedActor actor,
                         std::size_t slot, int model_id, int blade_id,
                         int shield_id, int flags);
// func_800774FC (146C.c:6562).
SpawnedActor CreateActorWithEquipment(ActorSpawnHost& host, int actor_id,
                                      int model_id, int blade_model,
                                      int blade_material, int shield_model,
                                      int shield_material,
                                      SpawnPlacement placement, int flags);
// func_800775C0 (146C.c:6578).
SpawnedActor SpawnRoomEnemy(ActorSpawnHost& host, int actor_id,
                            const RoomSpawn& spawn,
                            const CharacterTemplate& character, int flags);
// func_8007C8F8 (146C.c:8926): the queue index plus one.
int QueueAshleyModelLoad(ActorSpawnHost& host, int model_id);
// func_8007C928 (146C.c:8936): the queue index plus one, or 0.
int QueueScriptedActorSpawn(ActorSpawnHost& host, std::uint32_t actor_id,
                            int model_id, SpawnPlacement placement);
// func_8007CA20 (146C.c:8962).
void LoadActorEquipmentModels(ActorSpawnHost& host, int actor_id,
                              int blade_model, int shield_model);
// func_8007CAA4 (146C.c:8971). An actor without a shield releases its
// weapon model slot a second time instead of its shield's, as the original
// did.
void RefreshActorEquipmentModels(ActorSpawnHost& host, int actor_id);
// func_800F9AD8 (INITBTL.PRG 18.c:310).
void RestoreAshleyRuntimeState(ActorSpawnHost& host);
// func_800F9CCC (INITBTL.PRG 18.c:361).
void SpawnAshleyWithEquipment(ActorSpawnHost& host);
}  // namespace VS::Actors
