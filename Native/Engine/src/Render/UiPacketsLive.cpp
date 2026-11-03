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
#include "Render/Display.h"
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
// Both frames' ordering tables are 0x2088 bytes (Boot/GameLoop.cpp and
// Ending/Credits/StaffRoll.cpp allocate them).
constexpr std::size_t kOrderingTableEntries = 0x2088 / sizeof(u_long);

std::uint64_t dropped_packets = 0;
std::uint64_t unspliced_primitives = 0;
std::uint64_t truncated_numbers = 0;
std::uint64_t refused_texture_pages = 0;
std::uint64_t refused_sprites = 0;

// Counts an event, logging at powers of two so it cannot flood the log.
bool CountAndReport(std::uint64_t& count) {
  ++count;
  return std::has_single_bit(count);
}

void ReportUnspliced(SpliceResult result) {
  if (CountAndReport(unspliced_primitives)) {
    Core_Errorf("render",
                "overlay primitive not spliced (result %d, %" PRIu64
                " refused)",
                static_cast<int>(result), unspliced_primitives);
  }
}

void ReportDropped() {
  if (CountAndReport(dropped_packets)) {
    Core_Errorf("render",
                "UI packet dropped: no room in the frame primitive buffer or "
                "no chain (%" PRIu64 " dropped)",
                dropped_packets);
  }
}

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

class LiveImageRead final : public ImageReadTarget {
 public:
  explicit LiveImageRead(void* pixels) : pixels_(pixels) {}

  void Read(const ScreenRect& area) override {
    RECT rect{};
    setRECT(&rect, area.x, area.y, area.width, area.height);
    StoreImage(&rect, static_cast<u_long*>(pixels_));
  }

 private:
  void* pixels_;
};

// Packets stored in the frame's primitive buffer and spliced last into the
// overlay bucket; the cursor passes a packet only once it is linked.
class LiveOverlayQueue final : public OverlayPacketQueue {
 public:
  bool AppendOverlay(std::span<const std::uint32_t> packet) override {
    u_long* slot = ReserveLivePrimitiveWords(packet.size());
    if (slot == nullptr) {
      ReportDropped();
      return false;
    }
    std::ranges::copy(packet, slot);
    LivePrimitiveChain chain;
    const SpliceResult result = AppendOverlayPrimitive(
        chain, static_cast<std::uint32_t>(LegacyGpu_Handle(slot)));
    if (result != SpliceResult::kSpliced) {
      ReportUnspliced(result);
      return false;
    }
    Render_frameScratch.primitiveCursor = slot + packet.size();
    return true;
  }
};

class LiveImmediateTarget final : public ImmediatePacketTarget {
 public:
  void Submit(std::span<const std::uint32_t> packet) override {
    std::array<u_long, kImmediateSpriteWords> words{};
    if (packet.size() > words.size()) {
      ReportDropped();
      return;
    }
    std::ranges::copy(packet, words.begin());
    DrawPrim(words.data());
  }
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

std::uint32_t LivePacketQueue::ChainLink() const {
  return chain_ != nullptr ? static_cast<std::uint32_t>(*chain_) : 0;
}

bool LivePacketQueue::Emit(std::span<const std::uint32_t> packet) {
  u_long* slot =
      chain_ != nullptr ? ReserveLivePrimitiveWords(packet.size()) : nullptr;
  if (slot == nullptr) {
    ReportDropped();
    return false;
  }
  std::ranges::copy(packet, slot);
  *chain_ = LegacyGpu_Handle(slot);
  Render_frameScratch.primitiveCursor = slot + packet.size();
  return true;
}

u_long* LiveFrameOrderingEntry(LiveOrderingBase base, int slot) {
  void* const frame_table = Render_legacyOrderingTables[Render_frameBuf & 1];
  const auto table = reinterpret_cast<std::uintptr_t>(frame_table);
  const auto head = reinterpret_cast<std::uintptr_t>(
      base == LiveOrderingBase::kScene
          ? Render_frameScratch.orderingTable
          : Render_frameScratch.overlayOrderingTable);
  if (table == 0 || head < table || (head - table) % sizeof(u_long) != 0) {
    return nullptr;
  }
  const auto index = static_cast<std::int64_t>((head - table) / sizeof(u_long));
  const std::int64_t entry = index + slot;
  if (entry < 0 || entry >= static_cast<std::int64_t>(kOrderingTableEntries)) {
    return nullptr;
  }
  return static_cast<u_long*>(frame_table) + entry;
}

void AppendLiveOverlayPrimitive(void* primitive) {
  LivePrimitiveChain chain;
  const SpliceResult result =
      primitive != nullptr
          ? AppendOverlayPrimitive(
                chain, static_cast<std::uint32_t>(LegacyGpu_Handle(primitive)))
          : SpliceResult::kMissingPrimitive;
  if (result != SpliceResult::kSpliced) {
    ReportUnspliced(result);
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

void ReadLiveImageRectangle(int xy, void* pixels, int wh) {
  if (pixels == nullptr) {
    Core_Errorf("render", "image read from 0x%08X has no buffer",
                static_cast<unsigned>(xy));
    return;
  }
  LiveImageRead target(pixels);
  ReadImageRectangle(target, xy, wh);
}

void QueueLiveTextureRectangleMove(const ScreenRect& source, std::int32_t x,
                                   std::int32_t y) {
  LiveOverlayQueue queue;
  QueueTextureRectangleMove(queue, source, x, y);
}

void QueueLiveTextureRectangleMove(const RECT* source, int x, int y) {
  if (source == nullptr) {
    ReportDropped();
    return;
  }
  QueueLiveTextureRectangleMove(ScreenRect{.x = source->x,
                                           .y = source->y,
                                           .width = source->w,
                                           .height = source->h},
                                x, y);
}

void* AppendLiveTexturePage(int tpage, int ot_slot) {
  u_long* entry = LiveFrameOrderingEntry(LiveOrderingBase::kScene, ot_slot);
  if (entry == nullptr) {
    if (CountAndReport(refused_texture_pages)) {
      Core_Errorf("render",
                  "texture page 0x%X refused: ordering-table entry %d is "
                  "outside the frame's table (%" PRIu64 " refused)",
                  static_cast<unsigned>(tpage), ot_slot, refused_texture_pages);
    }
    return Render_frameScratch.primitiveCursor;
  }
  LivePacketQueue queue(entry);
  InsertTexturePageCommand(queue, static_cast<std::uint32_t>(tpage));
  return Render_frameScratch.primitiveCursor;
}

void DrawLiveImmediateSprite(int xy, int uv_clut, int wh, int tpage) {
  LiveImmediateTarget target;
  if (!DrawImmediateSprite(target, {.position = xy,
                                    .uv_clut = uv_clut,
                                    .size = wh,
                                    .texture_page = tpage}) &&
      CountAndReport(refused_sprites)) {
    Core_Errorf("render",
                "sprite at 0x%08X refused: texture page 0x%08X dims it "
                "outside a colour byte (%" PRIu64 " refused)",
                static_cast<unsigned>(xy), static_cast<unsigned>(tpage),
                refused_sprites);
  }
}

std::uint64_t DroppedLiveUiPacketCount() {
  return dropped_packets + refused_texture_pages + refused_sprites;
}
}  // namespace VS::Render
