// Hand-maintained (tools/port/owned.csv).
// Live binding for the screen overlays (ScreenOverlay.cpp).
#include "Render/ScreenOverlayLive.hpp"

#include <array>
#include <bit>
#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <memory>

#include "Core/Math/ScalarMath.hpp"
#include "Core/Memory/HeapLive.hpp"

extern "C" {
#include "Core/Diagnostics.h"
#include "Core/Gte.h"
#include "Field/RoomLoading.h"
#include "Field/Transitions.h"
#include "Render/Display.h"
#include "Render/FramebufferEffects.h"
#include "Render/ScreenTransition.h"
}

namespace VS::Render {
namespace {
namespace Math = VS::Core::Math;

constexpr const char* kChannel = "render";
// Ordering-table entries are words; the transition draws one entry in
// (146C.c:2249 `ot + 4`).
constexpr std::size_t kOrderingEntryBytes = 4;
constexpr std::size_t kTransitionEntry = 1;
static_assert(sizeof(u_long) == kOrderingEntryBytes);
constexpr std::int16_t kScreenWidth = 320;
constexpr std::int16_t kScreenHeight = 240;
constexpr std::int16_t kStripRight{kFramebufferEffectStripWidth};
constexpr std::int16_t kStripBottom{kFramebufferEffectStripHeight};
constexpr std::uint8_t kStripBottomTexel{kFramebufferEffectStripHeight};

// The original's packet block (146C.c:301, D_800F1ABC_t): one quad, sprite
// and draw mode per slot.
struct EffectPacketBlock {
  std::array<POLY_FT4, kFramebufferEffectSlotCount> rotated;
  std::array<SPRT, kFramebufferEffectSlotCount> flat;
  std::array<DR_MODE, kFramebufferEffectSlotCount> draw_modes;
};
constexpr std::size_t kOriginalPacketBlockBytes = 0x2D0;
static_assert(sizeof(EffectPacketBlock) == kOriginalPacketBlockBytes,
              "the block keeps the original's heap footprint");

FramebufferEffect live_effect;
EffectPacketBlock* live_packets = nullptr;
std::uint64_t refused_overlays = 0;

void ReportRefusal(const char* what) {
  if (std::has_single_bit(++refused_overlays)) {
    Core_Errorf(kChannel, "screen overlay refused: %s (%" PRIu64 " refused)",
                what, refused_overlays);
  }
}

[[nodiscard]] std::size_t CurrentBuffer() {
  return static_cast<std::size_t>(Render_frameBuf & 1);
}

[[nodiscard]] void* OrderingEntry(void* ordering_table, std::size_t entry) {
  return static_cast<unsigned char*>(ordering_table) +
         (entry * kOrderingEntryBytes);
}

class LiveScreenTransformer final : public ScreenTransformer {
 public:
  void LoadTransform(const Math::Transform3& transform) override {
    MATRIX matrix{};
    Math::StoreMatrix(matrix, transform.rotation);
    matrix.t[0] = transform.translation.x;
    matrix.t[1] = transform.translation.y;
    matrix.t[2] = transform.translation.z;
    Core_Gte_SetRotMatrix(&matrix);
    Core_Gte_SetTransMatrix(&matrix);
  }
  Math::Vec3 Transform(std::int16_t x, std::int16_t y) override {
    SVECTOR input = {x, y, 0, 0};
    VECTOR output{};
    Core_GteWord flags = 0;
    Core_Gte_RotTrans(&input, &output, &flags);
    return Math::VectorOf(output);
  }
};

// decomp: BATTLE.PRG func_8007D41C (src/BATTLE/BATTLE.PRG/146C.c:9285): every
// slot's quad and sprite before its first frame.
void FillPackets(EffectPacketBlock& block, const FramebufferEffectTint& tint) {
  const auto templates = BuildFramebufferEffectTemplates();
  for (std::size_t slot = 0; slot < kFramebufferEffectSlotCount; ++slot) {
    const FramebufferEffectSlotTemplate& entry = templates[slot];
    const auto right = static_cast<std::int16_t>(entry.left + kStripRight);
    POLY_FT4& quad = block.rotated[slot];
    setPolyFT4(&quad);
    setRGB0(&quad, tint.red, tint.green, tint.blue);
    setSemiTrans(&quad, 1);
    setUV4(&quad, 0, 0, entry.right_texel, 0, 0, kStripBottomTexel,
           entry.right_texel, kStripBottomTexel);
    setXY4(&quad, entry.left, 0, right, 0, entry.left, kStripBottom, right,
           kStripBottom);
    quad.tpage = entry.texture_page;
    SPRT& sprite = block.flat[slot];
    setSprt(&sprite);
    setRGB0(&sprite, tint.red, tint.green, tint.blue);
    setSemiTrans(&sprite, 1);
    setUV0(&sprite, 0, 0);
    setWH(&sprite, kStripRight, kStripBottom);
  }
}

void SubmitRotated(EffectPacketBlock& block,
                   const FramebufferEffectFrame& frame,
                   const FramebufferEffectStrip& strip, void* entry) {
  POLY_FT4& quad = block.rotated[strip.slot];
  const auto& corners = strip.corners;
  setRGB0(&quad, frame.tint.red, frame.tint.green, frame.tint.blue);
  quad.tpage = strip.texture_page;
  setXY4(&quad, corners[0].x, corners[0].y, corners[1].x, corners[1].y,
         corners[2].x, corners[2].y, corners[3].x, corners[3].y);
  AddPrim(entry, &quad);
}

// The sprite, then the draw mode that selects its page, which the ordering
// table runs first.
void SubmitFlat(EffectPacketBlock& block, const FramebufferEffectFrame& frame,
                const FramebufferEffectStrip& strip, void* entry) {
  SPRT& sprite = block.flat[strip.slot];
  setRGB0(&sprite, frame.tint.red, frame.tint.green, frame.tint.blue);
  sprite.x0 = strip.origin.x;
  sprite.y0 = strip.origin.y;
  AddPrim(entry, &sprite);
  DR_MODE& mode = block.draw_modes[strip.slot];
  RECT clip{};
  SetDrawMode(&mode, 0, 1, strip.texture_page, &clip);
  AddPrim(entry, &mode);
}

class LiveEffectPackets final : public FramebufferEffectPackets {
 public:
  explicit LiveEffectPackets(void* ordering_table = nullptr)
      : ordering_table_(ordering_table) {}

