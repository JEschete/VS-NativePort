// Actual HudRenderingLive, UiPacketsLive, SpritesLive, UiPackets, Sprites and
// ScalarMath execute here. The GPU handle sink and four unrelated gameplay
// services below are explicitly bounded fixtures; central tests use the real
// gameplay services and module registrations.
#include <array>
#include <csignal>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string_view>

#include "Actors/ActorTableLive.hpp"
#include "Battle/AbilitiesLive.hpp"
#include "Battle/HudRenderingLive.hpp"
#include "Battle/HudRenderingLiveFixture.hpp"
#include "Battle/HudShortcutLiveFixture.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
extern "C" {
#include "Core/Diagnostics.h"

int Battle_hudSlideOffset{};
int Battle_hudGaugePositions[4]{}, Battle_hudGaugeLabelPositions[4]{};
char Battle_hudGaugeLabelWidths[4]{}, Battle_hudGaugeLabelTexU[20]{};
u_short Battle_statusIconTexOffsets[32]{};
int Render_kStatGaugeColors[4]{};
int Render_frameBuf{};
void* Render_legacyOrderingTables[2]{};
void* Render_legacyPrimitiveBuffers[2]{};
void* Render_legacyCurrentOrderingTable{};
Battle_Actor* Battle_characterState{};
Actors_ActorRecord* Actors_actorRecords[17]{};
u_char Battle_spellClassCounts[4]{};
u_char* Battle_spellClassSpellIds[4]{};
char Battle_chainAbilityActionIds[16]{}, Battle_defenseAbilityActionIds[16]{};
char Menu_Magic_warlockSpellIds[20]{}, Menu_Magic_shamanSpellIds[8]{},
    Menu_Magic_sorcererSpellIds[20]{}, Menu_Magic_enchanterSpellIds[8]{};
Battle_ActionDefinition Battle_actionDefinitions[256]{};
}

namespace {
namespace b = VS::Battle;
namespace f = hud_rendering_fixture;
unsigned diagnostics = 0;
std::array<const void*, 128> gpu_handles{};
std::size_t next_gpu_handle = 1;
std::uint32_t restriction_flags = 0;
int spell_result = 0;
int ability_result = 0;
int art_result = 0;
Battle_Actor* replacement_character = nullptr;

[[noreturn]] void Unexpected(const char* operation) {
  Core_Errorf("hud.guard", "Unexpected out-of-scope service: %s", operation);
  std::abort();
}
}  // namespace

extern "C" {
void Core_Errorf(const char* channel, const char* format, ...) {
  ++diagnostics;
  std::fprintf(stderr, "%s: ", channel);
  std::va_list arguments;
  va_start(arguments, format);
  std::vfprintf(stderr, format, arguments);
  va_end(arguments);
  std::fputc('\n', stderr);
  std::fflush(stderr);
}

u_long LegacyGpu_Handle(const void* pointer) {
  for (std::size_t index = 1; index < next_gpu_handle; ++index) {
    if (gpu_handles.at(index) == pointer) return static_cast<u_long>(index);
  }
  if (next_gpu_handle == gpu_handles.size()) Unexpected("GPU handle capacity");
  gpu_handles.at(next_gpu_handle) = pointer;
  return static_cast<u_long>(next_gpu_handle++);
}
void* LegacyGpu_Pointer(u_long handle) {
  const auto index = static_cast<std::size_t>(handle & 0xFFFFFFU);
  return index < next_gpu_handle ? const_cast<void*>(gpu_handles.at(index))
                                 : nullptr;
}
void Core_Gte_SetRotMatrix(const MATRIX*) { Unexpected("view rotation"); }
void Core_Gte_SetTransMatrix(const MATRIX*) { Unexpected("view translation"); }
void DrawPrim(void*) { Unexpected("immediate GPU draw"); }
int LoadImage(RECT*, u_long*) { Unexpected("VRAM upload"); }
int StoreImage(RECT*, u_long*) { Unexpected("VRAM read"); }
}

namespace VS::Actors {
Actors_ActorRecord* LiveActorRecord(int actor, const char*) {
  if (actor != 0) Unexpected("nonzero actor-record fixture query");
  return Actors_actorRecords[0];
}
}  // namespace VS::Actors

namespace VS::Menu::Framework {
int LiveActionRestrictionFlags(int mode) {
  if (mode != 0) Unexpected("nonzero restriction mode");
  if (replacement_character != nullptr) {
    Battle_characterState = replacement_character;
  }
  return static_cast<int>(restriction_flags);
}
}  // namespace VS::Menu::Framework

namespace VS::Battle {
int LiveHasUnlockedSpellClass(int spell_class) {
  if (spell_class != 1) Unexpected("wrong shortcut spell class");
  return spell_result;
}
int LiveHasUnlockedBattleAbility(int defense) {
  if (defense != 0 && defense != 1) Unexpected("wrong battle-ability selector");
  return ability_result;
}
int LiveHasUnlockedWeaponBreakArt() { return art_result; }
}  // namespace VS::Battle

