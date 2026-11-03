#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Legacy/Psx.h"
#include "Render/CameraView.hpp"
#include "Render/CameraViewLive.hpp"
#include "Render/DisplayControl.hpp"
#include "Render/DisplayControlLive.hpp"
#include "Render/RoomDraw.hpp"
#include "Render/RoomDrawLive.hpp"
#include "Render/Sprites.hpp"
#include "Render/UiPackets.hpp"
#include "Render/UiPacketsLive.hpp"

extern "C" {
#include "Core/Gte.h"
#include "Render/Display.h"
#include "Render/Frame.h"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"
}

// Contracts of the #276 slice A4 (#279) camera, display and HUD packet
// policies: values traced from 146C.c, 5BF94.c, 44F14.c and main.c, and every
// input that the original left undefined given a defined result.
namespace {
namespace Render = VS::Render;
namespace Math = VS::Core::Math;

constexpr std::int32_t kOne = 4096;

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): one per axis.
Math::Mat3 Diagonal(std::int16_t x, std::int16_t y, std::int16_t z) {
  Math::Mat3 matrix{};
  matrix.m[0][0] = x;
  matrix.m[1][1] = y;
  matrix.m[2][2] = z;
  return matrix;
}

// --- Packets -------------------------------------------------------------

class FakeQueue final : public Render::PacketQueue {
 public:
  std::uint32_t link = 0x00ABCDEF;
  bool accept = true;
  std::vector<std::vector<std::uint32_t>> packets;

  [[nodiscard]] std::uint32_t ChainLink() const override { return link; }
  bool Emit(std::span<const std::uint32_t> packet) override {
    if (!accept) {
      return false;
    }
    packets.emplace_back(packet.begin(), packet.end());
    link = static_cast<std::uint32_t>(packets.size());
    return true;
  }
};

TEST_CASE("Draw-mode commands are one-word packets (#279)",
          "[render-display]") {
  CHECK(Render::BuildDrawModePacket(0xE1000200, 0xFF123456) ==
        Render::DrawModePacket{0x01123456, 0xE1000200});
  CHECK(Render::BuildTilePacket({0x60102030, 0x00100020, 0x00080004},
                                0x00654321) ==
        Render::TilePacket{0x03654321, 0x60102030, 0x00100020, 0x00080004});
  FakeQueue queue;
  REQUIRE(Render::InsertDrawModeCommand(queue, 0xE1000600));
  REQUIRE(queue.packets.size() == 1);
  CHECK(queue.packets[0] == std::vector<std::uint32_t>{0x01ABCDEF, 0xE1000600});
  REQUIRE(Render::InsertThreeWordPrimitive(queue, {1, 2, 3}));
  CHECK(queue.packets[1] == std::vector<std::uint32_t>{0x03000001, 1, 2, 3});
  queue.accept = false;
  CHECK_FALSE(Render::InsertDrawModeCommand(queue, 0xE1000600));
}

TEST_CASE("Live draw-mode commands advance the frame cursor by two words",
          "[render-display]") {
  std::array<u_long, 8> storage{};
  storage.fill(0xDEADBEEF);
  const Render_FrameScratch saved_frame = Render_frameScratch;
  void* const saved_buffer = Render_legacyPrimitiveBuffers[Render_frameBuf & 1];
  Render_legacyPrimitiveBuffers[Render_frameBuf & 1] = storage.data() + 1;
  Render_frameScratch.primitiveCursor = storage.data() + 1;
  std::array<u_long, 2> ordering{};
  ClearOTagR(ordering.data(), static_cast<int>(ordering.size()));
  const u_long old_head = ordering[1];

  VS::Render::InsertLiveDrawModeCommand(static_cast<int>(0xE1000600u),
                                        &ordering[1]);

  CHECK(Render_frameScratch.primitiveCursor == storage.data() + 3);
  CHECK((storage[1] >> 24) == 1);
  CHECK((storage[1] & 0xFFFFFF) == (old_head & 0xFFFFFF));
  CHECK(storage[2] == 0xE1000600);
  CHECK(storage[3] == 0xDEADBEEF);
  CHECK(nextPrim(&ordering[1]) == storage.data() + 1);
  Render_legacyPrimitiveBuffers[Render_frameBuf & 1] = saved_buffer;
  Render_frameScratch = saved_frame;
}

