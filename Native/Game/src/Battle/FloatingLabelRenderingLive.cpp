#include "Battle/FloatingLabelRenderingLive.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <span>
#include <string_view>

#include "Battle/FloatingLabelQueue.hpp"
#include "Battle/FloatingLabelRendering.hpp"
#include "Content/ModuleData.h"
#include "Render/GameFont.hpp"
#include "Render/SpritesLive.hpp"
#include "Render/UiPacketsLive.hpp"
#include "Render/UiTexturedQuads.hpp"

extern "C" {
#include "Battle/Feedback.h"
#include "Core/Diagnostics.h"
#include "Render/Display.h"
#include "Render/Render.h"
#include "Render/Text/CompactFont.h"
}

namespace VS::Battle {
namespace {
constexpr std::size_t kLabelBytes = 24;
constexpr std::uint32_t kHalfMask = 0xFFFF;
constexpr int kHalfShift = 16;
constexpr int kQuadLayer = -5;
constexpr int kTextLayer = -4;
constexpr std::uint8_t kFirstValueCaption = 3;
constexpr std::size_t kValueCaptionCount = 6;
constexpr std::size_t kCaptionRects = 33;
constexpr std::size_t kLockedHeightSteps = 5;
constexpr std::size_t kLockedPieceCount = 4;
constexpr std::size_t kCompactCharacterCount = 60;
constexpr std::size_t kCompactCapitalCount = 26;
constexpr std::size_t kTrimsPerByte = 4;
constexpr auto kCompactPairs = kCompactCapitalCount * kCompactCapitalCount;
constexpr auto kCompactTrimBytes = kCompactPairs / kTrimsPerByte;
constexpr std::string_view kBattleModule = "BATTLE/BATTLE.PRG";
static_assert(sizeof(Battle_FloatingLabel) == kLabelBytes);
static_assert(sizeof(u_long) == sizeof(std::uint32_t));

[[noreturn]] void Refuse(const char* reason) {
  Core_Errorf("battle.labels.render", "%s", reason);
  std::abort();
}

void RequireDataOwner(const void* owner, std::size_t bytes) {
  for (const auto& entry :
       std::span(Content_moduleData, Content_moduleDataCount)) {
    if (entry.target == owner && entry.module != nullptr &&
        entry.module == kBattleModule && entry.size >= bytes &&
        entry.capacity >= entry.size) {
      return;
    }
  }
  Refuse("Floating-label atlas or font has no bounded BATTLE data owner");
}

void RequireCaptionOwner() {
  for (const auto& entry :
       std::span(Content_moduleSymbols, Content_moduleSymbolCount)) {
    if (entry.target == Battle_kLabelCaptionRects && entry.module != nullptr &&
        entry.module == kBattleModule &&
        entry.size >= kCaptionRects * sizeof(RECT)) {
      return;
    }
  }
  Refuse("Floating-label caption atlas has no current bounded symbol owner");
}

FloatingLabelAppearance Appearance(const Battle_FloatingLabel* label) {
  if (Battle_floatingLabels != nullptr && label != nullptr) {
    for (const auto& current : *Battle_floatingLabels) {
      if (&current == label) {
        return {.elapsed_frames = current.frame,
                .value = current.value,
                .raw_mode = current.colorFlags,
                .text_length = current.textLength};
      }
    }
  }
  Refuse("Floating-label presentation is outside the current32-record owner");
}

FloatingLabelAtlasRect Atlas(const RECT& rectangle) {
  return {rectangle.x, rectangle.y, rectangle.w, rectangle.h};
}

void RequireCurrentFrame() {
  if (Render_frameBuf < 0 ||
      static_cast<std::size_t>(Render_frameBuf) >= Render::kFrameBufferCount) {
    Refuse("Floating-label frame index is outside the current frame owners");
  }
}

u_long* Primitive(std::size_t words) {
  RequireCurrentFrame();
  u_long* const primitive = Render::ReserveLivePrimitiveWords(words);
  if (primitive == nullptr ||
      reinterpret_cast<std::uintptr_t>(primitive) % alignof(u_long) != 0) {
    Refuse("Floating-label draw has no aligned current primitive capacity");
  }
  return primitive;
}

u_long* OrderingEntry(int layer) {
  RequireCurrentFrame();
  u_long* const entry =
      Render::LiveFrameOrderingEntry(Render::LiveOrderingBase::kScene, layer);
  if (entry == nullptr) {
    Refuse("Floating-label draw has no current scene ordering-table entry");
  }
  return entry;
}

class CheckedLabelQueue final : public Render::PacketQueue {
 public:
  explicit CheckedLabelQueue(int layer) : layer_(layer) {}
  [[nodiscard]] std::uint32_t ChainLink() const override {
    return *OrderingEntry(layer_);
  }
  bool Emit(std::span<const std::uint32_t> packet) override {
    static_cast<void>(Primitive(packet.size()));
    Render::LivePacketQueue queue(OrderingEntry(layer_));
    if (!queue.Emit(packet)) {
      Refuse("Floating-label packet refused by the current frame owner");
    }
    return true;
  }

