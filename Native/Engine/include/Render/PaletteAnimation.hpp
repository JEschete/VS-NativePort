#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

// The scene's animated colours (SLUS_010.40 main.c:10519-11650 and the BATTLE
// wrappers at 146C.c:8241-8285): fourteen CLUT rows whose colours step toward
// a target through per-channel delta tables, and the five room background
// gradient colours and seventeen model tints (the room's at 0, actor n's at
// n + 1), which step in 16.16 fixed point. All of it is one PaletteAnimation
// value. VRAM uploads, the room background and the tint consumers are reached
// only through a PaletteHost, so no transition touches a GPU, room or module
// global, and every row, tint and source span is bounds-checked: a request
// the original would have run past its arrays is refused (false) instead.
// PaletteAnimationLive.hpp binds it to the game.
namespace VS::Render {
inline constexpr std::size_t kAnimatedClutRowCount = 14;
inline constexpr std::size_t kClutColorCount = 256;
inline constexpr std::size_t kRgbChannelCount = 3;
inline constexpr std::size_t kGradientColorCount = 5;
inline constexpr std::size_t kModelTintCount = 17;
// The room's three CLUT rows, the first actor row (actor n uses row n + 4),
// and the scene effect row TransformSceneColors also drives.
inline constexpr std::size_t kRoomClutRowCount = 3;
inline constexpr std::size_t kActorClutRowBase = 4;
inline constexpr std::size_t kSceneEffectClutRow = 13;
// TransformActorColors drives actors 0 to 8 only.
inline constexpr std::uint32_t kColorTransformActorLimit = 9;
// A clut_offset moves the source by this many colours.
inline constexpr std::size_t kClutOffsetColors = 16;
// The animated rows' VRAM origin; a direct row r is uploaded to line 224 + r.
inline constexpr std::uint32_t kClutVramX = 768;
inline constexpr std::uint32_t kClutVramY = 224;
inline constexpr std::uint32_t kVramHeight = 512;
// The 5-bit channel limit and the delta tables' rows: row 31 holds a channel,
// row 31 + d moves it by d over one transition.
inline constexpr std::int32_t kChannelMaximum = 31;
inline constexpr std::uint8_t kHoldStepRow = 31;
inline constexpr std::size_t kStepRowCount = 63;
// The model tints rest at 0x40 per channel; the tint code is the channel less
// this.
inline constexpr std::uint8_t kNeutralTintChannel = 0x40;
// The room tint blend the grey and half-base modes move to.
inline constexpr std::int16_t kGreyRoomTintBlend = 0x4000;

// The modes the scripts and effects request. A value without a name offsets
// the current colours like kOffsetCurrent. The background treats kInvert,
// kThreeQuarterBase and kRestoreBaseFinal as kOffsetCurrent, the model tints
// treat kInvert and kThreeQuarterBase so, and kFadeThenRestore offsets the
// current colours before the CLUT rows and tints restore their base.
enum class ColorTransform : std::uint8_t {
  kOffsetCurrent = 0,
  kHalveCurrent = 1,
  // The weighted luminance (2r + 3g + b) / 6 in every channel, or / 12.
  kGreyCurrent = 2,
  kDarkGreyCurrent = 3,
  kOffsetBase = 4,
  kHalveBase = 5,
  kGreyBase = 6,
  kDarkGreyBase = 7,
  kRestoreBase = 8,
  // kOffsetBase, then alternating with half the offset until cancelled.
  kPulse = 9,
  // The pulse's half-offset leg; requested directly it offsets the current
  // colours.
  kPulseReturn = 10,
  // Clears the stored mode, ending a pulse after its current leg, and changes
  // nothing else.
  kCancelPulse = 11,
  // 31 - channel plus the offset (CLUT rows only).
  kInvert = 12,
  // Three quarters of the base plus the offset (CLUT rows only).
  kThreeQuarterBase = 13,
  kFadeThenRestore = 14,
  kRestoreBaseFinal = 15,
};

using RgbBytes = std::array<std::uint8_t, kRgbChannelCount>;
// Channel offsets. The original kept only their low 16 bits.
using ColorOffset = std::array<std::int16_t, kRgbChannelCount>;
using ClutRowColors = std::array<std::uint16_t, kClutColorCount>;
// Room background colours as 0x00BBGGRR words.
using BackgroundColors = std::array<std::int32_t, kGradientColorCount>;

struct ColorTransitionTimer {
  bool active = false;
  std::uint8_t frame = 0;
  std::uint8_t tick = 0;
  // The duration the transition started with, kept as its low byte.
  std::uint8_t speed = 0;
  ColorTransform mode = ColorTransform::kOffsetCurrent;

