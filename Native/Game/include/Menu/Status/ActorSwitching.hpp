#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

// The status screen's inspected actor (MENU4.PRG 120.c:528-843 and
// 1114-1142): the vitals panel's slide between actors, the L1/R1 search for
// the next inspectable actor, the model slide and reload when the inspected
// actor changes, the HP/MP/Risk panel with its analyzed and unanalyzed
// values, and the limb and equipment row counts. The rules work on typed
// state and actor views and reach the HUD, the actor preview model, the menu
// rows and the UI packets only through the host interfaces below.
// ActorSwitchingLive.hpp binds them to the copied MENU4.PRG globals.
// NOLINTNEXTLINE(readability-identifier-naming): the catalog's component.
namespace VS::Menu::Status {
// VS::Battle::actors slots the status screen cycles through (the original's %
// 15).
inline constexpr int kInspectableActorSlots = 15;
// Limbs a battle actor's stats record holds (vs_battle_actor2::limbs).
inline constexpr std::size_t kStatusLimbSlots = 6;

// _renderBasicStatsState values. The state is a byte the screen may leave at
// any value; values past kAwaitingHud only skip the update.
namespace vitals_phase {
inline constexpr std::uint8_t kIdle = 0;
inline constexpr std::uint8_t kHiding = 1;
inline constexpr std::uint8_t kSwapping = 2;
inline constexpr std::uint8_t kShowing = 3;
inline constexpr std::uint8_t kAwaitingHud = 4;
}  // namespace vitals_phase

// The vitals panel's slide state (_renderBasicStatsState,
// _basicStatsAnimState, _basicStatsAnimDelay and _initializedActor).
struct VitalsPanelState {
  std::uint8_t phase = vitals_phase::kIdle;
  std::uint8_t slide = 0;  // 0 hidden .. 4 fully shown
  std::uint8_t delay = 0;
  std::uint8_t shown_actor = 0;  // the slot + 1 the panel was set up for
};

// The HUD's slide offset once it has slid fully out (D_800EB9B0).
inline constexpr std::int32_t kHudFullyHiddenOffset = 0x200000;

// The vitals one actor's stats record shows (vs_battle_actor2).
struct ActorVitals {
  std::int16_t current_hp = 0;
  std::int16_t max_hp = 0;
  std::int16_t current_mp = 0;
  std::int16_t max_mp = 0;
  std::uint16_t risk = 0;
  bool analyzed = false;
};

// The HUD's HP/MP/RISK label sprites (D_800EBBEC, D_800EBBFC, D_800EBC00).
inline constexpr std::size_t kVitalsLabels = 3;
struct VitalsLabelLayout {
  std::array<std::int32_t, kVitalsLabels> positions{};
  std::array<std::uint8_t, kVitalsLabels> widths{};
  std::array<std::uint8_t, kVitalsLabels> texture_u{};
};

// One textured sprite (vs_battle_setSprite and
// vs_battle_setSpriteDefaultTexPage): the brightness/transparency word, the
// packed position and size, and the texture/CLUT word stored in its fourth
// word.
struct VitalsSprite {
  std::int32_t brightness = 0;
  std::int32_t xy = 0;
  std::int32_t wh = 0;
  std::uint32_t uv_clut = 0;
};

// The panel's side effects. The host owns the ordering-table position the
// packets go in front of.
class VitalsHost {
 public:
  VitalsHost() = default;
  VitalsHost(const VitalsHost&) = delete;
  VitalsHost& operator=(const VitalsHost&) = delete;
  VitalsHost(VitalsHost&&) = delete;
  VitalsHost& operator=(VitalsHost&&) = delete;
  virtual ~VitalsHost() = default;

  // func_800CB654: 1 slides the battle HUD out, 0 back in.
  virtual void SetHudHidden(std::int32_t hidden) = 0;
  // vs_battle_renderValue: draws `value` right-aligned at `xy` in `font`
  // (2 is the separator) and returns the packed position left of it.
  virtual std::int32_t DrawValue(std::int32_t font, std::int32_t xy,
                                 std::int32_t value) = 0;
  // vs_battle_setSprite.
  virtual void QueueSprite(const VitalsSprite& sprite) = 0;
  // vs_battle_setSpriteDefaultTexPage.
  virtual void QueueLabelSprite(const VitalsSprite& sprite) = 0;
  // vs_battle_renderStatBar: a gauge of colour `color` filled `fill` / 64.
  virtual void DrawGauge(std::int32_t color, std::int32_t fill,
                         std::int32_t xy) = 0;
};

// What one DrawVitals frame reads: the inspected actor (_selectedActor), the
// actor the panel is sliding away from (_previousActor), absent when its
// record is missing, the HUD's slide offset and the label layout.
struct VitalsFrame {
  std::optional<ActorVitals> selected;
  std::optional<ActorVitals> previous;
  std::int32_t hud_slide_offset = 0;
  VitalsLabelLayout labels{};
};

// decomp: MENU4.PRG _initEquipmentScreen (120.c:528).
// Starts the panel's slide for `actor` (the slot + 1, or 0 to hide it).
void BeginVitalsTransition(VitalsPanelState& state, std::int32_t actor,
                           VitalsHost& host);

// decomp: MENU4.PRG _renderUnknownValue (120.c:692).
// Draws "???" (with "??? / " in front when `with_max`) right-aligned at `xy`.
void DrawUnanalyzedValue(VitalsHost& host, std::int32_t xy, bool with_max);

// decomp: MENU4.PRG _renderStatBar (120.c:721).
// `color_index` holds the colour in its low byte and whether the actor is
// analyzed above it: an unanalyzed actor's bar is drawn full.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void DrawVitalBar(VitalsHost& host, std::int32_t color_index,
                  std::int32_t current, std::int32_t max, std::int32_t xy);

