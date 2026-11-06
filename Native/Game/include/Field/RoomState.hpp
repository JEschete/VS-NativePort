#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

// The room bookkeeping rules (BATTLE.PRG 146C.c:3205, 8619-8912 and
// 15756-15765; INITBTL.PRG 18.c:595): the location table that names each
// zone room, the visited rooms and areas, showing or hiding the loaded room
// and the order a gameplay session sets up its room state in.
namespace VS::Field {
// The zone and room each location id names (146C.c:8899 searches 512).
inline constexpr std::size_t kLocationCount = 512;
// The id of a room the table does not name.
inline constexpr int kUnknownLocationId = 0;

struct LocationDestination {
  std::uint8_t zone_id = 0;
  std::uint8_t room_id = 0;

  friend bool operator==(const LocationDestination&,
                         const LocationDestination&) = default;
};

// decomp: BATTLE.PRG _getLocationId (146C.c:8899)
// The first location naming the room, or kUnknownLocationId.
[[nodiscard]] int FindLocationId(std::span<const LocationDestination> locations,
                                 int zone_id, int room_id);

// The zone and room loaded now (the zone context's zone and map ids).
struct CurrentRoom {
  int zone_id = 0;
  int room_id = 0;
};

// decomp: BATTLE.PRG vs_battle_getCurrentRoomId (146C.c:8912)
// The location id of the loaded room, or kUnknownLocationId.
[[nodiscard]] int CurrentRoomId(std::span<const LocationDestination> locations,
                                const CurrentRoom& room);

// The visited rooms (by location id) and areas (by scene id) as bitsets
// (main.h:394 vs_main_mapStatus_t).
inline constexpr std::size_t kVisitWordBits = 32;
inline constexpr std::size_t kVisitedRoomWords = 16;
inline constexpr std::size_t kVisitedAreaWords = 2;

struct VisitedPlaces {
  std::array<std::uint32_t, kVisitedRoomWords> rooms{};
  std::array<std::uint32_t, kVisitedAreaWords> areas{};

  friend bool operator==(const VisitedPlaces&, const VisitedPlaces&) = default;
};

struct RoomVisitState {
  // Set when a room loads; the room counts as visited once play resumes.
  bool pending = false;
  VisitedPlaces visited{};

  friend bool operator==(const RoomVisitState&,
                         const RoomVisitState&) = default;
};

class RoomVisitHost {
 public:
  RoomVisitHost() = default;
  RoomVisitHost(const RoomVisitHost&) = delete;
  RoomVisitHost& operator=(const RoomVisitHost&) = delete;
  RoomVisitHost(RoomVisitHost&&) = delete;
  RoomVisitHost& operator=(RoomVisitHost&&) = delete;
  virtual ~RoomVisitHost() = default;

  [[nodiscard]] virtual int CurrentSceneId() const = 0;
  [[nodiscard]] virtual int CurrentRoomId() const = 0;
  // A scene or room past the visit records; the original set a bit beyond
  // them.
  virtual void ReportRefusedVisit(const char* record, int id) = 0;
};

// decomp: BATTLE.PRG _setRoomSeen (146C.c:3205)
// Records the current room and its area once a room visit is pending.
void MarkCurrentRoomVisited(RoomVisitState& state, RoomVisitHost& host);
// decomp: BATTLE.PRG func_8007C4D4 (146C.c:8803)
void CancelPendingRoomVisit(RoomVisitState& state);

// The loaded room's display (Field_roomData isLoaded, displayFlag and
// roomIndex).
inline constexpr int kPrimaryRoomOwner = 0;

struct RoomDisplay {
  bool loaded = false;
  bool displayed = false;
  int owner_room_index = 0;

  friend bool operator==(const RoomDisplay&, const RoomDisplay&) = default;
};

// decomp: BATTLE.PRG func_8008AB80 (146C.c:15756)
// Shows the room when it is loaded and `owner_room_index` owns it; true then.
bool EnableOwnedRoomRendering(RoomDisplay& display, int owner_room_index);
// decomp: BATTLE.PRG func_8008ABB8 (146C.c:15765)
bool DisableOwnedRoomRendering(RoomDisplay& display, int owner_room_index);
// decomp: BATTLE.PRG func_8007BF2C (146C.c:8619)
// Shows the primary room's geometry; true when it owns the loaded room.
bool ShowPrimaryRoomGeometry(RoomDisplay& display);
// decomp: BATTLE.PRG func_8007BF4C (146C.c:8621)
bool HidePrimaryRoomGeometry(RoomDisplay& display);

// The records a gameplay session clears, the low-heap buffers it takes (in
// heap order) and the flags it starts with.
enum class RoomRecord : std::uint8_t {
  kBackgroundReferenceVectors,
  kRoomData,
  kEntranceDoor,
  kUnusedMenuCursorMemory,
  kManualDisplay,
};
enum class RoomBuffer : std::uint8_t {
  kRoomPalettes,
  kFloatingLabels,
  kPlacedLabelPositions,
};

inline constexpr std::array kClearedRoomRecords{
    RoomRecord::kBackgroundReferenceVectors,
    RoomRecord::kRoomData,
    RoomRecord::kEntranceDoor,
    RoomRecord::kUnusedMenuCursorMemory,
    RoomRecord::kManualDisplay,
};
inline constexpr std::array kRoomBuffers{
    RoomBuffer::kRoomPalettes,
    RoomBuffer::kFloatingLabels,
    RoomBuffer::kPlacedLabelPositions,
};

struct RoomRuntimeDefaults {
  int entered_door_id = 0;
  bool room_updates_paused = false;
  bool room_tint_active = false;
  bool minimap_visible = false;
  int minimap_slide_frame = 0;

  friend bool operator==(const RoomRuntimeDefaults&,
                         const RoomRuntimeDefaults&) = default;
};

inline constexpr RoomRuntimeDefaults kRoomRuntimeDefaults{
    .entered_door_id = -1,
    .room_updates_paused = false,
    .room_tint_active = false,
    .minimap_visible = true,
    .minimap_slide_frame = 8,
};

class RoomRuntimeHost {
 public:
  RoomRuntimeHost() = default;
  RoomRuntimeHost(const RoomRuntimeHost&) = delete;
  RoomRuntimeHost& operator=(const RoomRuntimeHost&) = delete;
  RoomRuntimeHost(RoomRuntimeHost&&) = delete;
  RoomRuntimeHost& operator=(RoomRuntimeHost&&) = delete;
  virtual ~RoomRuntimeHost() = default;

  virtual void Clear(RoomRecord record) = 0;
  virtual void Allocate(RoomBuffer buffer) = 0;
  // Also forgets the unused submenu state and scene buffer.
  virtual void ApplyDefaults(const RoomRuntimeDefaults& defaults) = 0;
  virtual void InitializeRoomBackground() = 0;
  virtual void ResetGimmickAnimationQueue() = 0;
  virtual void ApplyBackgroundReferenceColors() = 0;
};

// decomp: INITBTL.PRG func_800FA6B8 (18.c:595)
void InitializeRoomRuntimeState(
    RoomRuntimeHost& host,
    const RoomRuntimeDefaults& defaults = kRoomRuntimeDefaults);
}  // namespace VS::Field
