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
#include <string>
#include <vector>

#include "Core/Diagnostics.hpp"
#include "Render/Display.hpp"
#include "Render/Frame.hpp"
#include "Render/NativeTextDraw.hpp"
#include "Render/Render.hpp"
#include "Render/SpritesLive.hpp"
#include "Render/UiPrimitives.hpp"
#include "Render/UiTexturedQuads.hpp"

using VS::Legacy::DrawPrim;
using VS::Legacy::GpuHandle;
using VS::Legacy::GpuPointer;
using VS::Legacy::LoadImage;
using VS::Legacy::RECT;
using VS::Legacy::setRECT;
using VS::Legacy::StoreImage;

namespace VS::Render {
namespace {
// The overlay bucket's head entry, 0x2084 bytes into the current ordering
// table; its chain ends at the entry before it (146C.c:8055).
constexpr std::size_t kOverlayBucketHead = 0x2084 / sizeof(std::uint32_t);
// vs_battle_renderValue's default chain (5BF94.c:3037).
constexpr std::uintptr_t kNumberChainOffset = 0x10;
// Both frames' ordering tables are 0x2088 bytes (Boot/GameLoop.cpp and
// Ending/Credits/StaffRoll.cpp allocate them).
constexpr std::size_t kOrderingTableEntries = 0x2088 / sizeof(std::uint32_t);

std::uint64_t droppedPackets = 0;
std::uint64_t unsplicedPrimitives = 0;
std::uint64_t truncatedNumbers = 0;
std::uint64_t refusedTexturePages = 0;
std::uint64_t refusedSprites = 0;

// Counts an event, logging at powers of two so it cannot flood the log.
bool CountAndReport(std::uint64_t& count) {
  ++count;
  return std::has_single_bit(count);
}

void ReportUnspliced(SpliceResult result) {
  if (CountAndReport(unsplicedPrimitives)) {
    VS::Core::Errorf("render",
                     "overlay primitive not spliced (result %d, %" PRIu64
                     " refused)",
                     static_cast<int>(result), unsplicedPrimitives);
  }
}

void ReportDropped() {
  if (CountAndReport(droppedPackets)) {
    VS::Core::Errorf(
        "render",
        "UI packet dropped: no room in the frame primitive buffer or "
        "no chain (%" PRIu64 " dropped)",
        droppedPackets);
  }
}

class LivePrimitiveChain final : public PrimitiveChain {
 public:
  [[nodiscard]] std::optional<std::uint32_t> TagWord(
      std::uint32_t handle) const override {
    const void* tag = GpuPointer(handle);
    if (tag == nullptr) {
      return std::nullopt;
    }
    std::uint32_t word = 0;
    std::memcpy(&word, tag, sizeof word);
    return word;
  }

  void SetTagWord(std::uint32_t handle, std::uint32_t word) override {
    void* tag = GpuPointer(handle);
    if (tag != nullptr) {
      std::memcpy(tag, &word, sizeof word);
    }
  }

  [[nodiscard]] std::optional<ChainBucket> OverlayBucket() const override {
    auto* table = static_cast<std::uint32_t*>(legacyCurrentOrderingTable);
    if (table == nullptr) {
      return std::nullopt;
    }
    return ChainBucket{.head = static_cast<std::uint32_t>(
                           GpuHandle(table + kOverlayBucketHead)),
                       .next = static_cast<std::uint32_t>(
                           GpuHandle(table + kOverlayBucketHead - 1))};
  }
};

class LiveImageUpload final : public ImageUploadTarget {
 public:
  explicit LiveImageUpload(const void* pixels) : pixels_(pixels) {}

  void Upload(const ScreenRect& area) override {
    RECT rect{};
    setRECT(&rect, area.x, area.y, area.width, area.height);
    // LoadImage only reads the pixels.
    LoadImage(&rect, static_cast<std::uint32_t*>(const_cast<void*>(pixels_)));
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
    StoreImage(&rect, static_cast<std::uint32_t*>(pixels_));
  }

