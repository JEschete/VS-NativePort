#pragma once
// Hand-maintained (tools/port/owned.csv).
// The memory card menu's resources: MCDATA.BIN's palettes and icons, then
// MCMAN.BIN's text table, the card's file headers and the directory entry
// (Save/MenuAssetsLive.hpp loads them).

#include <cstddef>
#include <cstdint>

#include "Legacy/Psx.h"
#include "Save/MemoryCard/GameFiles.hpp"
#include "Save/MemoryCard/SaveImage.hpp"

namespace VS::Save {

inline constexpr std::size_t kCardPaletteColors = 256;
inline constexpr std::size_t kCardFileIcons = 7;
inline constexpr std::size_t kCardReservedIconRows = 8;
inline constexpr std::size_t kCardReservedTailBytes = 32;
inline constexpr std::size_t kCardTextTableHalfwords = 0x800;

// decomp: mcdata_t (src/MENU/MENU7.PRG/260.c:162), upstream vs_Mcdata
// (src/SLUS_010.40/saveData.h:124-131) for the MCDATA.BIN part
struct CardUiResources {
  std::uint16_t location_palettes[2][kCardPaletteColors];
  char file_icon_palettes[kCardFileIcons][MemoryCard::kIconPaletteBytes];
  char file_icon_bitmaps[kCardFileIcons][MemoryCard::kIconBitmapBytes];
  char reserved_860[kCardFileIcons][MemoryCard::kRecordBytes];
  char reserved_be0[kCardReservedIconRows][MemoryCard::kRecordBytes];
  char reserved_fe0[kCardReservedTailBytes];
  std::uint16_t text_table[kCardTextTableHalfwords];
  CardFileInfoRecord file_headers[MemoryCard::kSlotCount];
  DIRENTRY directory_entry;
};

}  // namespace VS::Save

namespace VS::Save::MemoryCard {
// decomp: _mcData (src/TITLE/TITLE.PRG/22C.c), defined in
// Save/MenuAssetsLive.cpp
extern CardUiResources* uiResources;
}  // namespace VS::Save::MemoryCard
