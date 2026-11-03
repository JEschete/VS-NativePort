#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstdint>
#include <optional>
#include <span>

#include "Render/DisplayControl.hpp"

// An actor's animated texture cells (BATTLE.PRG 40564.c). Two keyed channels
// copy one cell of the actor's texture sheet over a region of its drawn
// texture, scripts copy single cells, and four regions scroll inside their
// bounds. The core turns the actor's texture state into VRAM rectangle moves;
// TextureAnimationLive.cpp binds the actor records and queues the moves.
namespace VS::Actors {
inline constexpr int kTextureAnimationChannels = 2;
inline constexpr int kTextureScrollLayers = 4;
inline constexpr int kTextureOverlayCount = 8;
// The record keeps a channel's timeline in four bits.
inline constexpr unsigned kTextureTimelineMask = 0xF;

// One optional value per channel or scroll layer.
template <typename T, int Count>
using OptionalEach = std::array<std::optional<T>, Count>;

// One key of a channel's timeline. A zero frame ends the timeline.
struct TextureAnimationKey {
  std::uint8_t frame = 0;
  std::uint8_t duration = 0;

  friend bool operator==(const TextureAnimationKey&,
                         const TextureAnimationKey&) = default;
};

enum class TextureAnimationMode : std::uint8_t {
  // Plays until the timeline shows frame 1, then clears the timeline.
  kStopping = 0,
  kPlaying = 1,
  kPaused = 2,
};

struct TextureAnimationSlot {
  // 0 is no timeline.
  std::uint8_t timeline = 0;
  TextureAnimationMode mode = TextureAnimationMode::kStopping;
  // The shown frame, 1-based; 0 before the first key.
  std::uint8_t current_frame = 0;
  std::uint8_t key_index = 0;
  std::uint8_t ticks = 0;

  friend bool operator==(const TextureAnimationSlot&,
                         const TextureAnimationSlot&) = default;
};

enum class TextureAnimationCommand : std::uint8_t {
  kStop = 0,
  kPlay = 1,
  kTogglePause = 2,
};

// BATTLE.PRG func_800AAD4C (40564.c:1301). Playing timeline 0 is ignored, and
// playing the timeline already playing keeps its place. Channel 1 starts its
// tick count at ten per actor id, so actors sharing a timeline stay out of
// step; the count keeps its low byte.
void SetTextureAnimation(TextureAnimationSlot& slot, int channel, int actor_id,
                         int timeline, TextureAnimationCommand command);

// The keys of each channel's timelines. Channel 0 timelines are 28 keys apart
// and channel 1 timelines 8 apart; a timeline with no zero frame runs on into
// the next.
class TextureAnimationKeys {
 public:
  virtual ~TextureAnimationKeys() = default;
  [[nodiscard]] virtual TextureAnimationKey Key(int channel, int timeline,
                                                int index) const = 0;
};

// The frame index (0-based) each channel copies this frame.
using TextureAnimationFrames = OptionalEach<int, kTextureAnimationChannels>;

// BATTLE.PRG func_800AAE9C (40564.c:1357). Steps each channel's timeline one
// tick and returns the frames to copy, channel 0 first. A key with a zero
// frame where a frame is due ends the whole update, so a later channel neither
// steps nor copies.
[[nodiscard]] TextureAnimationFrames StepTextureAnimations(
    std::span<TextureAnimationSlot, kTextureAnimationChannels> slots,
    const TextureAnimationKeys& keys);

// A region of the actor's drawn texture, in texels.
struct TextureOverlay {
  std::uint8_t x = 0;
  std::uint8_t y = 0;
  std::uint8_t width = 0;
  std::uint8_t height = 0;

  friend bool operator==(const TextureOverlay&,
                         const TextureOverlay&) = default;
};

// A channel's cell sheet: the first cell's texel position, the 1-based overlay
// the cells cover and the cells per sheet row.
struct TextureCellSheet {
  std::uint8_t overlay = 0;
  std::uint8_t source_x = 0;
  std::uint8_t source_y = 0;
  std::uint8_t columns = 0;