TEST_CASE("Live HUD packets outside the frame buffer are dropped and counted",
          "[render-display]") {
  std::array<u_long, 8> storage{};
  storage.fill(0xDEADBEEF);
  const Render_FrameScratch saved_frame = Render_frameScratch;
  void* const saved_buffer = Render_legacyPrimitiveBuffers[Render_frameBuf & 1];
  // The cursor lies before the frame buffer's start.
  Render_legacyPrimitiveBuffers[Render_frameBuf & 1] = storage.data() + 4;
  Render_frameScratch.primitiveCursor = storage.data() + 1;
  u_long chain = 0x00FFFFFF;
  const std::uint64_t dropped = VS::Render::DroppedLiveUiPacketCount();

  VS::Render::InsertLiveThreeWordPrimitive(&chain, 1, 2, 3);
  VS::Render::InsertLiveDrawModeCommand(static_cast<int>(0xE1000000u), nullptr);

  CHECK(VS::Render::DroppedLiveUiPacketCount() == dropped + 2);
  CHECK(chain == 0x00FFFFFF);
  CHECK(Render_frameScratch.primitiveCursor == storage.data() + 1);
  CHECK(std::ranges::all_of(storage,
                            [](u_long word) { return word == 0xDEADBEEF; }));
  Render_legacyPrimitiveBuffers[Render_frameBuf & 1] = saved_buffer;
  Render_frameScratch = saved_frame;
}

TEST_CASE("Numeric glyphs use the font's size, row and texture column",
          "[render-display]") {
  using Render::kSpritePositionWord;
  using Render::kSpriteSizeWord;
  using Render::kSpriteTagWord;
  using Render::kSpriteTextureWord;
  // 0x05CA0576 >> font * 4: 6x10, 7x12 and 5x5.
  const Render::SpritePacket digit =
      Render::BuildNumericGlyphPacket({0, 0x00200010, 3}, 0x00123456);
  CHECK((digit[kSpriteTagWord] & Render::kPacketLinkMask) == 0x123456);
  CHECK(digit[kSpritePositionWord] == 0x00200010);
  CHECK(digit[kSpriteSizeWord] == 0x000A0006);
  // ((3 * 2 + 64) * 3) - 192 replaces the u/v halfword; the CLUT stays.
  CHECK(digit[kSpriteTextureWord] == 0x37F40012);

  const Render::SpritePacket large =
      Render::BuildNumericGlyphPacket({1, 0x00200010, 9}, 0);
  CHECK(large[kSpriteSizeWord] == 0x000C0007);
  CHECK(large[kSpriteTextureWord] == 0x37F40088);

  // Font 2 draws the separator two rows lower with the base texture.
  const Render::SpritePacket separator =
      Render::BuildNumericGlyphPacket({2, 0x00200010, 7}, 0);
  CHECK(separator[kSpritePositionWord] == 0x00220010);
  CHECK(separator[kSpriteSizeWord] == 0x00050005);
  CHECK(separator[kSpriteTextureWord] == 0x37F40B20);
}

std::vector<std::uint32_t> DrawnDigits(const FakeQueue& queue) {
  std::vector<std::uint32_t> digits;
  for (const auto& packet : queue.packets) {
    digits.push_back(
        ((packet[Render::kSpriteTextureWord] & 0xFFFF) + 192) / 3 / 2 - 32);
  }
  return digits;
}

TEST_CASE("Right-aligned numbers draw each digit leftward",
          "[render-display]") {
  FakeQueue queue;
  const Render::RightAlignedNumber number =
      Render::DrawIntegerRightAligned(queue, {0, 100, 1234});
  CHECK(number.next_position == 80);
  CHECK_FALSE(number.truncated);
  CHECK(DrawnDigits(queue) == std::vector<std::uint32_t>{4, 3, 2, 1});
  CHECK(queue.packets[3][Render::kSpritePositionWord] == 85);

  FakeQueue zero;
  const Render::RightAlignedNumber single =
      Render::DrawIntegerRightAligned(zero, {1, 100, 0});
  CHECK(single.next_position == 94);
  CHECK(zero.packets.size() == 1);
}

TEST_CASE("Right-aligned numbers stop after ten digits", "[render-display]") {
  FakeQueue largest;
  const Render::RightAlignedNumber ten =
      Render::DrawIntegerRightAligned(largest, {0, 100, 1342177279});
  CHECK_FALSE(ten.truncated);
  CHECK(largest.packets.size() == Render::kMaxDecimalDigits);
  CHECK(DrawnDigits(largest) ==
        std::vector<std::uint32_t>{9, 7, 2, 7, 7, 1, 2, 4, 3, 1});

  // The original's loop never ended for these values.
  for (const std::int32_t value :
       {1342177280, std::numeric_limits<std::int32_t>::max(), -1,
        std::numeric_limits<std::int32_t>::min()}) {
    FakeQueue queue;
    const Render::RightAlignedNumber number =
        Render::DrawIntegerRightAligned(queue, {0, 100, value});
    CHECK(number.truncated);
    CHECK(number.next_position == 100 - 5 * Render::kMaxDecimalDigits);
    CHECK(queue.packets.size() == Render::kMaxDecimalDigits);
  }
}

