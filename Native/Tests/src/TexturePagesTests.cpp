#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Legacy/Psx.h"
#include "Render/TexturePages.hpp"
#include "Render/TexturePagesLive.hpp"
#include "Render/TimImage.hpp"
#include "Render/TimImageLive.hpp"
#include "Render/UiPackets.hpp"
#include "Render/UiPacketsLive.hpp"

extern "C" {
#include "Render/Display.h"
#include "Render/Frame.h"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"
}

// Contracts of the #276 row 6 texture slice: the lendable texture pages
// (146C.c:9480-9549), the TIM parse (146C.c:16935, D4.c:2235), the texture
// move (40564.c:110-1278), the texture-page command (D4.c:1103), the file
// menu's immediate sprite and colour blend (260.c:199-1718).
namespace {
namespace Render = VS::Render;
using Render::TexturePage;
using Render::TexturePageRange;
using Render::TexturePageResult;
using Render::TexturePageState;

constexpr TexturePage kFree{};
constexpr TexturePage kResident{.state = TexturePageState::kResident};
constexpr TexturePage kRetiring{.state = TexturePageState::kRetiring};
constexpr TexturePage kRetired{.state = TexturePageState::kRetired};

constexpr TexturePage Owned(std::uint8_t owner) {
  return {.state = TexturePageState::kOwned, .owner = owner};
}

Render::TexturePageTable InitializedPages() {
  Render::TexturePageTable pages{};
  Render::InitializeTexturePages(pages);
  return pages;
}

// --- Texture pages -------------------------------------------------------

TEST_CASE("Texture pages start resident below 25 and free above",
          "[render-textures]") {
  Render::TexturePageTable pages{};
  pages.fill(Owned(4));
  Render::InitializeTexturePages(pages);

  CHECK(std::all_of(pages.begin(), pages.begin() + 25,
                    [](const TexturePage& page) { return page == kResident; }));
  CHECK(std::all_of(pages.begin() + 25, pages.end(),
                    [](const TexturePage& page) { return page == kFree; }));
}

TEST_CASE("Reserving lends every free page of the range to its owner",
          "[render-textures]") {
  Render::TexturePageTable pages = InitializedPages();

  CHECK(Render::ReserveTexturePages(pages, {.first = 29, .count = 3}, 5) ==
        TexturePageResult::kDone);
  CHECK(pages[28] == kFree);
  CHECK(pages[29] == Owned(5));
  CHECK(pages[31] == Owned(5));
  // An empty range within bounds succeeds and lends nothing.
  CHECK(Render::ReserveTexturePages(pages, {.first = 32, .count = 0}, 5) ==
        TexturePageResult::kDone);
  CHECK(Render::ReserveTexturePages(pages, {.first = 25, .count = 0}, 6) ==
        TexturePageResult::kDone);
  CHECK(pages[25] == kFree);
}

TEST_CASE("Reservations outside pages 25-31 are refused, wrapping included",
          "[render-textures]") {
  Render::TexturePageTable pages = InitializedPages();
  const Render::TexturePageTable before = pages;

  CHECK(Render::ReserveTexturePages(pages, {.first = 24, .count = 1}, 5) ==
        TexturePageResult::kOutOfRange);
  CHECK(Render::ReserveTexturePages(pages, {.first = 31, .count = 2}, 5) ==
        TexturePageResult::kOutOfRange);
  CHECK(Render::ReserveTexturePages(pages, {.first = 33, .count = 0}, 5) ==
        TexturePageResult::kOutOfRange);
  // 29 + 0xFFFFFFFF wrapped to 28 and passed the original's check.
  CHECK(Render::ReserveTexturePages(pages, {.first = 29, .count = 0xFFFFFFFFU},
                                    5) == TexturePageResult::kOutOfRange);
  CHECK(Render::RetireTexturePages(pages, {.first = 0, .count = 1}, 5) ==
        TexturePageResult::kOutOfRange);
  CHECK(pages == before);
}

TEST_CASE("Occupied pages and owners that read as states are refused",
          "[render-textures]") {
  Render::TexturePageTable pages = InitializedPages();
  REQUIRE(Render::ReserveTexturePages(pages, {.first = 29, .count = 2}, 5) ==
          TexturePageResult::kDone);
  const Render::TexturePageTable before = pages;

  CHECK(Render::ReserveTexturePages(pages, {.first = 30, .count = 2}, 6) ==
        TexturePageResult::kOccupied);
  // Owner 0 read as free and 0xFD-0xFF as retiring, retired or resident.
  CHECK(Render::ReserveTexturePages(pages, {.first = 26, .count = 1}, 0) ==
        TexturePageResult::kInvalidOwner);
  CHECK(Render::ReserveTexturePages(pages, {.first = 26, .count = 1}, 0xFD) ==
        TexturePageResult::kInvalidOwner);
  CHECK(Render::RetireTexturePages(pages, {.first = 29, .count = 2}, 0x105) ==
        TexturePageResult::kInvalidOwner);
  CHECK(Render::ReserveTexturePages(pages, {.first = 26, .count = 1}, 0xFC) ==
        TexturePageResult::kDone);
  CHECK(pages[31] == before[31]);
  CHECK(pages[29] == Owned(5));
}

TEST_CASE("Retiring needs the owner on every page and frees after two frames",
          "[render-textures]") {
  Render::TexturePageTable pages = InitializedPages();
  REQUIRE(Render::ReserveTexturePages(pages, {.first = 26, .count = 3}, 6) ==
          TexturePageResult::kDone);

  CHECK(Render::RetireTexturePages(pages, {.first = 26, .count = 3}, 5) ==
        TexturePageResult::kOwnerMismatch);
  CHECK(Render::RetireTexturePages(pages, {.first = 26, .count = 4}, 6) ==
        TexturePageResult::kOwnerMismatch);
  CHECK(pages[28] == Owned(6));
  CHECK(Render::RetireTexturePages(pages, {.first = 26, .count = 3}, 6) ==
        TexturePageResult::kDone);
  CHECK(pages[27] == kRetiring);
  Render::AdvanceTexturePageRetirements(pages);
  CHECK(pages[27] == kRetired);
  CHECK(Render::ReserveTexturePages(pages, {.first = 27, .count = 1}, 6) ==
        TexturePageResult::kOccupied);
  Render::AdvanceTexturePageRetirements(pages);
  CHECK(pages[27] == kFree);
}

TEST_CASE("Advancing leaves resident and owned pages alone",
          "[render-textures]") {
  Render::TexturePageTable pages = InitializedPages();
  pages[25] = Owned(2);
  pages[26] = kRetiring;
  pages[27] = kRetired;
  Render::AdvanceTexturePageRetirements(pages);

  CHECK(pages[0] == kResident);
  CHECK(pages[25] == Owned(2));
  CHECK(pages[26] == kRetired);
  CHECK(pages[27] == kFree);
}

struct LivePagesFixture {
  Render::TexturePageTable saved_pages = Render::LiveTexturePages();

