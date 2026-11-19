// Hand-maintained (tools/port/owned.csv).
// Live binding for the manual illustrations (ManualIllustration.cpp): the help
// files on the game heap, the page's scroll position, VRAM and the current
// frame's primitive buffer.
#include "Menu/ManualIllustrationLive.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#include "Core/Log.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Legacy/Psx.h"
#include "Menu/Manual.hpp"
#include "Menu/ManualIllustration.hpp"
#include "Render/UiPackets.hpp"
#include "Render/UiPacketsLive.hpp"

namespace VS::Menu::manual {
namespace {
static_assert(sizeof(u_long) == sizeof(std::uint32_t));
static_assert(sizeof(VisibilityAnimation) ==
              kAnimationHalfwords * sizeof(std::int16_t));

constexpr std::uint32_t kRefusalLogLimit = 16;
constexpr std::uint32_t kRefusalLogInterval = 1024;
// SPRT: a raw-texture sprite of free size (setSprt, setShadeTex).
constexpr std::uint32_t kSpriteCode = 0x65;
constexpr std::size_t kSpriteWords = 5;
// LINE_F2: a flat line (setLineF2).
constexpr std::uint32_t kLineCode = 0x40;
constexpr std::size_t kLineWords = 4;
// The texture page's draw-mode packet (AppendLiveTexturePage).
constexpr std::size_t kTexturePageWords = 2;
constexpr int kUvRowShift = 8;
constexpr int kUvClutShift = 16;
std::uint64_t g_refusals = 0;

void Refuse(std::string_view reason) {
  ++g_refusals;
  if (g_refusals <= kRefusalLogLimit || g_refusals % kRefusalLogInterval == 0) {
    VS::Core::LogWarn("menu.manual", "{} (refusal {})", reason, g_refusals);
  }
}

// The elements from `first` to the end of the live heap payload holding it.
template <typename T>
std::optional<std::span<T>> LivePayloadTail(T* first) {
  if (first == nullptr ||
      reinterpret_cast<std::uintptr_t>(first) % alignof(T) != 0) {
    return std::nullopt;
  }
  const auto payload = Core::Memory::LivePayloadContaining(first);
  if (!payload) {
    return std::nullopt;
  }
  const auto* begin = reinterpret_cast<const std::byte*>(first);
  const std::byte* end = payload->data() + payload->size();
  if (begin < payload->data() || begin >= end) {
    return std::nullopt;
  }
  return std::span<T>(first, static_cast<std::size_t>(end - begin) / sizeof(T));
}

class LiveUploader final : public ImageUploader {
 public:
  void Upload(const Render::ScreenRect& area,
              std::span<const std::uint32_t> pixels) override {
    RECT rect{};
    setRECT(&rect, area.x, area.y, area.width, area.height);
    // LoadImage only reads the pixels.
    LoadImage(&rect, reinterpret_cast<u_long*>(
                         const_cast<std::uint32_t*>(pixels.data())));
  }
};

void UploadLive(const u_long* buffer, int count, bool cluts) {
  if (count <= 0) {
    return;
  }
  const auto words =
      LivePayloadTail(reinterpret_cast<const std::uint32_t*>(buffer));
  if (!words) {
    Refuse(cluts ? "Manual CLUT upload buffer is outside the game heap"
                 : "Manual tile upload buffer is outside the game heap");
    return;
  }
  LiveUploader uploader;
  const UploadResult result = cluts
                                  ? UploadCluts(*words, count, uploader)
                                  : UploadSpriteTiles(*words, count, uploader);
  if (result == UploadResult::kTooManyImages) {
    Refuse("Manual upload count exceeds its VRAM grid");
  } else if (result == UploadResult::kShortBuffer) {
    Refuse("Manual upload count runs past its help asset buffer");
  }
}

// The frame's first scene ordering-table entry, the original's scratch[1].
std::optional<Render::LivePacketQueue> SceneQueue(std::size_t words) {
  u_long* const entry =
      Render::LiveFrameOrderingEntry(Render::LiveOrderingBase::kScene, 0);
  if (entry == nullptr) {
    Refuse("Manual illustration has no frame ordering table");
    return std::nullopt;
  }
  Render::LivePacketQueue queue(entry);
  if (!queue.CanEmit(words)) {
    return std::nullopt;
  }
  return queue;
}

class LiveIllustrationTarget final : public IllustrationTarget {
 public:
  void DrawTile(const IllustrationTile& tile) override {
    auto queue = SceneQueue(kSpriteWords + kTexturePageWords);
    if (!queue) {
      return;
    }
    const std::array<std::uint32_t, kSpriteWords> sprite{
        Render::PackTagWord(kSpriteWords - 1, queue->ChainLink()),
        Render::PackColorCommand(kSpriteCode, 0, 0, 0),
        static_cast<std::uint32_t>(Render::PackScreenWord(tile.x, tile.y)),
        static_cast<std::uint32_t>(tile.u) |
            (static_cast<std::uint32_t>(tile.v) << kUvRowShift) |
            (static_cast<std::uint32_t>(tile.clut) << kUvClutShift),
        static_cast<std::uint32_t>(
            Render::PackScreenWord(tile.width, tile.height))};
    if (queue->Emit(sprite)) {
      Render::AppendLiveTexturePage(tile.texture_page, 0);
    }
  }
};

class LiveDiagramLineTarget final : public DiagramLineTarget {
 public:
  void DrawLine(const DiagramLine& line) override {
    auto queue = SceneQueue(kLineWords);
    if (!queue) {
      return;
    }
    const std::array<std::uint32_t, kLineWords> packet{
        Render::PackTagWord(kLineWords - 1, queue->ChainLink()),
        Render::PackColorCommand(kLineCode, line.red, line.green, line.blue),
        static_cast<std::uint32_t>(Render::PackScreenWord(line.x0, line.y0)),
        static_cast<std::uint32_t>(Render::PackScreenWord(line.x1, line.y1))};
    queue->Emit(packet);
  }
};
}  // namespace

// decomp: MENUE.PRG _copySprites (494.c:481).
void LiveUploadSpriteTiles(u_long const* buffer, int count) {
  UploadLive(buffer, count, false);
}

// decomp: MENUE.PRG _copyCluts (494.c:498).
void LiveUploadCluts(u_long* buffer, int count) {
  UploadLive(buffer, count, true);
}

// decomp: MENUE.PRG _drawSprite (494.c:629).
void LiveDrawIllustration(std::int16_t* data) {
  const auto record = LivePayloadTail(reinterpret_cast<std::int16_t*>(data));
  if (!record) {
    Refuse("Manual illustration record is outside the game heap");
    return;
  }
  LiveIllustrationTarget target;
  if (DrawIllustration(*record, scrollPosition, target) ==
      DrawResult::kRefused) {
    Refuse("Manual illustration record runs past its help text");
  }
}

// decomp: MENUE.PRG _drawContentLines (494.c:883).
void LiveDrawDiagramLines() {
  const auto help_text =
      LivePayloadTail(reinterpret_cast<std::int16_t*>(helpText));
  if (!help_text) {
    Refuse("Manual help text is outside the game heap");
    return;
  }
  LiveDiagramLineTarget target;
  if (DrawDiagramLines(*help_text, scrollPosition, target) ==
      DrawResult::kRefused) {
    Refuse("Manual diagram lines run past their help text");
  }
}

// decomp: MENUE.PRG _getAnimationState (494.c:940).
std::int16_t LiveAdvanceVisibilityAnimation(VisibilityAnimation* state) {
  if (state == nullptr) {
    Refuse("Manual visibility animation is null");
    return 0;
  }
  return AdvanceVisibilityAnimation(*state);
}

std::uint64_t LiveIllustrationRefusalCount() { return g_refusals; }
}  // namespace VS::Menu::manual