  friend bool operator==(const TextureCellSheet&,
                         const TextureCellSheet&) = default;
};

// The cell origin a channel falls back on when its sheet has no columns.
struct TextureCellOrigin {
  std::uint8_t x = 0;
  std::uint8_t y = 0;
  std::uint8_t columns = 0;
};

// The 1-based overlays the fallback sheets cover, by channel.
inline constexpr std::array<std::uint8_t, 2> kFallbackSheetOverlays{5, 7};
static_assert(kFallbackSheetOverlays.size() == kTextureAnimationChannels);

// The channel's sheet, the fallback from its cell origin when the sheet has no
// columns, or none when neither has columns.
[[nodiscard]] std::optional<TextureCellSheet> ChannelCellSheet(
    const TextureCellSheet& sheet, const TextureCellOrigin& origin,
    int channel);

// Texels per VRAM halfword: four for 4-bit textures, two for 8-bit ones.
enum class TextureDepth : std::uint8_t { k4Bit, k8Bit };

// Where the actor's texture lives in VRAM.
struct ActorTextureLayout {
  TextureDepth depth = TextureDepth::k4Bit;
  // The sheet's texture page column (the shared bank's page & 15) and its row
  // offset in the actor's bank.
  int sheet_page = 0;
  int sheet_row = 0;
  // The drawn texture's slot: 64-halfword columns, 256-line pages and 64-line
  // rows.
  int slot_x = 0;
  int slot_y = 0;
  int slot_row = 0;
};

// A VRAM-to-VRAM copy of `source` to (x, y).
struct TextureMove {
  Render::ScreenRect source;
  std::int32_t x = 0;
  std::int32_t y = 0;

  friend bool operator==(const TextureMove&, const TextureMove&) = default;
};

// BATTLE.PRG func_800AB098 (40564.c:1417). Frame `frame` of the sheet copied
// over its overlay. The sheet must have columns.
[[nodiscard]] TextureMove TextureFrameMove(const TextureCellSheet& sheet,
                                           const TextureOverlay& overlay,
                                           const ActorTextureLayout& layout,
                                           int frame);

// A scripted cell copy: the 0-based overlay and the cell's texel position.
struct TextureOverlayCopy {
  std::uint8_t overlay = 0;
  std::uint8_t source_x = 0;
  std::uint8_t source_y = 0;
};

// BATTLE.PRG func_800AB358 (40564.c:1502). The cell at (source_x, source_y)
// copied over the overlay.
[[nodiscard]] TextureMove TextureOverlayMove(int source_x, int source_y,
                                             const TextureOverlay& overlay,
                                             const ActorTextureLayout& layout);

struct TextureScrollBounds {
  std::uint8_t x = 0;
  std::uint8_t y = 0;
  // 0 is no scrolling.
  std::uint8_t width = 0;
  std::uint8_t height = 0;
  // Reverse at the bounds rather than snap back to the start.
  bool bounces = false;
};

struct TextureScrollState {
  std::uint8_t x = 0;
  std::uint8_t y = 0;
  std::int8_t step_x = 0;
  std::int8_t step_y = 0;
  std::uint8_t interval = 0;
  std::uint8_t timer = 0;

  friend bool operator==(const TextureScrollState&,
                         const TextureScrollState&) = default;
};

// An actor's scrolling regions; layer i scrolls overlay i.
struct TextureScrolling {
  std::array<TextureScrollBounds, kTextureScrollLayers> bounds{};
  std::array<TextureScrollState, kTextureScrollLayers> states{};
  std::array<TextureOverlay, kTextureScrollLayers> overlays{};
};

// The scroll's carried height when it matches no bounds width.
inline constexpr int kNoCarriedScrollHeight = -1;

using TextureScrollMoves = OptionalEach<TextureMove, kTextureScrollLayers>;

// BATTLE.PRG func_800AB4F0 (40564.c:1552). Advances each layer's timer by half
// the tick speed; a layer past its interval steps its window and copies it
// over its overlay. The original kept the last stepped overlay's height in a
// register and skipped a due layer whose bounds width equals it. Before any
// layer steps, that register held the caller's value: `carried_height`.
[[nodiscard]] TextureScrollMoves StepTextureScrolling(
    TextureScrolling& scrolling, const ActorTextureLayout& layout,
    int tick_speed, int carried_height);
}  // namespace VS::Actors