  LivePagesFixture() = default;
  LivePagesFixture(const LivePagesFixture&) = delete;
  LivePagesFixture& operator=(const LivePagesFixture&) = delete;
  ~LivePagesFixture() { Render::LiveTexturePages() = saved_pages; }
};

TEST_CASE("Live texture pages keep the original return values",
          "[render-textures]") {
  const LivePagesFixture fixture;
  Render::InitializeLiveTexturePages();
  const std::uint64_t refused = Render::RefusedLiveTexturePageRequestCount();

  CHECK(Render::ReserveLiveTexturePageRange(0x1D, 3, 5) == 0x1D);
  CHECK(Render::ReserveLiveTexturePageRange(0x1D, 1, 5) == 0);
  CHECK(Render::RetireLiveTexturePageRange(0x1D, 3, 6) == 0);
  CHECK(Render::RefusedLiveTexturePageRequestCount() == refused + 2);
  CHECK(Render::RetireLiveTexturePageRange(0x1D, 3, 5) == 1);
  Render::AdvanceLiveTexturePageRetirements();
  CHECK(Render::LiveTexturePages()[0x1D] == kRetired);
}

// --- TIM images ----------------------------------------------------------

class TimBuilder {
 public:
  explicit TimBuilder(std::uint32_t mode) {
    Word(0x10);
    Word(mode);
  }

