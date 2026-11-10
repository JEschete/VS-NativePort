#pragma once
// Hand-maintained (tools/port/owned.csv).

#include "Battle/Ai/ActorSettings.hpp"

struct Battle_Ai_SavedSettingsPair;
struct Battle_Ai_SettingsView;
union Battle_Ai_SettingsWord;

// Bindings from the battle actor records to the typed AI settings policies
// (ActorSettings.cpp). Each aborts with a diagnostic when given no record.
namespace VS::Battle::Ai {
// Applies the saved settings flags (the actor's room spawn word after its
// group word, read through `saved_settings`) to `settings_word`
// (func_800E5A74).
void ApplyLiveSavedAiSettingsFlags(
    int* settings_word, const Battle_Ai_SavedSettingsPair* saved_settings);

// Decodes `settings` and the state's actor spawn words into the new AI
// state, selects its decision behaviour (allocating the orbiting boss state
// for sub-types 0xAC-0xAD), then resolves linked actors and builds its action
// metadata (func_800E5A9C). Aborts when the room's waypoint binding does not
// belong to the active room auxiliary section.
void ApplyLiveActorAiSettings(Battle_Ai_SettingsView* state,
                              const Battle_Ai_SettingsWord* settings);
}  // namespace VS::Battle::Ai
