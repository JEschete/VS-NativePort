// Hand-maintained (tools/port/owned.csv).
#include "Menu/Status/ActorSwitchingLive.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#include "Actors/ActorStateLive.hpp"
#include "Battle/Actor.h"
#include "Battle/Damage.h"
#include "Battle/Hud.h"
#include "Battle/HudStateLive.hpp"
#include "Core/Log.hpp"
#include "Legacy/Psx.h"
#include "Menu/Framework/ActorPreviewLive.hpp"
#include "Menu/Framework/Framework.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Sounds.hpp"
#include "Menu/Status/ActorStatus.h"
#include "Menu/Status/ActorSwitching.hpp"
#include "Render/Render.h"
#include "Render/SpritesLive.hpp"
#include "Render/UiPacketsLive.hpp"
#include "Render/UiPrimitives.h"

// NOLINTNEXTLINE(readability-identifier-naming): the catalog's component.
namespace VS::Menu::Status {
namespace {
// The ordering-table offset the vitals packets go in front of
// (vs_scratch.unk4 - 0xC).
constexpr std::ptrdiff_t kVitalsChainBytes = 0xC;
// The sprite packet word holding the packed uv and CLUT.
constexpr std::size_t kSpriteUvClutWord = 4;

// _switchCurrentActor's function-local state word.
std::uint32_t actor_switch_phase_word = actor_switch_phase::kMenuLoaded;

std::uint64_t refused_calls = 0;

void Refuse(std::string_view call, std::string_view reason) {
  if (std::has_single_bit(++refused_calls)) {
    VS_LOG_WARN("menu", "status actor switching: {} refused: {} ({} refusals)",
                call, reason, refused_calls);
  }
}

// The stats record of Battle_actors[slot], or null outside the table or
// without an actor or record.
[[nodiscard]] const Battle_ActorStats* StatsOf(std::int32_t slot) {
  if (slot < 0 || static_cast<std::size_t>(slot) >= std::size(Battle_actors)) {
    return nullptr;
  }
  const Battle_Actor* const actor =
      Battle_actors[static_cast<std::size_t>(slot)];
  return actor != nullptr ? actor->stats : nullptr;
}

[[nodiscard]] std::optional<ActorVitals> VitalsOf(std::int32_t slot) {
  const Battle_ActorStats* const stats = StatsOf(slot);
  if (stats == nullptr) return std::nullopt;
  return ActorVitals{.current_hp = stats->currentHp,
                     .max_hp = stats->maxHp,
                     .current_mp = stats->currentMp,
                     .max_mp = stats->maxMp,
                     .risk = stats->risk,
                     .analyzed = stats->isAnalyzed != 0};
}

[[nodiscard]] std::array<std::uint8_t, kStatusLimbSlots> LimbNamesOf(
    const Battle_ActorStats& stats) {
  std::array<std::uint8_t, kStatusLimbSlots> names{};
  for (std::size_t limb = 0; limb < names.size(); ++limb) {
    names[limb] = stats.limbs[limb].nameIndex;
  }
  return names;
}

[[nodiscard]] u_long* VitalsChain() {
  auto* const table =
      static_cast<std::uint8_t*>(Render_frameScratch.orderingTable);
  if (table == nullptr) return nullptr;
  return reinterpret_cast<u_long*>(table - kVitalsChainBytes);
}

class LiveVitalsHost final : public VitalsHost {
 public:
  explicit LiveVitalsHost(u_long* chain) : chain_(chain) {}

  void SetHudHidden(std::int32_t hidden) override {
    Battle::SetLiveHudHidden(hidden);
  }
  std::int32_t DrawValue(std::int32_t font, std::int32_t xy,
                         std::int32_t value) override {
    return Render::DrawLiveIntegerRightAligned(font, xy, value, chain_);
  }
  void QueueSprite(const VitalsSprite& sprite) override {
    Render::QueueLiveSprite(sprite.brightness, sprite.xy, sprite.wh,
                            chain_)[kSpriteUvClutWord] = sprite.uv_clut;
  }
  void QueueLabelSprite(const VitalsSprite& sprite) override {
    Render::QueueLiveSpriteWithTexturePage(
        sprite.brightness, sprite.xy, sprite.wh, chain_)[kSpriteUvClutWord] =
        sprite.uv_clut;
  }
  void DrawGauge(std::int32_t color, std::int32_t fill,
                 std::int32_t xy) override {
    Render::DrawLiveStatGauge(color, fill, chain_, xy);
  }

