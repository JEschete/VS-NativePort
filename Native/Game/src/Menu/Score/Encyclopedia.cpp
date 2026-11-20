// Copied from the decomp by tools/port/copy_to_native.py; see each item's
// decomp line.
#include "Menu/Score/Encyclopedia.hpp"

#include <array>
#include <cstdint>

#include "Content/DiscLayout.h"

// decomp: _enemyNavigationState
// (_enemyNavigationState@src/MENU/MENU9.PRG/238.c)
std::int32_t VS::Menu::Score::enemyNavigationState;

// decomp: _paletteFlags (_paletteFlags@src/MENU/MENU9.PRG/592C.c)
std::uint8_t
    VS::Menu::Score::paletteFlags[VS::Menu::Score::kEnemyPreviewFiles] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
        1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1};

namespace {
struct PreviewZud {
  std::int32_t lba;
  std::int32_t size;
};

struct PreviewOverride {
  std::uint8_t enemy;
  PreviewZud zud;
};

// Enemies without a preview model of their own show Z003U00.
constexpr PreviewZud kDefaultPreview{VS_Z003U00_ZUD_LBA, VS_Z003U00_ZUD_SIZE};

constexpr std::array<PreviewOverride, 78> kPreviewOverrides{{
    {4, {VS_Z009U16_ZUD_LBA, VS_Z009U16_ZUD_SIZE}},
    {6, {VS_Z013U15_ZUD_LBA, VS_Z013U15_ZUD_SIZE}},
    {11, {VS_Z009U03_ZUD_LBA, VS_Z009U03_ZUD_SIZE}},
    {12, {VS_Z048U10_ZUD_LBA, VS_Z048U10_ZUD_SIZE}},
    {14, {VS_Z009U01_ZUD_LBA, VS_Z009U01_ZUD_SIZE}},
    {18, {VS_Z040U10_ZUD_LBA, VS_Z040U10_ZUD_SIZE}},
    {20, {VS_Z048U02_ZUD_LBA, VS_Z048U02_ZUD_SIZE}},
    {24, {VS_Z048U21_ZUD_LBA, VS_Z048U21_ZUD_SIZE}},
    {26, {VS_Z048U06_ZUD_LBA, VS_Z048U06_ZUD_SIZE}},
    {28, {VS_Z015U11_ZUD_LBA, VS_Z015U11_ZUD_SIZE}},
    {31, {VS_Z013U00_ZUD_LBA, VS_Z013U00_ZUD_SIZE}},
    {32, {VS_Z053U05_ZUD_LBA, VS_Z053U05_ZUD_SIZE}},
    {34, {VS_Z050U05_ZUD_LBA, VS_Z050U05_ZUD_SIZE}},
    {36, {VS_Z050U09_ZUD_LBA, VS_Z050U09_ZUD_SIZE}},
    {38, {VS_Z051U03_ZUD_LBA, VS_Z051U03_ZUD_SIZE}},
    {40, {VS_Z051U10_ZUD_LBA, VS_Z051U10_ZUD_SIZE}},
    {43, {VS_Z014U01_ZUD_LBA, VS_Z014U01_ZUD_SIZE}},
    {45, {VS_Z049U00_ZUD_LBA, VS_Z049U00_ZUD_SIZE}},
    {47, {VS_Z024U02_ZUD_LBA, VS_Z024U02_ZUD_SIZE}},
    {49, {VS_Z056U05_ZUD_LBA, VS_Z056U05_ZUD_SIZE}},
    {50, {VS_Z048U16_ZUD_LBA, VS_Z048U16_ZUD_SIZE}},
    {52, {VS_Z051U17_ZUD_LBA, VS_Z051U17_ZUD_SIZE}},
    {54, {VS_Z053U11_ZUD_LBA, VS_Z053U11_ZUD_SIZE}},
    {56, {VS_Z051U21_ZUD_LBA, VS_Z051U21_ZUD_SIZE}},
    {58, {VS_Z055U06_ZUD_LBA, VS_Z055U06_ZUD_SIZE}},
    {60, {VS_Z028U00_ZUD_LBA, VS_Z028U00_ZUD_SIZE}},
    {62, {VS_Z028U01_ZUD_LBA, VS_Z028U01_ZUD_SIZE}},
    {64, {VS_Z050U01_ZUD_LBA, VS_Z050U01_ZUD_SIZE}},
    {67, {VS_Z051U22_ZUD_LBA, VS_Z051U22_ZUD_SIZE}},
    {69, {VS_Z051U27_ZUD_LBA, VS_Z051U27_ZUD_SIZE}},
    {70, {VS_Z051U23_ZUD_LBA, VS_Z051U23_ZUD_SIZE}},
    {72, {VS_Z048U13_ZUD_LBA, VS_Z048U13_ZUD_SIZE}},
    {74, {VS_Z040U07_ZUD_LBA, VS_Z040U07_ZUD_SIZE}},
    {76, {VS_Z040U08_ZUD_LBA, VS_Z040U08_ZUD_SIZE}},
    {79, {VS_Z049U02_ZUD_LBA, VS_Z049U02_ZUD_SIZE}},
    {84, {VS_Z049U07_ZUD_LBA, VS_Z049U07_ZUD_SIZE}},
    {86, {VS_Z056U14_ZUD_LBA, VS_Z056U14_ZUD_SIZE}},
    {88, {VS_Z053U13_ZUD_LBA, VS_Z053U13_ZUD_SIZE}},
    {91, {VS_Z024U00_ZUD_LBA, VS_Z024U00_ZUD_SIZE}},
    {92, {VS_Z053U14_ZUD_LBA, VS_Z053U14_ZUD_SIZE}},
    {95, {VS_Z022U01_ZUD_LBA, VS_Z022U01_ZUD_SIZE}},
    {96, {VS_Z053U15_ZUD_LBA, VS_Z053U15_ZUD_SIZE}},
    {99, {VS_Z024U06_ZUD_LBA, VS_Z024U06_ZUD_SIZE}},
    {100, {VS_Z053U16_ZUD_LBA, VS_Z053U16_ZUD_SIZE}},
    {103, {VS_Z022U02_ZUD_LBA, VS_Z022U02_ZUD_SIZE}},
    {104, {VS_Z053U20_ZUD_LBA, VS_Z053U20_ZUD_SIZE}},
    {107, {VS_Z025U00_ZUD_LBA, VS_Z025U00_ZUD_SIZE}},
    {109, {VS_Z011U00_ZUD_LBA, VS_Z011U00_ZUD_SIZE}},
    {111, {VS_Z049U01_ZUD_LBA, VS_Z049U01_ZUD_SIZE}},
    {113, {VS_Z030U01_ZUD_LBA, VS_Z030U01_ZUD_SIZE}},
    {115, {VS_Z012U00_ZUD_LBA, VS_Z012U00_ZUD_SIZE}},
    {117, {VS_Z030U00_ZUD_LBA, VS_Z030U00_ZUD_SIZE}},
    {119, {VS_Z012U01_ZUD_LBA, VS_Z012U01_ZUD_SIZE}},
    {121, {VS_Z050U03_ZUD_LBA, VS_Z050U03_ZUD_SIZE}},
    {123, {VS_Z053U01_ZUD_LBA, VS_Z053U01_ZUD_SIZE}},
    {125, {VS_Z056U04_ZUD_LBA, VS_Z056U04_ZUD_SIZE}},
    {127, {VS_Z016U00_ZUD_LBA, VS_Z016U00_ZUD_SIZE}},
    {129, {VS_Z055U00_ZUD_LBA, VS_Z055U00_ZUD_SIZE}},
    {131, {VS_Z054U00_ZUD_LBA, VS_Z054U00_ZUD_SIZE}},
    {133, {VS_Z031U00_ZUD_LBA, VS_Z031U00_ZUD_SIZE}},
    {135, {VS_Z056U01_ZUD_LBA, VS_Z056U01_ZUD_SIZE}},
    {137, {VS_Z056U00_ZUD_LBA, VS_Z056U00_ZUD_SIZE}},
    {139, {VS_Z048U00_ZUD_LBA, VS_Z048U00_ZUD_SIZE}},
    {141, {VS_Z022U00_ZUD_LBA, VS_Z022U00_ZUD_SIZE}},
    {143, {VS_Z041U00_ZUD_LBA, VS_Z041U00_ZUD_SIZE}},
    {145, {VS_Z050U00_ZUD_LBA, VS_Z050U00_ZUD_SIZE}},
    {149, {VS_Z055U05_ZUD_LBA, VS_Z055U05_ZUD_SIZE}},
    {151, {VS_Z055U04_ZUD_LBA, VS_Z055U04_ZUD_SIZE}},
    {153, {VS_Z017U00_ZUD_LBA, VS_Z017U00_ZUD_SIZE}},
    {155, {VS_Z051U01_ZUD_LBA, VS_Z051U01_ZUD_SIZE}},
    {157, {VS_Z024U05_ZUD_LBA, VS_Z024U05_ZUD_SIZE}},
    {159, {VS_Z040U00_ZUD_LBA, VS_Z040U00_ZUD_SIZE}},
    {161, {VS_Z053U02_ZUD_LBA, VS_Z053U02_ZUD_SIZE}},
    {163, {VS_Z024U01_ZUD_LBA, VS_Z024U01_ZUD_SIZE}},
    {165, {VS_Z056U02_ZUD_LBA, VS_Z056U02_ZUD_SIZE}},
    {167, {VS_Z056U06_ZUD_LBA, VS_Z056U06_ZUD_SIZE}},
    {252, {VS_Z040U02_ZUD_LBA, VS_Z040U02_ZUD_SIZE}},
    {253, {VS_Z009U04_ZUD_LBA, VS_Z009U04_ZUD_SIZE}},
}};

using VS::Menu::Score::kEnemyPreviewFiles;
using PreviewColumn = std::array<std::int32_t, kEnemyPreviewFiles>;

// The original keeps one column per field (592C.c _zudLbas, _zudSizes).
constexpr PreviewColumn PreviewTableColumn(
    std::int32_t PreviewZud::* field) noexcept {
  PreviewColumn column{};
  column.fill(kDefaultPreview.*field);
  for (const PreviewOverride& entry : kPreviewOverrides) {
    column.at(entry.enemy) = entry.zud.*field;
  }
  return column;
}
}  // namespace

// decomp: _zudLbas (_zudLbas@src/MENU/MENU9.PRG/592C.c)
constinit PreviewColumn VS::Menu::Score::zudLbas =
    PreviewTableColumn(&PreviewZud::lba);

// decomp: _zudSizes (_zudSizes@src/MENU/MENU9.PRG/592C.c)
constinit PreviewColumn VS::Menu::Score::zudSizes =
    PreviewTableColumn(&PreviewZud::size);
