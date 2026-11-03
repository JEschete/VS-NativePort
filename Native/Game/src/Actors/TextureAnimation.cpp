// Hand-maintained (tools/port/owned.csv).
#include "Actors/TextureAnimation.hpp"  // NOLINT(build/include): native C++ interface.

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Render/DisplayControl.hpp"

namespace VS::Actors {
namespace {
// VRAM halfword columns per texture page and lines per page and row.
constexpr int kPageColumns = 64;
constexpr int kPageLines = 256;
constexpr int kRowLines = 64;
// The sheets sit in the texture pages' lower half.
constexpr int kSheetLines = 256;
// Texels in one VRAM halfword at each depth.
constexpr int k8BitTexelsPerHalfword = 2;
constexpr int k4BitTexelsPerHalfword = 4;
// Channel 1 staggers its first key by ten ticks per actor id.
constexpr int kChannel1TicksPerActor = 10;

[[nodiscard]] int TexelsPerHalfword(TextureDepth depth) {
  return depth == TextureDepth::k8Bit ? k8BitTexelsPerHalfword
                                      : k4BitTexelsPerHalfword;
}

[[nodiscard]] Render::ScreenRect SheetRect(int x, int y, int width,
                                           int height) {
  return {.x = static_cast<std::int16_t>(x),
          .y = static_cast<std::int16_t>(y),
          .width = static_cast<std::int16_t>(width),
          .height = static_cast<std::int16_t>(height)};
}

[[nodiscard]] TextureMove SheetCellMove(int source_x, int source_y,
                                        const TextureOverlay& overlay,
                                        const ActorTextureLayout& layout) {
  const int divisor = TexelsPerHalfword(layout.depth);
  return {.source = SheetRect((layout.sheet_page * kPageColumns) + source_x,
                              source_y + kSheetLines, overlay.width / divisor,
                              overlay.height),
          .x = (overlay.x / divisor) + (layout.slot_x * kPageColumns),
          .y = (layout.slot_y * kPageLines) + (layout.slot_row * kRowLines) +
               overlay.y};
}

// Steps a playing channel one tick. False when the due key has no frame.
[[nodiscard]] bool StepChannel(TextureAnimationSlot& slot, int channel,
                               const TextureAnimationKeys& keys) {
  TextureAnimationKey key = keys.Key(channel, slot.timeline, slot.key_index);
  ++slot.ticks;
  if (key.duration < slot.ticks) {
    slot.ticks = 0;
    // The original stepped its key pointer past the byte index's wrap.
    const int next = slot.key_index + 1;
    slot.key_index = static_cast<std::uint8_t>(next);
    key = keys.Key(channel, slot.timeline, next);
    if (key.frame == 0) {
      slot.key_index = 0;
      key = keys.Key(channel, slot.timeline, 0);
    }
  }
  if (key.frame == 0) {
    return false;
  }
  slot.current_frame = key.frame;
  return true;
}

// Steps one axis of a scroll window; true when the step reversed.
[[nodiscard]] int StepScrollAxis(int position, std::int8_t& step, int size,
                                 int minimum, int extent, bool bounces) {
  const int moved = position + step;
  if (minimum + extent < moved + size || moved < minimum) {
    if (!bounces) {
      return minimum;
    }
    const int reversed = moved - (step * 2);
    step = static_cast<std::int8_t>(-step);
    return reversed;
  }
  return moved;
}
}  // namespace

void SetTextureAnimation(TextureAnimationSlot& slot, int channel, int actor_id,
                         int timeline, TextureAnimationCommand command) {
  switch (command) {
    case TextureAnimationCommand::kStop:
      slot.mode = TextureAnimationMode::kStopping;
      break;
    case TextureAnimationCommand::kPlay:
      if (timeline == 0 || (slot.mode == TextureAnimationMode::kPlaying &&
                            slot.timeline == timeline)) {
        return;
      }
      slot.timeline = static_cast<std::uint8_t>(
          static_cast<unsigned>(timeline) & kTextureTimelineMask);
      slot.key_index = 0;
      slot.ticks = static_cast<std::uint8_t>(
          channel == 1 ? actor_id * kChannel1TicksPerActor : 0);
      slot.mode = TextureAnimationMode::kPlaying;
      break;
    case TextureAnimationCommand::kTogglePause:
      if (slot.mode == TextureAnimationMode::kPlaying) {
        slot.mode = TextureAnimationMode::kPaused;
      } else if (slot.mode == TextureAnimationMode::kPaused) {
        slot.mode = TextureAnimationMode::kPlaying;
      }
      break;
  }
}

TextureAnimationFrames StepTextureAnimations(
    std::span<TextureAnimationSlot, kTextureAnimationChannels> slots,
    const TextureAnimationKeys& keys) {
  TextureAnimationFrames frames;
  for (int channel = 0; channel < kTextureAnimationChannels; ++channel) {
    TextureAnimationSlot& slot = slots[static_cast<std::size_t>(channel)];
    if (slot.timeline == 0) {
      continue;
    }
    switch (slot.mode) {
      case TextureAnimationMode::kPaused:
        continue;
      case TextureAnimationMode::kStopping:
        if (slot.current_frame == 1) {
          slot.timeline = 0;
          continue;
        }
        [[fallthrough]];
      case TextureAnimationMode::kPlaying:
        if (!StepChannel(slot, channel, keys)) {
          return frames;
        }
        break;
      default:
        // Another mode copies the shown frame without stepping.
        break;
    }
    frames[static_cast<std::size_t>(channel)] = slot.current_frame - 1;
  }
  return frames;
}

std::optional<TextureCellSheet> ChannelCellSheet(
    const TextureCellSheet& sheet, const TextureCellOrigin& origin,
    int channel) {
  if (sheet.columns != 0) {
    return sheet;
  }
  if (origin.columns == 0) {
    return std::nullopt;
  }
  return TextureCellSheet{
      .overlay = kFallbackSheetOverlays[channel == 0 ? 0 : 1],
      .source_x = origin.x,
      .source_y = origin.y,
      .columns = origin.columns};
}

TextureMove TextureFrameMove(const TextureCellSheet& sheet,
                             const TextureOverlay& overlay,
                             const ActorTextureLayout& layout, int frame) {
  const int divisor = TexelsPerHalfword(layout.depth);
  const int source_x = (sheet.source_x / divisor) +
                       ((frame % sheet.columns) * (overlay.width / divisor));
  const int source_y = sheet.source_y +
                       ((frame / sheet.columns) * overlay.height) +
                       layout.sheet_row;
  return SheetCellMove(source_x, source_y, overlay, layout);
}

bool RestartsTextureFrames(int bank_index, int shared_bank,
                           std::uint8_t row) noexcept {
  return bank_index == shared_bank && row != kNoTextureRow;
}

TextureMove TextureOverlayMove(int source_x, int source_y,
                               const TextureOverlay& overlay,
                               const ActorTextureLayout& layout) {
  return SheetCellMove(source_x / TexelsPerHalfword(layout.depth),
                       source_y + layout.sheet_row, overlay, layout);
}

TextureScrollMoves StepTextureScrolling(TextureScrolling& scrolling,
                                        const ActorTextureLayout& layout,
                                        int tick_speed, int carried_height) {
  const int divisor = TexelsPerHalfword(layout.depth);
  const int origin_x = layout.slot_x * kPageColumns;
  const int origin_y =
      (layout.slot_y * kPageLines) + (layout.slot_row * kRowLines);
  TextureScrollMoves moves;
  for (std::size_t layer = 0; layer < moves.size(); ++layer) {
    const TextureScrollBounds& bounds = scrolling.bounds.at(layer);
    TextureScrollState& state = scrolling.states.at(layer);
    const TextureOverlay& overlay = scrolling.overlays.at(layer);
    if (bounds.width == 0) {
      continue;
    }
    state.timer = static_cast<std::uint8_t>(state.timer + (tick_speed / 2));
    if (state.timer <= state.interval) {
      continue;
    }
    state.timer = 0;
    if (bounds.width == carried_height) {
      continue;
    }
    carried_height = overlay.height;
    const int x = StepScrollAxis(state.x, state.step_x, overlay.width, bounds.x,
                                 bounds.width, bounds.bounces);
    const int y = StepScrollAxis(state.y, state.step_y, overlay.height,
                                 bounds.y, bounds.height, bounds.bounces);
    state.x = static_cast<std::uint8_t>(x);
    state.y = static_cast<std::uint8_t>(y);
    moves.at(layer) = TextureMove{
        .source = SheetRect(origin_x + (x / divisor), origin_y + y,
                            overlay.width / divisor, overlay.height),
        .x = origin_x + (overlay.x / divisor),
        .y = origin_y + overlay.y};
  }
  return moves;
}
}  // namespace VS::Actors