TEST_CASE("Live numbers default to the chain before the ordering table",
          "[render-display]") {
  std::array<u_long, 32> storage{};
  std::array<u_long, 8> ordering{};
  ClearOTagR(ordering.data(), static_cast<int>(ordering.size()));
  const Render_FrameScratch saved_frame = Render_frameScratch;
  void* const saved_buffer = Render_legacyPrimitiveBuffers[Render_frameBuf & 1];
  Render_legacyPrimitiveBuffers[Render_frameBuf & 1] = storage.data();
  Render_frameScratch.primitiveCursor = storage.data();
  Render_frameScratch.orderingTable = ordering.data() + 4;

  CHECK(VS::Render::DrawLiveIntegerRightAligned(0, 50, 42, nullptr) == 40);
  // Two sprites, each six words, linked in front of ordering[0].
  CHECK(Render_frameScratch.primitiveCursor == storage.data() + 12);
  CHECK(nextPrim(ordering.data()) == storage.data() + 6);
  CHECK(nextPrim(storage.data() + 6) == storage.data());
  Render_legacyPrimitiveBuffers[Render_frameBuf & 1] = saved_buffer;
  Render_frameScratch = saved_frame;
}

using GaugePalette = std::array<std::uint32_t, 4>;
constexpr GaugePalette kPalette{0x000000, 0x204060, 0x808080, 0x00FF00};

TEST_CASE("Stat gauges blend the fill toward the full colour",
          "[render-display]") {
  const auto empty =
      Render::BuildStatGaugePacket(kPalette, {1, 0, 0x00100020}, 0x00123456);
  REQUIRE(empty.has_value());
  CHECK((*empty)[0] == 0x03123456);

  const auto half =
      Render::BuildStatGaugePacket(kPalette, {0, 32, 0x00100020}, 0);
  REQUIRE(half.has_value());
  const Render::StatGaugePacket expected{
      0x0D000000, 0x60402800, 0x00100020, 0x00050042, 0xE1000200,
      0x384F787F, 0x00110021, 0x00000000, 0x00110041, 0x004F787F,
      0x00140021, 0x00000000, 0x00140041, 0xE1000000};
  CHECK(*half == expected);

  const auto full =
      Render::BuildStatGaugePacket(kPalette, {2, 64, 0x00100020}, 0);
  REQUIRE(full.has_value());
  CHECK((*full)[9] == 0x009EF0FF);
}

TEST_CASE("Stat gauges outside the palette draw nothing", "[render-display]") {
  CHECK_FALSE(
      Render::BuildStatGaugePacket(kPalette, {4, 32, 0}, 0).has_value());
  FakeQueue queue;
  CHECK_FALSE(Render::DrawStatGauge(queue, kPalette, {0xFFFFFFFF, 32, 0}));
  CHECK(queue.packets.empty());
  CHECK(Render::DrawStatGauge(queue, kPalette, {3, 32, 0}));
  CHECK(queue.packets.size() == 1);
}

TEST_CASE("Colour blends weight both colours and scale by brightness",
          "[render-display]") {
  CHECK(Render::BlendRgbWithBrightness(0x102030, 0x405060, 128, 0) == 0x102030);
  CHECK(Render::BlendRgbWithBrightness(0x102030, 0x405060, 128, 8) == 0x405060);
  CHECK(Render::BlendRgbWithBrightness(0x102030, 0x405060, 128, 4) == 0x283848);
  CHECK(Render::BlendRgbWithBrightness(0x102030, 0x405060, 64, 0) == 0x081018);
  // As in the original, an overflowing channel carries into the next one.
  CHECK(Render::BlendRgbWithBrightness(0x0000FF, 0, 256, 0) == 0x0001FE);
  CHECK(Render::BlendRgbWithBrightness(0xFF0000, 0, 256, 0) == 0x1FE0000);
}

// --- Primitive splices ---------------------------------------------------

class FakeChain final : public Render::PrimitiveChain {
 public:
  std::map<std::uint32_t, std::uint32_t> tags;
  std::optional<Render::ChainBucket> bucket;
  int writes = 0;

  [[nodiscard]] std::optional<std::uint32_t> TagWord(
      std::uint32_t handle) const override {
    const auto found = tags.find(handle);
    if (found == tags.end()) {
      return std::nullopt;
    }
    return found->second;
  }
  void SetTagWord(std::uint32_t handle, std::uint32_t word) override {
    tags[handle] = word;
    ++writes;
  }
  [[nodiscard]] std::optional<Render::ChainBucket> OverlayBucket()
      const override {
    return bucket;
  }
};

