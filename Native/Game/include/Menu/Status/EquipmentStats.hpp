#pragma once

#include <cstdint>

#include "Items/ItemRecords.hpp"
#include "Menu/Framework/EquipmentDetails.hpp"

namespace VS::Menu::Status {  // NOLINT(readability-identifier-naming)
struct EquipmentStatsEffects {
  int enabled_pages = 0;
  int dp_pp_bars = 0;
};

// decomp: MENU4.PRG drawWeaponInfo (src/MENU/MENU4.PRG/120.c:32; was
// _drawWeaponInfo)
[[nodiscard]] EquipmentStatsEffects PopulateWeaponStats(
    Framework::EquipmentDetailsState& state, const Items::WeaponLoadout& weapon,
    bool equipped);

// decomp: MENU4.PRG drawShieldInfo (src/MENU/MENU4.PRG/120.c:74; was
// _drawShieldInfo)
[[nodiscard]] EquipmentStatsEffects PopulateShieldStats(
    Framework::EquipmentDetailsState& state, const Items::ShieldLoadout& shield,
    bool equipped);

// decomp: MENU4.PRG drawArmorInfo (src/MENU/MENU4.PRG/120.c:110; was
// _drawArmorInfo)
[[nodiscard]] EquipmentStatsEffects PopulateArmorStats(
    Framework::EquipmentDetailsState& state, const Items::ArmorLoadout& armor,
    bool equipped);

// decomp: MENU4.PRG drawAccessoryInfo (src/MENU/MENU4.PRG/120.c:150; was
// _drawAccessoryInfo)
[[nodiscard]] EquipmentStatsEffects PopulateAccessoryStats(
    Framework::EquipmentDetailsState& state,
    const Items::AccessorySpawnLoadout& accessory);

// decomp: MENU4.PRG drawBladeInfo (src/MENU/MENU4.PRG/120.c:175; was
// _drawBladeInfo)
[[nodiscard]] EquipmentStatsEffects PopulateBladeStats(
    Framework::EquipmentDetailsState& state, const Items::EquipmentPart& blade,
    std::uint16_t weapon_current_pp, std::uint16_t weapon_max_pp);

// decomp: MENU4.PRG drawGripInfo (src/MENU/MENU4.PRG/120.c:197; was
// _drawGripInfo)
[[nodiscard]] EquipmentStatsEffects PopulateGripStats(
    Framework::EquipmentDetailsState& state, const Items::EquipmentPart& grip);

// decomp: MENU4.PRG drawGemInfo (src/MENU/MENU4.PRG/120.c:215; was
// _drawGemInfo)
[[nodiscard]] EquipmentStatsEffects PopulateGemStats(
    Framework::EquipmentDetailsState& state, const Items::EquipmentPart& gem);
}  // namespace VS::Menu::Status