#ifndef VS_HUD_RENDER_STORAGE_ONLY
namespace {
bool GaugeProbe(std::string_view selected) {
  f::LiveFrame frame;
  if (selected == "gauge-index-low") b::DrawLiveHudStatGauge(-1, 0, 0);
  if (selected == "gauge-index-high") b::DrawLiveHudStatGauge(4, 0, 0);
  if (selected == "gauge-index-no-byte-alias") {
    b::DrawLiveHudStatGauge(256, 0, 0);
  }
  if (selected == "gauge-missing-target") {
    frame.Disconnect();
    b::DrawLiveHudStatGauge(0, 1, 100);
  }
  if (selected == "gauge-missing-number-layer") {
    Render_frameScratch.orderingTable = frame.ordering.data() + 3;
    b::DrawLiveHudStatGauge(0, 1, 100);
  }
  if (selected == "gauge-full-buffer") {
    frame.FillBuffer();
    b::DrawLiveHudStatGauge(2, 1, 100);
  }
  if (selected == "gauge-nonterminating-number") {
    b::DrawLiveHudStatGauge(2, -1, 100);
  }
  if (selected == "gauge-zero-unused-target") {
    frame.Disconnect();
    b::DrawLiveHudStatGauge(0, -1, 0);
    return frame.UsedWords() == 0;
  }
  if (selected == "gauge-production-packets") {
    b::DrawLiveHudStatGauge(0, 7, 10);
    return frame.UsedWords() == 44 && frame.primitives[42] == 0x37F600E1;
  }
  return false;
}

bool IconProbe(std::string_view selected) {
  f::LiveFrame frame;
  if (selected == "icon-index-low") b::DrawLiveSlidingStatusIcon(-1, 0, 0);
  if (selected == "icon-index-high") b::DrawLiveSlidingStatusIcon(32, 0, 0);
  if (selected == "icon-missing-target") {
    frame.Disconnect();
    b::DrawLiveSlidingStatusIcon(0, 0, 0);
  }
  if (selected == "icon-second-packet-capacity") {
    Render_frameScratch.primitiveCursor =
        frame.primitives.data() + frame.primitives.size() - 6;
    b::DrawLiveSlidingStatusIcon(16, 0, 0);
  }
  if (selected == "icon-unused-target") {
    frame.Disconnect();
    b::DrawLiveSlidingStatusIcon(255, 0, 0);
    Battle_hudSlideOffset = 1;
    b::DrawLiveSlidingStatusIcon(-1, 0, 0);
    return frame.UsedWords() == 0;
  }
  if (selected == "icon-production-packets") {
    b::DrawLiveSlidingStatusIcon(16, -1, 0);
    return frame.UsedWords() == 12 && frame.primitives[10] == 0x37FF3068;
  }
  if (selected == "marker-missing-target") {
    frame.Disconnect();
    b::DrawLiveShortcutButtonMarker(0, 0x180);
  }
  if (selected == "marker-full-buffer") {
    frame.FillBuffer();
    b::DrawLiveShortcutButtonMarker(0, 0x180);
  }
  if (selected == "marker-production-packet") {
    b::DrawLiveShortcutButtonMarker(-1, 0x140);
    return frame.UsedWords() == 6 && frame.primitives[2] == 0x66404040 &&
           frame.primitives[3] == 0xFFFFFFFF;
  }
  return false;
}

bool ShortcutProbe(std::string_view selected) {
  f::LiveShortcut state;
  if (selected == "shortcut-missing-actor") Battle_characterState = nullptr;
  if (selected == "shortcut-missing-stats") state.actor.stats = nullptr;
  if (selected == "shortcut-missing-model") state.actor.model = nullptr;
  if (selected == "shortcut-missing-record") Actors_actorRecords[0] = nullptr;
  if (selected.starts_with("shortcut-missing-")) {
    static_cast<void>(b::LiveShortcutIconBrightness(4));
  }
  if (selected == "shortcut-used-null-spell-row") {
    Battle_spellClassCounts[1] = 1;
    static_cast<void>(b::LiveShortcutIconBrightness(0));
  }
  if (selected == "shortcut-used-overrun-spell-row") {
    auto* last = std::end(Menu_Magic_warlockSpellIds) - 1;
    *last = 100;
    f::LiveShortcut::SpellClass(1, last, 2);
    static_cast<void>(b::LiveShortcutIconBrightness(0));
  }
  if (selected == "shortcut-invalid-break-art-category") {
    state.actor.equippedWeaponCategory = 19;
    static_cast<void>(b::LiveShortcutIconBrightness(7));
  }
  if (selected == "shortcut-unused-null-spell-row") {
    return b::LiveShortcutIconBrightness(0) == 0;
  }
  if (selected == "shortcut-unused-bad-spell-row") {
    Battle_spellClassCounts[0] = 255;
    return b::LiveShortcutIconBrightness(-1) == 0x180 &&
           b::LiveShortcutIconBrightness(4) == 0x180;
  }
  if (selected == "shortcut-lazy-last-spell-alias") {
    auto* last = std::end(Menu_Magic_warlockSpellIds) - 1;
    *last = static_cast<char>(255);
    f::LiveShortcut::SpellClass(1, last, 2);
    Battle_actionDefinitions[255].unlocked = 1;
    spell_result = 1;
    return b::LiveShortcutIconBrightness(0) == 0x180;
  }
  if (selected == "shortcut-refreshed-break-art-owner") {
    Battle_Actor replacement = state.actor;
    state.actor.equippedWeaponCategory = 255;
    replacement.equippedWeaponCategory = 1;
    replacement_character = &replacement;
    Battle_actionDefinitions[184].unlocked = 1;
    art_result = 1;
    return b::LiveShortcutIconBrightness(7) == 0x180 &&
           Battle_characterState == &replacement;
  }
  return false;
}
}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) return 64;
  std::signal(SIGABRT, [](int) { std::_Exit(85); });
  const std::string_view selected(argv[1]);
  const bool passed =
      selected.starts_with("gauge-")
          ? GaugeProbe(selected)
          : (selected.starts_with("shortcut-") ? ShortcutProbe(selected)
                                               : IconProbe(selected));
  std::printf("%s diagnostics=%u\n", passed ? "PASS" : "FAIL", diagnostics);
  return passed && diagnostics == 0 ? 0 : 1;
}
#endif