  friend bool operator==(const ColorTransitionTimer&,
                         const ColorTransitionTimer&) = default;
};

// One CLUT colour's channels and the delta-table rows stepping it.
struct ClutColorFade {
  RgbBytes rgb{};
  // The STP bit.
  std::uint8_t mask = 0;
  RgbBytes step_rows{};

  friend bool operator==(const ClutColorFade&, const ClutColorFade&) = default;
};

struct ClutRowAnimation {
  // Nothing in the game sets it; a locked row ignores LoadClutColors.
  bool locked = false;
  ColorTransitionTimer timer;
  // The kPulse offset, kept as the original's signed bytes.
  std::array<std::int8_t, kRgbChannelCount> pulse{};
  std::array<ClutColorFade, kClutColorCount> colors{};
  ClutRowColors base{};

  friend bool operator==(const ClutRowAnimation&,
                         const ClutRowAnimation&) = default;
};

// A 16.16 channel and its per-frame step.
struct FixedChannel {
  std::int32_t value = 0;
  std::int32_t step = 0;

  friend bool operator==(const FixedChannel&, const FixedChannel&) = default;
};
using FixedRgb = std::array<FixedChannel, kRgbChannelCount>;

struct BackgroundGradient {
  ColorTransitionTimer timer;
  ColorOffset pulse{};
  std::array<FixedRgb, kGradientColorCount> colors{};
  std::array<RgbBytes, kGradientColorCount> base{};

  friend bool operator==(const BackgroundGradient&,
                         const BackgroundGradient&) = default;
};

struct ModelTintFade {
  ColorTransitionTimer timer;
  ColorOffset pulse{};
  // The room tint's (slot 0) blend and its per-frame step; the room shader
  // takes blend / 4. Actor slots keep them but never step them.
  std::int16_t blend = 0;
  std::int16_t blend_step = 0;
  FixedRgb color{};
  RgbBytes base{};

  friend bool operator==(const ModelTintFade&, const ModelTintFade&) = default;
};

// Zero-initialised like the original's .bss block: disabled until
// InitializeClutAnimation.
struct PaletteAnimation {
  bool upload_pending = false;
  bool enabled = false;
  std::array<ClutRowAnimation, kAnimatedClutRowCount> rows{};
  // The packed colours the animated rows upload.
  std::array<ClutRowColors, kAnimatedClutRowCount> colors{};
  BackgroundGradient background{};
  std::array<ModelTintFade, kModelTintCount> tints{};
  // Delta-table rows or frames outside their table, which the original would
  // have read past it; such a channel holds instead.
  std::uint32_t refused_steps = 0;

  friend bool operator==(const PaletteAnimation&,
                         const PaletteAnimation&) = default;
};

// Each channel less kNeutralTintChannel, wrapped to a byte.
using TintCode = RgbBytes;

class PaletteHost {
 public:
  PaletteHost() = default;
  PaletteHost(const PaletteHost&) = delete;
  PaletteHost& operator=(const PaletteHost&) = delete;
  PaletteHost(PaletteHost&&) = delete;
  PaletteHost& operator=(PaletteHost&&) = delete;
  virtual ~PaletteHost() = default;