 private:
  u_long* chain_;
};

class LiveRoster final : public InspectableActorRoster {
 public:
  std::uint32_t StateFlags(std::int32_t slot) override {
    return Actors::LiveActorStateFlags(slot);
  }
  std::optional<std::uint16_t> Kind(std::int32_t slot) override {
    if (slot < 0 ||
        static_cast<std::size_t>(slot) >= std::size(Battle_actors)) {
      return std::nullopt;
    }
    const Battle_Actor* const actor =
        Battle_actors[static_cast<std::size_t>(slot)];
    if (actor == nullptr) return std::nullopt;
    return actor->kind;
  }
};

class LiveActorSwitchHost final : public ActorSwitchHost {
 public:
  void PlaySwitchSound() override { PlayOpenSound(); }
  void ShowActorName(std::int32_t slot, const ActorNameRow& row) override {
    const Battle_ActorStats* const stats = StatsOf(slot);
    if (stats == nullptr) {
      Refuse("AnimateActorSwitch", "no actor record to name");
      return;
    }
    const auto name = std::as_bytes(std::span(stats->name));
    Framework::MenuRow& menu_row = Framework::SetLiveMenuItemText(
        {.row = row.row,
         .x = row.x,
         .y = row.y,
         .w = row.width,
         .background_width = row.background_width},
        {reinterpret_cast<const std::uint8_t*>(name.data()), name.size()});
    menu_row.selected = row.selected ? 1 : 0;
  }
  void ReleaseActorPreview() override { Framework::ReleaseLiveActorPreview(); }
  void LoadActorPreview(std::int32_t slot) override {
    Framework::LoadLiveActorPreview(slot);
  }
};

[[nodiscard]] VitalsPanelState LoadVitalsState() {
  return {
      .phase = static_cast<std::uint8_t>(Menu_Status_renderBasicStatsState),
      .slide = static_cast<std::uint8_t>(Menu_Status_basicStatsAnimState),
      .delay = static_cast<std::uint8_t>(Menu_Status_basicStatsAnimDelay),
      .shown_actor = static_cast<std::uint8_t>(Menu_Status_initializedActor)};
}

void StoreVitalsState(const VitalsPanelState& state) {
  Menu_Status_renderBasicStatsState = static_cast<char>(state.phase);
  Menu_Status_basicStatsAnimState = static_cast<char>(state.slide);
  Menu_Status_basicStatsAnimDelay = static_cast<char>(state.delay);
  Menu_Status_initializedActor = static_cast<char>(state.shown_actor);
}

[[nodiscard]] ActorSwitchState LoadSwitchState() {
  return {.phase = actor_switch_phase_word,
          .selected_actor = Menu_Status_selectedActor,
          .animation_step = Menu_Status_animationStep,
          .draw_background_first = Menu_Status_drawBackgroundFirst,
          .fade_screen = Menu_Status_fadeScreen,
          .delay_screen_update =
              static_cast<std::uint8_t>(Menu_Status_delayScreenUpdate)};
}

void StoreSwitchState(const ActorSwitchState& state) {
  actor_switch_phase_word = state.phase;
  Menu_Status_selectedActor = state.selected_actor;
  Menu_Status_animationStep = state.animation_step;
  Menu_Status_drawBackgroundFirst = state.draw_background_first;
  Menu_Status_fadeScreen = state.fade_screen;
  Menu_Status_delayScreenUpdate = static_cast<char>(state.delay_screen_update);
}

[[nodiscard]] VitalsLabelLayout LiveLabelLayout() {
  VitalsLabelLayout labels;
  for (std::size_t label = 0; label < kVitalsLabels; ++label) {
    labels.positions[label] = Battle_hudGaugeLabelPositions[label];
    labels.widths[label] =
        static_cast<std::uint8_t>(Battle_hudGaugeLabelWidths[label]);
    labels.texture_u[label] =
        static_cast<std::uint8_t>(Battle_hudGaugeLabelTexU[label]);
  }
  return labels;
}

// The selected actor's slot, (_selectedActor - 1).
[[nodiscard]] std::int32_t SelectedSlot() {
  return static_cast<std::int32_t>(
      static_cast<std::uint32_t>(Menu_Status_selectedActor) - 1U);
}
}  // namespace

// decomp: MENU4.PRG _initEquipmentScreen (120.c:528).
void LiveBeginVitalsTransition(int actor) {
  VitalsPanelState state = LoadVitalsState();
  LiveVitalsHost host(VitalsChain());
  BeginVitalsTransition(state, actor, host);
  StoreVitalsState(state);
}

// decomp: MENU4.PRG _getNextValidActor (120.c:548).
int LiveFindNextInspectableActor(int current_actor, int target_actor) {
  LiveRoster roster;
  const std::optional<std::int32_t> next =
      FindNextInspectableActor(roster, current_actor, target_actor);
  if (!next.has_value()) {
    Refuse("FindNextInspectableActor", "no inspectable actor on the cycle");
    return current_actor;
  }
  return *next;
}

// decomp: MENU4.PRG _switchCurrentActor (120.c:579).
int LiveAnimateActorSwitch(int init) {
  ActorSwitchState state = LoadSwitchState();
  LiveActorSwitchHost host;
  const bool complete = AnimateActorSwitch(state, init, host);
  StoreSwitchState(state);
  return complete ? 1 : 0;
}

// decomp: MENU4.PRG _renderUnknownValue (120.c:692).
void LiveDrawUnanalyzedValue(int xy, int with_max, u_long* before) {
  LiveVitalsHost host(before);
  DrawUnanalyzedValue(host, xy, with_max != 0);
}

// decomp: MENU4.PRG _renderStatBar (120.c:721).
void LiveDrawVitalBar(int color_index, int current, int max, int xy) {
  LiveVitalsHost host(VitalsChain());
  DrawVitalBar(host, color_index, current, max, xy);
}

// decomp: MENU4.PRG _renderBasicStats (120.c:741).
void LiveDrawVitals() {
  VitalsPanelState state = LoadVitalsState();
  const VitalsFrame frame{
      .selected = VitalsOf(SelectedSlot()),
      .previous = VitalsOf(static_cast<std::int32_t>(
          static_cast<std::uint32_t>(Menu_Status_previousActor) - 1U)),
      .hud_slide_offset = Battle_hudSlideOffset,
      .labels = LiveLabelLayout()};
  LiveVitalsHost host(VitalsChain());
  const bool drawn = DrawVitals(state, frame, host);
  StoreVitalsState(state);
  if (!drawn) Refuse("DrawVitals", "no stats record for the shown actor");
}

// decomp: MENU4.PRG _getLimbCount (120.c:1114).
int LiveLimbCount(int actor) {
  const Battle_ActorStats* const stats = StatsOf(actor);
  if (stats == nullptr) {
    Refuse("LimbCount", "no stats record for the actor");
    return 0;
  }
  return LimbCount(LimbNamesOf(*stats));
}

// decomp: MENU4.PRG _getEquipmentListRowCount (120.c:1132).
int LiveEquipmentRowCount() {
  const Battle_ActorStats* const stats = StatsOf(SelectedSlot());
  if (stats == nullptr) {
    Refuse("EquipmentRowCount", "no stats record for the selected actor");
    return 0;
  }
  return EquipmentRowCount(LimbNamesOf(*stats), stats->accessory.accessory.id);
}

void ResetLiveActorSwitchPhase() {
  actor_switch_phase_word = actor_switch_phase::kMenuLoaded;
}
}  // namespace VS::Menu::Status
