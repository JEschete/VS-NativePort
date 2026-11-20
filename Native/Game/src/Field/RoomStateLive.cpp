// Hand-maintained (tools/port/owned.csv).
#include "Field/RoomStateLive.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <span>

#include "Battle/Feedback.hpp"
#include "Battle/StatusEffects.hpp"
#include "Core/Log.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Core/Memory/WordCopyLive.hpp"
#include "Field/Background.hpp"
#include "Field/BackgroundLive.hpp"
#include "Field/BackgroundLiveState.hpp"
#include "Field/Doors.hpp"
#include "Field/GimmickAnimationQueueLive.hpp"
#include "Field/Gimmicks.hpp"
#include "Field/Locations.hpp"
#include "Field/Manual.hpp"
#include "Field/Minimap.hpp"
#include "Field/Mpd.hpp"
#include "Field/Room.hpp"
#include "Field/RoomLoadingLive.hpp"
#include "Field/RoomLoadingLiveState.hpp"
#include "Field/RoomRenderingLiveState.hpp"
#include "Field/RoomStateLiveState.hpp"
#include "Field/Zone.hpp"
#include "Legacy/Psx.hpp"
#include "Menu/MapTerminalLive.hpp"
#include "Menu/Score/ScoreScreen.hpp"
#include "State/Progress.hpp"

using VS::Core::Gte::DVECTOR;

// decomp: D_800F19A0 (D_800F19A0@BATTLE/BATTLE.PRG)
int VS::Field::isRoomVisitPending;

