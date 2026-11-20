#pragma once
// Hand-maintained (tools/port/owned.csv).

#include "Battle/Ai/ActorSettings.hpp"

namespace VS::Battle::Ai {
struct SavedSettingsPair;
struct SettingsView;
union SettingsWord;
}  // namespace VS::Battle::Ai

// Bindings from the battle actor records to the typed AI settings policies
// (ActorSettings.cpp). Each aborts with a diagnostic when given no record.
namespace VS::Battle::Ai {
// Applies the saved settings flags (the actor's room spawn word after its
// group word, read through `saved_settings`) to `settings_word`
// (func_800E5A74).
void ApplyLiveSavedAiSettingsFlags(
    int* settings_word,
    const VS::Battle::Ai::SavedSettingsPair* saved_settings);

// Decodes `settings` and the state's actor spawn words into the new AI
// state, selects its decision behaviour (allocating the orbiting boss state
// for sub-types 0xAC-0xAD), then resolves linked actors and builds its action
// metadata (func_800E5A9C). Aborts when the room's waypoint binding does not
// belong to the active room auxiliary section.
void ApplyLiveActorAiSettings(VS::Battle::Ai::SettingsView* state,
                              const VS::Battle::Ai::SettingsWord* settings);
}  // namespace VS::Battle::Ai
