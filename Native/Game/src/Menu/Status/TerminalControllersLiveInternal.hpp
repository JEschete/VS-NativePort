#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>

#include "Actors/ActorStateLive.hpp"
#include "Battle/Actor.hpp"
#include "Battle/Damage.hpp"
#include "Battle/HudStateLive.hpp"
#include "Menu/Framework/EquipmentDetailsLive.hpp"
#include "Menu/Framework/EquipmentDetailsLiveState.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Framework/FrameworkLiveState.hpp"
#include "Menu/Framework/Menus.hpp"
#include "Menu/Sounds.hpp"
#include "Menu/Status/ActorSwitchingLive.hpp"
#include "Menu/Status/TerminalControllersLive.hpp"
#include "Render/UiPacketsLive.hpp"

// NOLINTNEXTLINE(readability-identifier-naming): the catalog's component.
namespace VS::Menu::Status::terminal_live {
extern EquipmentDetailState detailState;
extern EquipmentListState listState;
extern StatusTerminalState screenState;

void Refuse(std::string_view reason);
[[nodiscard]] int Report(std::string_view entry, const TerminalResult& result);
[[nodiscard]] TerminalFrame LoadFrame();
void StoreFrame(const TerminalFrame& frame);
[[nodiscard]] TerminalView LoadView();
void StoreView(const TerminalView& view);
[[nodiscard]] bool Workspace();
[[nodiscard]] bool ItemText();
[[nodiscard]] bool RenderSpace();
[[nodiscard]] VS::Battle::ActorStats* Stats(int slot);
[[nodiscard]] std::optional<TerminalActor> ActorView(int slot);
[[nodiscard]] std::span<const std::uint8_t> Encoded(
    std::span<const std::uint8_t> bytes);
[[nodiscard]] std::span<const std::uint8_t> StatusText(int index, bool indexed);
[[nodiscard]] bool DrawBackgroundChecked();
[[nodiscard]] bool DrawFadeChecked();
[[nodiscard]] bool DrawFrameChecked();
[[nodiscard]] bool VitalsChecked();

class Roster final : public InspectableActorRoster {
 public:
  std::uint32_t StateFlags(std::int32_t slot) override {
    return Actors::LiveActorStateFlags(slot);
  }
  std::optional<std::uint16_t> Kind(std::int32_t slot) override {
    if (Stats(slot) == nullptr) {
      return std::nullopt;
    }
    return VS::Battle::actors[slot]->kind;
  }
};

// The scope is a synchronous borrow, not retained overlay ownership. Every
// callback sees pending writes; its changed fields are reborrowed afterwards.
template <typename Interface>
class HostBase : public Interface {
 public:
  HostBase() : frame(LoadFrame()), view(LoadView()) {}
  ~HostBase() override { Publish(); }
  TerminalFrame& Frame() override { return frame; }
  TerminalTables Tables() override {
    return {VS::Menu::Framework::rowAnimationSteps,
            VS::Menu::Framework::animationIndices,
            VS::Menu::Framework::statusViewCursorPositions};
  }
  std::optional<TerminalActor> Actor(int slot) override {
    return ActorView(slot);
  }
  std::optional<bool> MenuReady() override {
    if (!Workspace()) {
      return std::nullopt;
    }
    return Invoke([] { return Framework::AreLiveMenuRowsIdle() != 0; });
  }
  bool Sound(TerminalSound sound) override {
    Invoke([sound] {
      switch (sound) {
        case TerminalSound::kChange:
          Menu::PlayChangeSound();
          break;
        case TerminalSound::kLeave:
          Menu::PlayLeaveSound();
          break;
        case TerminalSound::kSelect:
          Menu::PlaySelectSound();
          break;
        case TerminalSound::kInvalid:
          Menu::PlayInvalidSound();
          break;
      }
    });
    return true;
  }
  std::optional<std::int32_t> Cursor(std::uint32_t animation,
                                     std::int32_t position) override {
    if (!RenderSpace()) {
      return std::nullopt;
    }
    const auto dropped = Render::DroppedLiveUiPacketCount();
    const auto result =
        Invoke([&] { return Framework::DrawLiveCursor(animation, position); });
    if (Render::DroppedLiveUiPacketCount() != dropped) {
      return std::nullopt;
    }
    return result;
  }
  std::optional<int> NextActor(int current, int step) override {
    if (current < 0 || current >= kInspectableActorSlots) {
      return std::nullopt;
    }
    Roster roster;
    return FindNextInspectableActor(roster, current, step);
  }
  std::optional<int> SwitchActor(int request) override {
    const int slot =
        request != 0 ? (request & 15) - 1 : frame.selected_actor - 1;
    if (Stats(slot) == nullptr || !Workspace()) {
      return std::nullopt;
    }
    return Invoke([request] { return LiveAnimateActorSwitch(request); });
  }
  bool BeginVitals(int actor) override {
    if (actor < 0 || actor > kInspectableActorSlots) {
      return false;
    }
    Invoke([actor] { LiveBeginVitalsTransition(actor); });
    return true;
  }
  bool PrimaryStats(int command) override {
    Invoke([command] { Framework::UpdateLivePrimaryStatPanel(command); });
    return true;
  }
  bool StatPages(int command) override {
    Invoke([command] { Framework::UpdateLiveEquipmentStatPages(command); });
    return true;
  }
  bool CommandLabel(Framework::MenuCommand command) override {
    Invoke([command] { Framework::SetLiveCommandLabel(command); });
    return true;
  }
  bool ClearRows(int except) override {
    if (!Workspace()) {
      return false;
    }
    Invoke([except] { Framework::ClearLiveMenuExcept(except); });
    return true;
  }
  bool Information(std::span<const std::uint8_t> text) override {
    const auto bounded = Encoded(text);
    if (bounded.empty() || !Workspace()) {
      return false;
    }
    Invoke([bounded] { Framework::SetLiveInformationMessageText(bounded); });
    return true;
  }
  std::span<const std::uint8_t> StatusText(int index, bool indexed) override {
    return terminal_live::StatusText(index, indexed);
  }

 protected:
  TerminalFrame frame;
  TerminalView view;

  void Publish() {
    StoreFrame(frame);
    StoreView(view);
  }
  template <typename Function>
  auto Invoke(Function&& function) {
    Publish();
    if constexpr (std::is_void_v<std::invoke_result_t<Function>>) {
      std::forward<Function>(function)();
      frame = LoadFrame();
      view = LoadView();
    } else {
      auto result = std::forward<Function>(function)();
      frame = LoadFrame();
      view = LoadView();
      return result;
    }
  }
};
}  // namespace VS::Menu::Status::terminal_live
