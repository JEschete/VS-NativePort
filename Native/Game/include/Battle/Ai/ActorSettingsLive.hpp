#pragma once
// Hand-maintained (tools/port/owned.csv).

#include "Battle/Ai/ActorSettings.hpp"

struct Battle_Ai_SavedSettingsPair;

// Bindings from the battle actor records to the typed AI settings policies
// (ActorSettings.cpp). Each aborts with a diagnostic when given no record.
namespace VS::Battle::Ai {
// Applies the saved settings flags (the actor's room spawn word after its
// group word, read through `saved_settings`) to `settings_word`
// (func_800E5A74).
void ApplyLiveSavedAiSettingsFlags(
    int* settings_word, const Battle_Ai_SavedSettingsPair* saved_settings);
}  // namespace VS::Battle::Ai