TEST_CASE("Splices link the primitive last in the bucket", "[render-display]") {
  FakeChain chain;
  chain.tags = {
      {10, 0x00000014}, {20, 0x0400001E}, {30, 0x09000005}, {40, 0x07FFFFFF}};
  REQUIRE(Render::SplicePrimitiveList(chain, {10, 5}, 40) ==
          Render::SpliceResult::kSpliced);
  CHECK(chain.tags[30] == 0x09000028);
  CHECK(chain.tags[40] == 0x07000005);
  CHECK(chain.tags[10] == 0x00000014);

  FakeChain empty;
  empty.tags = {{10, 0x00000005}, {40, 0x03000000}};
  empty.bucket = Render::ChainBucket{10, 5};
  REQUIRE(Render::AppendOverlayPrimitive(empty, 40) ==
          Render::SpliceResult::kSpliced);
  CHECK(empty.tags[10] == 0x00000028);
  CHECK(empty.tags[40] == 0x03000005);
}

TEST_CASE("Broken, cyclic or unbound chains are left untouched (#279)",
          "[render-display]") {
  FakeChain broken;
  broken.tags = {{10, 0x00000014}, {20, 0x00000063}, {40, 0}};
  CHECK(Render::SplicePrimitiveList(broken, {10, 5}, 40) ==
        Render::SpliceResult::kBrokenChain);
  CHECK(broken.writes == 0);

  FakeChain cycle;
  cycle.tags = {{10, 0x00000014}, {20, 0x0000000A}, {40, 0}};
  CHECK(Render::SplicePrimitiveList(cycle, {10, 5}, 40) ==
        Render::SpliceResult::kBrokenChain);
  CHECK(cycle.writes == 0);

  FakeChain missing;
  missing.tags = {{10, 0x00000005}};
  CHECK(Render::SplicePrimitiveList(missing, {10, 5}, 40) ==
        Render::SpliceResult::kMissingPrimitive);
  CHECK(missing.writes == 0);

  FakeChain unbound;
  unbound.tags = {{10, 0x00000005}, {40, 0}};
  CHECK(Render::AppendOverlayPrimitive(unbound, 40) ==
        Render::SpliceResult::kNoOrderingTable);
  CHECK(unbound.writes == 0);
}

TEST_CASE("Live overlay splices reach the ordering table's overlay bucket",
          "[render-display]") {
  constexpr int kOrderingWords = 0x822;
  std::vector<u_long> ordering(kOrderingWords);
  ClearOTagR(ordering.data(), kOrderingWords);
  std::array<u_long, 4> primitive{};
  setlen(primitive.data(), 3);
  void* const saved_table = Render_legacyCurrentOrderingTable;
  // The overlay bucket sits at byte 0x2084 of the table.
  constexpr std::size_t kOverlayHead = 0x821;
  u_long* const head = ordering.data() + kOverlayHead;

  Render_legacyCurrentOrderingTable = nullptr;
  VS::Render::AppendLiveOverlayPrimitive(primitive.data());
  CHECK(nextPrim(head) == head - 1);

  Render_legacyCurrentOrderingTable = ordering.data();
  VS::Render::AppendLiveOverlayPrimitive(nullptr);
  CHECK(nextPrim(head) == head - 1);
  VS::Render::AppendLiveOverlayPrimitive(primitive.data());
  CHECK(nextPrim(head) == primitive.data());
  CHECK(nextPrim(primitive.data()) == head - 1);
  CHECK((primitive[0] >> 24) == 3);
  Render_legacyCurrentOrderingTable = saved_table;
}

// --- Image uploads -------------------------------------------------------

class FakeUpload final : public Render::ImageUploadTarget {
 public:
  std::vector<Render::ScreenRect> areas;
  void Upload(const Render::ScreenRect& area) override {
    areas.push_back(area);
  }
};

TEST_CASE("Image rectangles unpack position and size", "[render-display]") {
  CHECK(Render::ImageRectangle(0x00200010, 0x00080004) ==
        Render::ScreenRect{16, 32, 4, 8});
  CHECK(Render::ImageRectangle(-1, 0x00010001) ==
        Render::ScreenRect{-1, -1, 1, 1});
  CHECK_FALSE(Render::ImageRectangle(0, 0x00080000).has_value());
  CHECK_FALSE(Render::ImageRectangle(0, 0x0008FFFF).has_value());
  CHECK_FALSE(Render::ImageRectangle(0, 0x80000004).has_value());

  FakeUpload target;
  CHECK(Render::UploadImageRectangle(target, 0x00200010, 0x00080004));
  CHECK_FALSE(Render::UploadImageRectangle(target, 0x00200010, 0));
  REQUIRE(target.areas.size() == 1);
  CHECK(target.areas[0] == Render::ScreenRect{16, 32, 4, 8});
}