  // A block: its byte length, rectangle and width x height halfwords.
  TimBuilder& Block(std::uint32_t length, std::int16_t x, std::int16_t y,
                    std::uint16_t width, std::uint16_t height) {
    Word(length);
    Half(static_cast<std::uint16_t>(x));
    Half(static_cast<std::uint16_t>(y));
    Half(width);
    Half(height);
    for (std::size_t pixel = 0; pixel < std::size_t{width} * height; ++pixel) {
      Half(static_cast<std::uint16_t>(pixel));
    }
    return *this;
  }

  TimBuilder& Pad(std::size_t bytes) {
    data_.resize(data_.size() + bytes);
    return *this;
  }

  [[nodiscard]] std::vector<std::byte>& Bytes() { return data_; }

 private:
  void Word(std::uint32_t value) {
    Half(static_cast<std::uint16_t>(value));
    Half(static_cast<std::uint16_t>(value >> 16));
  }
  void Half(std::uint16_t value) {
    data_.push_back(static_cast<std::byte>(value & 0xFF));
    data_.push_back(static_cast<std::byte>(value >> 8));
  }

  std::vector<std::byte> data_;
};

// A 4-bit TIM: a 16-colour CLUT block at byte 8, then a 4x2 image block.
std::vector<std::byte> ClutTim(std::uint32_t clut_length = 44) {
  TimBuilder tim(0x08);
  tim.Block(clut_length, 0, 480, 16, 1);
  if (clut_length > 44) {
    tim.Pad(clut_length - 44);
  }
  tim.Block(28, 640, 0, 4, 2);
  return tim.Bytes();
}

TEST_CASE("TIM parse locates the CLUT and image blocks", "[render-textures]") {
  const std::vector<std::byte> data = ClutTim();
  const std::optional<Render::TimImage> parsed = Render::ParseTim(data);

  REQUIRE(parsed.has_value());
  const Render::TimImage tim = parsed.value_or(Render::TimImage{});
  CHECK(tim.mode == 0x08);
  REQUIRE(tim.clut.has_value());
  const Render::TimBlock clut = tim.clut.value_or(Render::TimBlock{});
  CHECK(clut.area == Render::ScreenRect{0, 480, 16, 1});
  CHECK(clut.rect_offset == std::size_t{12});
  CHECK(clut.pixel_offset == std::size_t{20});
  CHECK(tim.image.area == Render::ScreenRect{640, 0, 4, 2});
  CHECK(tim.image.rect_offset == std::size_t{56});
  CHECK(tim.image.pixel_bytes == std::size_t{16});
}

TEST_CASE("TIM parse without a CLUT starts the image at byte 8",
          "[render-textures]") {
  TimBuilder builder(0x02);
  builder.Block(20, -4, 16, 2, 2);
  const std::optional<Render::TimImage> parsed =
      Render::ParseTim(builder.Bytes());

  REQUIRE(parsed.has_value());
  const Render::TimImage tim = parsed.value_or(Render::TimImage{});
  CHECK_FALSE(tim.clut.has_value());
  CHECK(tim.image.area == Render::ScreenRect{-4, 16, 2, 2});
  CHECK(tim.image.rect_offset == std::size_t{12});
  CHECK(tim.image.pixel_offset == std::size_t{20});
}

TEST_CASE("TIM parse rounds the CLUT length down to a word",
          "[render-textures]") {
  // The original stepped data[0] / 4 words past the CLUT block.
  CHECK(Render::TimImageBlockOffset(0x08, 47) == std::size_t{8 + 44});
  CHECK(Render::TimImageBlockOffset(0x09, 44) == std::size_t{8 + 44});
  CHECK(Render::TimImageBlockOffset(0x02, 0xFFFFFFFF) == std::size_t{8});
  CHECK_FALSE(Render::TimImageBlockOffset(0x08, 11).has_value());
  CHECK_FALSE(
      Render::TimImageBlockOffset(0x08, 12 + (1024 * 512 * 2) + 1).has_value());
  CHECK(Render::DeclaredTimSize(52, 4, 2) == std::size_t{52 + 12 + 16});
  CHECK_FALSE(Render::DeclaredTimSize(8, 1025, 1).has_value());
  CHECK_FALSE(Render::DeclaredTimSize(8, 1, 513).has_value());
}

TEST_CASE("TIM parse refuses truncated or inconsistent blocks",
          "[render-textures]") {
  std::vector<std::byte> data = ClutTim();
  CHECK_FALSE(
      Render::ParseTim(std::span(data).first(data.size() - 1)).has_value());
  CHECK_FALSE(Render::ParseTim(std::span(data).first(7)).has_value());
  // A CLUT block whose length cannot hold its own pixels.
  data[8] = std::byte{40};
  CHECK_FALSE(Render::ParseTim(data).has_value());

  TimBuilder wide(0x02);
  wide.Block(12, 0, 0, 1025, 0);
  CHECK_FALSE(Render::ParseTim(wide.Bytes()).has_value());
  TimBuilder empty(0x02);
  empty.Block(12, 0, 0, 0, 0);
  CHECK(Render::ParseTim(empty.Bytes()).has_value());
}

TEST_CASE("Live TIM parses set the original rectangle and pixel pointers",
          "[render-textures]") {
  std::vector<std::byte> data = ClutTim(48);
  TIM_IMAGE tim{};

  Render::ParseLiveTim(data.data(), &tim);
  std::byte* const base = data.data();
  CHECK(tim.mode == 0x08);
  CHECK(reinterpret_cast<std::byte*>(tim.crect) == base + 12);
  CHECK(reinterpret_cast<std::byte*>(tim.caddr) == base + 20);
  CHECK(reinterpret_cast<std::byte*>(tim.prect) == base + 60);
  CHECK(reinterpret_cast<std::byte*>(tim.paddr) == base + 68);
  CHECK(tim.prect->x == 640);
  CHECK(tim.prect->h == 2);
}

TEST_CASE("Live TIM parses refuse inconsistent data and leave no pointers",
          "[render-textures]") {
  std::vector<std::byte> data = ClutTim();
  data[8] = std::byte{8};
  TIM_IMAGE tim{};
  std::memset(&tim, 0xA5, sizeof tim);
  const std::uint64_t refused = Render::RefusedLiveTimCount();

  Render::ParseLiveTim(data.data(), &tim);
  Render::ParseLiveTim(nullptr, &tim);
  Render::ParseLiveTim(data.data(), nullptr);
  CHECK(Render::RefusedLiveTimCount() == refused + 3);
  CHECK(tim.crect == nullptr);
  CHECK(tim.caddr == nullptr);
  CHECK(tim.prect == nullptr);
  CHECK(tim.paddr == nullptr);
}

// --- Packets -------------------------------------------------------------

class FakeOverlay final : public Render::OverlayPacketQueue {
 public:
  bool accept = true;
  std::vector<std::vector<std::uint32_t>> packets;

