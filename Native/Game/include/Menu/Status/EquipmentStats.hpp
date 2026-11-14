#pragma once

#include <cstdint>

#include "Items/ItemRecords.hpp"
#include "Menu/Framework/EquipmentDetails.hpp"

namespace VS::Menu::Status {  // NOLINT(readability-identifier-naming)
struct EquipmentStatsEffects {
  int enabled_pages = 0;
  int dp_pp_bars = 0;
};

// decomp: MENU4.PRG _drawWeaponInfo (src/MENU/MENU4.PRG/120.c:26)
[[nodiscard]] EquipmentStatsEffects PopulateWeaponStats(
    Framework::EquipmentDetailsState& state, const Items::WeaponLoadout& weapon,
    bool equipped);

// decomp: MENU4.PRG _drawShieldInfo (src/MENU/MENU4.PRG/120.c:68)
[[nodiscard]] EquipmentStatsEffects PopulateShieldStats(
    Framework::EquipmentDetailsState& state, const Items::ShieldLoadout& shield,
    bool equipped);

// decomp: MENU4.PRG _drawArmorInfo (src/MENU/MENU4.PRG/120.c:104)
[[nodiscard]] EquipmentStatsEffects PopulateArmorStats(
    Framework::EquipmentDetailsState& state, const Items::ArmorLoadout& armor,
    bool equipped);

// decomp: MENU4.PRG _drawAccessoryInfo (src/MENU/MENU4.PRG/120.c:144)
[[nodiscard]] EquipmentStatsEffects PopulateAccessoryStats(
    Framework::EquipmentDetailsState& state,
    const Items::AccessorySpawnLoadout& accessory);

// decomp: MENU4.PRG _drawBladeInfo (src/MENU/MENU4.PRG/120.c:169)
[[nodiscard]] EquipmentStatsEffects PopulateBladeStats(
    Framework::EquipmentDetailsState& state, const Items::EquipmentPart& blade,
    std::uint16_t weapon_current_pp, std::uint16_t weapon_max_pp);

// decomp: MENU4.PRG _drawGripInfo (src/MENU/MENU4.PRG/120.c:191)
[[nodiscard]] EquipmentStatsEffects PopulateGripStats(
    Framework::EquipmentDetailsState& state, const Items::EquipmentPart& grip);

// decomp: MENU4.PRG _drawGemInfo (src/MENU/MENU4.PRG/120.c:209)
[[nodiscard]] EquipmentStatsEffects PopulateGemStats(
    Framework::EquipmentDetailsState& state, const Items::EquipmentPart& gem);
}  // namespace VS::Menu::Status
