#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string_view>

#include "Menu/Status/ActorSwitching.hpp"
#include "Render/EncodedText.hpp"
#include "Render/UiPackets.hpp"

// MENU4.PRG 120.c:847-1294,2228-2259. The host owns actors, text, frame
// packets and the shared MAINMENU workspace; these controllers own no leases.
// NOLINTNEXTLINE(readability-identifier-naming): the catalog's component.
namespace VS::Menu::Status {
inline constexpr std::size_t kStatusEffectSlots = 32;
inline constexpr std::size_t kLimbAffinitySlots = 8;
inline constexpr std::size_t kLimbDamageTypeSlots = 4;
inline constexpr std::size_t kLimbConditionCount = 5;

enum class LimbControllerError : std::uint8_t {
  kMissingActor,
  kInvalidLimb,
  kInvalidCondition,
  kInvalidAnimation,
  kEmptyEnteringPanel,
  kInvalidNavigation,
  kInvalidText,
  kInsufficientStats,
  kUnavailableService,
};
using LimbControllerResult = std::expected<void, LimbControllerError>;
[[nodiscard]] std::string_view LimbControllerErrorText(
    LimbControllerError error) noexcept;

struct LimbIndicatorRequest {
  std::int32_t condition = 0;
  std::int32_t position = 0;
  std::int32_t dimming = 0;
};
struct LimbLeaderRequest {
  std::int32_t limb = 0;
  std::int32_t position = 0;
  std::int32_t opacity = 0;
};

// decomp: drawStatusIndicator (MENU4.PRG src/MENU/MENU4.PRG/120.c:851; was
// _drawStatusIndicator, 80103E58). Shifts use the R3000's low five shift bits;
// packed position addition carries between halfwords, rather than independently
// clamping coordinates.
[[nodiscard]] LimbControllerResult DrawLimbConditionIndicator(
    Render::PacketQueue& queue, std::span<const std::uint32_t> palette,
    const LimbIndicatorRequest& request);
// decomp: drawLimbLeaderLine (MENU4.PRG src/MENU/MENU4.PRG/120.c:874; was
// _drawLimbLeaderLine, 80103EF8).
[[nodiscard]] LimbControllerResult DrawLimbLeaderLine(
    Render::PacketQueue& queue, const LimbLeaderRequest& request);

class StatusIconHost {
 public:
  virtual ~StatusIconHost() = default;
  [[nodiscard]] virtual bool QueueStatusSprite(const VitalsSprite& sprite) = 0;
};
// decomp: renderStatusIcons (MENU4.PRG src/MENU/MENU4.PRG/120.c:916; was
// _renderStatusIcons, 80103FEC).
[[nodiscard]] LimbControllerResult DrawStatusEffectIcons(
    StatusIconHost& host, std::uint32_t flags, std::int32_t animation_step,
    std::span<const std::uint16_t, kStatusEffectSlots> texture_offsets);

struct StatusEffectTarget {
  std::int32_t actor_slot = 0;
  std::uint32_t flags = 0;
};
struct StatusEffectNavigationState {
  std::uint8_t cursor_animation = 0;
  std::array<std::uint8_t, kStatusEffectSlots> active{};
  std::uint8_t selected = 0;
  std::uint8_t count = 0;
  // The original write-only stats pointer is an actor identity, not ownership.
  std::optional<std::int32_t> target_slot;
};
struct StatusEffectNavigationFrame {
  std::uint8_t selected_element = 0;
  std::uint8_t cursor_color = 0;
  std::int32_t yaw = 0;
  std::int32_t pitch = 0;
  std::uint32_t pressed = 0;
  std::uint32_t held = 0;
  std::uint32_t repeat = 0;
  std::int32_t stick_x = 0;
  std::int32_t stick_y = 0;
};
class StatusEffectNavigationHost {
 public:
  virtual ~StatusEffectNavigationHost() = default;
  virtual void PlayChangeSound() = 0;
  [[nodiscard]] virtual bool ShowStatusDescription(std::uint8_t effect) = 0;
  virtual std::int32_t DrawCursor(std::uint8_t animation,
                                  std::int32_t position) = 0;
};
// decomp: navigateStatusModifiers (MENU4.PRG src/MENU/MENU4.PRG/120.c:964; was
// _navigateStatusModifiers, 80104134). A target rebuilds the active list; null
// continues the previous navigation. Result: 0 stay, 1 leave the grid/empty
// target, 2 Triangle, 3 Cross.
[[nodiscard]] std::expected<std::int32_t, LimbControllerError>
NavigateStatusEffects(StatusEffectNavigationState& state,
                      StatusEffectNavigationFrame& frame,
                      std::optional<StatusEffectTarget> target,
                      bool selection_active, StatusEffectNavigationHost& host);

struct LimbPanelState {
  std::uint8_t phase = 0;  // 0 idle, 1 entering, 2 shown, 3 leaving
  std::uint8_t actor_slot = 0;
  std::array<std::int32_t, kStatusLimbSlots> animation_steps{};
};
struct LimbPanelFrame {
  std::int32_t selected_actor = 0;  // one-based
  std::uint8_t selection_and_dimming = 0;
  std::int32_t fade_screen = 0;
  std::int32_t background_first = 0;
  std::span<const std::uint8_t> row_offsets;
};
using LimbNameIndices = std::array<std::uint8_t, kStatusLimbSlots>;

class LimbPanelHost {
 public:
  virtual ~LimbPanelHost() = default;
  [[nodiscard]] virtual std::optional<LimbNameIndices> Names(
      std::uint8_t actor_slot) = 0;
  [[nodiscard]] virtual bool PopulateComparisonStats(
      std::uint8_t actor_slot) = 0;
  [[nodiscard]] virtual bool DrawIcons(std::uint8_t actor_slot,
                                       std::int32_t slide) = 0;
  [[nodiscard]] virtual bool DrawName(std::uint8_t name, std::int32_t position,
                                      std::optional<std::uint32_t> color) = 0;
  [[nodiscard]] virtual std::optional<std::uint8_t> Condition(
      std::uint8_t actor_slot, std::size_t limb) = 0;
  [[nodiscard]] virtual bool DrawCondition(
      std::uint8_t condition, std::int32_t position,
      std::optional<std::uint32_t> color) = 0;
  [[nodiscard]] virtual bool DrawIndicator(
      const LimbIndicatorRequest& request) = 0;
  [[nodiscard]] virtual std::optional<std::int32_t> ProjectAnchor(
      std::uint8_t actor_slot, std::size_t limb) = 0;
  [[nodiscard]] virtual bool DrawLeader(const LimbLeaderRequest& request) = 0;
};
// decomp: renderLimbUi (MENU4.PRG src/MENU/MENU4.PRG/120.c:1149; was
// _renderLimbUi, 801045B8). Positive requests store the low byte of request-1;
// -3 queries; -2 leaves; all other negative requests initialize. Entering an
// unnamed actor is refused (#419), rather than depending on the original
// uninitialized $s1.
[[nodiscard]] std::expected<std::int32_t, LimbControllerError> UpdateLimbPanel(
    LimbPanelState& state, LimbPanelFrame& frame, std::int32_t request,
    LimbPanelHost& host);

class SelectedLimbHost {
 public:
  virtual ~SelectedLimbHost() = default;
  [[nodiscard]] virtual bool ShowInformation(
      std::span<const std::uint8_t> text) = 0;
  [[nodiscard]] virtual bool SelectLimb(std::int32_t request) = 0;
};
struct SelectedLimbText {
  std::span<const std::uint8_t> name;
  std::span<const std::uint8_t> condition;
  std::span<const std::uint8_t> format;
};
// decomp: renderSelectedLimbCondition (MENU4.PRG src/MENU/MENU4.PRG/120.c:2231;
// was _renderSelectedLimbCondition, 80106308). Arguments retain the two
// borrowed text views. Other arguments and output bytes after the terminator
// remain untouched.
[[nodiscard]] LimbControllerResult ShowSelectedLimbCondition(
    std::uint8_t limb, const SelectedLimbText& text,
    Render::EncodedTextArguments& arguments, std::span<std::uint8_t> output,
    SelectedLimbHost& host);
struct LimbDefenseView {
  std::span<const std::int16_t, kLimbAffinitySlots> affinities;
  std::span<const std::int16_t, kLimbDamageTypeSlots> damage_types;
};
// decomp: renderSelectedLimbStats (MENU4.PRG src/MENU/MENU4.PRG/120.c:2248; was
// _renderSelectedLimbStats, 801063F8). Sequential halfword loads/stores
// deliberately preserve overlapping views.
[[nodiscard]] LimbControllerResult PopulateSelectedLimbDefense(
    std::uint8_t limb, const LimbDefenseView& defense,
    std::span<std::int16_t> equipment_stats, SelectedLimbHost& host);
}  // namespace VS::Menu::Status
