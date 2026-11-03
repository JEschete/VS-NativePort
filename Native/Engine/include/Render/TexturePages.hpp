#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstdint>

// The VRAM texture pages the engine lends to effects, menus and room
// backgrounds (BATTLE.PRG 146C.c:9480-9549). The first 25 pages stay
// resident; a page above them is reserved by an owner and retired over two
// frame advances, so a page a queued frame may still sample is not lent again
// at once. The original packed state and owner into one byte per page (0 free,
// 0xFF resident, 0xFD retiring, 0xFE retired, any other value an owner), so an
// owner of 0 or 0xFD-0xFF read as a state; here they are kept apart and such
// owners are refused (TexturePagesLive.cpp binds the engine's table).
namespace VS::Render {
inline constexpr std::uint32_t kTexturePageCount = 32;
inline constexpr std::uint32_t kResidentTexturePageCount = 25;
inline constexpr int kMinTexturePageOwner = 1;
inline constexpr int kMaxTexturePageOwner = 0xFC;

enum class TexturePageState : std::uint8_t {
  kFree,
  kResident,
  kOwned,
  kRetiring,
  kRetired,
};

struct TexturePage {
  TexturePageState state = TexturePageState::kFree;
  // The reserving owner while kOwned.
  std::uint8_t owner = 0;

  bool operator==(const TexturePage&) const = default;
};

using TexturePageTable = std::array<TexturePage, kTexturePageCount>;

struct TexturePageRange {
  std::uint32_t first = 0;
  std::uint32_t count = 0;
};

enum class TexturePageResult : std::uint8_t {
  kDone,
  // Not wholly within the lendable pages 25-31; the original's unsigned sum
  // could wrap and pass.
  kOutOfRange,
  kOccupied,
  kOwnerMismatch,
  kInvalidOwner,
};

// BATTLE.PRG func_8007DF48 (146C.c:9480): pages 0-24 resident, the rest free.
void InitializeTexturePages(TexturePageTable& pages) noexcept;

// BATTLE.PRG func_8007DF98 (146C.c:9495): retired pages become free and
// retiring pages retired.
void AdvanceTexturePageRetirements(TexturePageTable& pages) noexcept;

// BATTLE.PRG func_8007DFF0 (146C.c:9509): lends every page of the range to
// the owner when all are free; nothing changes otherwise. An empty range
// within bounds succeeds.
TexturePageResult ReserveTexturePages(TexturePageTable& pages,
                                      TexturePageRange range, int owner);

// BATTLE.PRG func_8007E0A8 (146C.c:9532): starts retiring the range when the
// owner holds every page of it; nothing changes otherwise.
TexturePageResult RetireTexturePages(TexturePageTable& pages,
                                     TexturePageRange range, int owner);
}  // namespace VS::Render
