#include "Battle/FloatingLabelRenderingLive.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <span>
#include <string_view>

#include "Battle/Feedback.hpp"
#include "Battle/FloatingLabelQueue.hpp"
#include "Battle/FloatingLabelRendering.hpp"
#include "Content/ModuleData.hpp"
#include "Core/Diagnostics.hpp"
#include "Render/Display.hpp"
#include "Render/GameFont.hpp"
#include "Render/Render.hpp"
#include "Render/SpritesLive.hpp"
#include "Render/Text/CompactFont.hpp"
#include "Render/UiPacketsLive.hpp"
#include "Render/UiTexturedQuads.hpp"

using VS::Legacy::RECT;

namespace VS::Battle {
namespace {
constexpr std::size_t kLabelBytes = 24;
constexpr std::uint32_t kHalfMask = 0xFFFF;
constexpr int kHalfShift = 16;
constexpr int kQuadLayer = -5;
constexpr int kTextLayer = -4;
constexpr std::uint8_t kFirstValueCaption = 3;
constexpr std::size_t kValueCaptionCount = 6;
constexpr std::size_t kLockedHeightSteps = 5;
constexpr std::size_t kLockedPieceCount = 4;
constexpr std::size_t kCompactCharacterCount = 60;
constexpr std::size_t kCompactCapitalCount = 26;
constexpr std::size_t kTrimsPerByte = 4;
constexpr auto kCompactPairs = kCompactCapitalCount * kCompactCapitalCount;
constexpr auto kCompactTrimBytes = kCompactPairs / kTrimsPerByte;
constexpr std::string_view kBattleModule = "BATTLE/BATTLE.PRG";
static_assert(sizeof(VS::Battle::FloatingLabel) == kLabelBytes);

[[noreturn]] void Refuse(const char* reason) {
  VS::Core::Errorf("battle.labels.render", "%s", reason);
  std::abort();
}

void RequireDataOwner(const void* owner, std::size_t bytes) {
  for (const auto& entry : VS::Content::ModuleDataEntries()) {
    if (entry.target == owner && entry.module != nullptr &&
        entry.module == kBattleModule && entry.size >= bytes &&
        entry.capacity >= entry.size) {
      return;
    }
  }
  Refuse("Floating-label atlas or font has no bounded BATTLE data owner");
}

void RequireCaptionOwner() {
  for (const auto& entry : VS::Content::ModuleSymbols()) {
    if (entry.target == kLabelCaptionRects.data() && entry.module != nullptr &&
        entry.module == kBattleModule &&
        entry.size >= sizeof(kLabelCaptionRects)) {
      return;
    }
  }
  Refuse("Floating-label caption atlas has no current bounded symbol owner");
}

FloatingLabelAppearance Appearance(const VS::Battle::FloatingLabel* label) {
  if (VS::Battle::floatingLabels != nullptr && label != nullptr) {
    for (const auto& current : *VS::Battle::floatingLabels) {
      if (&current == label) {
        return {.elapsed_frames = current.frame,
                .value = current.value,
                .raw_mode = current.color_flags,
                .text_length = current.text_length};
      }
    }
  }
  Refuse("Floating-label presentation is outside the current32-record owner");
}

FloatingLabelAtlasRect Atlas(const RECT& rectangle) {
  return {rectangle.x, rectangle.y, rectangle.w, rectangle.h};
}

void RequireCurrentFrame() {
  if (VS::Render::frameBuf < 0 ||
      static_cast<std::size_t>(VS::Render::frameBuf) >=
          Render::kFrameBufferCount) {
    Refuse("Floating-label frame index is outside the current frame owners");
  }
}

std::uint32_t* Primitive(std::size_t words) {
  RequireCurrentFrame();
  std::uint32_t* const primitive = Render::ReserveLivePrimitiveWords(words);
  if (primitive == nullptr ||
      reinterpret_cast<std::uintptr_t>(primitive) % alignof(std::uint32_t) !=
          0) {
    Refuse("Floating-label draw has no aligned current primitive capacity");
  }
  return primitive;
}

std::uint32_t* OrderingEntry(int layer) {
  RequireCurrentFrame();
  std::uint32_t* const entry =
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
    RequireDataOwner(VS::Battle::valueCaptionRects.data(),
                     kValueCaptionCount * sizeof(int));
    const auto rectangle = std::span(VS::Battle::valueCaptionRects)[index];
    if (rectangle < 0 ||
        static_cast<std::size_t>(rectangle) >= kLabelCaptionRects.size()) {
      return std::nullopt;
    }
    RequireCaptionOwner();
    return Atlas(
        std::span(kLabelCaptionRects)[static_cast<std::size_t>(rectangle)]);
  }
  std::optional<std::int16_t> LockedHalfHeight(std::int32_t step) override {
    if (step < 0 || static_cast<std::size_t>(step) >= kLockedHeightSteps) {
      return std::nullopt;
    }
    RequireDataOwner(
        VS::Battle::lockedLabelHalfHeights.data(),
        kLockedHeightSteps * sizeof(VS::Battle::lockedLabelHalfHeights[0]));
    return std::span(
        VS::Battle::lockedLabelHalfHeights)[static_cast<std::size_t>(step)];
  }
  std::optional<FloatingLabelAtlasRect> LockedPiece(
      std::size_t piece) override {
    if (piece >= kLockedPieceCount) return std::nullopt;
    RequireDataOwner(VS::Battle::lockedLabelPieces.data(),
                     kLockedPieceCount * sizeof(RECT));
    return Atlas(std::span(VS::Battle::lockedLabelPieces)[piece]);
  }
  bool DrawQuad(const FloatingLabelQuad& quad) override {
    return DrawLiveFloatingLabelQuad(quad, kQuadLayer);
  }
  bool DrawText(std::string_view text, FloatingLabelScreenPoint origin,
                std::uint32_t color) override {
    RequireDataOwner(VS::Render::compactGlyphWidths,
                     kCompactCharacterCount / 2);
    RequireDataOwner(VS::Render::compactGlyphLeftTrim, kCompactTrimBytes);
    RequireDataOwner(VS::Render::compactGlyphIndices, kCompactCharacterCount);
    const Render::CompactFontTables font{VS::Render::compactGlyphWidths,
                                         VS::Render::compactGlyphLeftTrim,
                                         VS::Render::compactGlyphIndices};
    CheckedLabelQueue queue(kTextLayer);
    const auto position = static_cast<std::int32_t>(
        (static_cast<std::uint32_t>(origin.x) & kHalfMask) |
        (static_cast<std::uint32_t>(origin.y) << kHalfShift));
    return Render::DrawCompactText(queue, font, text, position, color);
  }
};
}  // namespace

