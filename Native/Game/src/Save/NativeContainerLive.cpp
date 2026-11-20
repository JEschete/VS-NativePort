// Hand-maintained (tools/port/owned.csv).
#include "Save/NativeContainerLive.hpp"

#include <cstdint>
#include <cstdlib>
#include <span>

#include "Battle/Hud.h"
#include "Battle/HudStateLive.hpp"
#include "Content/StringTables.hpp"
#include "Core/Diagnostics.hpp"
#include "Core/Memory/Copy.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Core/Memory/WordCopyLive.hpp"
#include "Field/RoomRenderingLive.hpp"
#include "Input/ControllerLiveState.hpp"
#include "Items/InventoryLive.hpp"
#include "Items/ItemRecords.hpp"
#include "Menu/Container/Storage.hpp"
#include "Menu/FilesLive.hpp"
#include "Menu/FilesLiveState.hpp"
#include "Menu/Framework/EquipmentDetailsLive.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Framework/FrameworkLiveState.hpp"
#include "Menu/Framework/TextBoxes.hpp"
#include "Menu/Sounds.hpp"
#include "Menu/TextBoxStateLive.hpp"
#include "Render/Display.hpp"
#include "Render/Render.hpp"
#include "Save/FileMenu.hpp"
#include "Save/FileMenuLive.hpp"
#include "Save/MemoryCard/CardResources.hpp"
#include "Save/MemoryCard/SaveImage.hpp"
#include "Save/MenuAssetsLive.hpp"
#include "Save/NativeContainer.hpp"
#include "Save/NativeMenu.hpp"
#include "Title/SaveFiles.hpp"

namespace menu7_container = VS::Content::text::menu7_prg::container;

namespace VS::Menu::Files {
namespace {

constexpr int kContainerComparisonActor = 16;
constexpr int kConfirmContainerOpening = 5;
constexpr int kConfirmContainerDiscard = 1;
constexpr int kContainerHeadingX = 320;
constexpr int kContainerHeadingY = 18;
constexpr int kContainerHeadingWidth = 126;
constexpr int kContainerHeadingBackgroundWidth = 8;
constexpr std::int16_t kContainerHeadingTargetX = 180;
constexpr int kContainerInventoryMenu = 14;
constexpr int kContainerHiddenHudOffset = 0x200000;
constexpr int kContainerBackgroundPriority = 2046;
constexpr int kContainerBackgroundAlpha = 128;
constexpr int kContainerRestorePriority = -4;
constexpr int kContainerInformationBox = 7;

char* ContainerString(int offset) {
  return reinterpret_cast<char*>(&containerStrings[offset]);
}

VS::Menu::container::InventoryStorage& Storage() {
  if (VS::Menu::Framework::inventoryStorage == nullptr) {
    VS::Core::Errorf("save", "Container save submenu used without its storage");
    std::abort();
  }
  return *VS::Menu::Framework::inventoryStorage;
}

class LiveContainerSave final : public ContainerSaveHost {
 public:
  void BeginSession() override {
    VS::Menu::PlayOpenSound();
    VS::Menu::Framework::containerEmptyBackup =
        static_cast<char>(VS::Input::settings.container_empty);
    VS::Menu::Framework::inventoryStorage =
        static_cast<VS::Menu::container::InventoryStorage*>(
            VS::Core::Memory::AllocateLiveFromHighEnd(
                sizeof *VS::Menu::Framework::inventoryStorage));
    static_assert(sizeof(VS::Menu::container::InventoryStorage) %
                      sizeof(std::uint32_t) ==
                  0);
    VS::Core::Memory::ZeroWordsBackward(
        std::as_writable_bytes(std::span(&Storage(), 1)),
        static_cast<std::int32_t>(sizeof Storage()));
    VS::Menu::Framework::SetLiveActorComparisonStats(kContainerComparisonActor);
  }

  void OpenInformationBox() override {
    VS::Menu::InitLiveInformationTextBox(InformationHeader::kCaution);
  }

  void ShowPrompt(ContainerPrompt prompt) override {
    const bool opening = prompt == ContainerPrompt::kOpenContainer;
    VS::Menu::Files::UpdateLiveContainerConfirmation(
        opening ? kConfirmContainerOpening : kConfirmContainerDiscard);
    VS::Menu::Framework::SetLiveInformationMessage(
        ContainerString(opening ? menu7_container::offset::kCheckContainer
                                : menu7_container::offset::kCancelWarning));
  }

  int PollPrompt() override {
    return VS::Menu::Files::UpdateLiveContainerConfirmation(0);
  }

  void ShowContainerHeading() override {
    VS::Save::MemoryCard::StepLiveMenuResources(1);
    VS::Menu::Framework::SetLiveBackgroundFade(1);
    VS::Menu::Framework::SetLiveCommandLabel(Framework::MenuCommand::kMenu);
    VS::Menu::Framework::MenuRow* row = VS::Menu::Framework::SetLiveMenuItem(
        0, kContainerHeadingX, kContainerHeadingY, kContainerHeadingWidth,
        kContainerHeadingBackgroundWidth,
        ContainerString(menu7_container::offset::kContainer));
    row->state = VS::Menu::Framework::RowState::kSlideX;
    row->target0 = kContainerHeadingTargetX;
    row->selected = 1;
  }