TEST_CASE("Live image uploads write the packed VRAM rectangle",
          "[render-display]") {
  RECT area{};
  setRECT(&area, 1000, 500, 4, 2);
  std::array<std::uint16_t, 8> saved{};
  StoreImage(&area, reinterpret_cast<u_long*>(saved.data()));
  DrawSync(0);
  const std::array<std::uint16_t, 8> pixels{1, 2, 3, 4, 5, 6, 7, 8};

  VS::Render::UploadLiveImageRectangle(1000 | (500 << 16), pixels.data(),
                                       4 | (2 << 16));
  VS::Render::UploadLiveImageRectangle(1000 | (500 << 16), nullptr,
                                       4 | (2 << 16));
  DrawSync(0);
  std::array<std::uint16_t, 8> stored{};
  StoreImage(&area, reinterpret_cast<u_long*>(stored.data()));
  DrawSync(0);
  CHECK(stored == pixels);
  LoadImage(&area, reinterpret_cast<u_long*>(saved.data()));
  DrawSync(0);
}

// --- Camera --------------------------------------------------------------

// VectorNormal's table quantization leaves unit axes a few units off 4096:
// (0, 0, 10) normalizes to 4095, the right axis (-4095, 0, 0) to -4103 and the
// up axis (0, 4101, 0) to 4101 (SourceNormalizationTests' reference).
TEST_CASE("An axis-aligned camera keeps the 9/8 view aspect",
          "[render-display]") {
  const Render::CameraView view = Render::BuildCameraViewMatrix(
      {.position = {0, 0, 0}, .look_at = {0, 0, 10 * kOne}, .far_clip = kOne});
  CHECK(view.basis.m == Diagonal(4103, 4101, 4095).m);
  // The x row is widened by 4608 / 4096.
  CHECK(view.view.rotation.m == Diagonal(4615, 4101, 4095).m);
  CHECK(view.view.translation == Math::Vec3{});

  // (0, 0, 100) normalizes to 4098; a far clip of 8192 doubles the rotation
  // and the translation.
  const Render::CameraView far = Render::BuildCameraViewMatrix(
      {.position = {0, 0, -100 * kOne}, .look_at = {}, .far_clip = 2 * kOne});
  CHECK(far.view.rotation.m == Diagonal(9220, 8200, 8196).m);
  CHECK(far.view.translation == Math::Vec3{0, 0, 200});
}

TEST_CASE("Camera roll turns the view about its axis", "[render-display]") {
  const Render::CameraView view =
      Render::BuildCameraViewMatrix({.position = {0, 0, 0},
                                     .look_at = {0, 0, 10 * kOne},
                                     .roll = 1024,
                                     .far_clip = kOne});
  Math::Mat3 expected{};
  expected.m[0] = {0, -4101, 0};
  expected.m[1] = {4615, 0, 0};
  expected.m[2] = {0, 0, 4095};
  CHECK(view.view.rotation.m == expected.m);
  CHECK(view.basis.m == Diagonal(4103, 4101, 4095).m);
}

TEST_CASE("A camera looking straight up still has a right axis",
          "[render-display]") {
  const Render::CameraView view = Render::BuildCameraViewMatrix(
      {.position = {}, .look_at = {0, 5 * kOne, 0}, .far_clip = kOne});
  // The look direction (0, 5, 0) becomes (0, 5, 1): forward (0, 4016, 803)
  // and right (-803, 0, 0), which normalizes to -4102.
  CHECK(view.basis.m[0] == std::array<std::int16_t, 3>{4102, 0, 0});
  CHECK(view.basis.m[2] == std::array<std::int16_t, 3>{0, 4016, 803});
}

TEST_CASE("Look-at views keep full-word translations", "[render-display]") {
  CHECK(Render::BuildLookAtViewMatrix({0, 0, 0}, {0, 0, kOne}) ==
        Math::Transform3{Diagonal(kOne, kOne, kOne), {}});
  const Math::Vec3 position{100000, 0, -200000};
  const Math::Transform3 view = Render::BuildLookAtViewMatrix(
      position, {position.x, position.y, position.z + kOne});
  CHECK(view.rotation == Diagonal(kOne, kOne, kOne));
  CHECK(view.translation == Math::Vec3{-100000, 0, 200000});
}

TEST_CASE("Actor camera matrices invert the basis with the actor aspect",
          "[render-display]") {
  const Math::Mat3 identity = Diagonal(kOne, kOne, kOne);
  const Render::ActorCameraMatrices plain =
      Render::PrepareActorCameraMatrices(identity, 0, kOne);
  CHECK(plain.far_scale == kOne);
  CHECK(plain.rotation == Diagonal(3640, kOne, kOne));

  const Render::ActorCameraMatrices far =
      Render::PrepareActorCameraMatrices(identity, 0, 2 * kOne);
  CHECK(far.far_scale == 2048);
  CHECK(far.rotation == Diagonal(1820, 2048, 2048));

  const Render::ActorCameraMatrices rolled =
      Render::PrepareActorCameraMatrices(identity, 1024, kOne);
  Math::Mat3 expected{};
  expected.m[0] = {0, 3640, 0};
  expected.m[1] = {-kOne, 0, 0};
  expected.m[2] = {0, 0, kOne};
  CHECK(rolled.rotation == expected);

  // The original divided by zero; R3000 division gives -1.
  CHECK(Render::PrepareActorCameraMatrices(identity, 0, 0).far_scale == -1);
}