 private:
  void* pixels_;
};

// Packets stored in the frame's primitive buffer and spliced last into the
// overlay bucket; the cursor passes a packet only once it is linked.
class LiveOverlayQueue final : public OverlayPacketQueue {
 public:
  bool AppendOverlay(std::span<const std::uint32_t> packet) override {
    std::uint32_t* slot = ReserveLivePrimitiveWords(packet.size());
    if (slot == nullptr) {
      ReportDropped();
      return false;
    }
    std::ranges::copy(packet, slot);
    LivePrimitiveChain chain;
    const SpliceResult result = AppendOverlayPrimitive(
        chain, static_cast<std::uint32_t>(GpuHandle(slot)));
    if (result != SpliceResult::kSpliced) {
      ReportUnspliced(result);
      return false;
    }
    FrameScratchpad().primitive_cursor = slot + packet.size();
    return true;
  }
};

class LiveImmediateTarget final : public ImmediatePacketTarget {
 public:
  void Submit(std::span<const std::uint32_t> packet) override {
    std::array<std::uint32_t, kImmediateSpriteWords> words{};
    if (packet.size() > words.size()) {
      ReportDropped();
      return;
    }
    std::ranges::copy(packet, words.begin());
    DrawPrim(words.data());
  }
};

std::uint32_t* DefaultNumberChain() {
  const auto table =
      reinterpret_cast<std::uintptr_t>(FrameScratchpad().ordering_table);
  if (table < kNumberChainOffset) {
    return nullptr;
  }
  return reinterpret_cast<std::uint32_t*>(table - kNumberChainOffset);
}
}  // namespace

std::uint32_t LivePacketQueue::ChainLink() const {
  return chain_ != nullptr ? static_cast<std::uint32_t>(*chain_) : 0;
}

bool LivePacketQueue::CanEmit(std::size_t words) const {
  if (chain_ == nullptr || ReserveLivePrimitiveWords(words) == nullptr) {
    ReportDropped();
    return false;
  }
  return true;
}

bool LivePacketQueue::Emit(std::span<const std::uint32_t> packet) {
  std::uint32_t* slot =
      chain_ != nullptr ? ReserveLivePrimitiveWords(packet.size()) : nullptr;
  if (slot == nullptr) {
    ReportDropped();
    return false;
  }
  std::ranges::copy(packet, slot);
  *chain_ = GpuHandle(slot);
  FrameScratchpad().primitive_cursor = slot + packet.size();
  return true;
}

std::uint32_t* LiveFrameOrderingEntry(LiveOrderingBase base, int slot) {
  void* const frame_table = legacyOrderingTables[frameBuf & 1];
  const auto table = reinterpret_cast<std::uintptr_t>(frame_table);
  const auto head = reinterpret_cast<std::uintptr_t>(
      base == LiveOrderingBase::kScene
          ? FrameScratchpad().ordering_table
          : FrameScratchpad().overlay_ordering_table);
  if (table == 0 || head < table ||
      (head - table) % sizeof(std::uint32_t) != 0) {
    return nullptr;
  }
  const auto index =
      static_cast<std::int64_t>((head - table) / sizeof(std::uint32_t));
  const std::int64_t entry = index + slot;
  if (entry < 0 || entry >= static_cast<std::int64_t>(kOrderingTableEntries)) {
    return nullptr;
  }
  return static_cast<std::uint32_t*>(frame_table) + entry;
}

void AppendLiveOverlayPrimitive(void* primitive) {
  LivePrimitiveChain chain;
  const SpliceResult result =
      primitive != nullptr
          ? AppendOverlayPrimitive(
                chain, static_cast<std::uint32_t>(GpuHandle(primitive)))
          : SpliceResult::kMissingPrimitive;
  if (result != SpliceResult::kSpliced) {
    ReportUnspliced(result);
  }
}

void DrawLiveNumericGlyph(int font, int xy, int digit, std::uint32_t* before) {
  LivePacketQueue queue(before);
  DrawNumericGlyph(queue, {.font = font, .position = xy, .digit = digit});
}

int DrawLiveIntegerRightAligned(int font, int xy, int value,
                                std::uint32_t* before) {
  LivePacketQueue queue(before != nullptr ? before : DefaultNumberChain());
  const RightAlignedNumber number = DrawIntegerRightAligned(
      queue, {.font = font, .position = xy, .value = value});
  if (number.truncated && CountAndReport(truncatedNumbers)) {
    VS::Core::Errorf(
        "render",
        "number %d does not end its digit loop; %d digits drawn (%" PRIu64
        " truncated)",
        value, kMaxDecimalDigits, truncatedNumbers);
  }
  return number.next_position;
}

void DrawLiveStatGauge(int color_index, int w, std::uint32_t* before, int xy) {
  std::array<std::uint32_t, std::size(kStatGaugeColors)> palette{};
  std::ranges::transform(kStatGaugeColors, palette.begin(), [](int color) {
    return static_cast<std::uint32_t>(color);
  });
  const auto index = static_cast<std::uint32_t>(color_index);
  if (index >= palette.size()) {
    VS::Core::Errorf("render", "stat gauge colour %d is outside the palette",
                     color_index);
    return;
  }
  LivePacketQueue queue(before);
  DrawStatGauge(queue, palette,
                {.color_index = index, .fill_width = w, .position = xy});
}

void InsertLiveThreeWordPrimitive(std::uint32_t* before, int rgb0, int xy,
                                  int wh) {
  LivePacketQueue queue(before);
  InsertThreeWordPrimitive(queue,
                           {.color_command = static_cast<std::uint32_t>(rgb0),
                            .position = static_cast<std::uint32_t>(xy),
                            .size = static_cast<std::uint32_t>(wh)});
}

void InsertLiveDrawModeCommand(int tpage, std::uint32_t* before) {
  LivePacketQueue queue(before);
  InsertDrawModeCommand(queue, static_cast<std::uint32_t>(tpage));
}

void UploadLiveImageRectangle(int xy, const void* pixels, int wh) {
  if (pixels == nullptr) {
    VS::Core::Errorf("render", "image upload to 0x%08X has no pixels",
                     static_cast<unsigned>(xy));
    return;
  }
  LiveImageUpload target(pixels);
  UploadImageRectangle(target, xy, wh);
}

void ReadLiveImageRectangle(int xy, void* pixels, int wh) {
  if (pixels == nullptr) {
    VS::Core::Errorf("render", "image read from 0x%08X has no buffer",
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
  std::uint32_t* entry =
      LiveFrameOrderingEntry(LiveOrderingBase::kScene, ot_slot);
  if (entry == nullptr) {
    if (CountAndReport(refusedTexturePages)) {
      VS::Core::Errorf("render",
                       "texture page 0x%X refused: ordering-table entry %d is "
                       "outside the frame's table (%" PRIu64 " refused)",
                       static_cast<unsigned>(tpage), ot_slot,
                       refusedTexturePages);
    }
    return FrameScratchpad().primitive_cursor;
  }
  LivePacketQueue queue(entry);
  InsertTexturePageCommand(queue, static_cast<std::uint32_t>(tpage));
  return FrameScratchpad().primitive_cursor;
}

void DrawLiveImmediateSprite(int xy, int uv_clut, int wh, int tpage) {
  LiveImmediateTarget target;
  if (!DrawImmediateSprite(target, {.position = xy,
                                    .uv_clut = uv_clut,
                                    .size = wh,
                                    .texture_page = tpage}) &&
      CountAndReport(refusedSprites)) {
    VS::Core::Errorf("render",
                     "sprite at 0x%08X refused: texture page 0x%08X dims it "
                     "outside a colour byte (%" PRIu64 " refused)",
                     static_cast<unsigned>(xy), static_cast<unsigned>(tpage),
                     refusedSprites);
  }
}

std::uint64_t DroppedLiveUiPacketCount() {
  return droppedPackets + refusedTexturePages + refusedSprites;
}

std::expected<void, std::string> DrawLiveNativeText(
    std::span<const NativeTextDraw> draws, const Content::NativeImage& atlas) {
  constexpr int kFontPaletteColors = 16;
  class Collector final : public ImmediatePacketTarget {
   public:
    std::vector<UiTexturedQuadPacket> packets;
    void Submit(std::span<const std::uint32_t> packet) override {
      UiTexturedQuadPacket owned{};
      std::ranges::copy(packet, owned.begin());
      packets.push_back(owned);
    }
  } target;
  auto checked = DrawNativeText(target, draws, atlas);
  if (!checked) return checked;
  const auto pixels = Content::PackedWords(atlas);
  std::vector<std::uint16_t> previous(pixels.size());
  std::array<std::uint16_t, kFontPaletteColors> palette{};
  const int position = PackScreenWord(832, 0);
  const int size = PackScreenWord(static_cast<int>(atlas.width / 4),
                                  static_cast<int>(atlas.height));
  const int clut = PackScreenWord(896, 222);
  const int clut_size = PackScreenWord(kFontPaletteColors, 1);
  ReadLiveImageRectangle(position, previous.data(), size);
  ReadLiveImageRectangle(clut, palette.data(), clut_size);
  UploadLiveImageRectangle(position, pixels.data(), size);
  UploadLiveImageRectangle(clut, atlas.palettes.front().data(), clut_size);
  for (auto& packet : target.packets) DrawPrim(packet.data());
  UploadLiveImageRectangle(position, previous.data(), size);
  UploadLiveImageRectangle(clut, palette.data(), clut_size);
  return {};
}
}  // namespace VS::Render