namespace VS::Field {
namespace {
constexpr const char* kChannel = "field";
// The room's palette copy: 64 CLUTs of 16 colours (18.c:595 0x800).
constexpr std::size_t kRoomPaletteBytes = 0x800;
// A screen position for each of the 32 floating labels (18.c:595 0x80).
constexpr std::size_t kPlacedLabelPositionBytes = 0x80;

VisitedPlaces LoadVisitedPlaces() {
  VisitedPlaces visited;
  for (std::size_t word = 0; word < visited.rooms.size(); ++word) {
    visited.rooms.at(word) = static_cast<std::uint32_t>(
        VS::State::roomVisitProgress.room_flags[word]);
  }
  for (std::size_t word = 0; word < visited.areas.size(); ++word) {
    visited.areas.at(word) = static_cast<std::uint32_t>(
        VS::State::roomVisitProgress.area_flags[word]);
  }
  return visited;
}

void StoreVisitedPlaces(const VisitedPlaces& visited) {
  for (std::size_t word = 0; word < visited.rooms.size(); ++word) {
    VS::State::roomVisitProgress.room_flags[word] =
        static_cast<int>(visited.rooms.at(word));
  }
  for (std::size_t word = 0; word < visited.areas.size(); ++word) {
    VS::State::roomVisitProgress.area_flags[word] =
        static_cast<int>(visited.areas.at(word));
  }
}

RoomVisitState LoadRoomVisit() {
  return RoomVisitState{.pending = isRoomVisitPending != 0,
                        .visited = LoadVisitedPlaces()};
}

void StoreRoomVisit(const RoomVisitState& state) {
  isRoomVisitPending = state.pending ? 1 : 0;
  StoreVisitedPlaces(state.visited);
}

static_assert(sizeof(VS::State::roomVisitProgress.room_flags) ==
              kVisitedRoomWords * sizeof(std::uint32_t));
static_assert(sizeof(VS::State::roomVisitProgress.area_flags) ==
              kVisitedAreaWords * sizeof(std::uint32_t));

class LiveRoomVisitHost final : public RoomVisitHost {
 public:
  [[nodiscard]] int CurrentSceneId() const override {
    return VS::Field::LiveCurrentSceneId();
  }
  [[nodiscard]] int CurrentRoomId() const override {
    return LiveCurrentRoomId();
  }
  void ReportRefusedVisit(const char* record, int id) override {
    VS::Core::LogWarn(kChannel, "visited {} {} outside the visit records",
                      record, id);
  }
};

RoomDisplay LoadRoomDisplay() {
  return RoomDisplay{.loaded = VS::Field::roomData.is_loaded == 1,
                     .displayed = VS::Field::roomData.display_flag != 0,
                     .owner_room_index = VS::Field::roomData.room_index};
}

// The display flag is written only when the owner check passed (146C.c:15756
// stores 1 or 0 then); the room keeps its load state and owner.
void StoreRoomDisplayed(const RoomDisplay& display) {
  VS::Field::roomData.display_flag =
      static_cast<char>(display.displayed ? 1 : 0);
}

template <typename Rule>
bool RunRoomDisplay(Rule&& rule) {
  RoomDisplay display = LoadRoomDisplay();
  const bool owned = rule(display);
  if (owned) {
    StoreRoomDisplayed(display);
  }
  return owned;
}

class LiveRoomRuntimeHost final : public RoomRuntimeHost {
 public:
  void Clear(RoomRecord record) override {
    namespace Memory = Core::Memory;
    switch (record) {
      case RoomRecord::kBackgroundReferenceVectors:
        Memory::ZeroLiveAlignedWords(backgroundReferenceVectors,
                                     sizeof backgroundReferenceVectors);
        return;
      case RoomRecord::kRoomData:
        Memory::ZeroLiveAlignedWords(&VS::Field::roomData,
                                     sizeof VS::Field::roomData);
        return;
      case RoomRecord::kEntranceDoor:
        Memory::ZeroLiveAlignedWords(&VS::Field::entranceDoorRecord,
                                     sizeof VS::Field::entranceDoorRecord);
        return;
      case RoomRecord::kUnusedMenuCursorMemory:
        Memory::ZeroLiveAlignedWords(&VS::Battle::menu9CursorMemory,
                                     sizeof VS::Battle::menu9CursorMemory);
        return;
      case RoomRecord::kManualDisplay:
        Memory::ZeroLiveAlignedWords(&manualDisplayState,
                                     sizeof manualDisplayState);
        return;
    }
  }
  void Allocate(RoomBuffer buffer) override {
    namespace Memory = Core::Memory;
    switch (buffer) {
      case RoomBuffer::kRoomPalettes:
        roomPalettes = static_cast<std::uint16_t*>(
            Memory::AllocateLiveFromLowEnd(kRoomPaletteBytes));
        return;
      case RoomBuffer::kFloatingLabels:
        VS::Battle::floatingLabels =
            static_cast<decltype(VS::Battle::floatingLabels)>(
                Memory::AllocateLiveFromLowEnd(
                    sizeof *VS::Battle::floatingLabels));
        return;
      case RoomBuffer::kPlacedLabelPositions:
        VS::Battle::placedLabelPositions = static_cast<DVECTOR*>(
            Memory::AllocateLiveFromLowEnd(kPlacedLabelPositionBytes));
        return;
    }
  }
  void ApplyDefaults(const RoomRuntimeDefaults& defaults) override {
    VS::Field::enteredDoorId =
        static_cast<std::int16_t>(defaults.entered_door_id);
    areRoomUpdatesPaused =
        static_cast<std::uint16_t>(defaults.room_updates_paused ? 1 : 0);
    roomTint.is_active =
        static_cast<std::int16_t>(defaults.room_tint_active ? 1 : 0);
    VS::Battle::submenuState = 0;
    VS::Battle::sceneBuffer = nullptr;
    isMinimapVisible =
        static_cast<std::uint8_t>(defaults.minimap_visible ? 1 : 0);
    minimapSlideFrame = static_cast<std::int16_t>(defaults.minimap_slide_frame);
  }
  void InitializeRoomBackground() override { LiveInitializeRoomBackground(); }
  void ResetGimmickAnimationQueue() override {
    ResetGimmickAnimationQueueEntry();
  }
  // The original hands the cleared reference vectors' first five words to
  // the background as its gradient colours (18.c:595).
  void ApplyBackgroundReferenceColors() override {
    const auto& [first, second] = backgroundReferenceVectors;
    const GradientColors colors{
        static_cast<std::int32_t>(first.vx),
        static_cast<std::int32_t>(first.vy),
        static_cast<std::int32_t>(first.vz),
        static_cast<std::int32_t>(first.pad),
        static_cast<std::int32_t>(second.vx),
    };
    LiveSetRoomBackgroundColors(colors.data());
  }
};
}  // namespace

std::array<LocationDestination, kLocationCount> LiveLocationTable() {
  return locationDestinations;
}

void LiveMarkCurrentRoomVisited() {
  RoomVisitState state = LoadRoomVisit();
  LiveRoomVisitHost host;
  MarkCurrentRoomVisited(state, host);
  StoreRoomVisit(state);
}

void LiveCancelPendingRoomVisit() {
  RoomVisitState state = LoadRoomVisit();
  CancelPendingRoomVisit(state);
  StoreRoomVisit(state);
}

int LiveFindLocationId(int zone_id, int room_id) {
  const std::array<LocationDestination, kLocationCount> table =
      LiveLocationTable();
  return FindLocationId(table, zone_id, room_id);
}

int LiveCurrentRoomId() {
  const std::array<LocationDestination, kLocationCount> table =
      LiveLocationTable();
  return CurrentRoomId(table, CurrentRoom{.zone_id = zoneContext.zone_id,
                                          .room_id = zoneContext.room_id});
}

int LiveEnableOwnedRoomRendering(int owner_room_index) {
  return RunRoomDisplay([owner_room_index](RoomDisplay& display) {
    return EnableOwnedRoomRendering(display, owner_room_index);
  })
             ? 1
             : 0;
}

int LiveDisableOwnedRoomRendering(int owner_room_index) {
  return RunRoomDisplay([owner_room_index](RoomDisplay& display) {
    return DisableOwnedRoomRendering(display, owner_room_index);
  })
             ? 1
             : 0;
}

void LiveShowPrimaryRoomGeometry() {
  static_cast<void>(RunRoomDisplay(ShowPrimaryRoomGeometry));
}

void LiveHidePrimaryRoomGeometry() {
  static_cast<void>(RunRoomDisplay(HidePrimaryRoomGeometry));
}

void LiveInitializeRoomRuntimeState() {
  if (!Menu::map::DrainMapTerminalLive()) {
    VS::Core::LogError(kChannel,
                       "map cleanup refused before room runtime reset");
    std::abort();
  }
  LiveRoomRuntimeHost host;
  InitializeRoomRuntimeState(host);
}
}  // namespace VS::Field
