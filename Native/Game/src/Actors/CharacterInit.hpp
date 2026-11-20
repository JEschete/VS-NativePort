#pragma once
// A character's initial record: the zone file's enemy templates and Ashley's
// starting loadout. The spawner decodes it (DecodeCharacterTemplate in
// ActorSpawningLive.cpp). Plain `char` fields of the original are unsigned
// bytes, as its compiler read them.

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "Items/ItemRecords.hpp"

namespace VS::Actors {
namespace layout {
inline constexpr std::size_t kLimbTypes = 4;
inline constexpr std::size_t kLimbAffinities = 8;
inline constexpr std::size_t kLimbActionSlots = 4;
inline constexpr std::size_t kLimbDpSplashTargets = 6;
inline constexpr std::size_t kLimbReservedBytes = 2;
inline constexpr std::size_t kCharacterNameBytes = 24;
inline constexpr std::size_t kCharacterLimbs = 6;
}  // namespace layout

// decomp: Actions (src/BATTLE/BATTLE.PRG/charInit.h:6-10)
struct LimbActionInit {
  std::uint16_t action_id = 0;
  std::uint8_t effect_model = 0;  // upstream unk2
  std::uint8_t index = 0;
};

// decomp: LimbInit (src/BATTLE/BATTLE.PRG/charInit.h:12-22)
struct LimbInit {
  std::uint16_t hp = 0;
  std::uint8_t agility_defense_bonus = 0;
  std::uint8_t chain_evasion = 0;
  std::array<std::int8_t, layout::kLimbTypes> types{};
  std::array<std::int8_t, layout::kLimbAffinities> affinities{};
  std::array<LimbActionInit, layout::kLimbActionSlots> actions{};
  VS::Items::ArmorInit armor{};
  std::array<std::uint8_t, layout::kLimbDpSplashTargets> dp_splash{};
  std::array<std::uint8_t, layout::kLimbReservedBytes> reserved_5a{};
};

// decomp: vs_CharInitData (src/BATTLE/BATTLE.PRG/charInit.h:24-51)
struct CharacterInit {
  std::uint16_t sub_type = 0;
  // The body model id (upstream unk2).
  std::uint8_t model_id = 0;
  std::uint8_t reach : 5 = 0;
  std::uint8_t enemy_class : 3 = 0;  // upstream unk3_5
  std::array<char, layout::kCharacterNameBytes> name{};
  std::int16_t hp = 0;
  std::int16_t mp = 0;
  std::uint8_t strength = 0;
  std::uint8_t intelligence = 0;
  std::uint8_t agility = 0;
  std::uint8_t recovery_delay_bonus = 0;  // upstream unk23
  std::uint8_t material = 0;              // upstream unk24
  std::uint8_t walk_speed = 0;
  std::uint8_t gait_class = 0;  // upstream unk26
  std::uint8_t run_speed = 0;
  std::int32_t status_immunities = 0;  // upstream unk28
  std::int32_t ai_settings = 0;        // upstream unk2C
  // The two limbs whose loss slows the character (upstream unk30, unk31).
  std::array<std::uint8_t, 2> mobility_limbs{};
  std::uint8_t default_target_limb = 0;  // upstream unk32
  std::uint8_t reserved_33 = 0;
  VS::Items::WeaponSpawnLoadout weapon{};
  VS::Items::ShieldSpawnLoadout shield{};
  VS::Items::AccessoryInit accessory{};
  std::array<LimbInit, layout::kCharacterLimbs> limbs{};
  std::int32_t zud_index = 0;  // upstream mpdIdentifer
};

namespace layout {
inline constexpr std::size_t kLimbActionInitBytes = 0x4;
static_assert(sizeof(LimbActionInit) == kLimbActionInitBytes);
inline constexpr std::size_t kLimbInitBytes = 0x5C;
static_assert(sizeof(LimbInit) == kLimbInitBytes);
inline constexpr std::size_t kLimbArmorOffset = 0x20;
static_assert(offsetof(LimbInit, armor) == kLimbArmorOffset);
inline constexpr std::size_t kLimbDpSplashOffset = 0x54;
static_assert(offsetof(LimbInit, dp_splash) == kLimbDpSplashOffset);
inline constexpr std::size_t kCharacterInitBytes = 0x464;
static_assert(sizeof(CharacterInit) == kCharacterInitBytes);
inline constexpr std::size_t kCharacterNameOffset = 0x04;
static_assert(offsetof(CharacterInit, name) == kCharacterNameOffset);
inline constexpr std::size_t kCharacterHpOffset = 0x1C;
static_assert(offsetof(CharacterInit, hp) == kCharacterHpOffset);
inline constexpr std::size_t kImmunitiesAt = 0x28;
static_assert(offsetof(CharacterInit, status_immunities) == kImmunitiesAt);
inline constexpr std::size_t kCharacterWeaponOffset = 0x34;
static_assert(offsetof(CharacterInit, weapon) == kCharacterWeaponOffset);
static_assert(std::is_trivially_copyable_v<CharacterInit>);
}  // namespace layout
}  // namespace VS::Actors
