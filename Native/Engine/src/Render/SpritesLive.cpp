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

#include "Core/Diagnostics.hpp"
#include "Core/Gte.hpp"
#include "Render/Display.hpp"
#include "Render/Frame.hpp"
#include "Render/Render.hpp"
#include "Render/UiPrimitives.hpp"

using VS::Core::Gte::SetRotMatrix;
using VS::Core::Gte::SetTransMatrix;

using VS::Legacy::GpuHandle;

namespace VS::Render {
namespace {
// The frame primitive buffers are low-end allocations of this size
// (Field/RoomLoading.cpp, Ending/Credits/StaffRoll.cpp).
constexpr std::size_t kPrimitiveBufferBytes = 0x20000;

std::array<std::uint32_t, kSpritePacketWords> discard_packet{};
std::uint64_t dropped_sprites = 0;

class LiveSpriteQueue final : public SpriteQueue {
 public:
  explicit LiveSpriteQueue(std::uint32_t* chain) : chain_(chain) {}

  [[nodiscard]] std::uint32_t ChainLink() const override {
    return chain_ != nullptr ? static_cast<std::uint32_t>(*chain_) : 0;
  }

  void Emit(const SpritePacket& packet) override {
    std::uint32_t* slot = chain_ != nullptr
                              ? ReserveLivePrimitiveWords(kSpritePacketWords)
                              : nullptr;
    if (slot == nullptr) {
      Drop();
      return;
    }
    std::ranges::copy(packet, slot);
    *chain_ = GpuHandle(slot);
    FrameScratchpad().primitive_cursor = slot + kSpritePacketWords;
    packet_ = slot;
  }

  [[nodiscard]] std::uint32_t* Packet() const { return packet_; }

 private:
  void Drop() {
    discard_packet.fill(0);
    packet_ = discard_packet.data();
    ++dropped_sprites;
    // Logged at powers of two so a full buffer cannot flood the log.
    if (std::has_single_bit(dropped_sprites)) {
      VS::Core::Errorf("render",
                       "sprite packet dropped: no room in the frame primitive "
                       "buffer or no chain (%" PRIu64 " dropped)",
                       dropped_sprites);
    }
  }

  std::uint32_t* chain_;
  std::uint32_t* packet_ = nullptr;
};

class LiveFrameView final : public FrameViewTarget {
 public:
  void LoadViewRotation() override {
    SetRotMatrix(&FrameScratchpad().view_matrix);
  }
  void LoadViewTranslation() override {
    SetTransMatrix(&FrameScratchpad().view_matrix);
  }
};
}  // namespace

std::uint32_t* ReserveLivePrimitiveWords(std::size_t words) {
  const auto base =
      reinterpret_cast<std::uintptr_t>(legacyPrimitiveBuffers[frameBuf & 1]);
  const auto cursor =
      reinterpret_cast<std::uintptr_t>(FrameScratchpad().primitive_cursor);
  if (base == 0 || cursor < base || cursor - base > kPrimitiveBufferBytes ||
      (kPrimitiveBufferBytes - (cursor - base)) / sizeof(std::uint32_t) <
          words) {
    return nullptr;
  }
  return static_cast<std::uint32_t*>(FrameScratchpad().primitive_cursor);
}

std::uint32_t* QueueLiveDefaultSprite(int wh, int xy) {
  LiveSpriteQueue queue(
      static_cast<std::uint32_t*>(FrameScratchpad().overlay_ordering_table));
  QueueDefaultSprite(queue, wh, xy);
  return queue.Packet();
}

std::uint32_t* QueueLiveSpriteWithTexturePage(int brightness_transparency,
                                              int xy, int wh,
                                              std::uint32_t* next_prim) {
  LiveSpriteQueue queue(next_prim);
  QueueSpriteWithTexturePage(queue, brightness_transparency, xy, wh);
  return queue.Packet();
}

std::uint32_t* QueueLiveSprite(int brightness_transparency, int xy, int wh,
                               std::uint32_t* next_prim) {
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