  bool AppendOverlay(std::span<const std::uint32_t> packet) override {
    if (!accept) {
      return false;
    }
    packets.emplace_back(packet.begin(), packet.end());
    return true;
  }
};

TEST_CASE("Texture moves match the original DR_MOVE words",
          "[render-textures]") {
  RECT rect{};
  setRECT(&rect, -16, 20, 8, 2);
  DR_MOVE original{};
  SetDrawMove(&original, &rect, 264, 84);
  const Render::TextureMovePacket packet =
      Render::BuildTextureMovePacket({-16, 20, 8, 2}, 264, 84);

  CHECK((packet[0] >> 24) == reinterpret_cast<const P_TAG*>(&original)->len);
  CHECK(
      std::equal(packet.begin() + 1, packet.end(), std::begin(original.code)));
  CHECK(packet[3] == 0x0014FFF0);

  FakeOverlay queue;
  CHECK(Render::QueueTextureRectangleMove(queue, {0, 0, 4, 4}, 1, 2));
  queue.accept = false;
  CHECK_FALSE(Render::QueueTextureRectangleMove(queue, {0, 0, 4, 4}, 1, 2));
  REQUIRE(queue.packets.size() == 1);
  CHECK(queue.packets[0].size() == Render::kTextureMovePacketWords);
}

struct LiveFrameFixture {
  Render_FrameScratch saved_frame = Render_frameScratch;
  void* saved_buffer = Render_legacyPrimitiveBuffers[0];
  void* saved_table = Render_legacyOrderingTables[0];
  void* saved_overlay_table = Render_legacyCurrentOrderingTable;
  int saved_framebuffer = Render_frameBuf;
  // The frame's whole 0x20000-byte primitive buffer.
  std::vector<u_long> storage = std::vector<u_long>(0x20000 / 4, 0xDEADBEEF);
  std::vector<u_long> ordering = std::vector<u_long>(0x2088 / 4);