 private:
  int layer_;
};

class LiveLabelRenderWorld final : public FloatingLabelRenderWorld {
 public:
  std::optional<FloatingLabelAtlasRect> ValueCaption(
      std::uint8_t raw_mode) override {
    const auto index =
        static_cast<std::uint32_t>(raw_mode) - kFirstValueCaption;
    if (index >= kValueCaptionCount) return std::nullopt;
    RequireDataOwner(Battle_valueCaptionRects,
                     kValueCaptionCount * sizeof(int));
    const auto rectangle = std::span(Battle_valueCaptionRects)[index];
    if (rectangle < 0 || static_cast<std::size_t>(rectangle) >= kCaptionRects) {
      return std::nullopt;
    }
    RequireCaptionOwner();
    return Atlas(std::span(Battle_kLabelCaptionRects,
                           kCaptionRects)[static_cast<std::size_t>(rectangle)]);
  }
  std::optional<std::int16_t> LockedHalfHeight(std::int32_t step) override {
    if (step < 0 || static_cast<std::size_t>(step) >= kLockedHeightSteps) {
      return std::nullopt;
    }
    RequireDataOwner(
        Battle_lockedLabelHalfHeights,
        kLockedHeightSteps * sizeof(Battle_lockedLabelHalfHeights[0]));
    return std::span(
        Battle_lockedLabelHalfHeights)[static_cast<std::size_t>(step)];
  }
  std::optional<FloatingLabelAtlasRect> LockedPiece(
      std::size_t piece) override {
    if (piece >= kLockedPieceCount) return std::nullopt;
    RequireDataOwner(Battle_lockedLabelPieces,
                     kLockedPieceCount * sizeof(RECT));
    return Atlas(std::span(Battle_lockedLabelPieces)[piece]);
  }
  bool DrawQuad(const FloatingLabelQuad& quad) override {
    Render::UiTexturedQuadPacket storage{};
    std::ranges::copy(std::span(Primitive(Render::kUiTexturedQuadWords),
                                Render::kUiTexturedQuadWords),
                      storage.begin());
    CheckedLabelQueue queue(kQuadLayer);
    const auto packet =
        Render::BuildUiTexturedQuadPacket(quad, storage, queue.ChainLink());
    return queue.Emit(packet);
  }
  bool DrawText(std::string_view text, FloatingLabelScreenPoint origin,
                std::uint32_t color) override {
    RequireDataOwner(Render_compactGlyphWidths, kCompactCharacterCount / 2);
    RequireDataOwner(Render_compactGlyphLeftTrim, kCompactTrimBytes);
    RequireDataOwner(Render_compactGlyphIndices, kCompactCharacterCount);
    const Render::CompactFontTables font{Render_compactGlyphWidths,
                                         Render_compactGlyphLeftTrim,
                                         Render_compactGlyphIndices};
    CheckedLabelQueue queue(kTextLayer);
    const auto position = static_cast<std::int32_t>(
        (static_cast<std::uint32_t>(origin.x) & kHalfMask) |
        (static_cast<std::uint32_t>(origin.y) << kHalfShift));
    return Render::DrawCompactText(queue, font, text, position, color);
  }
};
}  // namespace

// decomp: F00723 func_80094E18 (src/BATTLE/BATTLE.PRG/2842C.c:2138).
int DrawLiveValueLabelCaption(int x, int y, Battle_FloatingLabel* label) {
  const auto appearance = Appearance(label);
  LiveLabelRenderWorld world;
  const auto width = DrawValueLabelCaption(world, {x, y}, appearance);
  if (!width) {
    Refuse("Floating value caption has no valid selected atlas entry");
  }
  return width.value();
}

// decomp: F00724 func_8009506C (src/BATTLE/BATTLE.PRG/2842C.c:2167).
void DrawLiveLockedFloatingLabel(int x, int y, Battle_FloatingLabel* label) {
  const auto appearance = Appearance(label);
  LiveLabelRenderWorld world;
  if (!DrawLockedFloatingLabel(world, {x, y}, appearance)) {
    Refuse("Locked label has an out-of-owner animation step or atlas piece");
  }
}

// decomp: F00726 func_800957D0 (src/BATTLE/BATTLE.PRG/2842C.c:2312).
void DrawLiveStatusRemovalMarker(int x, int y, int animation_step) {
  LiveLabelRenderWorld world;
  if (!DrawStatusRemovalMarker(world, {x, y}, animation_step)) {
    Refuse("Status-removal marker could not reach its render target");
  }
}

// decomp: F00727 func_800958A4 (src/BATTLE/BATTLE.PRG/2842C.c:2332).
void DrawLiveAbilityTimingResult(int x, int y, Battle_FloatingLabel* label) {
  const auto appearance = Appearance(label);
  LiveLabelRenderWorld world;
  if (!DrawAbilityTimingResult(world, {x, y}, appearance)) {
    Refuse("Ability timing result has an out-of-owner frame or phrase index");
  }
}

// decomp: F00728 func_80095A4C (src/BATTLE/BATTLE.PRG/2842C.c:2359).
void DrawLiveChainCount(int x, int y, Battle_FloatingLabel* label) {
  const auto appearance = Appearance(label);
  LiveLabelRenderWorld world;
  if (!DrawChainCount(world, {x, y}, appearance)) {
    Refuse("Chain label has an out-of-owner frame or formatted text");
  }
}

// decomp: F00741 func_800970BC (src/BATTLE/BATTLE.PRG/2842C.c:2864).
void DrawLiveAbilityTimingCue() {
  const FloatingLabelScreenPoint origin{Battle_abilityTimingCuePosition.vx,
                                        Battle_abilityTimingCuePosition.vy};
  LiveLabelRenderWorld world;
  if (!DrawAbilityTimingCue(world, origin)) {
    Refuse("Ability timing cue could not reach its render target");
  }
}
}  // namespace VS::Battle
