#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Field/RoomRenderingLive.hpp"
#include "Menu/Framework/ActorPreviewLive.hpp"
#include "Menu/Framework/ItemRowsLive.hpp"
#include "Menu/Framework/ItemRowsLiveState.hpp"
#include "Menu/Status/LimbControllersLive.hpp"
#include "Menu/Status/TerminalControllersLiveInternal.hpp"
#include "Render/Camera.hpp"
#include "Render/CameraViewLive.hpp"
#include "Render/GameFontLive.hpp"

// NOLINTNEXTLINE(readability-identifier-naming): the catalog's component.
namespace VS::Menu::Status::terminal_live {
namespace {
class ScreenHost final : public HostBase<StatusTerminalHost> {
 public:
  bool HasItemText() override {
    return VS::Menu::Framework::itemNames != nullptr;
  }
  std::optional<int> LoadItemText(bool start) override {
    return Invoke([start] {
      return Framework::LoadLiveItemText(static_cast<int>(start));
    });
  }
  std::optional<int> UnloadItemText() override {
    return Invoke([] { return Framework::UnloadLiveItemTextWhenReady(); });
  }
  bool FieldRendering(bool enabled) override {
    Invoke([enabled] {
      Field::LiveSetFieldRenderingEnabled(static_cast<int>(enabled));
    });
    return true;
  }
  bool BackgroundPriority(int offset, int brightness) override {
    Invoke([=] { Framework::SetLiveBackgroundPriority(offset, brightness); });
    return true;
  }
  std::int32_t ProjectionDistance() override {
    return VS::Render::projectionDistance;
  }
  bool SetProjection(std::int32_t distance) override {
    Invoke([distance] { Render::SetLiveCameraProjectionDistance(distance); });
    return true;
  }
  TerminalView& View() override { return view; }
  bool InitializeInformation() override {
    if (!Workspace()) {
      return false;
    }
    Invoke([] { Framework::InitializeLiveInformationBox(); });
    return true;
  }
  bool DismissInformation() override {
    if (!Workspace()) {
      return false;
    }
    Invoke([] { Framework::DismissLiveInformationBox(); });
    return true;
  }
  bool Condition() override {
    if (!Workspace()) {
      return false;
    }
    const auto refused = RefusedLiveLimbControllerCount();
    Invoke([] { LiveShowSelectedLimbCondition(); });
    return RefusedLiveLimbControllerCount() == refused;
  }
  bool Defense() override {
    const auto refused = RefusedLiveLimbControllerCount();
    Invoke([] { LivePopulateSelectedLimbDefense(); });
    return RefusedLiveLimbControllerCount() == refused;
  }
  std::optional<int> LimbPanel(int command) override {
    const auto refused = RefusedLiveLimbControllerCount();
    const int result =
        Invoke([command] { return LiveUpdateLimbPanel(command); });
    return RefusedLiveLimbControllerCount() == refused
               ? std::optional<int>{result}
               : std::nullopt;
  }
  std::optional<int> StatusEffects(std::optional<int> actor,
                                   bool selection) override {
    VS::Battle::ActorStats* target = nullptr;
    if (actor) {
      target = Stats(*actor);
      if (target == nullptr) {
        return std::nullopt;
      }
    }
    if (selection && !RenderSpace()) {
      return std::nullopt;
    }
    const auto refused = RefusedLiveLimbControllerCount();
    const int result = Invoke([=] {
      return LiveNavigateStatusEffects(target, static_cast<int>(selection));
    });
    return RefusedLiveLimbControllerCount() == refused
               ? std::optional<int>{result}
               : std::nullopt;
  }
  std::optional<int> EquipmentList(int request) override {
    const auto refused = RefusedLiveTerminalControllerCount();
    const int result =
        Invoke([request] { return LiveUpdateEquipmentList(request); });
    return RefusedLiveTerminalControllerCount() == refused
               ? std::optional<int>{result}
               : std::nullopt;
  }
  bool Vitals() override {
    return Invoke([] { return VitalsChecked(); });
  }
  bool ReleasePreview() override {
    Invoke([] { Framework::ReleaseLiveActorPreview(); });
    return true;
  }
  bool BackgroundFade() override {
    Invoke([] { Framework::SetLiveBackgroundFade(0); });
    return true;
  }
  bool DrawFrame() override {
    return Invoke([] { return DrawFrameChecked(); });
  }
  bool Prompt(int offset, bool available) override {
    if (!RenderSpace()) {
      return false;
    }
    const auto dropped = Render::DroppedLiveUiPacketCount();
    Invoke([=] {
      constexpr int kPromptX = 16;
      constexpr int kPromptY = 38;
      constexpr int kPromptWidth = 88;
      constexpr int kPromptHeight = 10;
      constexpr int kButtonX = 8;
      constexpr int kButtonY = 36;
      constexpr int kTextX = 28;
      constexpr int kTextColor = 0x404040;
      Framework::DrawLiveButtonPromptBackground(kPromptX - offset, kPromptY,
                                                kPromptWidth, kPromptHeight);
      Framework::DrawLiveButtonIcon(Framework::ButtonIcon::kSquare,
                                    kButtonX - offset, kButtonY, nullptr);
      Render::DrawLiveCompactTextColor(
          "STATUS", Render::PackScreenWord(kTextX - offset, kPromptY),
          kTextColor << static_cast<int>(available), nullptr);
    });
    return Render::DroppedLiveUiPacketCount() == dropped;
  }
};
}  // namespace
}  // namespace VS::Menu::Status::terminal_live

// NOLINTNEXTLINE(readability-identifier-naming): the catalog's component.
namespace VS::Menu::Status {
// decomp: vs_menu4_exec (src/MENU/MENU4.PRG/120.c:2270)
int LiveUpdateStatusTerminal(std::uint8_t* state) {
  const auto address = reinterpret_cast<std::uintptr_t>(state);
  const auto start =
      reinterpret_cast<std::uintptr_t>(VS::Menu::Framework::submenuStates);
  if (state == nullptr || address < start ||
      address - start >= std::size(VS::Menu::Framework::submenuStates)) {
    terminal_live::Refuse(
        "entry state is not a live BATTLE submenu-state byte");
    return 0;
  }
  if (!terminal_live::Workspace()) {
    return 0;
  }
  if (VS::Menu::Framework::itemNames != nullptr && !terminal_live::ItemText()) {
    return 0;
  }
  terminal_live::ScreenHost host;
  return terminal_live::Report(
      "update", UpdateStatusTerminal(terminal_live::screenState, host, *state));
}
}  // namespace VS::Menu::Status