class FakeClip final : public Render::CameraClipTarget {
 public:
  std::vector<std::string> calls;
  void RecordNearClip(std::int32_t value) override { Log("near", value); }
  void ClipRoomPolygonsNear(std::int32_t value) override {
    Log("room-near", value);
  }
  void RecordProjectionDistance(std::int32_t value) override {
    Log("projection", value);
  }
  void SetScreenDistance(std::int32_t value) override { Log("screen", value); }
  void RecordFarClip(std::int32_t value) override { Log("far", value); }
  void SetRoomTextureLodDepth(std::int32_t value) override {
    Log("lod", value);
  }

 private:
  void Log(const char* name, std::int32_t value) {
    calls.push_back(std::string(name) + "=" + std::to_string(value));
  }
};

TEST_CASE("Texture LOD depth follows the far clip within its range",
          "[render-display]") {
  CHECK(Render::TextureLodDepthForFarClip(0) == 0);
  CHECK(Render::TextureLodDepthForFarClip(15) == 1);
  CHECK(Render::TextureLodDepthForFarClip(-7) == 0);
  CHECK(Render::TextureLodDepthForFarClip(-4096) == 0);
  CHECK(Render::TextureLodDepthForFarClip(0x4000) == 0x800);
  CHECK(Render::TextureLodDepthForFarClip(0x4008) == 0x800);
  CHECK(Render::TextureLodDepthForFarClip(
            std::numeric_limits<std::int32_t>::max()) == 0x800);
  CHECK(Render::TextureLodDepthForFarClip(
            std::numeric_limits<std::int32_t>::min()) == 0);
}

TEST_CASE("Clip settings are recorded and applied", "[render-display]") {
  FakeClip clip;
  Render::SetCameraNearClip(clip, 64);
  Render::SetCameraProjectionDistance(clip, 512);
  Render::SetCameraFarClip(clip, 800, false);
  Render::SetCameraFarClip(clip, 1600, true);
  CHECK(clip.calls == std::vector<std::string>{
                          "near=64", "room-near=64", "projection=512",
                          "screen=512", "far=800", "lod=100", "far=1600"});
}

TEST_CASE("Live far clips keep the room texture LOD when asked (#279)",
          "[render-display]") {
  Render::RoomClipState& room = Render::LiveRoomClipState();
  const Render::RoomClipState saved_room = room;
  const int saved_far = Render_frameScratch.camera.farClip;

  VS::Render::SetLiveCameraFarClip(800, 0);
  CHECK(room.lod_depth == 100U);
  CHECK(Render_frameScratch.camera.farClip == 800);
  // Effects/Camera.cpp passes its z offset as the preserve flag.
  VS::Render::SetLiveCameraFarClip(1600, -3);
  CHECK(room.lod_depth == 100U);
  CHECK(Render_frameScratch.camera.farClip == 1600);
  room = saved_room;
  Render_frameScratch.camera.farClip = saved_far;
}

// --- Display -------------------------------------------------------------

class FakeDisplay final : public Render::DisplayDevice {
 public:
  std::vector<std::string> calls;
  std::optional<Render::FieldDisplayLayout> layout;
  std::vector<Render::ScreenRect> uploads;
  std::size_t uploaded_pixels = 0;

  void SetGeometryOffset(std::int32_t x, std::int32_t y) override {
    calls.push_back("offset " + std::to_string(x) + "," + std::to_string(y));
  }
  void SetProjectionDistance(std::int32_t distance) override {
    calls.push_back("projection " + std::to_string(distance));
  }
  void ApplyFieldLayout(const Render::FieldDisplayLayout& value) override {
    calls.emplace_back("layout");
    layout = value;
  }
  void ShowDisplayArea(const Render::ScreenRect& area,
                       const Render::ScreenRect& screen) override {
    calls.push_back("show " + Text(area) + " " + Text(screen));
  }
  void ClearToBlack(const Render::ScreenRect& area) override {
    calls.push_back("clear " + Text(area));
  }
  void WaitVerticalBlanks(std::int32_t count) override {
    calls.push_back("vsync " + std::to_string(count));
  }
  void UploadImage(const Render::ScreenRect& area,
                   std::span<const std::uint16_t> pixels) override {
    calls.push_back("upload " + Text(area));
    uploads.push_back(area);
    uploaded_pixels = pixels.size();
  }
  void SetDisplayEnabled(bool enabled) override {
    calls.emplace_back(enabled ? "display on" : "display off");
  }