  [[nodiscard]] bool Ready() const override { return live_packets != nullptr; }
  bool Acquire(const FramebufferEffectTint& tint) override {
    void* memory =
        Core::Memory::AllocateLiveFromHighEnd(sizeof(EffectPacketBlock));
    if (memory == nullptr) {
      ReportRefusal("no heap block for the framebuffer effect packets");
      return false;
    }
    live_packets = std::construct_at(static_cast<EffectPacketBlock*>(memory));
    FillPackets(*live_packets, tint);
    return true;
  }
  void Release() override {
    if (live_packets != nullptr) {
      Core::Memory::FreeLiveFromHighEnd(live_packets);
      live_packets = nullptr;
    }
  }
  void Forget() override { live_packets = nullptr; }
  void Submit(const FramebufferEffectFrame& frame) override {
    if (live_packets == nullptr || ordering_table_ == nullptr) {
      ReportRefusal("framebuffer effect frame without packets or table");
      return;
    }
    void* entry = OrderingEntry(ordering_table_, frame.depth);
    for (const FramebufferEffectStrip& strip : frame.strips) {
      if (strip.slot >= kFramebufferEffectSlotCount) {
        ReportRefusal("framebuffer effect strip past the packet block");
      } else if (frame.layout == FramebufferEffectLayout::kTransformed) {
        SubmitRotated(*live_packets, frame, strip, entry);
      } else {
        SubmitFlat(*live_packets, frame, strip, entry);
      }
    }
  }

 private:
  void* ordering_table_;
};

template <typename Quad>
void SetQuadBetween(Quad& quad, const ScreenTransitionWipe::Column& left,
                    const ScreenTransitionWipe::Column& right) {
  setXY4(&quad, left.top.x, left.top.y, right.top.x, right.top.y, left.bottom.x,
         left.bottom.y, right.bottom.x, right.bottom.y);
}

// The Field transition's packets for the frame being built.
class LiveTransitionTarget final : public ScreenTransitionTarget {
 public:
  explicit LiveTransitionTarget(void* entry) : entry_(entry) {}