  bool MenuRowsIdle() override {
    return VS::Menu::Framework::AreLiveMenuRowsIdle() != 0;
  }

  void BeginResourceLoad() override {
    VS::Save::MemoryCard::StepLiveMenuResources(1);
  }

  bool ResourcesReady() override {
    return VS::Save::MemoryCard::StepLiveMenuResources(0) != 0;
  }

  void OpenFilePicker() override {
    VS::Field::LiveSetFieldRenderingEnabled(0);
    VS::Battle::SetLiveHudHidden(1);
    Battle_hudSlideOffset = kContainerHiddenHudOffset;
    VS::Menu::Framework::SetLiveBackgroundPriority(kContainerBackgroundPriority,
                                                   kContainerBackgroundAlpha);
    VS::Menu::Files::ResetLiveElements();
    VS::Game::Save::SelectContainer(1);
  }

  int PollFilePicker() override { return VS::Game::Save::SelectContainer(0); }

  void DrawFiles() override {
    VS::Menu::Files::DrawLiveFrame(VS::Render::frameBuf);
  }

  void OpenContainerScreen() override {
    VS::Menu::container::InventoryStorage& storage = Storage();
    VS::Core::Memory::CopyLiveHalfwordBlocks(&storage.saved_inventory,
                                             &VS::Items::inventory,
                                             sizeof storage.saved_inventory);
    VS::Core::Memory::CopyLiveHalfwordBlocks(
        &storage.saved_inventory_indices, &VS::Items::inventoryIndices,
        sizeof storage.saved_inventory_indices);
    VS::Core::Memory::CopyLiveHalfwordBlocks(&storage.stored_container,
                                             VS::Game::Save::ContainerData(),
                                             sizeof storage.stored_container);
    VS::Save::MemoryCard::ReleaseLiveMenuResources();
    VS::Menu::Framework::menuState.control.return_state =
        VS::Menu::Framework::menuState.control.current_menu;
    VS::Menu::Framework::menuState.control.current_menu =
        kContainerInventoryMenu;
  }

  ContainerScreenResult ContainerScreenOutcome() override {
    switch (VS::Menu::Framework::menuState.control.return_state) {
      case 0:
        return ContainerScreenResult::kRestore;
      case 1:
        return ContainerScreenResult::kConfirmDiscard;
      default:
        return ContainerScreenResult::kSave;
    }
  }

  void BeginWrite() override {
    VS::Menu::container::InventoryStorage& storage = Storage();
    VS::Menu::Files::CopyLiveSelectedContainerItems(
        VS::Game::Save::ContainerData(), &storage.container.items,
        &storage.container.indices);
    UpdateLiveDataSaveMenu(kContainerSaveInit);
  }

  int PollWrite() override { return UpdateLiveDataSaveMenu(0); }

  bool DataNotSaved() override { return dataNotSaved != 0; }

  void RestoreInventory() override {
    VS::Menu::container::InventoryStorage& storage = Storage();
    VS::Input::settings.container_empty =
        VS::Menu::Framework::containerEmptyBackup;
    VS::Core::Memory::CopyLiveHalfwordBlocks(&VS::Items::inventory,
                                             &storage.saved_inventory,
                                             sizeof storage.saved_inventory);
    VS::Core::Memory::CopyLiveHalfwordBlocks(
        &VS::Items::inventoryIndices, &storage.saved_inventory_indices,
        sizeof storage.saved_inventory_indices);
  }

  void CloseFilePicker() override {
    VS::Field::LiveSetFieldRenderingEnabled(1);
    VS::Battle::SetLiveHudHidden(0);
    Battle_hudSlideOffset = 0;
    VS::Menu::Framework::ClearLiveMenuExcept(VS::Menu::Framework::kNoRow);
    VS::Menu::Framework::SetLiveBackgroundPriority(kContainerRestorePriority,
                                                   kContainerBackgroundAlpha);
  }

  void DismissMenu() override {
    VS::Menu::Framework::SetLiveCommandLabel(Framework::MenuCommand::kNone);
    VS::Menu::Framework::DismissLiveInformationBox();
    VS::Menu::Framework::SetLiveBackgroundFade(0);
  }

  void ReleaseResources() override {
    VS::Save::MemoryCard::ReleaseLiveMenuResources();
  }

  bool InformationBoxClosed() override {
    return VS::Menu::DismissLiveTextBox(kContainerInformationBox) == 0 &&
           VS::Menu::Framework::backgroundFadeStep == 0;
  }

  void EndSession() override {
    VS::Core::Memory::FreeLiveFromHighEnd(
        VS::Menu::Framework::inventoryStorage);
    VS::Menu::Framework::inventoryStorage = nullptr;
    VS::Game::Save::ReleaseContainer();
  }

  void ReportInvalidState(unsigned state) override {
    VS::Core::Errorf("save", "Invalid container menu state: %u", state);
  }
};

}  // namespace

int UpdateLiveContainerSave(char& state) {
  auto typed =
      static_cast<ContainerSaveState>(static_cast<std::uint8_t>(state));
  LiveContainerSave host;
  const bool finished = UpdateContainerSave(typed, host);
  state = static_cast<char>(static_cast<std::uint8_t>(typed));
  return finished ? 1 : 0;
}

}  // namespace VS::Menu::Files
