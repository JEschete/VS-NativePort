#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Render/DisplayControl.hpp"
#include "Render/UiPackets.hpp"

// The event scripts' full-screen GIM images (logos, staff roll, epilogue and
// demo stills): one file of up to three tiled layers, loaded into lendable
// texture pages and drawn each frame with one of four routines. The set is a
// typed value; its file, VRAM uploads, texture pages and random numbers come
// through a GimHost and its packets go to a PacketQueue, so the core reaches
// no global state (GimImagesLive.cpp binds the game's).
namespace VS::Render {
inline constexpr std::size_t kGimLayerCount = 3;
inline constexpr std::size_t kGimScanlineCount = 240;
// gim_t's 254-entry tile table less its four header words (5BF94.c:82).
inline constexpr std::size_t kGimTileCapacity = 250;
inline constexpr std::size_t kGimTileHeaderWords = 4;
inline constexpr int kGimUnitScale = 0x1000;
// scaleAndOrder bit 14 draws the layer in the scene's ordering table rather
// than the overlay's; bits 0-13 are the 4.12 scale.
inline constexpr int kGimSceneOrderFlag = 0x4000;
inline constexpr int kGimScaleMask = 0x3FFF;
inline constexpr int kGimTextureOwner = 6;
inline constexpr std::uint8_t kGimFullBrightness = 0x80;
inline constexpr int kGimImageCount = 64;
// gim_t.texturePageCount is three bits.
inline constexpr std::uint32_t kGimMaxPageCount = 7;
inline constexpr int kGimScreenWidth = 320;
inline constexpr int kGimScreenHeight = 240;
// Each tile is 64x15 texels; a page strip holds 17 tile rows (255 lines).
inline constexpr int kGimTileWidth = 64;
inline constexpr int kGimTileHeight = 15;
inline constexpr int kGimTileRowsPerPage = 17;

enum class GimDrawRoutine : std::uint8_t {
  kTiles = 0,
  kFramebufferFade = 1,
  kScanlineWipe = 2,
  kCrossfade = 3,
};
inline constexpr int kGimDrawRoutineCount = 4;

enum class GimLoadPhase : std::uint8_t {
  // The disc read is in flight.
  kReading = 1,
  // The tiles and palettes are in VRAM; the file is freed next frame.
  kUploaded = 2,
  // The enabled layers draw each frame.
  kReady = 3,
};

// GIM texels: 4-bit through a layer's 16-colour CLUT, or 8-bit.
enum class GimTexelDepth : std::uint8_t { kFourBit = 0, kEightBit = 1 };

// The script-controlled placement of a layer (4A0A8.c:857-905).
struct GimPlacement {
  std::int16_t offset_x = 0;
  std::int16_t offset_y = 0;
  // The 4.12 scale and kGimSceneOrderFlag.
  std::int16_t scale_and_order = kGimUnitScale;
  std::uint8_t brightness = kGimFullBrightness;

