// Hand-maintained (tools/port/owned.csv).
// Live binding for the sprite packets (Sprites.cpp): the current frame's
// primitive buffer, ordering chains and GTE view matrix.
#include "Render/SpritesLive.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cinttypes>
#include <cstddef>
#include <cstdint>

extern "C" {
#include "Core/Diagnostics.h"
#include "Core/Gte.h"
#include "Render/Display.h"
#include "Render/Frame.h"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"
}

namespace VS::Render {
namespace {
// The frame primitive buffers are low-end allocations of this size
// (Field/RoomLoading.cpp, Ending/Credits/StaffRoll.cpp).
constexpr std::size_t kPrimitiveBufferBytes = 0x20000;
constexpr std::size_t kPacketBytes = kSpritePacketWords * sizeof(u_long);
static_assert(sizeof(u_long) == sizeof(std::uint32_t));

std::array<u_long, kSpritePacketWords> discard_packet{};
std::uint64_t dropped_sprites = 0;

// Room for one packet at the cursor in the current frame's buffer, or null.
u_long* PacketSlot() {
  const auto base = reinterpret_cast<std::uintptr_t>(
      Render_legacyPrimitiveBuffers[Render_frameBuf & 1]);
  const auto cursor =
      reinterpret_cast<std::uintptr_t>(Render_frameScratch.primitiveCursor);
  if (base == 0 || cursor < base || cursor - base > kPrimitiveBufferBytes ||
      kPrimitiveBufferBytes - (cursor - base) < kPacketBytes) {
    return nullptr;
  }
  return static_cast<u_long*>(Render_frameScratch.primitiveCursor);
}

class LiveSpriteQueue final : public SpriteQueue {
 public:
  explicit LiveSpriteQueue(u_long* chain) : chain_(chain) {}

  [[nodiscard]] std::uint32_t ChainLink() const override {
    return chain_ != nullptr ? static_cast<std::uint32_t>(*chain_) : 0;
  }

  void Emit(const SpritePacket& packet) override {
    u_long* slot = chain_ != nullptr ? PacketSlot() : nullptr;
    if (slot == nullptr) {
      Drop();
      return;
    }
    std::ranges::copy(packet, slot);
    *chain_ = LegacyGpu_Handle(slot);
    Render_frameScratch.primitiveCursor = slot + kSpritePacketWords;
    packet_ = slot;
  }

  [[nodiscard]] u_long* Packet() const { return packet_; }

 private:
  void Drop() {
    discard_packet.fill(0);
    packet_ = discard_packet.data();
    ++dropped_sprites;
    // Logged at powers of two so a full buffer cannot flood the log.
    if (std::has_single_bit(dropped_sprites)) {
      Core_Errorf("render",
                  "sprite packet dropped: no room in the frame primitive "
                  "buffer or no chain (%" PRIu64 " dropped)",
                  dropped_sprites);
    }
  }

  u_long* chain_;
  u_long* packet_ = nullptr;
};

class LiveFrameView final : public FrameViewTarget {
 public:
  void LoadViewRotation() override {
    Core_Gte_SetRotMatrix(&Render_frameScratch.viewMatrix);
  }
  void LoadViewTranslation() override {
    Core_Gte_SetTransMatrix(&Render_frameScratch.viewMatrix);
  }
};
}  // namespace

u_long* QueueLiveDefaultSprite(int wh, int xy) {
  LiveSpriteQueue queue(
      static_cast<u_long*>(Render_frameScratch.overlayOrderingTable));
  QueueDefaultSprite(queue, wh, xy);
  return queue.Packet();
}

u_long* QueueLiveSpriteWithTexturePage(int brightness_transparency, int xy,
                                       int wh, u_long* next_prim) {
  LiveSpriteQueue queue(next_prim);
  QueueSpriteWithTexturePage(queue, brightness_transparency, xy, wh);
  return queue.Packet();
}

u_long* QueueLiveSprite(int brightness_transparency, int xy, int wh,
                        u_long* next_prim) {
  LiveSpriteQueue queue(next_prim);
  QueueSprite(queue, brightness_transparency, xy, wh);
  return queue.Packet();
}

void LoadLiveFrameViewMatrix() {
  LiveFrameView target;
  LoadFrameViewMatrix(target);
}

std::uint64_t DroppedLiveSpriteCount() { return dropped_sprites; }
}  // namespace VS::Render
