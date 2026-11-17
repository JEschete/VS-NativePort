#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "Menu/SceneNotifications.hpp"
#include "Render/EncodedText.hpp"

namespace VS::tests::notifications {
enum class Call {
  kAllocate,
  kInit,
  kBind,
  kShape,
  kFormat,
  kSet,
  kPoll,
  kDismiss,
  kFree
};

struct World final : Menu::SceneNotificationHost {
  Menu::SceneNotificationState state;
  std::array<std::uint8_t, Menu::kSceneNotificationBytes> storage{};
  std::array<std::uint8_t, 3> action_name{7, 8, Render::kTextTerminator};
  std::array<std::uint8_t, 2> item_name{9, Render::kTextTerminator};
  std::array<std::uint8_t, 3> ordinary{Render::kTextPrintString, 0,
                                       Render::kTextTerminator};
  std::array<std::uint8_t, 4> action_message{0x25, Render::kTextPrintString, 0,
                                             Render::kTextTerminator};
  std::array<std::uint8_t, 4> spell_message{0x33, Render::kTextPrintString, 0,
                                            Render::kTextTerminator};
  std::optional<Menu::SceneNotificationText> action_name_override;
  std::vector<Call> calls;
  Render::EncodedTextArguments arguments;
  std::size_t allocation_bytes = storage.size();
  std::uint8_t observed_timer = 0;
  std::uint8_t observed_timed = 0;
  const std::uint8_t* observed_buffer = nullptr;
  const std::uint8_t* released_buffer = nullptr;
  const std::uint8_t* shown_buffer = nullptr;
  bool format_allowed = true;
  bool format_succeeds = true;
  bool done = false;
  std::int32_t poll_result = 1;

  World() { storage.fill(0xA5); }
  std::optional<Menu::SceneNotificationAction> Action(
      std::int32_t index) const override {
    if (index < 0 || index >= 8) return std::nullopt;
    return Menu::SceneNotificationAction{
        action_name_override.value_or(action_name), 2};
  }
  std::optional<Menu::SceneNotificationText> ItemName(
      std::int32_t subject) const override {
    if (subject != 458) return std::nullopt;
    return item_name;
  }
  std::optional<Menu::SceneNotificationText> NotificationText(
      std::int32_t kind) const override {
    if (kind < 0 || kind >= 11) return std::nullopt;
    return ordinary;
  }
  std::optional<Menu::SceneNotificationText> ActionText(
      std::uint8_t type) const override {
    if (type != 2) return std::nullopt;
    return action_message;
  }
  std::optional<Menu::SceneNotificationText> SpellMasteredText()
      const override {
    return spell_message;
  }
  bool CanFormat(Menu::SceneNotificationText source,
                 Menu::SceneNotificationText subject,
                 std::size_t capacity) const override {
    auto candidate = arguments;
    candidate.strings[0] = subject;
    const auto bytes = Render::EncodedTextBytes(source, candidate);
    return format_allowed && bytes && *bytes <= capacity;
  }
  std::span<std::uint8_t> Allocate() override {
    calls.push_back(Call::kAllocate);
    return std::span(storage).first(allocation_bytes);
  }
  void InitializeBox() override {
    calls.push_back(Call::kInit);
    observed_buffer = state.buffer.data();
    observed_timer = state.timer;
    observed_timed = state.timed;
  }
  void BindSubject(Menu::SceneNotificationText subject) override {
    calls.push_back(Call::kBind);
    arguments.strings[0] = subject;
  }
  void ClearFrameShape() override { calls.push_back(Call::kShape); }
  bool Format(std::span<std::uint8_t> destination,
              Menu::SceneNotificationText source) override {
    calls.push_back(Call::kFormat);
    return format_succeeds &&
           Render::FormatEncodedText(destination, source, arguments)
               .has_value();
  }
  void SetText(std::span<std::uint8_t> text) override {
    calls.push_back(Call::kSet);
    shown_buffer = text.data();
  }
  bool BoxDone() const override { return done; }
  std::int32_t PollText() override {
    calls.push_back(Call::kPoll);
    return poll_result;
  }
  void DismissBox() override { calls.push_back(Call::kDismiss); }
  void Release(std::span<std::uint8_t> buffer) override {
    calls.push_back(Call::kFree);
    released_buffer = buffer.data();
  }
};
}  // namespace VS::tests::notifications
