#include "Menu/SceneNotificationsLive.hpp"

#include <bit>
#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <span>

#include "Battle/ActionTable.h"
#include "Battle/TargetingState.h"
#include "Battle/TextBoxes.hpp"
#include "Content/StringTables.h"
#include "Core/Diagnostics.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Menu/Framework/Menus.hpp"
#include "Menu/SceneNotifications.hpp"
#include "Menu/TextBox.hpp"
#include "Menu/TextBoxStateLive.hpp"
#include "Render/EncodedText.hpp"
#include "Render/EncodedTextLive.hpp"
#include "State/Timers.hpp"

namespace VS::Menu {
namespace {
constexpr int kNotificationBox = 7;
constexpr int kNotificationLayout = 0x304;
constexpr int kNotificationLines = 3;
constexpr std::int32_t kFirstNotificationItem = 0x1CA;
constexpr std::size_t kActionTextStart = 22;
constexpr std::size_t kActionTextCount{
    Text_Battle_menuStrings_Index_spellMastered - kActionTextStart,
};
constexpr std::int32_t kNotificationCount{
    Text_Battle_doorStrings_Index_doorLocked + 1,
};
std::uint64_t refused_notifications = 0;

void Refuse(const char* reason) {
  ++refused_notifications;
  if (std::has_single_bit(refused_notifications)) {
    VS::Core::Warnf("menu",
                    "Scene notification refused: %s (count=%" PRIu64 ")",
                    reason, refused_notifications);
  }
}

void Publish(const SceneNotificationState& state) {
  sceneNotificationBuffer = reinterpret_cast<int*>(state.buffer.data());
  sceneNotificationTimer = state.timer;
  sceneNotificationTimed = state.timed;
}

bool Load(SceneNotificationState& state) {
  state.timer = sceneNotificationTimer;
  state.timed = sceneNotificationTimed;
  if (sceneNotificationBuffer == nullptr) return true;
  const auto capacity =
      Core::Memory::LivePayloadCapacity(sceneNotificationBuffer);
  if (!capacity || *capacity < kSceneNotificationBytes) {
    Refuse("the active buffer is not a complete game-heap allocation");
    return false;
  }
  state.buffer = {reinterpret_cast<std::uint8_t*>(sceneNotificationBuffer),
                  kSceneNotificationBytes};
  return true;
}

template <typename T, std::size_t N>
SceneNotificationText Bytes(const T (&text)[N]) {
  return {reinterpret_cast<const std::uint8_t*>(text), sizeof text};
}

std::optional<SceneNotificationText> TextAt(
    std::span<const std::uint16_t> table, std::size_t offset) {
  if (offset >= table.size()) return std::nullopt;
  const auto tail = table.subspan(offset);
  return SceneNotificationText(
      reinterpret_cast<const std::uint8_t*>(tail.data()), tail.size_bytes());
}

class LiveHost final : public SceneNotificationHost {
 public:
  explicit LiveHost(SceneNotificationState& state) : state_(state) {}
  std::optional<SceneNotificationAction> Action(
      std::int32_t index) const override {
    if (index < 0 || static_cast<std::size_t>(index) >=
                         std::size(Battle_actionDefinitions)) {
      return std::nullopt;
    }
    const auto& action = Battle_actionDefinitions[index];
    return SceneNotificationAction{Bytes(action.name),
                                   static_cast<std::uint8_t>(action.type)};
  }
  std::optional<SceneNotificationText> ItemName(
      std::int32_t subject) const override {
    const auto row =
        (static_cast<std::int64_t>(subject) - kFirstNotificationItem) * 2;
    if (row < 0 ||
        row >= static_cast<std::int64_t>(std::size(kNotificationItemNames))) {
      return std::nullopt;
    }
    return Bytes(kNotificationItemNames[static_cast<std::size_t>(row)]);
  }
  std::optional<SceneNotificationText> NotificationText(
      std::int32_t kind) const override {
    if (kind < 0 || kind >= kNotificationCount) return std::nullopt;
    return TextAt(VS::Battle::doorStrings, VS::Battle::doorStrings[kind]);
  }
  std::optional<SceneNotificationText> ActionText(
      std::uint8_t type) const override {
    if (type >= kActionTextCount) return std::nullopt;
    return TextAt(VS::Menu::menuStrings,
                  VS::Menu::menuStrings[kActionTextStart + type]);
  }
  std::optional<SceneNotificationText> SpellMasteredText() const override {
    return TextAt(VS::Menu::menuStrings,
                  Text_Battle_menuStrings_Offset_spellMastered);
  }
  bool CanFormat(SceneNotificationText source, SceneNotificationText subject,
                 std::size_t capacity) const override {
    auto arguments = Render::LiveEncodedTextArguments();
    arguments.strings[0] = subject;
    const auto bytes = Render::EncodedTextBytes(source, arguments);
    return bytes && *bytes <= capacity;
  }
  std::span<std::uint8_t> Allocate() override {
    auto* buffer = static_cast<std::uint8_t*>(
        Core::Memory::AllocateLiveFromHighEnd(kSceneNotificationBytes));
    return buffer == nullptr ? std::span<std::uint8_t>{}
                             : std::span(buffer, kSceneNotificationBytes);
  }
  void InitializeBox() override {
    Publish(state_);
    InitLiveTextBox(kNotificationBox, kNotificationLayout, 0, 1, 0,
                    kNotificationLines, 0, 0);
  }
  void BindSubject(SceneNotificationText subject) override {
    subject_ = subject;
    Battle_stringContext.strings[0] =
        reinterpret_cast<char*>(const_cast<std::uint8_t*>(subject.data()));
  }
  void ClearFrameShape() override {
    auto box = ReadLiveTextBox(kNotificationBox);
    box.flags.frame_shape = 0;
    WriteLiveTextBox(kNotificationBox, box);
  }
  bool Format(std::span<std::uint8_t> destination,
              SceneNotificationText source) override {
    auto arguments = Render::LiveEncodedTextArguments();
    arguments.strings[0] = subject_;
    return Render::FormatEncodedText(destination, source, arguments)
        .has_value();
  }
  void SetText(std::span<std::uint8_t> text) override {
    Publish(state_);
    SetLiveTextBoxText(kNotificationBox, reinterpret_cast<char*>(text.data()));
  }
  bool BoxDone() const override {
    return ReadLiveTextBox(kNotificationBox).flags.done;
  }
  std::int32_t PollText() override {
    Publish(state_);
    return SetLiveTextBoxText(kNotificationBox, nullptr);
  }
  void DismissBox() override {
    Publish(state_);
    DismissLiveTextBox(kNotificationBox);
  }
  void Release(std::span<std::uint8_t> buffer) override {
    auto box = ReadLiveTextBox(kNotificationBox);
    const auto borrowed = Core::Memory::LivePayloadContaining(box.text);
    if (borrowed &&
        borrowed->data() == reinterpret_cast<const std::byte*>(buffer.data())) {
      // Kind4 draws only its backdrop while closing; it no longer reads text.
      box.text = nullptr;
      WriteLiveTextBox(kNotificationBox, box);
    }
    Core::Memory::FreeLiveFromHighEnd(buffer.data());
  }