 private:
  static std::string Text(const Render::ScreenRect& area) {
    return std::to_string(area.x) + "," + std::to_string(area.y) + "," +
           std::to_string(area.width) + "x" + std::to_string(area.height);
  }
};

TEST_CASE("The field display double-buffers side by side", "[render-display]") {
  const Render::FieldDisplayLayout layout =
      Render::FieldLayoutFor({320, 240, 512, {1, 2, 3}});
  CHECK(layout.draw_areas[0] == Render::ScreenRect{0, 0, 320, 224});
  CHECK(layout.draw_areas[1] == Render::ScreenRect{320, 0, 320, 224});
  CHECK(layout.display_areas[0] == Render::ScreenRect{320, 0, 320, 224});
  CHECK(layout.display_areas[1] == Render::ScreenRect{0, 0, 320, 224});
  CHECK(layout.screen == Render::ScreenRect{0, 8, 256, 224});
  CHECK(layout.viewport == Render::ScreenRect{0, 0, 320, 224});
  CHECK(layout.clear == Render::ClearColor{1, 2, 3});
}

TEST_CASE("Configuring the field display centres the geometry offset",
          "[render-display]") {
  FakeDisplay device;
  Render::ConfigureFieldDisplay(device, {320, 240, 512, {1, 2, 3}});
  CHECK(device.calls ==
        std::vector<std::string>{"offset 160,128", "projection 512", "layout"});
  CHECK(device.layout == Render::FieldLayoutFor({320, 240, 512, {1, 2, 3}}));

  // The original's height - 16 rounds toward zero before halving.
  FakeDisplay tiny;
  Render::ConfigureFieldDisplay(tiny, {7, 3, 0, {}});
  CHECK(tiny.calls[0] == "offset 3,10");
}

TEST_CASE("Display masks forward to the device", "[render-display]") {
  FakeDisplay device;
  Render::ShowDisplay(device);
  Render::HideDisplay(device);
  Render::EnableDisplay(device);
  Render::DisableDisplay(device);
  CHECK(device.calls == std::vector<std::string>{"display on", "display off",
                                                 "display on", "display off"});
}

void CheckLiveDrawEnvironment(int index) {
  CAPTURE(index);
  const DRAWENV& draw = Render_legacyDrawEnvironments[index];
  CHECK(draw.clip.x == (index == 0 ? 0 : 320));
  CHECK(draw.clip.w == 320);
  CHECK(draw.clip.h == 224);
  CHECK(draw.dtd == 1);
  CHECK(draw.dfe == 0);
  CHECK(draw.isbg == 0);
  CHECK(draw.r0 == 1);
  CHECK(draw.g0 == 2);
  CHECK(draw.b0 == 3);
}

void CheckLiveDisplayEnvironment(int index) {
  CAPTURE(index);
  const DISPENV& display = Render_legacyDisplayEnvironments[index];
  CHECK(display.disp.x == (index == 0 ? 320 : 0));
  CHECK(display.disp.h == 224);
  CHECK(display.screen.y == 8);
  CHECK(display.screen.w == 256);
  CHECK(display.screen.h == 224);
}

TEST_CASE("Live field display configures both environments",
          "[render-display]") {
  std::array<DRAWENV, 2> saved_draw{};
  std::array<DISPENV, 2> saved_display{};
  std::copy_n(Render_legacyDrawEnvironments, 2, saved_draw.begin());
  std::copy_n(Render_legacyDisplayEnvironments, 2, saved_display.begin());
  const auto saved_left = Render_legacyViewportLeft;
  const auto saved_top = Render_legacyViewportTop;
  const auto saved_width = Render_viewportWidth;
  const auto saved_height = Render_viewportHeight;

  VS::Render::ConfigureLiveFieldDisplay(320, 240, 512, 1, 2, 3);

  for (int index = 0; index < 2; ++index) {
    CheckLiveDrawEnvironment(index);
    CheckLiveDisplayEnvironment(index);
  }
  CHECK(Render_viewportWidth == 320);
  CHECK(Render_viewportHeight == 224);

  std::copy(saved_draw.begin(), saved_draw.end(),
            Render_legacyDrawEnvironments);
  std::copy(saved_display.begin(), saved_display.end(),
            Render_legacyDisplayEnvironments);
  Render_legacyViewportLeft = saved_left;
  Render_legacyViewportTop = saved_top;
  Render_viewportWidth = saved_width;
  Render_viewportHeight = saved_height;
  Core_Gte_Reset();
}

