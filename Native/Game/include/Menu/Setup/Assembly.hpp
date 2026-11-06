#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstdint>

#include "Items/ItemRecords.hpp"
#include "Menu/Framework/Framework.hpp"
#include "Menu/Setup/SetupScreen.hpp"

// MENUC.PRG's weapon assembly and gem attachment (168.c:56-2374): the part
// selection history, assembling and taking apart weapons in Ashley's
// inventory, the blade, grip and gem lists and the attachment screens.
namespace VS::Menu::Setup {
// _isValidGrip: per grip category, the blade categories it holds.
inline constexpr std::array<std::uint16_t, 5> kGripBladeCategories{
    0, 0xE, 0xF0, 0x100, 0x200,
};
// _assembleMenu's cursor positions: the weapon row, the blade, the grip and
// the gem rows.
inline constexpr std::array<std::uint32_t, 7> kAssemblyCursorPositions{
    Framework::PackXy(141, 10),  Framework::PackXy(148, 26),
    Framework::PackXy(148, 42),  Framework::PackXy(155, 58),
    Framework::PackXy(155, 74),  Framework::PackXy(155, 90),
    Framework::PackXy(155, 106),
};

// The gems as the packed int the original passes (byte i is gem i).
[[nodiscard]] std::int32_t PackGems(const GemIds& gems);

void RecordSlotSelection(Context& context, int index);
int PopSlotSelection(Context& context, int index);
void DisassembleWeapon(Context& context, int weapon_index);
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void AssembleWeapon(Context& context, int blade_index, int grip_index,
                    const GemIds& gems);
void DetachShieldGems(Context& context, int shield_index);
// A missing part fits anything.
[[nodiscard]] bool IsGripCompatible(Context& context,
                                    const Items::InventoryBlade* blade,
                                    const Items::InventoryGrip* grip);
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void SetAssemblyStats(Context& context, int blade, int grip);
void SetAssemblyHeader(Context& context, int blade_grip);
int SelectAssemblyBlade(Context& context, int params);
int SelectAssemblyGrip(Context& context, int params);
int SelectGem(Context& context, int params);
int CancelAssembly(Context& context);
int UpdateAssembly(Context& context, bool initialize);
void CreateEmptyGemSlotRow(Context& context, int row);
int RestoreGemsAndLeaveSelection(Context& context, const GemIds& gems,
                                 bool is_shield);
int UpdateGemAttachment(Context& context, int category);
int UpdateGemCategoryMenu(Context& context, bool initialize);
}  // namespace VS::Menu::Setup
