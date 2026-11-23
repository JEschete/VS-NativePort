// Copied from the decomp by tools/port/copy_to_native.py; see each item's
// decomp line.
#include "Menu/Score/Encyclopedia.hpp"

#include <array>
#include <cstdint>

#include "Content/DiscLayout.hpp"

using VS::Content::disc::LocateAs;

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
constexpr PreviewZud kDefaultPreview = LocateAs<PreviewZud>("MAP/Z003U00.ZUD");

constexpr std::array<PreviewOverride, 78> kPreviewOverrides{{
    {4, LocateAs<PreviewZud>("MAP/Z009U16.ZUD")},
    {6, LocateAs<PreviewZud>("MAP/Z013U15.ZUD")},
    {11, LocateAs<PreviewZud>("MAP/Z009U03.ZUD")},
    {12, LocateAs<PreviewZud>("MAP/Z048U10.ZUD")},
    {14, LocateAs<PreviewZud>("MAP/Z009U01.ZUD")},
    {18, LocateAs<PreviewZud>("MAP/Z040U10.ZUD")},
    {20, LocateAs<PreviewZud>("MAP/Z048U02.ZUD")},
    {24, LocateAs<PreviewZud>("MAP/Z048U21.ZUD")},
    {26, LocateAs<PreviewZud>("MAP/Z048U06.ZUD")},
    {28, LocateAs<PreviewZud>("MAP/Z015U11.ZUD")},
    {31, LocateAs<PreviewZud>("MAP/Z013U00.ZUD")},
    {32, LocateAs<PreviewZud>("MAP/Z053U05.ZUD")},
    {34, LocateAs<PreviewZud>("MAP/Z050U05.ZUD")},
    {36, LocateAs<PreviewZud>("MAP/Z050U09.ZUD")},
    {38, LocateAs<PreviewZud>("MAP/Z051U03.ZUD")},
    {40, LocateAs<PreviewZud>("MAP/Z051U10.ZUD")},
    {43, LocateAs<PreviewZud>("MAP/Z014U01.ZUD")},
    {45, LocateAs<PreviewZud>("MAP/Z049U00.ZUD")},
    {47, LocateAs<PreviewZud>("MAP/Z024U02.ZUD")},
    {49, LocateAs<PreviewZud>("MAP/Z056U05.ZUD")},
    {50, LocateAs<PreviewZud>("MAP/Z048U16.ZUD")},
    {52, LocateAs<PreviewZud>("MAP/Z051U17.ZUD")},
    {54, LocateAs<PreviewZud>("MAP/Z053U11.ZUD")},
    {56, LocateAs<PreviewZud>("MAP/Z051U21.ZUD")},
    {58, LocateAs<PreviewZud>("MAP/Z055U06.ZUD")},
    {60, LocateAs<PreviewZud>("MAP/Z028U00.ZUD")},
    {62, LocateAs<PreviewZud>("MAP/Z028U01.ZUD")},
    {64, LocateAs<PreviewZud>("MAP/Z050U01.ZUD")},
    {67, LocateAs<PreviewZud>("MAP/Z051U22.ZUD")},
    {69, LocateAs<PreviewZud>("MAP/Z051U27.ZUD")},
    {70, LocateAs<PreviewZud>("MAP/Z051U23.ZUD")},
    {72, LocateAs<PreviewZud>("MAP/Z048U13.ZUD")},
    {74, LocateAs<PreviewZud>("MAP/Z040U07.ZUD")},
    {76, LocateAs<PreviewZud>("MAP/Z040U08.ZUD")},
    {79, LocateAs<PreviewZud>("MAP/Z049U02.ZUD")},
    {84, LocateAs<PreviewZud>("MAP/Z049U07.ZUD")},
    {86, LocateAs<PreviewZud>("MAP/Z056U14.ZUD")},
    {88, LocateAs<PreviewZud>("MAP/Z053U13.ZUD")},
    {91, LocateAs<PreviewZud>("MAP/Z024U00.ZUD")},
    {92, LocateAs<PreviewZud>("MAP/Z053U14.ZUD")},
    {95, LocateAs<PreviewZud>("MAP/Z022U01.ZUD")},
    {96, LocateAs<PreviewZud>("MAP/Z053U15.ZUD")},
    {99, LocateAs<PreviewZud>("MAP/Z024U06.ZUD")},
    {100, LocateAs<PreviewZud>("MAP/Z053U16.ZUD")},
    {103, LocateAs<PreviewZud>("MAP/Z022U02.ZUD")},
    {104, LocateAs<PreviewZud>("MAP/Z053U20.ZUD")},
    {107, LocateAs<PreviewZud>("MAP/Z025U00.ZUD")},
    {109, LocateAs<PreviewZud>("MAP/Z011U00.ZUD")},
    {111, LocateAs<PreviewZud>("MAP/Z049U01.ZUD")},
    {113, LocateAs<PreviewZud>("MAP/Z030U01.ZUD")},
    {115, LocateAs<PreviewZud>("MAP/Z012U00.ZUD")},
    {117, LocateAs<PreviewZud>("MAP/Z030U00.ZUD")},
    {119, LocateAs<PreviewZud>("MAP/Z012U01.ZUD")},
    {121, LocateAs<PreviewZud>("MAP/Z050U03.ZUD")},
    {123, LocateAs<PreviewZud>("MAP/Z053U01.ZUD")},
    {125, LocateAs<PreviewZud>("MAP/Z056U04.ZUD")},
    {127, LocateAs<PreviewZud>("MAP/Z016U00.ZUD")},
    {129, LocateAs<PreviewZud>("MAP/Z055U00.ZUD")},
    {131, LocateAs<PreviewZud>("MAP/Z054U00.ZUD")},
    {133, LocateAs<PreviewZud>("MAP/Z031U00.ZUD")},
    {135, LocateAs<PreviewZud>("MAP/Z056U01.ZUD")},
    {137, LocateAs<PreviewZud>("MAP/Z056U00.ZUD")},
    {139, LocateAs<PreviewZud>("MAP/Z048U00.ZUD")},
    {141, LocateAs<PreviewZud>("MAP/Z022U00.ZUD")},
    {143, LocateAs<PreviewZud>("MAP/Z041U00.ZUD")},
    {145, LocateAs<PreviewZud>("MAP/Z050U00.ZUD")},
    {149, LocateAs<PreviewZud>("MAP/Z055U05.ZUD")},
    {151, LocateAs<PreviewZud>("MAP/Z055U04.ZUD")},
    {153, LocateAs<PreviewZud>("MAP/Z017U00.ZUD")},
    {155, LocateAs<PreviewZud>("MAP/Z051U01.ZUD")},
    {157, LocateAs<PreviewZud>("MAP/Z024U05.ZUD")},
    {159, LocateAs<PreviewZud>("MAP/Z040U00.ZUD")},
    {161, LocateAs<PreviewZud>("MAP/Z053U02.ZUD")},
    {163, LocateAs<PreviewZud>("MAP/Z024U01.ZUD")},
    {165, LocateAs<PreviewZud>("MAP/Z056U02.ZUD")},
    {167, LocateAs<PreviewZud>("MAP/Z056U06.ZUD")},
    {252, LocateAs<PreviewZud>("MAP/Z040U02.ZUD")},
    {253, LocateAs<PreviewZud>("MAP/Z009U04.ZUD")},
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