  LiveFrameFixture() {
    Render_frameBuf = 0;
    Render_legacyPrimitiveBuffers[0] = storage.data();
    Render_legacyOrderingTables[0] = ordering.data();
    Render_legacyCurrentOrderingTable = ordering.data();
    Render_frameScratch.primitiveCursor = storage.data();
    Render_frameScratch.orderingTable = ordering.data() + 18;
    ClearOTagR(ordering.data(), static_cast<int>(ordering.size()));
  }
  LiveFrameFixture(const LiveFrameFixture&) = delete;
  LiveFrameFixture& operator=(const LiveFrameFixture&) = delete;
  ~LiveFrameFixture() {
    Render_frameScratch = saved_frame;
    Render_legacyPrimitiveBuffers[0] = saved_buffer;
    Render_legacyOrderingTables[0] = saved_table;
    Render_legacyCurrentOrderingTable = saved_overlay_table;
    Render_frameBuf = saved_framebuffer;
  }
};

TEST_CASE("Live texture moves are spliced last into the overlay bucket",
          "[render-textures]") {
  LiveFrameFixture frame;
  RECT rect{};
  setRECT(&rect, 16, 20, 8, 2);
  u_long* const head = &frame.ordering[0x821];

  Render::QueueLiveTextureRectangleMove(&rect, 264, 84);
  CHECK(Render_frameScratch.primitiveCursor == frame.storage.data() + 6);
  CHECK(nextPrim(head) == frame.storage.data());
  CHECK(nextPrim(frame.storage.data()) == head - 1);
  CHECK(frame.storage[2] == 0x80000000);
  CHECK(frame.storage[4] == ((84U << 16) | 264U));
  CHECK(frame.storage[6] == 0xDEADBEEF);
}

TEST_CASE("Live texture moves without room or a source are dropped",
          "[render-textures]") {
  LiveFrameFixture frame;
  RECT rect{};
  setRECT(&rect, 16, 20, 8, 2);
  u_long* const near_end = frame.storage.data() + frame.storage.size() - 5;
  Render_frameScratch.primitiveCursor = near_end;
  const std::uint64_t dropped = Render::DroppedLiveUiPacketCount();

  Render::QueueLiveTextureRectangleMove(&rect, 264, 84);
  Render::QueueLiveTextureRectangleMove(nullptr, 264, 84);
  CHECK(Render::DroppedLiveUiPacketCount() == dropped + 2);
  CHECK(Render_frameScratch.primitiveCursor == near_end);
  CHECK(*near_end == 0xDEADBEEF);
  CHECK(nextPrim(&frame.ordering[0x821]) == &frame.ordering[0x820]);
}

class FakeQueue final : public Render::PacketQueue {
 public:
  std::vector<std::vector<std::uint32_t>> packets;

