// Isolated linkage doubles only external engine/framework endpoints. The six
// terminal cores/Live adapters, heap, packet queues, delivered components,
// transitions, limb controllers, HUD policies and item-text formatting are
// real.
#include <algorithm>
#include <bit>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iterator>
#include <span>
#include <string_view>

#include "Actors/ActorStateLive.hpp"
#include "Actors/SkeletonLive.hpp"
#include "Content/CdDriveLive.hpp"
#include "Core/Log.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Field/MapLive.hpp"
#include "Field/RoomRenderingLive.hpp"
#include "Items/EquipmentLive.hpp"
#include "Menu/Container/ContainerDetailHelpersLive.hpp"
#include "Menu/Framework/ActorPreviewLive.hpp"
#include "Menu/Framework/EquipmentDetailsLive.hpp"
#include "Menu/Framework/Framework.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Sounds.hpp"
#include "Menu/Status/ActorSwitchingLive.hpp"
#include "Render/CameraViewLive.hpp"
#include "Render/GameFontLive.hpp"

namespace {
constexpr std::uintptr_t kGpuHandleMask = 0xFFFFFFU;
constexpr int kActorIndexMask = 0xF;
}  // namespace

extern "C" {
#include "Actors/Movement.h"
#include "Audio/Seq/Channel.h"
#include "Battle/Actor.h"
#include "Battle/Damage.h"
#include "Battle/Hud.h"
#include "Battle/TargetingState.h"
#include "Core/Arena.h"
#include "Core/Diagnostics.h"
#include "DiscQueue.h"
#include "Input/ControllerLiveState.h"
#include "Menu/Container/ItemView.h"
#include "Menu/Framework/EquipmentDetailsLiveState.h"
#include "Menu/Framework/FrameworkLiveState.h"
#include "Menu/Framework/Menus.h"
#include "Menu/Inventory/ItemList.h"
#include "Menu/Status/ActorStatus.h"
#include "Menu/TextBox.h"
#include "OverlayWorkspaceLiveState.h"
#include "Render/Camera.h"
#include "Render/Display.h"
#include "Render/Frame.h"
#include "Render/Text/VariableFont.h"
#include "Render/UiPrimitives.h"

decltype(Battle_actors) Battle_actors{};
decltype(Actors_actorRecords) Actors_actorRecords{};
decltype(Battle_limbStatusColors) Battle_limbStatusColors{};
decltype(Battle_statusIconTexOffsets) Battle_statusIconTexOffsets{};
decltype(Battle_upperStatusIconFlags) Battle_upperStatusIconFlags{};
char Battle_hudHidden = 0;
char Battle_hudStatusIconsHidden = 0;
char Battle_hudBattleMode = 0;
char Battle_lowerScreenUiState = 0;
char Battle_shortcutHudRequested = 0;
decltype(Battle_limbNames) Battle_limbNames{};
decltype(Battle_statusStrings) Battle_statusStrings{};
Menu_StringContext Battle_stringContext{};
decltype(Menu_Status_statusStrings) Menu_Status_statusStrings{};
decltype(Menu_Status_limbStatuses) Menu_Status_limbStatuses{};
int Menu_Status_selectedActor = 0;
int Menu_Status_previousActor = 0;
u_char Menu_Status_selectedElement = 0;
char Menu_Status_selectedStatRow = 0;
int Menu_Status_animationStep = 0;
int Menu_Status_xPos = 0;
int Menu_Status_yPos = 0;
int Menu_Status_fadeScreen = 0;
int Menu_Status_drawBackgroundFirst = 0;
char Menu_Status_basicStatsAnimDelay = 0;
char Menu_Status_basicStatsAnimState = 0;
char Menu_Status_delayScreenUpdate = 0;
char Menu_Status_initializedActor = 0;
char Menu_Status_renderBasicStatsState = 0;
char Menu_Framework_cursorColor = 0;
char Menu_Framework_equipmentSubtype = 0;
char Menu_Framework_enabledStatPages = 0;
char Menu_Framework_itemStatPage = 0;
char Menu_Inventory_freezeTabArrows = 0;
char* Menu_Framework_stringBuf = nullptr;
Menu_Framework_MenuRow* Menu_Framework_menuItems = nullptr;
int* Menu_Framework_rowTypeBuf = nullptr;
u_short* Menu_Framework_itemsText = nullptr;
char Menu_Framework_submenuStates[kMenuSubmenuStateCount]{};
Menu_Framework_MenuMemory Menu_Framework_menuMemory{};
Menu_Framework_MenuControlState Menu_Framework_menuState{};
decltype(Menu_Framework_rowAnimationSteps) Menu_Framework_rowAnimationSteps{};
decltype(Menu_Framework_animationIndices) Menu_Framework_animationIndices{};
decltype(Menu_Framework_statusViewCursorPositions)
    Menu_Framework_statusViewCursorPositions{};
decltype(Menu_Framework_statusViewNavigationMap)
    Menu_Framework_statusViewNavigationMap{};
int16_t Menu_Framework_equipmentStats[kMenuEquipmentStatCount]{};
int16_t Menu_Framework_strIntAgi[kMenuPrimaryStatCount]{};
int16_t Menu_Framework_comparisonStatDeltas[kMenuComparisonStatCount]{};
int16_t Menu_Framework_comparisonStatTotals[kMenuComparisonStatCount]{};
int16_t Menu_Framework_comparisonAccessoryStats[kMenuComparisonStatCount]{};
Items_InventoryWeapon* Menu_Framework_weapons = nullptr;
Items_InventoryBlade* Menu_Framework_blades = nullptr;
Items_InventoryGrip* Menu_Framework_grips = nullptr;
Items_InventoryShield* Menu_Framework_shields = nullptr;
Items_InventoryArmor* Menu_Framework_armor = nullptr;
Items_InventoryGem* Menu_Framework_gems = nullptr;
Items_Inventory Audio_Seq_unusedInventory{};
Items_Container* Menu_Container_containerData = nullptr;
decltype(Menu_textBoxes) Menu_textBoxes{};
Input_StickPosition Input_stickPosBuf{};
Input_PressedButtonsView Input_buttonsPressed{};
State_Settings Input_settings{};
int Input_buttonsState = 0;
int Input_buttonRepeat = 0;
char Content_overlayWorkspaceOwner = 0;
void* Render_legacyPrimitiveBuffers[2]{};
void* Render_legacyOrderingTables[2]{};
void* Render_legacyCurrentOrderingTable = nullptr;
int Render_frameBuf = 0;
int Render_projectionDistance = 0;
decltype(Render_kStatGaugeColors) Render_kStatGaugeColors{};
decltype(Render_variableFontGlyphWidths) Render_variableFontGlyphWidths{};

void Core_Warnf(const char* channel, const char* format, ...) {
  std::fprintf(stderr, "[%s] ", channel);
  va_list arguments;
  va_start(arguments, format);
  std::vfprintf(stderr, format, arguments);
  va_end(arguments);
  std::fputc('\n', stderr);
}
void Core_Errorf(const char* channel, const char* format, ...) {
  std::fprintf(stderr, "[%s] ", channel);
  va_list arguments;
  va_start(arguments, format);
  std::vfprintf(stderr, format, arguments);
  va_end(arguments);
  std::fputc('\n', stderr);
}
void* Core_GameHeapArena() { std::abort(); }
u_long LegacyGpu_Handle(const void* pointer) {
  return static_cast<u_long>(reinterpret_cast<std::uintptr_t>(pointer) &
                             kGpuHandleMask);
}
void* LegacyGpu_Pointer(u_long) { std::abort(); }
int LoadImage(RECT*, u_long*) { std::abort(); }
int StoreImage(RECT*, u_long*) { std::abort(); }
void DrawPrim(void*) { std::abort(); }
void Core_Gte_SetRotMatrix(const MATRIX*) {}
void Core_Gte_SetTransMatrix(const MATRIX*) {}
}
namespace VS::Core {
bool IsLogEnabled(LogLevel, std::string_view) { return true; }
void LogWrite(LogLevel, std::string_view channel, std::string_view message) {
  std::fprintf(stderr, "[%.*s] %.*s\n", static_cast<int>(channel.size()),
               channel.data(), static_cast<int>(message.size()),
               message.data());
}
}  // namespace VS::Core
namespace VS::Menu {
void PlayChangeSound() {}
void PlayLeaveSound() {}
void PlaySelectSound() {}
void PlayInvalidSound() {}
}  // namespace VS::Menu
namespace VS::Menu::Framework {
MenuRow& LiveMenuRow(int row) {
  if (row < 0 || static_cast<std::size_t>(row) >= kRowCount ||
      Menu_Framework_menuItems == nullptr) {
    std::abort();
  }
  return *reinterpret_cast<MenuRow*>(&Menu_Framework_menuItems[row]);
}
MenuRow& SetLiveMenuItemText(const RowPlacement& placement,
                             std::span<const std::uint8_t> text) {
  auto& row = LiveMenuRow(placement.row);
  row.x = static_cast<std::int16_t>(placement.x);
  row.y = static_cast<std::int16_t>(placement.y);
  row.w = static_cast<std::uint8_t>(placement.w);
  row.background_width = static_cast<std::uint8_t>(placement.background_width);
  if (text.empty()) {
    std::abort();
  }
  return row;
}
Menu_Framework_MenuRow* SetLiveMenuItem(int row, int x, int y, int width,
                                        int background, const char* text) {
  const RowPlacement placement{row, x, y, width, background};
  auto& item = SetLiveMenuItemText(
      placement, {reinterpret_cast<const std::uint8_t*>(text), 1});
  return reinterpret_cast<Menu_Framework_MenuRow*>(&item);
}
void SetLiveCommandLabel(int) {}
void DeactivateLiveRow(int row) {
  LiveMenuRow(row).state = RowState::kInactive;
}
void FlyOutLiveRowRight(int row) { LiveMenuRow(row).state = RowState::kSlideX; }
void FlyOutLiveRowLeft(int row) { LiveMenuRow(row).state = RowState::kSlideX; }
void ClearLiveMenuExcept(int except) {
  for (int row = 0; row < static_cast<int>(kRowCount); ++row) {
    if (row != except) {
      DeactivateLiveRow(row);
    }
  }
}
int AreLiveMenuRowsIdle() {
  for (int row = 0; row < static_cast<int>(kRowCount); ++row) {
    if (static_cast<int>(LiveMenuRow(row).state) > 1) {
      return 0;
    }
  }
  return 1;
}
void CreateLiveMenuRows(int count, int, char*[], int types[]) {
  Menu_Framework_itemsText =
      static_cast<u_short*>(Core::Memory::AllocateLiveFromHighEnd(
          static_cast<std::size_t>(count) * kStringBufferBytes));
  std::copy_n(types, count, Menu_Framework_rowTypeBuf);
}
int CloseLiveRowsAndRememberCursor() { return 0; }
int LiveSelectedRow() { return -1; }
void InitializeLiveInformationBox() {}
void DismissLiveInformationBox() {}
void SetLiveInformationMessageText(std::span<const std::uint8_t>) {}
void DrawLiveButtonIcon(int, int, int, u_long*) {}
void DrawLiveButtonPromptBackground(int, int, int, int) {}
void DrawLiveTabNavigation(int) {}
int DrawLiveCursor(u_int step, int) { return std::bit_cast<int>(step + 1U); }
void SetLiveBackgroundPriority(int, int) {}
void SetLiveBackgroundFade(int) {}
void UpdateLiveEquipmentStatPages(int) {}
void UpdateLivePrimaryStatPanel(int) {}
void UpdateLiveDpPpBars(int) {}
void SetLiveActorComparisonStats(int) {}
void ResetLiveEquipmentStats() {}
void PopulateLiveWeaponStats(Items_WeaponLoadout*) {}
void PopulateLiveShieldStats(Items_ShieldLoadout*) {}
void PopulateLiveArmorStats(Items_ArmorLoadout*) {}
void PopulateLiveAccessoryStats(Items_AccessorySpawnLoadout*) {}
void PopulateLiveBladeStats(Items_WeaponLoadout*) {}
void PopulateLiveGripStats(Items_EquipmentPartStats*) {}
void PopulateLiveGemStats(Items_EquipmentPartStats*) {}
int DrawLiveActorPreview(MATRIX*) { return 0; }
void ReleaseLiveActorPreview() {}
}  // namespace VS::Menu::Framework
namespace VS::Menu::Status {
int LiveAnimateActorSwitch(int request) {
  if (request != 0) {
    Menu_Status_selectedActor = request & kActorIndexMask;
  }
  return 1;
}
void LiveBeginVitalsTransition(int actor) {
  Menu_Status_initializedActor = static_cast<char>(actor);
}
void LiveDrawVitals() {}
}  // namespace VS::Menu::Status
namespace VS::Actors {
int ProjectLiveActorBone(int, int, int*, int) { return -1; }
std::uint32_t LiveActorStateFlags(int slot) {
  return slot >= 0 &&
                 static_cast<std::size_t>(slot) < std::size(Battle_actors) &&
                 Battle_actors[slot] != nullptr
             ? 1U
             : 0U;
}
}  // namespace VS::Actors
namespace VS::Field {
int SetLiveMinimapVisible(int) { std::abort(); }
void LiveSetFieldRenderingEnabled(int) {}
}  // namespace VS::Field
namespace VS::Render {
void BuildLiveCameraViewMatrix() {}
void SetLiveCameraProjectionDistance(int distance) {
  Render_projectionDistance = distance;
}
void DrawLiveCompactTextColor(const char*, int, int, u_long*) {}
void DrawLiveCompactText(const char*, int, void*) {}
}  // namespace VS::Render
namespace VS::Content {
Content_DiscQueueSlot* AllocateLiveDiscRequest(const Content_DiscFile*) {
  std::abort();
}
void EnqueueLiveDiscRequest(Content_DiscQueueSlot*, void*) { std::abort(); }
Content_DiscQueueSlot* CompleteLiveRefusedDiscRequest(const Content_DiscFile*,
                                                      void*) {
  std::abort();
}
void ReleaseLiveDiscRequest(Content_DiscQueueSlot*) { std::abort(); }
}  // namespace VS::Content
namespace VS::Items {
void LoadLiveInventoryWeaponStats(Items_WeaponLoadout*,
                                  const Items_InventoryWeapon*) {
  std::abort();
}
void LoadLiveInventoryShieldStats(Items_ShieldLoadout*,
                                  const Items_InventoryShield*) {
  std::abort();
}
}  // namespace VS::Items
namespace VS::Menu::container {
void BuildLiveWeaponStats(Items_WeaponLoadout*, Items_InventoryWeapon*,
                          Items_ContainerData*) {
  std::abort();
}
void BuildLiveShieldStats(Items_ShieldLoadout*, Items_InventoryShield*,
                          Items_ContainerData*) {
  std::abort();
}
}  // namespace VS::Menu::container
