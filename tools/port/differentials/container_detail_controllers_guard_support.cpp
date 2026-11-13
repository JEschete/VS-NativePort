#include <cstdlib>
#include <expected>
#include <string>
#include <string_view>
#include <vector>

#include "Actors/TextureAnimationLive.hpp"
#include "Audio/SfxLive.hpp"
#include "Content/DataStore.hpp"
#include "Content/ModuleData.h"
#include "Platform/CrashHandler.hpp"

extern "C" {
#include "Battle/TargetingState.h"
#include "Menu/TextBox.h"
#include "Render/Text/CompactFont.h"
#include "Render/Text/VariableFont.h"

Menu_StringContext Battle_stringContext{};
Menu_TextBoxState Menu_textBoxes[8]{};
u_long *Menu_activeTextBoxOrderingTable{};
int State_tickSpeed{};
int Render_kStatGaugeColors[4]{};
u_short Render_kGlyphPixelExpansion[16]{};
// Exact font bindings from production Boot/ModuleData.cpp G00149-G00152.
// The probe never resolves function addresses through this font-only subset.
const Content_ModuleDataEntry Content_moduleData[]{
    {"BATTLE/BATTLE.PRG", 0x82F08, sizeof Render_compactGlyphLeftTrim,
     Render_compactGlyphLeftTrim, sizeof Render_compactGlyphLeftTrim},
    {"BATTLE/BATTLE.PRG", 0x82FB4, sizeof Render_compactGlyphWidths,
     Render_compactGlyphWidths, sizeof Render_compactGlyphWidths},
    {"BATTLE/BATTLE.PRG", 0x82FD4, sizeof Render_compactGlyphIndices,
     Render_compactGlyphIndices, sizeof Render_compactGlyphIndices},
    {"BATTLE/BATTLE.PRG", 0x83010, sizeof Render_variableFontGlyphWidths,
     Render_variableFontGlyphWidths, sizeof Render_variableFontGlyphWidths}};
const unsigned Content_moduleDataCount = 4;
const Content_ModuleSymbol Content_moduleSymbols[1]{};
const unsigned Content_moduleSymbolCount = 0;
const Content_ModulePointerTable Content_modulePointers[1]{};
const unsigned Content_modulePointerCount = 0;
}

// These unrelated disc/audio/actor crash-report endpoints exist only to close
// the production objects' unused entry points. Reaching one fails the process.
namespace VS::Content {
const DataStore *DataStore::Get() { std::abort(); }
const DataStore::Entry *DataStore::Find(std::string_view) const {
  std::abort();
}
std::expected<std::vector<u8>, std::string>
DataStore::ReadAll(const Entry &) const {
  std::abort();
}
} // namespace VS::Content
namespace VS::Audio {
void LivePlaySfxCentered(int, int) { std::abort(); }
} // namespace VS::Audio
namespace VS::Actors {
int SetLiveActorTextureAnimation(int, int, int, int) { std::abort(); }
} // namespace VS::Actors
namespace VS::Platform {
void LogAddress(const void *) { std::abort(); }
} // namespace VS::Platform