  [[nodiscard]] std::uint32_t ChainLink() const override { return 0x123456; }
  bool Emit(std::span<const std::uint32_t> packet) override {
    packets.emplace_back(packet.begin(), packet.end());
    return true;
  }
};

TEST_CASE("Texture-page commands match SetDrawTPage with dithering",
          "[render-textures]") {
  for (const std::uint32_t page : {0x2EU, 0x4EU, 0xFFFFU, 0x1A0U}) {
    DR_TPAGE original{};
    SetDrawTPage(&original, 0, 1, static_cast<int>(page));
    FakeQueue queue;
    REQUIRE(Render::InsertTexturePageCommand(queue, page));
    REQUIRE(queue.packets.size() == 1);
    CHECK(queue.packets[0][0] == ((1U << 24) | 0x123456U));
    CHECK(queue.packets[0][1] == original.code[0]);
  }
}

TEST_CASE("Live texture pages link into the frame's ordering-table slot",
          "[render-textures]") {
  LiveFrameFixture frame;
  u_long* const slot = &frame.ordering[18 + 3];

  void* next = Render::AppendLiveTexturePage(0x2E, 3);
  CHECK(next == frame.storage.data() + 2);
  CHECK(Render_frameScratch.primitiveCursor == next);
  CHECK(nextPrim(slot) == frame.storage.data());
  CHECK(frame.storage[1] == 0xE100022EU);
  CHECK(Render::AppendLiveTexturePage(0x2E, 0x7FF) == frame.storage.data() + 4);
}

TEST_CASE("Live texture pages outside the frame's table are refused",
          "[render-textures]") {
  LiveFrameFixture frame;
  const std::uint64_t dropped = Render::DroppedLiveUiPacketCount();
  // The table holds 2082 entries and the frame's pointer is 18 in.
  const int past_end = static_cast<int>(frame.ordering.size()) - 18;

  CHECK(Render::AppendLiveTexturePage(0x2E, past_end) == frame.storage.data());
  CHECK(Render::AppendLiveTexturePage(0x2E, -19) == frame.storage.data());
  CHECK(Render::DroppedLiveUiPacketCount() == dropped + 2);
  CHECK(frame.storage[0] == 0xDEADBEEF);
  CHECK(Render::AppendLiveTexturePage(0x2E, -18) == frame.storage.data() + 2);
}

class FakeImmediate final : public Render::ImmediatePacketTarget {
 public:
  std::vector<std::vector<std::uint32_t>> packets;