class FakeFrame final : public Render::FieldFrameHost {
 public:
  int buffer = 0;
  bool paused = false;
  std::vector<std::uint32_t> presses;
  std::vector<std::string> calls;

  [[nodiscard]] int FrameBuffer() const override { return buffer; }
  void SelectFrameBuffer(int index) override {
    buffer = index;
    calls.push_back("select " + std::to_string(index));
  }
  void AdvanceRandom() override { calls.emplace_back("random"); }
  void SetGeometryOffset(std::int32_t x, std::int32_t y) override {
    calls.push_back("offset " + std::to_string(x) + "," + std::to_string(y));
  }
  [[nodiscard]] std::int32_t TickSpeed() const override { return 3; }
  std::int32_t UpdatePlayTime(std::int32_t ticks) override {
    calls.push_back("time " + std::to_string(ticks));
    return ticks;
  }
  [[nodiscard]] bool FrameStepPaused() const override { return paused; }
  void ToggleFrameStepPause() override {
    paused = !paused;
    calls.emplace_back("toggle");
  }
  std::uint32_t PollSecondControllerPressed() override {
    calls.emplace_back("poll");
    if (presses.empty()) {
      return Render::kFrameStepAdvanceButton;
    }
    const std::uint32_t pressed = presses.front();
    presses.erase(presses.begin());
    return pressed;
  }
  void PresentFrameBuffer(int index) override {
    calls.push_back("present " + std::to_string(index));
  }
  void CommitPalettes() override { calls.emplace_back("cluts"); }
  void DrawOrderingTable() override { calls.emplace_back("draw"); }
};

TEST_CASE("Field frames flip the buffer and submit the frame",
          "[render-display]") {
  FakeFrame frame;
  CHECK(Render::PresentFieldFrame(frame) == 3);
  CHECK(frame.calls == std::vector<std::string>{"select 1", "random",
                                                "offset 160,112", "time 3",
                                                "present 1", "cluts", "draw"});
  frame.calls.clear();
  Render::PresentFieldFrame(frame);
  CHECK(frame.calls.front() == "select 0");
}

TEST_CASE("The debug frame step waits for the second controller",
          "[render-display]") {
  FakeFrame step;
  step.paused = true;
  step.presses = {0, 0, Render::kFrameStepAdvanceButton};
  CHECK(Render::PresentFieldFrame(step) == 3);
  CHECK(std::ranges::count(step.calls, std::string("poll")) == 3);
  CHECK(std::ranges::count(step.calls, std::string("time 2")) == 3);
  CHECK(step.paused);
  CHECK(step.calls.back() == "draw");

  FakeFrame resume;
  resume.paused = true;
  resume.presses = {Render::kFrameStepToggleButton};
  Render::PresentFieldFrame(resume);
  CHECK_FALSE(resume.paused);
  CHECK(std::ranges::count(resume.calls, std::string("poll")) == 1);
}

TEST_CASE("The loading screen fills both buffers", "[render-display]") {
  const std::vector<std::uint16_t> pixels(std::size_t{126} * 20, 0x7FFF);
  const Render::LoadingImage image{126, 20, pixels};
  CHECK(Render::LoadingImageArea(image) ==
        Render::ScreenRect{97, 102, 126, 20});

  FakeDisplay device;
  CHECK(Render::ShowLoadingScreen(device, image));
  CHECK(device.calls ==
        std::vector<std::string>{
            "show 0,0,320x224 0,8,256x224", "clear 0,0,1024x512", "vsync 2",
            "upload 97,102,126x20", "upload 417,102,126x20", "display on"});
  CHECK(device.uploaded_pixels == pixels.size());

  FakeDisplay back;
  CHECK(Render::UploadLoadingScreenBackBuffer(back, image, 0));
  CHECK(Render::UploadLoadingScreenBackBuffer(back, image, 1));
  CHECK(back.uploads == std::vector<Render::ScreenRect>{{417, 102, 126, 20},
                                                        {97, 102, 126, 20}});
}

TEST_CASE("A loading image larger than its pixels is not uploaded",
          "[render-display]") {
  const std::vector<std::uint16_t> pixels((std::size_t{126} * 20) - 1);
  FakeDisplay device;
  CHECK_FALSE(Render::ShowLoadingScreen(device, {126, 20, pixels}));
  CHECK(device.uploads.empty());
  CHECK(device.calls.back() == "display on");
  CHECK_FALSE(
      Render::UploadLoadingScreenBackBuffer(device, {126, 20, pixels}, 0));
  CHECK_FALSE(Render::ShowLoadingScreen(device, {0, 20, pixels}));
  CHECK_FALSE(Render::ShowLoadingScreen(device, {-126, 20, pixels}));
  CHECK(device.uploads.empty());
}
}  // namespace
