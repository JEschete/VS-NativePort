#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <string_view>

#include "Content/CatalogTable.hpp"

// MENU9's enemy preview table: the Gazette's preview code for a bestiary entry
// loads one archetype's ZUD. Each code is the archetype's legacy numeric alias
// in the bestiary's preview_archetype column; codes not listed load the
// default archetype, code 0's.
namespace VS::Content {

// decomp: zudLbas (src/MENU/MENU9.PRG/592C.c:28; was _zudLbas)
inline constexpr auto kEnemyPreviews = std::to_array<EnumValue>({
    {0, "archetype.map.z003u00"},   {4, "archetype.map.z009u16"},
    {6, "archetype.map.z013u15"},   {11, "archetype.map.z009u03"},
    {12, "archetype.map.z048u10"},  {14, "archetype.map.z009u01"},
    {18, "archetype.map.z040u10"},  {20, "archetype.map.z048u02"},
    {24, "archetype.map.z048u21"},  {26, "archetype.map.z048u06"},
    {28, "archetype.map.z015u11"},  {31, "archetype.map.z013u00"},
    {32, "archetype.map.z053u05"},  {34, "archetype.map.z050u05"},
    {36, "archetype.map.z050u09"},  {38, "archetype.map.z051u03"},
    {40, "archetype.map.z051u10"},  {43, "archetype.map.z014u01"},
    {45, "archetype.map.z049u00"},  {47, "archetype.map.z024u02"},
    {49, "archetype.map.z056u05"},  {50, "archetype.map.z048u16"},
    {52, "archetype.map.z051u17"},  {54, "archetype.map.z053u11"},
    {56, "archetype.map.z051u21"},  {58, "archetype.map.z055u06"},
    {60, "archetype.map.z028u00"},  {62, "archetype.map.z028u01"},
    {64, "archetype.map.z050u01"},  {67, "archetype.map.z051u22"},
    {69, "archetype.map.z051u27"},  {70, "archetype.map.z051u23"},
    {72, "archetype.map.z048u13"},  {74, "archetype.map.z040u07"},
    {76, "archetype.map.z040u08"},  {79, "archetype.map.z049u02"},
    {84, "archetype.map.z049u07"},  {86, "archetype.map.z056u14"},
    {88, "archetype.map.z053u13"},  {91, "archetype.map.z024u00"},
    {92, "archetype.map.z053u14"},  {95, "archetype.map.z022u01"},
    {96, "archetype.map.z053u15"},  {99, "archetype.map.z024u06"},
    {100, "archetype.map.z053u16"}, {103, "archetype.map.z022u02"},
    {104, "archetype.map.z053u20"}, {107, "archetype.map.z025u00"},
    {109, "archetype.map.z011u00"}, {111, "archetype.map.z049u01"},
    {113, "archetype.map.z030u01"}, {115, "archetype.map.z012u00"},
    {117, "archetype.map.z030u00"}, {119, "archetype.map.z012u01"},
    {121, "archetype.map.z050u03"}, {123, "archetype.map.z053u01"},
    {125, "archetype.map.z056u04"}, {127, "archetype.map.z016u00"},
    {129, "archetype.map.z055u00"}, {131, "archetype.map.z054u00"},
    {133, "archetype.map.z031u00"}, {135, "archetype.map.z056u01"},
    {137, "archetype.map.z056u00"}, {139, "archetype.map.z048u00"},
    {141, "archetype.map.z022u00"}, {143, "archetype.map.z041u00"},
    {145, "archetype.map.z050u00"}, {149, "archetype.map.z055u05"},
    {151, "archetype.map.z055u04"}, {153, "archetype.map.z017u00"},
    {155, "archetype.map.z051u01"}, {157, "archetype.map.z024u05"},
    {159, "archetype.map.z040u00"}, {161, "archetype.map.z053u02"},
    {163, "archetype.map.z024u01"}, {165, "archetype.map.z056u02"},
    {167, "archetype.map.z056u06"}, {252, "archetype.map.z040u02"},
    {253, "archetype.map.z009u04"},
});
inline constexpr std::int64_t kLastEnemyPreviewCode = 255;

// The archetype preview `code` loads.
[[nodiscard]] constexpr std::string_view EnemyPreviewArchetype(
    std::int64_t code) noexcept {
  const auto found = std::ranges::find(kEnemyPreviews, code, &EnumValue::code);
  return found == kEnemyPreviews.end() ? kEnemyPreviews.front().name
                                       : found->name;
}

// Whether `code` is the code an archetype is authored by.
[[nodiscard]] constexpr bool IsEnemyPreviewCode(std::int64_t code) noexcept {
  return std::ranges::find(kEnemyPreviews, code, &EnumValue::code) !=
         kEnemyPreviews.end();
}

}  // namespace VS::Content