  // `colors` to VRAM line kClutVramY + row from column kClutVramX +
  // first_color; the span stays within the line and VRAM.
  virtual void UploadClutColors(std::uint32_t row, std::uint32_t first_color,
                                std::span<const std::uint16_t> colors) = 0;
  // The animated rows as one 256 x 14 block at (kClutVramX, kClutVramY).
  virtual void UploadAnimatedClutRows(
      std::span<const ClutRowColors, kAnimatedClutRowCount> rows) = 0;
  virtual void SetRoomBackgroundColors(const BackgroundColors& colors) = 0;
  virtual void SetRoomTint(const TintCode& tint, std::int32_t blend) = 0;
  virtual void SetActorTint(std::uint32_t actor, const TintCode& tint) = 0;
};

// _shift_left (main.c:10778): a channel as 16.16, wrapping like the original.
[[nodiscard]] std::int32_t ToFixed16(std::int32_t value) noexcept;

// _weightedLuminance (main.c:10590): 2r + 3g + b of a 15-bit colour.
[[nodiscard]] std::int32_t WeightedLuminance(std::uint16_t color) noexcept;

// The duration arguments are halves of the transition's frame count (the CLUT
// rows take 2 to 32 frames by speed band); zero applies the colours at once.
// The functions marked "gated" do nothing while the animator is disabled.

// _loadClutTransition (main.c:10519): steps the row toward the 256 colours of
// `source` (its STP bits are ignored).
bool BuildClutTransition(PaletteAnimation& animation, std::int16_t duration,
                         std::size_t row,
                         std::span<const std::uint16_t> source) noexcept;
// vs_main_loadClutTransition (main.c:10553), gated: BuildClutTransition from
// colour clut_offset * 16 of `source`.
bool StartClutTransition(PaletteAnimation& animation, std::int16_t duration,
                         std::size_t row, std::span<const std::uint16_t> source,
                         std::uint32_t clut_offset) noexcept;
// _loadClutTransitionWithBase (main.c:10562), gated: StartClutTransition, then
// the first 256 colours of `source` (not those at clut_offset) become the base.
bool StartClutTransitionWithBase(PaletteAnimation& animation,
                                 std::int16_t duration, std::size_t row,
                                 std::span<const std::uint16_t> source,
                                 std::uint32_t clut_offset) noexcept;
// _saveClutBase (main.c:10579), gated: the row's current colours become its
// base.
bool SaveClutBase(PaletteAnimation& animation, std::size_t row) noexcept;
// _transformClutSlot (main.c:10595).
bool ApplyClutRowTransform(PaletteAnimation& animation, ColorTransform mode,
                           std::int16_t duration, std::size_t row,
                           const ColorOffset& offset) noexcept;
// vs_main_transformClutSlot (main.c:10768), gated.
bool TransformClutRow(PaletteAnimation& animation, ColorTransform mode,
                      std::int16_t duration, std::size_t row,
                      const ColorOffset& offset) noexcept;
// func_800472D0 (main.c:10780), gated.
void SetBackgroundGradientColors(
    PaletteAnimation& animation, PaletteHost& host, std::int16_t duration,
    std::span<const RgbBytes, kGradientColorCount> colors);
// func_80047464 (main.c:10822), gated: also makes `colors` the base.
void SetBackgroundGradientBase(
    PaletteAnimation& animation, PaletteHost& host, std::int16_t duration,
    std::span<const RgbBytes, kGradientColorCount> colors);
// func_800474DC (main.c:10834).
void TransformBackgroundGradient(PaletteAnimation& animation, PaletteHost& host,
                                 ColorTransform mode, std::int16_t duration,
                                 const ColorOffset& offset);
// func_800478E0 (main.c:10990), gated.
void RequestBackgroundGradientTransform(PaletteAnimation& animation,
                                        PaletteHost& host, ColorTransform mode,
                                        std::int16_t duration,
                                        const ColorOffset& offset);
// func_80047910 (main.c:10997), gated.
bool SetModelTint(PaletteAnimation& animation, PaletteHost& host,
                  std::int16_t duration, std::size_t tint,
                  const RgbBytes& color);
// func_80047AB4 (main.c:11047), gated: also makes `color` the base.
bool SetModelTintBase(PaletteAnimation& animation, PaletteHost& host,
                      std::int16_t duration, std::size_t tint,
                      const RgbBytes& color);
// func_80047B30 (main.c:11055).
bool TransformModelTint(PaletteAnimation& animation, PaletteHost& host,
                        ColorTransform mode, std::int16_t duration,
                        std::size_t tint, const ColorOffset& offset);
// func_80047FC0 (main.c:11222), gated.
bool RequestModelTintTransform(PaletteAnimation& animation, PaletteHost& host,
                               ColorTransform mode, std::int16_t duration,
                               std::size_t tint, const ColorOffset& offset);
// func_80047FFC (main.c:11229), once a frame.
void UpdateBackgroundGradientTransition(PaletteAnimation& animation,
                                        PaletteHost& host);
// func_800481C0 (main.c:11287), once a frame.
void UpdateModelTintTransitions(PaletteAnimation& animation, PaletteHost& host);
// func_800483FC (main.c:11345), once a frame.
void UpdateClutTransitions(PaletteAnimation& animation) noexcept;
// func_80048A3C (main.c:11468).
bool StopClutTransition(PaletteAnimation& animation, std::size_t row) noexcept;
// vs_main_loadClut (main.c:11470): an animated row (below 14) takes the
// colours as its current colours and base unless locked, stopping its
// transition; any other row is uploaded straight to VRAM.
bool LoadClutColors(PaletteAnimation& animation, PaletteHost& host,
                    std::uint32_t row, std::uint32_t first_color,
                    std::span<const std::uint16_t> colors);
// func_80048B8C (main.c:11499): the row's base becomes `source` transformed
// (kHalveBase, kGreyBase and kDarkGreyBase read it as the base; any other mode
// offsets it), zero colours staying zero; update_current_colors also makes it
// the uploaded colours, and the row's channels when no transition runs.
bool TransformClutBase(PaletteAnimation& animation, ColorTransform mode,
                       std::span<const std::uint16_t, kClutColorCount> source,
                       std::size_t row, const ColorOffset& offset,
                       bool update_current_colors) noexcept;
// func_80048E68 (main.c:11598): TransformClutBase by the plain offset.
bool OffsetClutBase(PaletteAnimation& animation,
                    std::span<const std::uint16_t, kClutColorCount> source,
                    std::size_t row, const ColorOffset& offset,
                    bool update_current_colors) noexcept;
// func_80048EC4 (main.c:11604): the room's colours become the background base
// and current colours.
void CaptureRoomBackgroundColors(PaletteAnimation& animation,
                                 const BackgroundColors& room_colors) noexcept;
// vs_main_commitClut (main.c:11621): uploads the animated rows if any changed.
void CommitClutRows(PaletteAnimation& animation, PaletteHost& host);
// vs_main_setClutState (main.c:11632): the original kept the low 16 bits.
void SetClutAnimationEnabled(PaletteAnimation& animation,
                             std::int32_t enabled) noexcept;
// vs_main_clutInit (main.c:11634).
void InitializeClutAnimation(PaletteAnimation& animation) noexcept;

// The BATTLE wrappers (146C.c:8241-8285).
// func_8007B10C: the room rows, gated.
void TransformRoomPalettes(PaletteAnimation& animation, ColorTransform mode,
                           std::int16_t duration,
                           const ColorOffset& offset) noexcept;
// func_8007B1B8: the room rows, the room tint and the scene effect row, gated.
void TransformSceneColors(PaletteAnimation& animation, PaletteHost& host,
                          ColorTransform mode, std::int16_t duration,
                          const ColorOffset& offset);
// func_8007B29C: the actor's CLUT row and tint, gated; actors from
// kColorTransformActorLimit on are ignored.
void TransformActorColors(PaletteAnimation& animation, PaletteHost& host,
                          ColorTransform mode, std::int16_t duration,
                          std::uint32_t actor, const ColorOffset& offset);
// func_8007B344: RequestBackgroundGradientTransform.
void TransformBackgroundColors(PaletteAnimation& animation, PaletteHost& host,
                               ColorTransform mode, std::int16_t duration,
                               const ColorOffset& offset);
// func_8007B378 (unused by the game): StartClutTransition of the three room
// rows from consecutive 256-colour blocks of `source`. The original passed no
// clut_offset for rows 1 and 2, so they read an undefined register; here every
// row takes clut_offset.
bool StartRoomPaletteTransition(PaletteAnimation& animation,
                                std::int16_t duration,
                                std::span<const std::uint16_t> source,
                                std::uint32_t clut_offset) noexcept;
// func_8007B3D0 (unused by the game): StartClutTransition of row
// shared_row + 4.
bool StartSharedPaletteTransition(PaletteAnimation& animation,
                                  std::int16_t duration, std::size_t shared_row,
                                  std::span<const std::uint16_t> source,
                                  std::uint32_t clut_offset) noexcept;
// func_8007B3F0: SetBackgroundGradientColors.
void ApplyBackgroundGradientColors(
    PaletteAnimation& animation, PaletteHost& host, std::int16_t duration,
    std::span<const RgbBytes, kGradientColorCount> colors);
}  // namespace VS::Render
