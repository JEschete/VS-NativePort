// Hand-maintained (tools/port/owned.csv).
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "Ending/DisplayHost.hpp"

// ENDING.PRG's staff roll: the scripted credits, the element renderers that
// scroll its text and fade its illustrations, the illustration decoder and the
// frame loop. The script, glyph widths, illustration hold times and the
// illustration images stay module or disc data; the host lends them as spans
// (EndingLive.cpp reads them from the module image).
namespace VS::Ending::Credits {
inline constexpr std::size_t kElementCount = 32;
inline constexpr std::size_t kIllustrationCount = 16;
inline constexpr std::size_t kEmbeddedArtCount = 12;
inline constexpr std::size_t kFirstDiscIllustration = 5;
inline constexpr std::size_t kRippleRows = 4;
inline constexpr std::size_t kRippleColumns = 16;
// The 4x16 distance grid plus the row and entry the last row of quads reads
// past it (see FillRippleGrid).
inline constexpr std::size_t kRippleGridRows = kRippleRows + 1;
inline constexpr std::size_t kRippleGridArea = kRippleGridRows * kRippleColumns;
inline constexpr std::size_t kRippleGridEntries = kRippleGridArea + 1;
inline constexpr std::uint32_t kStartButton = 1U << 11U;
// The scene ordering slot the frame is drawn from.
inline constexpr int kFrameOrderingSlot = 0x7FF;

enum class ElementKind : std::uint8_t {
  kNone,
  kPhantomPainRipple,
  kFinCaption,
  kScrollingTitleLogo,
  kIllustration,
  kScrollingText,
  kScrollingCopyright,
  kBackground,
  kFadeInBackground,
  kFadeOutBackground,
  kFadeOutForSkip
};

// _creditsElement: phase 0 is a free slot, a negative phase is freed on the
// next DrawElements pass and a positive phase is drawn.
struct Element {
  ElementKind kind = ElementKind::kNone;
  std::int16_t phase = 0;
  std::uint16_t animation_frame = 0;
  std::uint16_t illustration_index = 0;
  std::uint16_t width = 0;
  std::int32_t x_fixed8 = 0;
  std::int32_t y_fixed8 = 0;
  // Script offset of the text's length byte.
  std::size_t text_offset = 0;
};

struct RippleState {
  std::int32_t phase_angle = 0;
  std::int32_t scale_frame = 0;
  std::array<std::uint8_t, kRippleGridEntries> distance_grid{};
};

struct IllustrationState {
  std::int16_t brightness = 0;
  std::int16_t texture_page_x = 0;
  std::uint16_t palette_y = 0;
  std::array<std::uint16_t, kIllustrationCount> hold_frames{};
};

enum class DecodePhase : std::uint8_t { kIdle = 0, kQueued = 1, kRunning = 2 };

// The illustration decoder's queued resource and its byte cursors into the
// compressed resource and the illustration workspace.
struct DecoderState {
  DecodePhase phase = DecodePhase::kIdle;
  std::size_t resource = 0;
  std::size_t input = 0;
  std::size_t output = 0;
  std::int16_t budget_start_tick = 0;
};

struct State {
  std::array<Element, kElementCount> elements{};
  bool has_finished = false;
  bool is_background_fading = false;
  bool was_skip_requested = false;
  std::int32_t elapsed_frames = 0;
  std::int32_t script_delay_frames = 0;
  std::size_t script_cursor = 0;
  std::int16_t scroll_speed_fixed8 = 0;
  std::uint16_t background_brightness = 0;
  RippleState ripple{};
  IllustrationState illustration{};
  DecoderState decoder{};
};

struct ModelVertex {
  std::int16_t x = 0;
  std::int16_t y = 0;
  std::int16_t z = 0;
};

// A Gouraud quad's corner: its screen position and grey level.
struct ShadedVertex {
  ScreenPoint position{};
  std::uint8_t brightness = 0;
};

struct DisplaySettings {
  std::int16_t width = 0;
  std::int16_t height = 0;
  int projection_distance = 0;
  Render::ClearColor background{};
};

class StaffRollHost : public DisplayHost {
 public:
  // Loads the identity rotation and a translation `depth` along z.
  virtual void BeginRippleProjection(int depth) = 0;
  // RotTransPers4 under the current projection.
  [[nodiscard]] virtual std::array<ScreenPoint, kQuadCorners> ProjectQuad(
      const std::array<ModelVertex, kQuadCorners>& vertices) = 0;
  // Starts the ending XA stream with the original byte-count argument.
  virtual void StartEndingMusic(int byte_count) = 0;
  virtual void StopMusic() = 0;
  [[nodiscard]] virtual bool PrepareEmbeddedArt() = 0;
  // An embedded TIM; empty when the index is not one.
  [[nodiscard]] virtual std::span<const std::byte> EmbeddedArt(
      std::size_t index) = 0;
  // The TIM the illustration decoder writes and UploadTexture uploads.
  [[nodiscard]] virtual std::span<std::byte> IllustrationWorkspace() = 0;
  // Allocates a disc illustration's buffer and starts its read.
  virtual void BeginIllustrationRead(std::size_t resource) = 0;
  [[nodiscard]] virtual bool IsIllustrationReadComplete() = 0;
  virtual void EndIllustrationRead() = 0;
  virtual void FreeIllustration(std::size_t resource) = 0;
  // A compressed illustration; empty when it is not loaded.
  [[nodiscard]] virtual std::span<const std::byte> IllustrationResource(
      std::size_t resource) = 0;
  [[nodiscard]] virtual std::span<const std::uint8_t> Script() = 0;
  [[nodiscard]] virtual std::span<const std::uint8_t> GlyphWidths() = 0;
  virtual void UpdateClearProgress() = 0;
  virtual void ShowClearResults() = 0;
};

// Fills the distance grid, including the entries past its 4x16 rows that the
// last row of quads reads.
void FillRippleGrid(RippleState& ripple);

void DrawPhantomPainRipple(Element& element, RippleState& ripple,
                           StaffRollHost& host);
void DrawFinCaption(Element& element, DisplayHost& host);
void DrawScrollingTitleLogo(Element& element, const State& state,
                            DisplayHost& host);
[[nodiscard]] int DistanceFromScreenCenter(int x, int y);
void ApplyRadialVertexFade(std::span<ShadedVertex, kQuadCorners> corners,
                           std::int16_t fade_step, bool fade_out);
void DrawIllustration(Element& element, State& state, StaffRollHost& host);
[[nodiscard]] std::int16_t MeasureTextWidth(
    std::span<const std::uint8_t> text,
    std::span<const std::uint8_t> glyph_widths);
void DrawScrollingText(Element& element, const State& state,
                       StaffRollHost& host);
void DrawScrollingCopyright(Element& element, const State& state,
                            DisplayHost& host);
void DrawBackground(const State& state, DisplayHost& host);
void FadeInBackground(Element& element, State& state);
void FadeOutBackground(Element& element, State& state);
void FadeOutForSkip(Element& element, State& state, DisplayHost& host);
void RunStaffRoll(State& state, StaffRollHost& host);
[[nodiscard]] DrawEnvironment ConfigureDrawEnvironment();
void RunFrames(State& state, StaffRollHost& host);
// Uploads the TIM's CLUT block, when its mode has one, then its image block;
// false when a block lies outside the bytes.
bool UploadTexture(std::span<const std::byte> tim, DisplayHost& host);
void ResetElements(State& state);
int DrawElements(State& state, StaffRollHost& host);
// The first free slot after the main one, set to start `kind`; null when
// every slot is in use.
Element* AddElement(State& state, ElementKind kind);
// The main slot set to start `kind`; null while it is in use.
Element* ReserveMainElement(State& state, ElementKind kind);
void ExecuteScript(State& state, StaffRollHost& host);
void QueueIllustrationDecode(DecoderState& decoder, std::size_t resource);
// Decodes until the frame's time budget runs out; true when the image is
// complete.
bool DecodeIllustrationStep(State& state, StaffRollHost& host);
void DrawSmallText(ScreenPoint origin, int palette_row_offset,
                   std::span<const std::uint8_t> text, DisplayHost& host);
void InitializeDisplay(const DisplaySettings& settings, DisplayHost& host);
int PresentFrame(int ordering_slot, DisplayHost& host);
void AllocateRenderBuffers(DisplayHost& host);
void BeginFrame(DisplayHost& host);
void StartMusic(StaffRollHost& host);
void WaitForMusicEnd(StaffRollHost& host);
}  // namespace VS::Ending::Credits