bool DrawLiveFloatingLabelQuad(const FloatingLabelQuad& quad, int layer) {
  Render::UiTexturedQuadPacket storage{};
  std::ranges::copy(std::span(Primitive(Render::kUiTexturedQuadWords),
                              Render::kUiTexturedQuadWords),
                    storage.begin());
  CheckedLabelQueue queue(layer);
  const auto packet =
      Render::BuildUiTexturedQuadPacket(quad, storage, queue.ChainLink());
  return queue.Emit(packet);
}

FloatingLabelRenderWorld& LiveFloatingLabelRenderer() {
  static LiveLabelRenderWorld world;
  return world;
}

// decomp: F00723 func_80094E18 (src/BATTLE/BATTLE.PRG/2842C.c:2138).
int DrawLiveValueLabelCaption(int x, int y, VS::Battle::FloatingLabel* label) {
  const auto appearance = Appearance(label);
  LiveLabelRenderWorld world;
  const auto width = DrawValueLabelCaption(world, {x, y}, appearance);
  if (!width) {
    Refuse("Floating value caption has no valid selected atlas entry");
  }
  return width.value();
}

// decomp: F00724 func_8009506C (src/BATTLE/BATTLE.PRG/2842C.c:2167).
void DrawLiveLockedFloatingLabel(int x, int y,
                                 VS::Battle::FloatingLabel* label) {
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
void DrawLiveAbilityTimingResult(int x, int y,
                                 VS::Battle::FloatingLabel* label) {
  const auto appearance = Appearance(label);
  LiveLabelRenderWorld world;
  if (!DrawAbilityTimingResult(world, {x, y}, appearance)) {
    Refuse("Ability timing result has an out-of-owner frame or phrase index");
  }
}

// decomp: F00728 func_80095A4C (src/BATTLE/BATTLE.PRG/2842C.c:2359).
void DrawLiveChainCount(int x, int y, VS::Battle::FloatingLabel* label) {
  const auto appearance = Appearance(label);
  LiveLabelRenderWorld world;
  if (!DrawChainCount(world, {x, y}, appearance)) {
    Refuse("Chain label has an out-of-owner frame or formatted text");
  }
}

// decomp: F00741 func_800970BC (src/BATTLE/BATTLE.PRG/2842C.c:2864).
void DrawLiveAbilityTimingCue() {
  const FloatingLabelScreenPoint origin{
      VS::Battle::abilityTimingCuePosition.vx,
      VS::Battle::abilityTimingCuePosition.vy};
  LiveLabelRenderWorld world;
  if (!DrawAbilityTimingCue(world, origin)) {
    Refuse("Ability timing cue could not reach its render target");
  }
}
}  // namespace VS::Battle
