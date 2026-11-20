#pragma once
// Hand-maintained (tools/port/owned.csv).
// The memory card save file's on-card layout: the 23-block game save image
// and the records the file menu reads from it. The byte offsets the typed
// codec uses are in Save/MemoryCard/GameFiles.hpp; GameFilesLive.cpp checks
// them against these layouts.

#include <cstddef>
#include <cstdint>

#include "Field/Mpd.hpp"
#include "Input/Settings.hpp"
#include "Items/ItemRecords.hpp"
#include "Save/MemoryCard/GameFiles.hpp"
#include "Save/Snapshot.hpp"
#include "State/Ashley.hpp"
#include "State/GameClock.hpp"
#include "State/Progress.hpp"

namespace VS::Save {

inline constexpr std::size_t kSaveSlotBaseBytes = 0xC;
inline constexpr std::size_t kSaveCipherHeaderBytes = 0x10;
inline constexpr std::size_t kCardTitleTailChars = 16;
inline constexpr std::size_t kCardHeaderReservedBytes = 0x1C;
inline constexpr std::size_t kIconHeaderBytes = 0x20;
inline constexpr std::size_t kBlockChecksumBytes = 0x5C;
inline constexpr std::size_t kStateFlagBytes = 0x440;
inline constexpr std::size_t kMapStatusBytes = 0x48;
inline constexpr std::size_t kRecentRoomEnemyStateBytes = 0x520;
inline constexpr std::size_t kImageTailReservedBytes = 0x220;

// decomp: slotState_e (src/MENU/MENU7.PRG/260.c:21), upstream
// src/SLUS_010.40/saveData.h:10-15
enum class CardFileSlotStatus : std::int32_t {
  kUnavailable = 0,
  kAvailable = 1,
  kTemporary = 2,
  kInUse = 3,
};

// decomp: saveBase_t (src/MENU/MENU7.PRG/260.c:49), upstream SaveBase
// (src/SLUS_010.40/saveData.h:20-24)
struct SaveSlotBase {
  CardFileSlotStatus slot_state;
  std::uint32_t generation;
  // Upstream `version`: the save format revision.
  std::int32_t format_signature;
};
static_assert(sizeof(SaveSlotBase) == kSaveSlotBaseBytes,
              "SaveSlotBase must keep its PS1 layout");

// decomp: stats_t (src/MENU/MENU7.PRG/260.c:63), upstream Stats
// (src/SLUS_010.40/saveData.h:26-38)
struct SaveFileStats {
  VS::State::PlayTime game_time;
  std::uint16_t cumulative_save_count;
  std::uint16_t current_game_save_count;
  std::uint16_t current_hp;
  std::uint16_t max_hp;
  char save_location;
  char clear_count;
  char map_completion;
  char reserved_f;
  std::uint16_t current_mp;
  std::uint16_t max_mp;
};

// decomp: saveFileSubInfo_t (src/MENU/MENU7.PRG/260.c:69)
struct SaveFileSummary {
  SaveSlotBase base;
  SaveFileStats stats;
  std::uint16_t card_title_tail[kCardTitleTailChars];
};

// decomp: saveFileInfo_t (src/MENU/MENU7.PRG/260.c:76), the leading
// 0x80 bytes of upstream SaveTitleFrame (src/SLUS_010.40/saveData.h:40-48)
struct CardFileInfoRecord {
  std::uint32_t key;
  SaveFileSummary summary;
  char reserved_44[kCardHeaderReservedBytes];
  char icon_header_bytes[kIconHeaderBytes];
};

// decomp: savedata_unk180_2_t (src/MENU/MENU7.PRG/260.c:81), upstream
// vs_SaveIdentity (src/SLUS_010.40/saveData.h:50-53)
struct SaveCipherHeader {
  std::int32_t cipher_key;
  SaveSlotBase slot_header;
};
static_assert(sizeof(SaveCipherHeader) == kSaveCipherHeaderBytes,
              "SaveCipherHeader must keep its PS1 layout");

// decomp: savedata_unk180_t (src/MENU/MENU7.PRG/260.c:87), upstream
// vs_SaveInfoFrame (src/SLUS_010.40/saveData.h:55-59)
struct SaveProtectedHeader {
  SaveCipherHeader cipher_header;
  SaveFileStats summary;
  char block_checksums[kBlockChecksumBytes];
};

// decomp: savedata_t (src/MENU/MENU7.PRG/260.c:108), upstream vs_Savedata
// (src/SLUS_010.40/saveData.h:61-80)
struct GameSaveImage {
  CardFileInfoRecord card_header;
  char icon_bitmap[MemoryCard::kIconBitmapBytes];
  char reserved_100[MemoryCard::kRecordBytes];
  SaveProtectedHeader protected_header;
  char state_flags[kStateFlagBytes];
  char learned_actions[kLearnedActionBytes];
  char map_status[kMapStatusBytes];
  VS::Input::Settings settings;
  VS::State::AshleySnapshot ashley_snapshot;
  VS::Items::InventoryContents inventory;
  VS::Items::InventoryIndices inventory_indices;
  VS::Field::MpdDoorRecord entry_door;
  VS::State::ScoreData score_data;
  std::int32_t room_state_slot;
  char recent_room_enemy_states[kRecentRoomEnemyStateBytes];
  VS::State::ArtsStatus arts_status;
  VS::Items::ContainerData container_data;
  char reserved_59e0[kImageTailReservedBytes];
};

}  // namespace VS::Save
