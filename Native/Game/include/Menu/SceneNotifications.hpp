#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace VS::Menu {
inline constexpr std::size_t kSceneNotificationBytes = 112;
using SceneNotificationText = std::span<const std::uint8_t>;

struct SceneNotificationAction {
  SceneNotificationText name;
  std::uint8_t type = 0;
};

struct SceneNotificationState {
  std::span<std::uint8_t> buffer;
  std::uint8_t timer = 0;
  std::uint8_t timed = 0;
};

class SceneNotificationHost {
 public:
  virtual ~SceneNotificationHost() = default;
  [[nodiscard]] virtual std::optional<SceneNotificationAction> Action(
      std::int32_t index) const = 0;
  [[nodiscard]] virtual std::optional<SceneNotificationText> ItemName(
      std::int32_t subject) const = 0;
  [[nodiscard]] virtual std::optional<SceneNotificationText> NotificationText(
      std::int32_t kind) const = 0;
  [[nodiscard]] virtual std::optional<SceneNotificationText> ActionText(
      std::uint8_t type) const = 0;
  [[nodiscard]] virtual std::optional<SceneNotificationText> SpellMasteredText()
      const = 0;
  [[nodiscard]] virtual bool CanFormat(SceneNotificationText source,
                                       SceneNotificationText subject,
                                       std::size_t capacity) const = 0;
  [[nodiscard]] virtual std::span<std::uint8_t> Allocate() = 0;
  virtual void InitializeBox() = 0;
  virtual void BindSubject(SceneNotificationText subject) = 0;
  virtual void ClearFrameShape() = 0;
  virtual bool Format(std::span<std::uint8_t> destination,
                      SceneNotificationText source) = 0;
  virtual void SetText(std::span<std::uint8_t> text) = 0;
  [[nodiscard]] virtual bool BoxDone() const = 0;
  virtual std::int32_t PollText() = 0;
  virtual void DismissBox() = 0;
  virtual void Release(std::span<std::uint8_t> buffer) = 0;
};

enum class SceneNotificationResult {
  kShown,
  kAlreadyActive,
  kInvalidText,
  kAllocationFailed,
  kFormattingFailed,
};

[[nodiscard]] SceneNotificationResult ShowSceneNotification(
    SceneNotificationState& state, SceneNotificationHost& host,
    std::int32_t kind, std::int32_t subject, std::int32_t timed);
[[nodiscard]] std::int32_t IsSceneNotificationPending(
    SceneNotificationState& state, SceneNotificationHost& host,
    std::int32_t tick_speed);
void ReleaseSceneNotification(SceneNotificationState& state,
                              SceneNotificationHost& host);
}  // namespace VS::Menu