  void DrawFade(const ScreenTransitionFade& fade) override {
    const std::size_t buffer = CurrentBuffer();
    POLY_F4& quad = Render_legacyTransitionFadePackets[buffer];
    DR_MODE& mode = Render_legacyTransitionDrawModes[buffer];
    RECT clip{};
    setXY4(&quad, 0, 0, kScreenWidth, 0, 0, kScreenHeight, kScreenWidth,
           kScreenHeight);
    SetDrawMode(&mode, 0, 1, fade.texture_page, &clip);
    setSemiTrans(&quad, fade.semi_transparent ? 1 : 0);
    setRGB0(&quad, fade.level, fade.level, fade.level);
    AddPrim(entry_, &quad);
    AddPrim(entry_, &mode);
  }
  void DrawWipe(const ScreenTransitionWipe& wipe) override {
    const std::size_t buffer = CurrentBuffer();
    Render_LegacyTransitionWipePacket& packet =
        Render_legacyTransitionWipePackets[buffer];
    POLY_G4& gradient = Render_legacyTransitionEdgePackets[buffer];
    DR_MODE& mode = Render_legacyTransitionDrawModes[buffer];
    const auto& [covered_left, edge_left, gradient_left, gradient_right] =
        wipe.columns;
    SetQuadBetween(packet.coveredQuad, covered_left, edge_left);
    SetQuadBetween(packet.edgeQuad, edge_left, gradient_left);
    SetQuadBetween(gradient, gradient_left, gradient_right);
    AddPrim(entry_, &packet.coveredQuad);
    AddPrim(entry_, &packet.edgeQuad);
    AddPrim(entry_, &gradient);
    RECT clip{};
    SetDrawMode(&mode, 0, 1, wipe.texture_page, &clip);
    AddPrim(entry_, &mode);
  }

