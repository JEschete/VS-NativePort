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
#include "Battle/Actor.hpp"
#include "Battle/Damage.hpp"
#include "Battle/Hud.hpp"
#include "Battle/HudStateLive.hpp"
#include "Core/Log.hpp"
#include "Legacy/Psx.hpp"
#include "Menu/Framework/ActorPreviewLive.hpp"
#include "Menu/Framework/Framework.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Sounds.hpp"
#include "Menu/Status/ActorStatus.hpp"
#include "Menu/Status/ActorSwitching.hpp"
#include "Render/Render.hpp"
#include "Render/SpritesLive.hpp"
#include "Render/UiPacketsLive.hpp"
#include "Render/UiPrimitives.hpp"

// NOLINTNEXTLINE(readability-identifier-naming): the catalog's component.
namespace VS::Menu::Status {
namespace {
// The ordering-table offset the vitals packets go in front of
// (vs_scratch.unk4 - 0xC).
constexpr std::ptrdiff_t kVitalsChainBytes = 0xC;
// The sprite packet word holding the packed uv and CLUT.
constexpr std::size_t kSpriteUvClutWord = 4;

// _switchCurrentActor's function-local state word.
std::uint32_t actorSwitchPhaseWord = actor_switch_phase::kMenuLoaded;

std::uint64_t refusedCalls = 0;

void Refuse(std::string_view call, std::string_view reason) {
  if (std::has_single_bit(++refusedCalls)) {
    VS::Core::LogWarn("menu",
                      "status actor switching: {} refused: {} ({} refusals)",
                      call, reason, refusedCalls);
  }
}

// The stats record of VS::Battle::actors[slot], or null outside the table or
// without an actor or record.
[[nodiscard]] const VS::Battle::ActorStats* StatsOf(std::int32_t slot) {
  if (slot < 0 ||
      static_cast<std::size_t>(slot) >= std::size(VS::Battle::actors)) {
    return nullptr;
  }
  const VS::Battle::Actor* const actor =
      VS::Battle::actors[static_cast<std::size_t>(slot)];
  return actor != nullptr ? actor->stats : nullptr;
}

[[nodiscard]] std::optional<ActorVitals> VitalsOf(std::int32_t slot) {
  const VS::Battle::ActorStats* const stats = StatsOf(slot);
  if (stats == nullptr) return std::nullopt;
  return ActorVitals{.current_hp = stats->current_hp,
                     .max_hp = stats->max_hp,
                     .current_mp = stats->current_mp,
                     .max_mp = stats->max_mp,
                     .risk = stats->risk,
                     .analyzed = stats->is_analyzed != 0};
}

[[nodiscard]] std::array<std::uint8_t, kStatusLimbSlots> LimbNamesOf(
    const VS::Battle::ActorStats& stats) {
  std::array<std::uint8_t, kStatusLimbSlots> names{};
  for (std::size_t limb = 0; limb < names.size(); ++limb) {
    names[limb] = stats.limbs[limb].name_index;
  }
  return names;
}

[[nodiscard]] std::uint32_t* VitalsChain() {
  auto* const table =
      static_cast<std::uint8_t*>(VS::Render::FrameScratchpad().ordering_table);
  if (table == nullptr) return nullptr;
  return reinterpret_cast<std::uint32_t*>(table - kVitalsChainBytes);
}

class LiveVitalsHost final : public VitalsHost {
 public:
  explicit LiveVitalsHost(std::uint32_t* chain) : chain_(chain) {}

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
  std::uint32_t* chain_;
};

class LiveRoster final : public InspectableActorRoster {
 public:
  std::uint32_t StateFlags(std::int32_t slot) override {
    return Actors::LiveActorStateFlags(slot);
  }
  std::optional<std::uint16_t> Kind(std::int32_t slot) override {
    if (slot < 0 ||
        static_cast<std::size_t>(slot) >= std::size(VS::Battle::actors)) {
      return std::nullopt;
    }
    const VS::Battle::Actor* const actor =
        VS::Battle::actors[static_cast<std::size_t>(slot)];
    if (actor == nullptr) return std::nullopt;
    return actor->kind;
  }
};

class LiveActorSwitchHost final : public ActorSwitchHost {
 public:
  void PlaySwitchSound() override { PlayOpenSound(); }
  void ShowActorName(std::int32_t slot, const ActorNameRow& row) override {
    const VS::Battle::ActorStats* const stats = StatsOf(slot);
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
  return {.phase = renderBasicStatsState,
          .slide = basicStatsAnimState,
          .delay = basicStatsAnimDelay,
          .shown_actor = initializedActor};
}

void StoreVitalsState(const VitalsPanelState& state) {
  renderBasicStatsState = state.phase;
  basicStatsAnimState = state.slide;
  basicStatsAnimDelay = state.delay;
  initializedActor = state.shown_actor;
}

[[nodiscard]] ActorSwitchState LoadSwitchState() {
  return {.phase = actorSwitchPhaseWord,
          .selected_actor = selectedActor,
          .animation_step = screenAnimationStep,
          .draw_background_first = drawBackgroundFirst,
          .fade_screen = fadeScreen,
          .delay_screen_update = delayScreenUpdate};
}

void StoreSwitchState(const ActorSwitchState& state) {
  actorSwitchPhaseWord = state.phase;
  selectedActor = state.selected_actor;
  screenAnimationStep = state.animation_step;
  drawBackgroundFirst = state.draw_background_first;
  fadeScreen = state.fade_screen;
  delayScreenUpdate = state.delay_screen_update;
}

[[nodiscard]] VitalsLabelLayout LiveLabelLayout() {
  VitalsLabelLayout labels;
  for (std::size_t label = 0; label < kVitalsLabels; ++label) {
    labels.positions[label] = VS::Battle::hudGaugeLabelPositions[label];
    labels.widths[label] =
        static_cast<std::uint8_t>(VS::Battle::hudGaugeLabelWidths[label]);
    labels.texture_u[label] =
        static_cast<std::uint8_t>(VS::Battle::hudGaugeLabelTexU[label]);
  }
  return labels;
}

// The selected actor's slot, (_selectedActor - 1).
[[nodiscard]] std::int32_t SelectedSlot() {
  return static_cast<std::int32_t>(static_cast<std::uint32_t>(selectedActor) -
                                   1U);
}
}  // namespace

// decomp: MENU4.PRG initEquipmentScreen (src/MENU/MENU4.PRG/120.c:532; was
// _initEquipmentScreen).
void LiveBeginVitalsTransition(int actor) {
  VitalsPanelState state = LoadVitalsState();
  LiveVitalsHost host(VitalsChain());
  BeginVitalsTransition(state, actor, host);
  StoreVitalsState(state);
}

// decomp: MENU4.PRG getNextValidActor (src/MENU/MENU4.PRG/120.c:552; was
// _getNextValidActor).
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

// decomp: MENU4.PRG switchCurrentActor (src/MENU/MENU4.PRG/120.c:583; was
// _switchCurrentActor).
int LiveAnimateActorSwitch(int init) {
  ActorSwitchState state = LoadSwitchState();
  LiveActorSwitchHost host;
  const bool complete = AnimateActorSwitch(state, init, host);
  StoreSwitchState(state);
  return complete ? 1 : 0;
}

// decomp: MENU4.PRG renderUnknownValue (src/MENU/MENU4.PRG/120.c:696; was
// _renderUnknownValue).
void LiveDrawUnanalyzedValue(int xy, int with_max, std::uint32_t* before) {
  LiveVitalsHost host(before);
  DrawUnanalyzedValue(host, xy, with_max != 0);
}

// decomp: MENU4.PRG renderStatBar (src/MENU/MENU4.PRG/120.c:725; was
// _renderStatBar).
void LiveDrawVitalBar(int color_index, int current, int max, int xy) {
  LiveVitalsHost host(VitalsChain());
  DrawVitalBar(host, color_index, current, max, xy);
}

// decomp: MENU4.PRG renderBasicStats (src/MENU/MENU4.PRG/120.c:745; was
// _renderBasicStats).
void LiveDrawVitals() {
  VitalsPanelState state = LoadVitalsState();
  const VitalsFrame frame{.selected = VitalsOf(SelectedSlot()),
                          .previous = VitalsOf(static_cast<std::int32_t>(
                              static_cast<std::uint32_t>(previousActor) - 1U)),
                          .hud_slide_offset = VS::Battle::hudSlideOffset,
                          .labels = LiveLabelLayout()};
  LiveVitalsHost host(VitalsChain());
  const bool drawn = DrawVitals(state, frame, host);
  StoreVitalsState(state);
  if (!drawn) Refuse("DrawVitals", "no stats record for the shown actor");
}

// decomp: MENU4.PRG getLimbCount (src/MENU/MENU4.PRG/120.c:1117; was
// _getLimbCount).
int LiveLimbCount(int actor) {
  const VS::Battle::ActorStats* const stats = StatsOf(actor);
  if (stats == nullptr) {
    Refuse("LimbCount", "no stats record for the actor");
    return 0;
  }
  return LimbCount(LimbNamesOf(*stats));
}

// decomp: MENU4.PRG getEquipmentListRowCount (src/MENU/MENU4.PRG/120.c:1135;
// was _getEquipmentListRowCount).
int LiveEquipmentRowCount() {
  const VS::Battle::ActorStats* const stats = StatsOf(SelectedSlot());
  if (stats == nullptr) {
    Refuse("EquipmentRowCount", "no stats record for the selected actor");
    return 0;
  }
  return EquipmentRowCount(LimbNamesOf(*stats), stats->accessory.accessory.id);
}

void ResetLiveActorSwitchPhase() {
  actorSwitchPhaseWord = actor_switch_phase::kMenuLoaded;
}
}  // namespace VS::Menu::Status