  void Submit(std::span<const std::uint32_t> packet) override {
    packets.emplace_back(packet.begin(), packet.end());
  }
};

TEST_CASE("Immediate sprites dim from the neutral brightness",
          "[render-textures]") {
  const std::optional<Render::ImmediateSpritePacket> packet =
      Render::BuildImmediateSpritePacket({.position = 0x000A0014,
                                          .uv_clut = 0x12345678,
                                          .size = 0x00400040,
                                          .texture_page = 0x00401234});

  REQUIRE(packet.has_value());
  CHECK(((*packet)[0] >> 24) == 5);
  CHECK((*packet)[1] == 0xE1000034U);
  CHECK((*packet)[2] == 0x64404040U);
  CHECK((*packet)[3] == 0x000A0014U);
  CHECK((*packet)[4] == 0x12345678U);
  CHECK((*packet)[5] == 0x00400040U);
}

TEST_CASE("Immediate sprites dimmed outside a colour byte are refused",
          "[render-textures]") {
  // A high half of -127 brightens to 255; -128 and 0x81 leave the byte.
  const auto brightened = Render::BuildImmediateSpritePacket(
      {.texture_page = static_cast<std::int32_t>(0xFF810000U)});
  REQUIRE(brightened.has_value());
  CHECK((*brightened)[2] == 0x64FFFFFFU);
  CHECK_FALSE(Render::BuildImmediateSpritePacket(
                  {.texture_page = static_cast<std::int32_t>(0xFF800000U)})
                  .has_value());
  CHECK_FALSE(Render::BuildImmediateSpritePacket({.texture_page = 0x00810000})
                  .has_value());

  FakeImmediate target;
  CHECK(Render::DrawImmediateSprite(target, {.texture_page = 0x00800000}));
  CHECK_FALSE(
      Render::DrawImmediateSprite(target, {.texture_page = 0x00810000}));
  CHECK(target.packets.size() == 1);
}

TEST_CASE("Live immediate sprites count refused dimming", "[render-textures]") {
  const std::uint64_t dropped = Render::DroppedLiveUiPacketCount();
  Render::DrawLiveImmediateSprite(0, 0, 0, 0x00810000);
  CHECK(Render::DroppedLiveUiPacketCount() == dropped + 1);
}

// _interpolateRGB (260.c:1718) as the original computes it.
std::uint32_t OriginalBlend(std::uint32_t colour1, std::uint32_t colour2,
                            std::uint32_t factor) {
  std::uint32_t out = 0;
  for (int i = 0; i < 3; ++i) {
    const auto component =
        static_cast<std::int32_t>(((colour1 >> 16) & 0xFF) * (8 - factor));
    colour1 <<= 8;
    out = (out << 8) + ((static_cast<std::uint32_t>(component) +
                         (((colour2 >> 16) & 0xFF) * factor)) >>
                        3);
    colour2 <<= 8;
  }
  return out;
}

TEST_CASE("Colour blends match the original arithmetic", "[render-textures]") {
  CHECK(Render::BlendRgb(0x000000, 0xFFFFFF, 8) == 0xFFFFFF);
  CHECK(Render::BlendRgb(0x000000, 0xFFFFFF, 4) == 0x7F7F7F);
  CHECK(Render::BlendRgb(0x123456, 0xABCDEF, 0) == 0x123456);
  bool all_match = true;
  for (const std::uint32_t factor : {0U, 1U, 3U, 7U, 8U, 9U, 0xFFFFFFFFU}) {
    for (const std::uint32_t colour1 :
         {0U, 0x123456U, 0xFF00FFU, 0xFFFFFFFFU}) {
      for (const std::uint32_t colour2 : {0U, 0x80FF01U, 0xFFFFFFU}) {
        all_match = all_match && Render::BlendRgb(colour1, colour2, factor) ==
                                     OriginalBlend(colour1, colour2, factor);
      }
    }
  }
  CHECK(all_match);
}

// --- Image reads ---------------------------------------------------------

class FakeRead final : public Render::ImageReadTarget {
 public:
  std::vector<Render::ScreenRect> areas;
  void Read(const Render::ScreenRect& area) override { areas.push_back(area); }
};

TEST_CASE("Image reads unpack position and size", "[render-textures]") {
  FakeRead target;
  CHECK(Render::ReadImageRectangle(target, 0x00E30300, 0x00010010));
  CHECK_FALSE(Render::ReadImageRectangle(target, 0x00E30300, 0x00010000));
  REQUIRE(target.areas.size() == 1);
  CHECK(target.areas[0] == Render::ScreenRect{768, 227, 16, 1});
}

TEST_CASE("Live image reads return what the upload wrote",
          "[render-textures]") {
  RECT area{};
  setRECT(&area, 1000, 500, 4, 2);
  std::array<std::uint16_t, 8> saved{};
  StoreImage(&area, reinterpret_cast<u_long*>(saved.data()));
  DrawSync(0);
  const std::array<std::uint16_t, 8> pixels{9, 8, 7, 6, 5, 4, 3, 2};
  const int position = 1000 | (500 << 16);
  const int size = 4 | (2 << 16);

  Render::UploadLiveImageRectangle(position, pixels.data(), size);
  DrawSync(0);
  std::array<std::uint16_t, 8> read{};
  Render::ReadLiveImageRectangle(position, read.data(), size);
  Render::ReadLiveImageRectangle(position, nullptr, size);
  DrawSync(0);
  CHECK(read == pixels);
  LoadImage(&area, reinterpret_cast<u_long*>(saved.data()));
  DrawSync(0);
}
}  // namespace
