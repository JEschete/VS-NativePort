#include "Menu/SceneNotifications.hpp"

#include <algorithm>
#include <array>

#include "Core/Math/ScalarMath.hpp"

namespace VS::Menu {
namespace {
constexpr std::int32_t kActionMessage = 11;
constexpr std::int32_t kSpellMastered = 12;
constexpr std::uint8_t kInitialTimer = 120;
constexpr std::uint8_t kTimerStopThreshold = 4;
constexpr std::array<std::uint8_t, 4> kTextPrefix{0xF8, 0, 0xFB, 4};

struct Message {
  SceneNotificationText source;
  SceneNotificationText subject;
  std::size_t prefix = 0;
};

std::optional<Message> SelectMessage(const SceneNotificationHost& host,
                                     std::int32_t kind, std::int32_t subject) {
  Message message;
  if (kind == kActionMessage || kind == kSpellMastered || subject == 0) {
    const auto action = host.Action(subject);
    if (!action) return std::nullopt;
    message.subject = action->name;
    if (kind == kActionMessage || kind == kSpellMastered) {
      const auto source = kind == kSpellMastered
                              ? host.SpellMasteredText()
                              : host.ActionText(action->type);
      if (!source) return std::nullopt;
      message.source = *source;
      message.prefix = kTextPrefix.size();
      return message;
    }
  } else {
    // The item branch replaces the original's unused intermediate action name.
    const auto item = host.ItemName(subject);
    if (!item) return std::nullopt;
    message.subject = *item;
  }
  const auto source = host.NotificationText(kind);
  if (!source) return std::nullopt;
  message.source = *source;
  return message;
}
}  // namespace

// decomp: F01379 vs_battle_displaySceneMessage
// (src/BATTLE/BATTLE.PRG/5BF94.c:3771, 800CB2B8-800CB45C).
SceneNotificationResult ShowSceneNotification(SceneNotificationState& state,
                                              SceneNotificationHost& host,
                                              std::int32_t kind,
                                              std::int32_t subject,
                                              std::int32_t timed) {
  if (!state.buffer.empty()) return SceneNotificationResult::kAlreadyActive;
  const auto message = SelectMessage(host, kind, subject);
  if (!message || !host.CanFormat(message->source, message->subject,
                                  kSceneNotificationBytes - message->prefix)) {
    return SceneNotificationResult::kInvalidText;
  }
  auto allocation = host.Allocate();
  if (allocation.size() < kSceneNotificationBytes) {
    if (!allocation.empty()) host.Release(allocation);
    return SceneNotificationResult::kAllocationFailed;
  }
  state.buffer = allocation.first(kSceneNotificationBytes);
  state.timed = static_cast<std::uint8_t>(timed);
  if (state.timed != 0) state.timer = kInitialTimer;
  host.InitializeBox();
  std::ranges::copy(kTextPrefix, state.buffer.begin());
  host.BindSubject(message->subject);
  if (kind == kActionMessage) host.ClearFrameShape();
  if (!host.Format(state.buffer.subspan(message->prefix), message->source)) {
    ReleaseSceneNotification(state, host);
    return SceneNotificationResult::kFormattingFailed;
  }
  host.SetText(state.buffer);
  return SceneNotificationResult::kShown;
}

// decomp: F01380 func_800CB45C (src/BATTLE/BATTLE.PRG/5BF94.c:3802).
std::int32_t IsSceneNotificationPending(SceneNotificationState& state,
                                        SceneNotificationHost& host,
                                        std::int32_t tick_speed) {
  if (state.buffer.empty()) return 0;
  if (!host.BoxDone()) return 1;
  if (state.timed != 0) {
    state.timer = state.timer < kTimerStopThreshold
                      ? 0
                      : static_cast<std::uint8_t>(
                            static_cast<std::uint32_t>(state.timer) -
                            static_cast<std::uint32_t>(tick_speed));
    if (state.timer == 0) return 0;
  }
  return Core::Math::WrappingSubtract(0, host.PollText());
}

// decomp: F01381 func_800CB50C (src/BATTLE/BATTLE.PRG/5BF94.c:3829).
void ReleaseSceneNotification(SceneNotificationState& state,
                              SceneNotificationHost& host) {
  if (state.buffer.empty()) return;
  host.DismissBox();
  host.Release(state.buffer);
  state.buffer = {};
}
}  // namespace VS::Menu
