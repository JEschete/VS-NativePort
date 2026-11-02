#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Legacy/Psx.h"
#include "Render/Sprites.hpp"
#include "Render/SpritesLive.hpp"

extern "C" {
#include "Render/Display.h"
#include "Render/Frame.h"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"
}

namespace {
using VS::Render::SpritePacket;

class RecordingQueue final : public VS::Render::SpriteQueue {
 public:
  std::uint32_t link = 0;
  std::vector<SpritePacket> packets;

  [[nodiscard]] std::uint32_t ChainLink() const override { return link; }
  void Emit(const SpritePacket& packet) override { packets.push_back(packet); }
};

class RecordingView final : public VS::Render::FrameViewTarget {
 public:
  std::string calls;
  void LoadViewRotation() override { calls += 'R'; }
  void LoadViewTranslation() override { calls += 'T'; }
};

// One frame primitive buffer (0x20000 bytes) with a guard word on each side.
constexpr std::size_t kBufferWords = 0x20000 / sizeof(u_long);
constexpr u_long kGuard = 0xDEADBEEF;
constexpr u_long kChainHead = 0x00ABCDEF;

struct LiveSpriteFixture {
  Render_FrameScratch saved_frame = Render_frameScratch;
  void* saved_buffer = Render_legacyPrimitiveBuffers[0];
  int saved_framebuffer = Render_frameBuf;
  std::vector<u_long> storage = std::vector<u_long>(kBufferWords + 2, kGuard);
  u_long chain = kChainHead;

  LiveSpriteFixture() {
    Render_frameBuf = 0;
    Render_legacyPrimitiveBuffers[0] = Base();
    Render_frameScratch.primitiveCursor = Base();
  }
  ~LiveSpriteFixture() {
    Render_frameScratch = saved_frame;
    Render_legacyPrimitiveBuffers[0] = saved_buffer;
    Render_frameBuf = saved_framebuffer;
  }
  LiveSpriteFixture(const LiveSpriteFixture&) = delete;
  LiveSpriteFixture& operator=(const LiveSpriteFixture&) = delete;

  u_long* Base() { return storage.data() + 1; }
  [[nodiscard]] bool GuardsIntact() const {
    return storage.front() == kGuard && storage.back() == kGuard;
  }
};
}  // namespace

TEST_CASE("sprite packets keep the original layout", "[sprites]") {
  RecordingQueue queue;
  queue.link = 0x12345678;
  VS::Render::QueueSprite(queue, 0x40, 0x00200010, 0x00080004);
  VS::Render::QueueSpriteWithTexturePage(queue, 0x180, 0x00100020, 0x00060006);
  VS::Render::QueueDefaultSprite(queue, 0x000C000C, 0x00300040);
  REQUIRE(queue.packets.size() == 3);

  CHECK(queue.packets[0] ==
        SpritePacket{0x05345678, 0, 0x64404040, 0x00200010, 0, 0x00080004});
  // Bit 8 of the brightness word selects the semi-transparent sprite (0x66).
  CHECK(queue.packets[1] == SpritePacket{0x05345678, 0xE100000C, 0x66808080,
                                         0x00100020, 0, 0x00060006});
  // The default sprite takes the size before the position.
  CHECK(queue.packets[2] == SpritePacket{0x05345678, 0xE100000C, 0x64808080,
                                         0x00300040, 0, 0x000C000C});
}

TEST_CASE("the frame view matrix loads rotation then translation",
          "[sprites]") {
  RecordingView view;
  VS::Render::LoadFrameViewMatrix(view);
  CHECK(view.calls == "RT");
}

TEST_CASE("live sprites link into the chain and advance the cursor",
          "[sprites]") {
  LiveSpriteFixture fixture;
  u_long* packet =
      VS::Render::QueueLiveSprite(0x80, 0x00100020, 0x00080008, &fixture.chain);
  CHECK(packet == fixture.Base());
  CHECK(fixture.chain == LegacyGpu_Handle(packet));
  CHECK(packet[0] == (0x05000000 | kChainHead));
  CHECK(packet[2] == 0x64808080);
  CHECK(Render_frameScratch.primitiveCursor ==
        fixture.Base() + VS::Render::kSpritePacketWords);
  CHECK(fixture.GuardsIntact());
}

TEST_CASE("a live sprite that exactly fits the buffer end is queued",
          "[sprites]") {
  LiveSpriteFixture fixture;
  u_long* last = fixture.Base() + kBufferWords - VS::Render::kSpritePacketWords;
  Render_frameScratch.primitiveCursor = last;
  u_long* packet = VS::Render::QueueLiveSpriteWithTexturePage(
      0x80, 0, 0x00040004, &fixture.chain);
  CHECK(packet == last);
  CHECK(Render_frameScratch.primitiveCursor == fixture.Base() + kBufferWords);
  CHECK(fixture.GuardsIntact());
}

TEST_CASE("a live sprite past the buffer end gets discard storage",
          "[sprites]") {
  LiveSpriteFixture fixture;
  const std::uint64_t dropped = VS::Render::DroppedLiveSpriteCount();
  u_long* overflow =
      fixture.Base() + kBufferWords - VS::Render::kSpritePacketWords + 1;
  Render_frameScratch.primitiveCursor = overflow;
  u_long* packet =
      VS::Render::QueueLiveSprite(0x80, 0, 0x00040004, &fixture.chain);
  REQUIRE(packet != nullptr);
  CHECK(packet != overflow);
  // Callers patch the texture and draw-mode words of the returned packet.
  packet[1] = 1;
  packet[4] = 1;
  CHECK(fixture.chain == kChainHead);
  CHECK(Render_frameScratch.primitiveCursor == overflow);
  CHECK(fixture.GuardsIntact());
  CHECK(VS::Render::DroppedLiveSpriteCount() == dropped + 1);
}

TEST_CASE("a live sprite with no buffer or chain is not linked", "[sprites]") {
  LiveSpriteFixture fixture;
  const std::uint64_t dropped = VS::Render::DroppedLiveSpriteCount();
  u_long* null_chain =
      VS::Render::QueueLiveSprite(0x80, 0, 0x00040004, nullptr);
  CHECK(null_chain != nullptr);
  CHECK(Render_frameScratch.primitiveCursor == fixture.Base());

  Render_legacyPrimitiveBuffers[0] = nullptr;
  u_long* no_buffer =
      VS::Render::QueueLiveSprite(0x80, 0, 0x00040004, &fixture.chain);
  CHECK(no_buffer != nullptr);
  CHECK(no_buffer != fixture.Base());
  CHECK(fixture.chain == kChainHead);
  CHECK(Render_frameScratch.primitiveCursor == fixture.Base());
  CHECK(VS::Render::DroppedLiveSpriteCount() == dropped + 2);
}
