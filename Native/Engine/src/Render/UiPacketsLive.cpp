// Hand-maintained (tools/port/owned.csv).
// Live binding for the HUD packets (UiPackets.cpp): the current frame's
// primitive buffer and ordering tables, the stat-gauge palette (module data)
// and the VRAM upload.
#include "Render/UiPacketsLive.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <optional>
#include <span>

#include "Render/SpritesLive.hpp"

extern "C" {
#include "Core/Diagnostics.h"
#include "Render/Frame.h"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"
}

namespace VS::Render {
namespace {
static_assert(sizeof(u_long) == sizeof(std::uint32_t));

// The overlay bucket's head entry, 0x2084 bytes into the current ordering
// table; its chain ends at the entry before it (146C.c:8055).
constexpr std::size_t kOverlayBucketHead = 0x2084 / sizeof(u_long);
// vs_battle_renderValue's default chain (5BF94.c:3037).
constexpr std::uintptr_t kNumberChainOffset = 0x10;

std::uint64_t dropped_packets = 0;
std::uint64_t unspliced_primitives = 0;
std::uint64_t truncated_numbers = 0;

// Counts an event, logging at powers of two so it cannot flood the log.
bool CountAndReport(std::uint64_t& count) {
  ++count;
  return std::has_single_bit(count);
}

class LivePacketQueue final : public PacketQueue {
 public:
  explicit LivePacketQueue(u_long* chain) : chain_(chain) {}

  [[nodiscard]] std::uint32_t ChainLink() const override {
    return chain_ != nullptr ? static_cast<std::uint32_t>(*chain_) : 0;
  }

  bool Emit(std::span<const std::uint32_t> packet) override {
    u_long* slot =
        chain_ != nullptr ? ReserveLivePrimitiveWords(packet.size()) : nullptr;
    if (slot == nullptr) {
      if (CountAndReport(dropped_packets)) {
        Core_Errorf("render",
                    "HUD packet dropped: no room in the frame primitive "
                    "buffer or no chain (%" PRIu64 " dropped)",
                    dropped_packets);
      }
      return false;
    }
    std::ranges::copy(packet, slot);
    *chain_ = LegacyGpu_Handle(slot);
    Render_frameScratch.primitiveCursor = slot + packet.size();
    return true;
  }

 private:
  u_long* chain_;
};

class LivePrimitiveChain final : public PrimitiveChain {
 public:
  [[nodiscard]] std::optional<std::uint32_t> TagWord(
      std::uint32_t handle) const override {
    const void* tag = LegacyGpu_Pointer(handle);
    if (tag == nullptr) {
      return std::nullopt;
    }
    std::uint32_t word = 0;
    std::memcpy(&word, tag, sizeof word);
    return word;
  }

  void SetTagWord(std::uint32_t handle, std::uint32_t word) override {
    void* tag = LegacyGpu_Pointer(handle);
    if (tag != nullptr) {
      std::memcpy(tag, &word, sizeof word);
    }
  }

  [[nodiscard]] std::optional<ChainBucket> OverlayBucket() const override {
    auto* table = static_cast<u_long*>(Render_legacyCurrentOrderingTable);
    if (table == nullptr) {
      return std::nullopt;
    }
    return ChainBucket{.head = static_cast<std::uint32_t>(
                           LegacyGpu_Handle(table + kOverlayBucketHead)),
                       .next = static_cast<std::uint32_t>(
                           LegacyGpu_Handle(table + kOverlayBucketHead - 1))};
  }
};

class LiveImageUpload final : public ImageUploadTarget {
 public:
  explicit LiveImageUpload(const void* pixels) : pixels_(pixels) {}

  void Upload(const ScreenRect& area) override {
    RECT rect{};
    setRECT(&rect, area.x, area.y, area.width, area.height);
    // LoadImage only reads the pixels.
    LoadImage(&rect, static_cast<u_long*>(const_cast<void*>(pixels_)));
  }

 private:
  const void* pixels_;
};

u_long* DefaultNumberChain() {
  const auto table =
      reinterpret_cast<std::uintptr_t>(Render_frameScratch.orderingTable);
  if (table < kNumberChainOffset) {
    return nullptr;
  }
  return reinterpret_cast<u_long*>(table - kNumberChainOffset);
}
}  // namespace

void AppendLiveOverlayPrimitive(void* primitive) {
  LivePrimitiveChain chain;
  const SpliceResult result =
      primitive != nullptr
          ? AppendOverlayPrimitive(
                chain, static_cast<std::uint32_t>(LegacyGpu_Handle(primitive)))
          : SpliceResult::kMissingPrimitive;
  if (result != SpliceResult::kSpliced &&
      CountAndReport(unspliced_primitives)) {
    Core_Errorf("render",
                "overlay primitive not spliced (result %d, %" PRIu64
                " refused)",
                static_cast<int>(result), unspliced_primitives);
  }
}

void DrawLiveNumericGlyph(int font, int xy, int digit, u_long* before) {
  LivePacketQueue queue(before);
  DrawNumericGlyph(queue, {.font = font, .position = xy, .digit = digit});
}

int DrawLiveIntegerRightAligned(int font, int xy, int value, u_long* before) {
  LivePacketQueue queue(before != nullptr ? before : DefaultNumberChain());
  const RightAlignedNumber number = DrawIntegerRightAligned(
      queue, {.font = font, .position = xy, .value = value});
  if (number.truncated && CountAndReport(truncated_numbers)) {
    Core_Errorf(
        "render",
        "number %d does not end its digit loop; %d digits drawn (%" PRIu64
        " truncated)",
        value, kMaxDecimalDigits, truncated_numbers);
  }
  return number.next_position;
}

void DrawLiveStatGauge(int color_index, int w, u_long* before, int xy) {
  std::array<std::uint32_t, std::size(Render_kStatGaugeColors)> palette{};
  std::ranges::transform(
      Render_kStatGaugeColors, palette.begin(),
      [](int color) { return static_cast<std::uint32_t>(color); });
  const auto index = static_cast<std::uint32_t>(color_index);
  if (index >= palette.size()) {
    Core_Errorf("render", "stat gauge colour %d is outside the palette",
                color_index);
    return;
  }
  LivePacketQueue queue(before);
  DrawStatGauge(queue, palette,
                {.color_index = index, .fill_width = w, .position = xy});
}

void InsertLiveThreeWordPrimitive(u_long* before, int rgb0, int xy, int wh) {
  LivePacketQueue queue(before);
  InsertThreeWordPrimitive(queue,
                           {.color_command = static_cast<std::uint32_t>(rgb0),
                            .position = static_cast<std::uint32_t>(xy),
                            .size = static_cast<std::uint32_t>(wh)});
}

void InsertLiveDrawModeCommand(int tpage, u_long* before) {
  LivePacketQueue queue(before);
  InsertDrawModeCommand(queue, static_cast<std::uint32_t>(tpage));
}

void UploadLiveImageRectangle(int xy, const void* pixels, int wh) {
  if (pixels == nullptr) {
    Core_Errorf("render", "image upload to 0x%08X has no pixels",
                static_cast<unsigned>(xy));
    return;
  }
  LiveImageUpload target(pixels);
  UploadImageRectangle(target, xy, wh);
}

std::uint64_t DroppedLiveUiPacketCount() { return dropped_packets; }
}  // namespace VS::Render