 private:
  SceneNotificationState& state_;
  SceneNotificationText subject_;
};

const char* RefusalReason(SceneNotificationResult result) {
  switch (result) {
    case SceneNotificationResult::kAlreadyActive:
      return "another notification still owns the box";
    case SceneNotificationResult::kInvalidText:
      return "source, subject or expanded text exceeds its bounded storage";
    case SceneNotificationResult::kAllocationFailed:
      return "the game heap could not supply the notification payload";
    case SceneNotificationResult::kFormattingFailed:
      return "formatting changed after preflight";
    case SceneNotificationResult::kShown:
      return "unexpected shown-result refusal";
  }
  return "unknown notification result";
}
}  // namespace

// decomp: F01379 vs_battle_displaySceneMessage (5BF94.c:3771).
void ShowLiveSceneNotification(std::int32_t kind, std::int32_t subject,
                               std::int32_t timed) {
  SceneNotificationState state;
  if (!Load(state)) return;
  LiveHost host(state);
  const auto result = ShowSceneNotification(state, host, kind, subject, timed);
  Publish(state);
  if (result != SceneNotificationResult::kShown) Refuse(RefusalReason(result));
}

// decomp: F01380 func_800CB45C (5BF94.c:3802).
std::int32_t IsLiveSceneNotificationPending() {
  SceneNotificationState state;
  if (!Load(state)) return 1;
  LiveHost host(state);
  const auto result =
      IsSceneNotificationPending(state, host, VS::State::tickSpeed);
  Publish(state);
  return result;
}

bool CleanupLiveSceneNotificationOwnedState() {
  SceneNotificationState state;
  if (!Load(state)) return false;
  LiveHost host(state);
  ReleaseSceneNotification(state, host);
  Publish(state);
  return true;
}

// decomp: F01381 func_800CB50C (5BF94.c:3829).
void ReleaseLiveSceneNotification() {
  static_cast<void>(CleanupLiveSceneNotificationOwnedState());
}

std::uint64_t RefusedLiveSceneNotificationCount() {
  return refused_notifications;
}
}  // namespace VS::Menu
