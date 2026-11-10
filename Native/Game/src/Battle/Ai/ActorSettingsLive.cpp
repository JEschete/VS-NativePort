// Hand-maintained (tools/port/owned.csv).
// Live binding for the actor AI settings policies (ActorSettings.cpp).
#include "Battle/Ai/ActorSettingsLive.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

extern "C" {
#include "Battle/Actor.h"
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/AiViews.h"
#include "Core/Diagnostics.h"
}

namespace VS::Battle::Ai {
namespace {
// The actor-state creation view reads the saved pair from the actor's group
// word, so its second word is the room spawn's saved settings flags.
static_assert(offsetof(Battle_Ai_AiActorView, savedAiSettings) +
                  offsetof(Battle_Ai_SavedSettingsPair, opaque4) ==
              offsetof(Battle_Ai_AiActorView, savedAiSettingsFlags));
static_assert(offsetof(Battle_Ai_AiActorView, savedAiSettingsFlags) ==
              offsetof(Battle_Actor, reserved34));
static_assert(offsetof(Battle_Ai_AiActorView, aiSettings) ==
              offsetof(Battle_Actor, templateWord2C));

void Require(bool valid, const char* message) {
  if (!valid) {
    Core_Errorf("ai", "%s", message);
    std::abort();
  }
}
}  // namespace

void ApplyLiveSavedAiSettingsFlags(
    int* settings_word, const Battle_Ai_SavedSettingsPair* saved_settings) {
  Require(settings_word != nullptr && saved_settings != nullptr,
          "AI saved settings require a settings word and a saved pair");
  *settings_word = std::bit_cast<int>(ApplySavedAiSettingsFlags(
      std::bit_cast<std::uint32_t>(*settings_word), saved_settings->opaque4));
}
}  // namespace VS::Battle::Ai