// decomp: MENU4.PRG _renderBasicStats (120.c:741).
// Advances the panel's slide and draws the HP/MP/RISK values, labels and
// bars. False, with nothing changed, when the selected actor is missing, and
// false, after the slide step, when the previous actor it slides away from is.
[[nodiscard]] bool DrawVitals(VitalsPanelState& state, const VitalsFrame& frame,
                              VitalsHost& host);

// The battle actor roster the L1/R1 search probes.
class InspectableActorRoster {
 public:
  InspectableActorRoster() = default;
  InspectableActorRoster(const InspectableActorRoster&) = delete;
  InspectableActorRoster& operator=(const InspectableActorRoster&) = delete;
  InspectableActorRoster(InspectableActorRoster&&) = delete;
  InspectableActorRoster& operator=(InspectableActorRoster&&) = delete;
  virtual ~InspectableActorRoster() = default;

  // func_800A0BE0: the slot's actor state flags.
  virtual std::uint32_t StateFlags(std::int32_t slot) = 0;
  // The slot's actor kind (vs_battle_actor::unk1C), absent with no actor.
  virtual std::optional<std::uint16_t> Kind(std::int32_t slot) = 0;
};

// decomp: MENU4.PRG _getNextValidActor (120.c:548).
// Steps `current` by `step` modulo 15 until it reaches an inspectable actor.
// Absent where the original would read before VS::Battle::actors (a negative
// remainder) or loop forever (no inspectable actor on the cycle).
[[nodiscard]] std::optional<std::int32_t> FindNextInspectableActor(
    InspectableActorRoster& roster, std::int32_t current, std::int32_t step);

// _switchCurrentActor's states. The state word may hold any value; values
// past kComplete leave the switch idle.
namespace actor_switch_phase {
inline constexpr std::uint32_t kMenuLoaded = 0;
inline constexpr std::uint32_t kMenuLoading = 1;
inline constexpr std::uint32_t kAnimateForward = 2;
inline constexpr std::uint32_t kMenuLoadedReverse = 3;
inline constexpr std::uint32_t kMenuLoadingReverse = 4;
inline constexpr std::uint32_t kAnimateReverse = 5;
inline constexpr std::uint32_t kComplete = 6;
}  // namespace actor_switch_phase

// The model slide between inspected actors (_switchCurrentActor's state and
// _selectedActor, _animationStep, _drawBackgroundFirst, _fadeScreen and
// _delayScreenUpdate).
struct ActorSwitchState {
  std::uint32_t phase = actor_switch_phase::kMenuLoaded;
  std::int32_t selected_actor = 0;  // the slot + 1, 0 before the first
  std::int32_t animation_step = 0;
  std::int32_t draw_background_first = 0;
  std::int32_t fade_screen = 0;
  std::uint8_t delay_screen_update = 0;
};

// The name row a switch shows (vs_battle_setMenuItem(4, 180, 18, 140, 8)).
struct ActorNameRow {
  std::int32_t row = 0;
  std::int32_t x = 0;
  std::int32_t y = 0;
  std::int32_t width = 0;
  std::int32_t background_width = 0;
  bool selected = false;
};

// The switch's side effects: the sound, the name row and the actor preview
// model the menu slides out and in.
class ActorSwitchHost {
 public:
  ActorSwitchHost() = default;
  ActorSwitchHost(const ActorSwitchHost&) = delete;
  ActorSwitchHost& operator=(const ActorSwitchHost&) = delete;
  ActorSwitchHost(ActorSwitchHost&&) = delete;
  ActorSwitchHost& operator=(ActorSwitchHost&&) = delete;
  virtual ~ActorSwitchHost() = default;

  // vs_battle_playSfx10.
  virtual void PlaySwitchSound() = 0;
  // The menu row with VS::Battle::actors[`slot`]'s name.
  virtual void ShowActorName(std::int32_t slot, const ActorNameRow& row) = 0;
  // func_800F9E0C.
  virtual void ReleaseActorPreview() = 0;
  // func_800F9A24.
  virtual void LoadActorPreview(std::int32_t slot) = 0;
};

// decomp: MENU4.PRG _switchCurrentActor (120.c:579).
// A nonzero `request` starts a switch to slot (request & 0xF) - 1, in reverse
// when bit 4 is set, and returns false; zero steps the slide and returns true
// once it is complete.
bool AnimateActorSwitch(ActorSwitchState& state, std::int32_t request,
                        ActorSwitchHost& host);

// decomp: MENU4.PRG _getLimbCount (120.c:1114).
// The limbs before the first unnamed one, at most kStatusLimbSlots.
[[nodiscard]] std::int32_t LimbCount(
    std::span<const std::uint8_t> limb_name_indices);

// decomp: MENU4.PRG _getEquipmentListRowCount (120.c:1132).
// The weapon and shield rows, one per limb and one for an accessory.
[[nodiscard]] std::int32_t EquipmentRowCount(
    std::span<const std::uint8_t> limb_name_indices,
    std::uint16_t accessory_id);
}  // namespace VS::Menu::Status