 private:
  void* entry_;
};

// Draws nothing; the transition still steps.
class DiscardedTransitionTarget final : public ScreenTransitionTarget {
 public:
  void DrawFade(const ScreenTransitionFade& /*fade*/) override {}
  void DrawWipe(const ScreenTransitionWipe& /*wipe*/) override {}
};
}  // namespace

// decomp: BATTLE.PRG _drawScreenTransition (src/BATTLE/BATTLE.PRG/146C.c:2223).
void DrawLiveScreenTransition(void* ordering_table) {
  ScreenTransition transition{
      Field_screenTransitionAlpha, Field_screenTransitionStep,
      Field_screenTransitionEffect, Field_screenTransitionWipeAngle,
      Field_screenTransitionSpeed};
  LiveScreenTransformer transformer;
  if (ordering_table == nullptr) {
    if (transition.alpha != 0) {
      ReportRefusal("screen transition without an ordering table");
    }
    DiscardedTransitionTarget target;
    DrawScreenTransition(transition, target, transformer);
  } else {
    LiveTransitionTarget target(
        OrderingEntry(ordering_table, kTransitionEntry));
    DrawScreenTransition(transition, target, transformer);
  }
  Field_screenTransitionAlpha = transition.alpha;
  Field_screenTransitionStep = transition.step;
}

// decomp: BATTLE.PRG func_8007D360 (src/BATTLE/BATTLE.PRG/146C.c:9258).
void ResetLiveFramebufferEffect() { ResetFramebufferEffect(live_effect); }

// decomp: BATTLE.PRG func_8007D3F8 (src/BATTLE/BATTLE.PRG/146C.c:9278).
void InitializeLiveFramebufferEffect() {
  LiveEffectPackets packets;
  InitializeFramebufferEffect(live_effect, packets);
}

// decomp: BATTLE.PRG func_8007D734 (src/BATTLE/BATTLE.PRG/146C.c:9311).
void DrawLiveFramebufferEffect(void* ordering_table) {
  LiveEffectPackets packets(ordering_table);
  LiveScreenTransformer transformer;
  if (DrawFramebufferEffect(live_effect, packets, transformer,
                            CurrentBuffer()) ==
      FramebufferEffectDraw::kNoPackets) {
    ReportRefusal("framebuffer effect drawn without its packet block");
  }
}

// decomp: BATTLE.PRG func_8007DD50 (src/BATTLE/BATTLE.PRG/146C.c:9417).
void SetLiveFramebufferEffectEnabled(int enabled) {
  LiveEffectPackets packets;
  static_cast<void>(
      SetFramebufferEffectEnabled(live_effect, packets, enabled != 0));
}

// decomp: BATTLE.PRG func_8007DDAC (src/BATTLE/BATTLE.PRG/146C.c:9430).
void SetLiveFramebufferEffectRotation(int roll_angle) {
  SetFramebufferEffectRoll(live_effect.settings, roll_angle);
}

// decomp: BATTLE.PRG func_8007DDB8 (src/BATTLE/BATTLE.PRG/146C.c:9432).
void SetLiveFramebufferEffectScale(
    const Render_FramebufferEffectTranslation* scale) {
  if (scale == nullptr) {
    ReportRefusal("null framebuffer effect scale");
    return;
  }
  SetFramebufferEffectScale(live_effect.settings, {scale->x, scale->y});
}

// decomp: BATTLE.PRG func_8007DDD4 (src/BATTLE/BATTLE.PRG/146C.c:9438).
void SetLiveFramebufferEffectTint(const CVECTOR* tint) {
  if (tint == nullptr) {
    ReportRefusal("null framebuffer effect tint");
    return;
  }
  SetFramebufferEffectTint(live_effect.settings, {tint->r, tint->g, tint->b});
}

// decomp: BATTLE.PRG func_8007DDF8 (src/BATTLE/BATTLE.PRG/146C.c:9440).
void SetLiveFramebufferEffectOffset(
    const Render_FramebufferEffectTranslation* offset) {
  if (offset == nullptr) {
    ReportRefusal("null framebuffer effect offset");
    return;
  }
  SetFramebufferEffectOffset(live_effect.settings, {offset->x, offset->y});
}

// decomp: BATTLE.PRG func_8007DE2C (src/BATTLE/BATTLE.PRG/146C.c:9442).
void SetLiveFramebufferEffectBlendMode(int blend_mode) {
  SetFramebufferEffectBlendMode(live_effect.settings, blend_mode);
}

// decomp: BATTLE.PRG func_8007DE44 (src/BATTLE/BATTLE.PRG/146C.c:9446). A
// depth past the table is ignored, as in the original.
void SetLiveFramebufferEffectDepth(u_int ordering_depth) {
  static_cast<void>(
      SetFramebufferEffectDepth(live_effect.settings, ordering_depth));
}

// decomp: BATTLE.PRG func_8007DE5C (src/BATTLE/BATTLE.PRG/146C.c:9453).
void SetLiveFramebufferPreviousFrame(int use_previous_frame) {
  SetFramebufferEffectPreviousFrame(live_effect.settings, use_previous_frame);
}

// decomp: BATTLE.PRG func_8007DE88 (src/BATTLE/BATTLE.PRG/146C.c:9459).
void ReadLiveFramebufferEffectScale(
    Render_FramebufferEffectTranslation* output) {
  if (output == nullptr) {
    ReportRefusal("null framebuffer effect scale output");
    return;
  }
  const FramebufferEffectPair scale =
      ReadFramebufferEffectScale(live_effect.settings);
  output->x = scale.x;
  output->y = scale.y;
  output->reserved8 = 0;
}

// decomp: BATTLE.PRG func_8007DEA8 (src/BATTLE/BATTLE.PRG/146C.c:9466).
void ReadLiveFramebufferEffectTint(CVECTOR* output) {
  if (output == nullptr) {
    ReportRefusal("null framebuffer effect tint output");
    return;
  }
  const FramebufferEffectTint tint =
      ReadFramebufferEffectTint(live_effect.settings);
  output->r = tint.red;
  output->g = tint.green;
  output->b = tint.blue;
}

// decomp: BATTLE.PRG func_8007DECC (src/BATTLE/BATTLE.PRG/146C.c:9468).
void ReadLiveFramebufferEffectOffset(
    Render_FramebufferEffectTranslation* output) {
  if (output == nullptr) {
    ReportRefusal("null framebuffer effect offset output");
    return;
  }
  const FramebufferEffectPair offset =
      ReadFramebufferEffectOffset(live_effect.settings);
  output->x = offset.x;
  output->y = offset.y;
}

bool IsLiveFramebufferEffectShowing() noexcept {
  return live_effect.phase == FramebufferEffectPhase::kStarting ||
         live_effect.phase == FramebufferEffectPhase::kDrawing;
}

const FramebufferEffect& LiveFramebufferEffect() noexcept {
  return live_effect;
}

bool HasLiveFramebufferEffectPackets() noexcept {
  return live_packets != nullptr;
}

std::uint64_t RefusedLiveScreenOverlayCount() noexcept {
  return refused_overlays;
}
}  // namespace VS::Render