  friend bool operator==(const GimPlacement&, const GimPlacement&) = default;
};

struct GimLayer {
  bool draw_enabled = false;
  bool loading = false;
  bool owns_palette = false;
  GimTexelDepth depth = GimTexelDepth::kFourBit;
  GimDrawRoutine routine = GimDrawRoutine::kTiles;
  std::uint8_t columns = 0;
  std::uint8_t rows = 0;
  // A 15-bit colour behind and around the tiles; 0 draws nothing there.
  std::uint16_t backdrop_color = 0;
  GimPlacement placement;
  // Each entry is an atlas cell (bits 0-1 column, 2-6 row, 8+ page); 0 is an
  // empty tile.
  std::array<std::uint16_t, kGimTileCapacity> tiles{};
  // The scanline wipe's per-line widths (func_800C8550).
  std::array<std::uint16_t, kGimScanlineCount> wipe_widths{};
};

struct GimImageSet {
  std::array<GimLayer, kGimLayerCount> layers{};
  std::int16_t image_id = 0;
  std::uint8_t first_page = 0;
  std::uint8_t page_count = 0;
  GimLoadPhase phase = GimLoadPhase::kReading;
  // The file parsed; a refused file leaves the set unable to draw but its
  // phases still advance, so scripts waiting on it continue.
  bool usable = false;
  bool pages_reserved = false;
  // The crossfade's workspace page (the page after the image's), reserved
  // once a crossfade layer is active and the image's page count is known.
  std::optional<std::uint8_t> workspace_page;
};

enum class GimRefusal : std::uint8_t {
  kUnknownImage,
  kNoHeap,
  kNoDiscRequest,
  kLayerOutOfRange,
  kRoutineOutOfRange,
  kTileTableTooLarge,
  kFileTooShort,
  kTooManyPages,
  kPagesUnavailable,
  kNoSet,
  kNoOrderingEntry,
};

// The file, VRAM, texture pages and random numbers the set loads through.
class GimHost {
 public:
  virtual ~GimHost() = default;
  // The disc read has finished (Content_DiscRequestState_Loaded).
  [[nodiscard]] virtual bool FileReadFinished() = 0;
  virtual void ReleaseFileRead() = 0;
  // The file buffer: sectors * 2048 bytes as halfwords.
  [[nodiscard]] virtual std::span<const std::uint16_t> FileWords() = 0;
  virtual void FreeFile() = 0;
  // `pixels` covers the area.
  virtual void UploadImage(const ScreenRect& area,
                           std::span<const std::uint16_t> pixels) = 0;
  // 256 colours into CLUT row 3 from colour 0 (vs_main_loadClut).
  virtual void LoadClutColors(std::span<const std::uint16_t> colors) = 0;
  // True when the pages were lent to kGimTextureOwner.
  [[nodiscard]] virtual bool ReservePages(std::uint32_t first,
                                          std::uint32_t count) = 0;
  virtual void RetirePages(std::uint32_t first, std::uint32_t count) = 0;
  // vs_main_getRandSmall: a random number of `bit_count` bits.
  [[nodiscard]] virtual std::uint32_t RandomBits(int bit_count) = 0;
  virtual void Refuse(GimRefusal refusal) = 0;
};

// BATTLE.PRG vs_battle_loadGim (5BF94.c:2347): a set reading image `image_id`
// into pages from `first_page`; every layer is loading at full brightness and
// unit scale.
[[nodiscard]] GimImageSet MakeGimImageSet(int image_id, int first_page);

// BATTLE.PRG func_800C8550 (5BF94.c:2377): the layer draws from now on; a
// scanline wipe draws fresh random widths and a crossfade reserves the
// workspace page. The original reserved it on every crossfade activation,
// even before the file had set the page count (taking the image's own first
// page); here it is reserved once, after the file is read.
void ActivateGimLayer(GimImageSet& set, std::size_t layer, GimHost& host);

// BATTLE.PRG func_800B7014 (4A0A8.c:857): the script's routine and placement,
// then the activation.
struct GimLayerSetup {
  GimDrawRoutine routine = GimDrawRoutine::kTiles;
  GimPlacement placement;
};
bool ConfigureGimLayer(GimImageSet& set, std::size_t layer,
                       const GimLayerSetup& setup, GimHost& host);

// BATTLE.PRG func_800C86AC (5BF94.c:2422): once ready, the image's pages and
// the workspace page are retired and the set may be freed (true). Before
// then the original did nothing, and nothing is done. Only pages this set
// reserved are retired (the original retired the image's range even when
// its reservation had been refused, and the workspace once per crossfade
// layer).
[[nodiscard]] bool ReleaseGimImageSet(GimImageSet& set, GimHost& host);

// BATTLE.PRG func_800C8778 (5BF94.c:2440) phases 1 and 2: the finished read
// is checked, then uploaded (the whole layout is checked against the file
// before anything is uploaded), then the file is freed. Returns true when
// the set is ready and its enabled layers should be drawn (phase 3).
[[nodiscard]] bool UpdateGimImageSet(GimImageSet& set, GimHost& host);

// What a routine draws with: the layer's CLUT word (getClut << 16), the
// image's pages and the frame being drawn (Render_frameBuf).
struct GimTexture {
  std::uint32_t clut_word = 0;
  std::uint8_t first_page = 0;
  std::uint8_t page_count = 0;
  int framebuffer = 0;
};

// BATTLE.PRG func_800CBBCC (5BF94.c:4056): the tiles, as sprites at unit
// scale and quads otherwise, inside the backdrop colour. A grid past the tile
// table draws nothing and returns false.
bool DrawGimTiles(PacketQueue& queue, const GimLayer& layer,
                  const GimTexture& texture);
// BATTLE.PRG func_800CC128 (5BF94.c:4192): below full brightness, the tiles
// blend over the other framebuffer, drawn dimmed by the remainder.
bool DrawGimFramebufferFade(PacketQueue& queue, const GimLayer& layer,
                            const GimTexture& texture);
// BATTLE.PRG func_800CC204 (5BF94.c:4220): each scanline shows the image to
// its wipe width times the scale; brightness picks the wiped side.
bool DrawGimScanlineWipe(PacketQueue& queue, const GimLayer& layer,
                         const GimTexture& texture);
// BATTLE.PRG func_800CC600 (5BF94.c:4331): each tile column is composed in
// the workspace page and blended with the framebuffer.
bool DrawGimCrossfade(PacketQueue& queue, const GimLayer& layer,
                      const GimTexture& texture);

// BATTLE.PRG func_800CC580 (5BF94.c:4319): draws into the 64-wide workspace
// page from x = `page_x`.
bool SetGimWorkspaceDrawArea(PacketQueue& queue, int page_x);
// BATTLE.PRG func_800CC5C0 (5BF94.c:4325): draws into the 320x240
// framebuffer from x = `page_x`.
bool SetGimFramebufferDrawArea(PacketQueue& queue, int page_x);

// The ordering-table entry a layer draws in: entry 0x7FE - layer from the
// scene table (orderingTable) with kGimSceneOrderFlag, else 6 - 2 * layer
// from the overlay table (5BF94.c:4449).
struct GimOrderingEntry {
  bool scene = false;
  int entry = 0;

  friend bool operator==(const GimOrderingEntry&,
                         const GimOrderingEntry&) = default;
};
[[nodiscard]] GimOrderingEntry GimLayerOrderingEntry(const GimImageSet& set,
                                                     std::size_t layer);

// The layer's CLUT word: its own 16 colours for 4-bit texels, else the
// image's 256-colour palette or the shared row 227 CLUT.
[[nodiscard]] std::uint32_t GimLayerClutWord(const GimImageSet& set,
                                             std::size_t layer);

// BATTLE.PRG func_800CCA90 (5BF94.c:4429): the layer's routine, unless it is
// dark (a scanline wipe draws at any brightness) or the set is unusable.
bool DrawGimLayer(PacketQueue& queue, const GimImageSet& set, std::size_t layer,
                  int framebuffer);
}  // namespace VS::Render
