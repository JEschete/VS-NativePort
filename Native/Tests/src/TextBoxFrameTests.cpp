#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <random>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Core/Memory/HeapLive.hpp"
#include "Event/ScriptVmLive.hpp"
#include "Event/ScriptVmLiveState.hpp"
#include "Menu/TextBox.hpp"
#include "Menu/TextBoxFrames.hpp"
#include "Menu/TextBoxFramesLive.hpp"
#include "Menu/TextBoxStateLive.hpp"
#include "Render/Display.hpp"
#include "Render/Frame.hpp"
#include "Render/Render.hpp"
#include "Render/UiPrimitives.hpp"

namespace {

class FrameQueue final : public VS::Render::PacketQueue {
 public:
  std::vector<std::vector<std::uint32_t>> packets;
  std::size_t capacity = std::numeric_limits<std::size_t>::max();
  std::size_t words = 0;
  std::vector<std::vector<std::uint32_t>>* emitted = nullptr;

  std::uint32_t ChainLink() const override {
    return static_cast<std::uint32_t>(packets.size() + 1);
  }

  bool Emit(std::span<const std::uint32_t> packet) override {
    if (packet.size() > capacity - words) {
      return false;
    }
    words += packet.size();
    packets.emplace_back(packet.begin(), packet.end());
    if (emitted != nullptr) {
      emitted->push_back(packets.back());
    }
    return true;
  }
};

struct InformationCommand {
  enum class Kind : std::uint8_t { kHeader, kRow, kFill };
  Kind kind;
  std::array<std::int32_t, 5> values{};
  friend bool operator==(const InformationCommand&,
                         const InformationCommand&) = default;
};

class InformationWorld final : public VS::Menu::InformationBoxWorld {
 public:
  VS::Menu::TextBox box;
  bool alternate = false;
  bool open_when_drawn = false;
  std::size_t capacity = std::numeric_limits<std::size_t>::max();
  std::vector<InformationCommand> commands;

  VS::Menu::TextBox ReadBox() const override { return box; }
  bool AlternateFramebuffer() const override { return alternate; }
  bool DrawHeader(std::size_t kind, std::int32_t x, std::int32_t y) override {
    return Record({InformationCommand::Kind::kHeader,
                   {static_cast<std::int32_t>(kind), x, y}});
  }
  bool DrawFramebufferRow(std::int32_t x, std::int32_t y, std::int32_t width,
                          std::int32_t source_x,
                          std::uint8_t brightness) override {
    return Record(
        {InformationCommand::Kind::kRow, {x, y, width, source_x, brightness}});
  }
  bool DrawBlackFill(std::int32_t x, std::int32_t y, std::int32_t width,
                     std::int32_t height) override {
    return Record({InformationCommand::Kind::kFill, {x, y, width, height}});
  }

 private:
  bool Record(const InformationCommand& command) {
    if (commands.size() == capacity) {
      return false;
    }
    commands.push_back(command);
    if (open_when_drawn) {
      box.state = static_cast<std::int16_t>(
          static_cast<std::uint8_t>(box.open_duration) + 1);
    }
    return true;
  }
};

using Triangle = std::array<std::uint8_t, 14>;
const Triangle kTriangle = {3, 0, 3, 1, 0, 0, 255, 0, 0, 255, 0, 1, 2, 255};

class LiveFrameFixture {
 public:
  LiveFrameFixture()
      : saved_box_(VS::Menu::textBoxes[0]),
        saved_table_(VS::Menu::activeTextBoxOrderingTable),
        saved_frame_(VS::Render::FrameScratchpad()),
        slot_(VS::Render::frameBuf & 1),
        saved_buffer_(VS::Render::legacyPrimitiveBuffers[slot_]),
        storage_(kPrimitiveWords) {
    std::ranges::copy(VS::Event::textBoxFrameStyles, saved_styles_.begin());
    VS::Menu::activeTextBoxOrderingTable = ordering_.data();
    VS::Render::legacyPrimitiveBuffers[slot_] = storage_.data();
  }

  ~LiveFrameFixture() {
    VS::Menu::textBoxes[0] = saved_box_;
    std::ranges::copy(saved_styles_, VS::Event::textBoxFrameStyles);
    VS::Menu::activeTextBoxOrderingTable = saved_table_;
    VS::Render::FrameScratchpad() = saved_frame_;
    VS::Render::legacyPrimitiveBuffers[slot_] = saved_buffer_;
  }

  std::vector<std::vector<std::uint32_t>> Draw(
      std::vector<std::uint8_t>& bytes, const VS::Menu::FrameRequest& request) {
    auto* source = static_cast<std::uint8_t*>(
        VS::Core::Memory::AllocateLiveFromHighEnd(bytes.size()));
    REQUIRE(source != nullptr);
    std::ranges::copy(bytes, source);
    auto packets = DrawSource(source, request);
    VS::Core::Memory::FreeLiveFromHighEnd(source);
    return packets;
  }

  std::vector<std::vector<std::uint32_t>> DrawSource(
      const std::uint8_t* source, const VS::Menu::FrameRequest& request,
      unsigned style = 0, std::size_t capacity_words = kPrimitiveWords) {
    VS::Menu::textBoxes[0] = {};
    auto& box = VS::Menu::textBoxes[0];
    box.flags.frame_shape = style;
    box.flags.frame_part = static_cast<unsigned>(request.part);
    box.flags.alignment = request.alignment;
    box.frame_x = static_cast<std::int16_t>(request.x);
    box.frame_y = static_cast<std::int16_t>(request.y);
    box.frame_width = static_cast<std::int16_t>(request.width);
    box.frame_height = static_cast<std::int16_t>(request.height);
    box.brightness = request.brightness;
    VS::Event::textBoxFrameStyles[style] = const_cast<std::uint8_t*>(source);
    const auto* start = BeginDraw(capacity_words);
    VS::Menu::DrawLiveTextBoxFrame(0);
    return ReadPackets(start);
  }

  std::vector<std::vector<std::uint32_t>> DrawBackdrop(
      const VS::Menu::TextBox& box,
      std::size_t capacity_words = kPrimitiveWords) {
    VS::Menu::WriteLiveTextBox(0, box);
    const auto* start = BeginDraw(capacity_words);
    VS::Menu::DrawLiveInformationBoxBackdrop(0);
    return ReadPackets(start);
  }

 private:
  std::uint32_t* BeginDraw(std::size_t capacity_words) {
    REQUIRE(capacity_words <= kPrimitiveWords);
    ordering_.fill(0xFFFFFF);
    auto* start = storage_.data() + kPrimitiveWords - capacity_words;
    VS::Render::FrameScratchpad().primitive_cursor = start;
    VS::Render::FrameScratchpad().overlay_ordering_table = ordering_.data();
    return start;
  }

  std::vector<std::vector<std::uint32_t>> ReadPackets(
      const std::uint32_t* start) {
    std::vector<std::vector<std::uint32_t>> packets;
    const auto* end = static_cast<const std::uint32_t*>(
        VS::Render::FrameScratchpad().primitive_cursor);
    REQUIRE(end <= storage_.data() + storage_.size());
    for (const auto* cursor = start; cursor < end;) {
      const auto words = (*cursor >> 24) + 1;
      REQUIRE(words <= static_cast<std::size_t>(end - cursor));
      packets.emplace_back(cursor, cursor + words);
      packets.back()[0] &= 0xFF000000;
      cursor += words;
    }
    return packets;
  }

  static constexpr std::size_t kPrimitiveWords =
      0x20000 / sizeof(std::uint32_t);
  static constexpr std::size_t kArenaBytes = 4096;
  std::array<std::byte, kArenaBytes> arena_{};
  VS::Core::Memory::ScopedLiveGameHeap heap_{arena_};
  VS::Menu::TextBoxState saved_box_;
  std::array<unsigned char*, 16> saved_styles_{};
  decltype(VS::Menu::activeTextBoxOrderingTable) saved_table_;
  VS::Render::FrameScratch saved_frame_;
  int slot_;
  void* saved_buffer_;
  std::vector<std::uint32_t> storage_;
  std::array<std::uint32_t, 4> ordering_{};
};

VS::Menu::FrameRequest RandomFrameRequest(std::mt19937& random) {
  return {static_cast<int>(random() % 600) - 300,
          static_cast<int>(random() % 400) - 200,
          static_cast<int>(random() % 640) - 320,
          static_cast<int>(random() % 480) - 240,
          0,
          static_cast<std::uint8_t>(random()),
          0};
}

void CompareFramePackets(LiveFrameFixture& original,
                         std::vector<std::uint8_t>& bytes,
                         const VS::Menu::FrameRequest& request) {
  const auto expected = original.Draw(bytes, request);
  std::vector<std::vector<std::uint32_t>> actual;
  FrameQueue foreground;
  FrameQueue shadow;
  foreground.emitted = &actual;
  shadow.emitted = &actual;
  REQUIRE(VS::Menu::DrawTextBoxFrame(foreground, shadow, bytes, request));
  for (auto& packet : actual) {
    packet[0] &= 0xFF000000;
  }
  REQUIRE(actual == expected);
}

std::uint64_t HashFramePackets(
    std::uint64_t hash,
    const std::vector<std::vector<std::uint32_t>>& packets) {
  constexpr std::uint64_t kPrime = 1099511628211;
  for (const auto& packet : packets) {
    hash = (hash ^ packet.size()) * kPrime;
    for (const auto word : packet) {
      hash = (hash ^ word) * kPrime;
    }
  }
  return hash;
}

void BindNativeFrame(
    const std::shared_ptr<const std::vector<std::uint8_t>>& owner) {
  namespace event = VS::Event;
  REQUIRE(
      event::MountLiveScriptSource(event::ScriptRegion::kEventScript, owner));
  const auto source =
      event::LeaseLiveScriptSource(event::ScriptRegion::kEventScript);
  REQUIRE(source);
  REQUIRE(source->At(0));
  event::LiveScriptVm().SetTextBoxFrameStyle(9, *source->At(0));
}

struct FrameVmScope {
  FrameVmScope() { VS::Event::ResetLiveScriptRuntime(); }
  ~FrameVmScope() { VS::Event::ResetLiveScriptRuntime(); }
};

}  // namespace

TEST_CASE("Information headers select only the three artwork entries",
          "[text-box-frame][information-box]") {
  InformationWorld world;
  for (std::uint32_t kind = 0; kind < 3; ++kind) {
    REQUIRE(VS::Menu::DrawInformationBoxHeader(world, -17, kind));
    CHECK(world.commands.back() ==
          InformationCommand{InformationCommand::Kind::kHeader,
                             {static_cast<std::int32_t>(kind), 10, -17}});
  }
  for (const auto absent : {3U, 255U, 0xFFFFFFFFU}) {
    CHECK(VS::Menu::DrawInformationBoxHeader(world, 42, absent));
  }
  CHECK(world.commands.size() == 3);
  world.capacity = world.commands.size();
  CHECK_FALSE(VS::Menu::DrawInformationBoxHeader(world, 42, 1));
}

TEST_CASE("Information backdrops shade both framebuffer strips toward the edge",
          "[text-box-frame][information-box]") {
  InformationWorld world;
  world.box.line_count = 1;
  world.box.frame_height = 10;
  REQUIRE(VS::Menu::DrawInformationBoxBackdrop(world));
  REQUIRE(world.commands.size() == 15);
  CHECK(world.commands[0] == InformationCommand{InformationCommand::Kind::kRow,
                                                {256, 10, 64, 576, 112}});
  CHECK(world.commands[1] == InformationCommand{InformationCommand::Kind::kRow,
                                                {0, 10, 256, 320, 112}});
  CHECK(world.commands[13].values ==
        std::array<std::int32_t, 5>{0, 4, 256, 320, 16});
  CHECK(world.commands.back() ==
        InformationCommand{InformationCommand::Kind::kFill, {0, 0, 320, 4}});
}

TEST_CASE("Information backdrops clamp unsigned rows and reread header state",
          "[text-box-frame][information-box]") {
  InformationWorld world;
  world.alternate = true;
  world.open_when_drawn = true;
  world.box.y = 1;
  world.box.line_count = 1;
  world.box.frame_height = 20;
  world.box.open_duration = -1;
  world.box.flags.frame_shape = 2;
  REQUIRE(VS::Menu::DrawInformationBoxBackdrop(world));
  REQUIRE(world.commands.size() == 34);
  CHECK(world.commands.front().values ==
        std::array<std::int32_t, 5>{256, 220, 64, 256, 120});
  CHECK(world.commands[32] ==
        InformationCommand{InformationCommand::Kind::kFill, {0, 236, 320, 4}});
  CHECK(world.commands.back() ==
        InformationCommand{InformationCommand::Kind::kHeader, {1, 10, 229}});

  world.commands.clear();
  world.open_when_drawn = false;
  world.box.y = 0;
  world.box.line_count = -3;
  world.box.frame_height = 3289;
  world.box.state = 0;
  REQUIRE(VS::Menu::DrawInformationBoxBackdrop(world));
  REQUIRE(world.commands.size() == 2);
  CHECK(world.commands.front().values[4] == 102);
}

TEST_CASE("Information backdrops stop at screen and output capacity boundaries",
          "[text-box-frame][information-box]") {
  InformationWorld world;
  world.box.line_count = 4;
  REQUIRE(VS::Menu::DrawInformationBoxBackdrop(world));
  REQUIRE(world.commands.size() == 2);
  CHECK(world.commands[0].values[1] == 0);

  world.commands.clear();
  world.capacity = 1;
  CHECK_FALSE(VS::Menu::DrawInformationBoxBackdrop(world));
  CHECK(world.commands.size() == 1);
}

TEST_CASE("Text box shape reflection preserves packed coordinate word behavior",
          "[text-box-frame]") {
  using VS::Menu::FlipTextBoxShapePoint;
  CHECK(FlipTextBoxShapePoint(0x0102, 0) == 0x0102);
  CHECK(FlipTextBoxShapePoint(0x0102, 1) == 0xFE02);
  CHECK(FlipTextBoxShapePoint(0x0102, 2) == 0x01FD);
  CHECK(FlipTextBoxShapePoint(0x0102, 3) == 0xFEFD);
  CHECK(FlipTextBoxShapePoint(0x89ABCDEF, 0xFFFFFFFC) == 0x89ABCDEF);
  CHECK(FlipTextBoxShapePoint(0x89ABCDEF, 2) == 0xCD10);
  CHECK(FlipTextBoxShapePoint(0xFFFFFFFF, 1) == 0x100FF);
  CHECK(VS::Menu::FlipLiveTextBoxShapePoint(-1, 1) == 0x100FF);
}

TEST_CASE("Live information backdrops emit bounded strips fill and header",
          "[text-box-frame][information-box]") {
  LiveFrameFixture frame;
  VS::Menu::TextBox box;
  box.y = 1;
  box.frame_height = 20;
  box.line_count = 1;
  box.open_duration = 3;
  box.state = 4;
  box.flags.frame_shape = 2;
  const auto packets = frame.DrawBackdrop(box);
  REQUIRE(packets.size() == 34);
  CHECK(packets[0][3] == 0x00DC0100);
  CHECK(packets[0][4] == 0x0000DC00);
  CHECK(packets[32][1] == 0x60000000);
  CHECK(packets[32][3] == 0x00040140);
  CHECK(packets[33][3] == 0x00E5000A);
  for (std::size_t capacity = 0; capacity <= 202; ++capacity) {
    const auto bounded = frame.DrawBackdrop(box, capacity);
    std::size_t written = 0;
    for (const auto& packet : bounded) {
      written += packet.size();
    }
    CHECK(written <= capacity);
  }
  VS::Menu::DrawLiveInformationBoxBackdrop(-1);
  VS::Menu::DrawLiveInformationBoxBackdrop(8);
}

TEST_CASE("Text box frames decode owned geometry", "[text-box-frame]") {
  std::vector<std::uint8_t> bytes{3, 0, 3,   1, 0, 0, 255,
                                  0, 0, 255, 0, 1, 2, 255};
  auto decoded = VS::Menu::DecodeTextBoxFrame(bytes);
  REQUIRE(decoded.has_value());
  CHECK(decoded->vertices.size() == 3);
  CHECK(decoded->faces.size() == 1);
  bytes.clear();
  CHECK(decoded->vertices[1][0] == 255);
  CHECK(decoded->faces[0][2] == 2);
}

TEST_CASE("Text box frames reject incomplete and invalid definitions",
          "[text-box-frame]") {
  const std::vector<std::uint8_t> valid{3, 0, 3,   1, 0, 0, 255,
                                        0, 0, 255, 0, 1, 2, 255};
  for (std::size_t length = 0; length < valid.size(); ++length) {
    CHECK_FALSE(VS::Menu::DecodeTextBoxFrame(
                    std::span<const std::uint8_t>(valid).first(length))
                    .has_value());
  }
  const std::array<std::uint8_t, 10> bad_face{1, 0, 1, 1, 0, 0, 255, 0, 0, 255};
  CHECK(VS::Menu::DecodeTextBoxFrame(bad_face).error() ==
        VS::Menu::FrameDecodeError::kFace);
  const std::array<std::uint8_t, 16> backward{4, 1, 3, 0, 2, 0, 1, 0,
                                              0, 0, 0, 0, 0, 0, 0, 0};
  CHECK(VS::Menu::DecodeTextBoxFrame(backward).error() ==
        VS::Menu::FrameDecodeError::kOutline);
}

TEST_CASE("Text box frames do not inherit scratch point capacity",
          "[text-box-frame]") {
  constexpr std::uint8_t kVertices = 255;
  std::vector<std::uint8_t> bytes{kVertices, 0, kVertices, 0};
  bytes.resize(4 + kVertices * 2);
  const auto decoded = VS::Menu::DecodeTextBoxFrame(bytes);
  REQUIRE(decoded.has_value());
  CHECK(decoded->vertices.size() == kVertices);
}

TEST_CASE("Text box frame triangle packets preserve fill and shadow words",
          "[text-box-frame]") {
  FrameQueue foreground;
  FrameQueue shadow;
  const VS::Menu::FrameRequest request{10, 20, 256, 256, 0, 128, 0};
  REQUIRE(VS::Menu::DrawTextBoxFrame(foreground, shadow, kTriangle, request));
  REQUIRE(foreground.packets.size() == 6);
  CHECK(foreground.packets[0] ==
        std::vector<std::uint32_t>{0x03000001, 0x40303030, 0x0014000A,
                                   0x00140109});
  CHECK(foreground.packets[3] ==
        std::vector<std::uint32_t>{0x01000004, 0xE2000000});
  CHECK(foreground.packets[4] ==
        std::vector<std::uint32_t>{0x07000005, 0x24808080, 0x0014000A,
                                   0x373E140A, 0x00140109, 0x00171409,
                                   0x0113000A, 0x0000130A});
  CHECK(foreground.packets[5] ==
        std::vector<std::uint32_t>{0x03000006, 0xE1000017, 0xE2000318, 0});
  REQUIRE(shadow.packets.size() == 1);
  CHECK(shadow.packets[0] ==
        std::vector<std::uint32_t>{0x0C000001, 0x20000000, 0x0016000C,
                                   0x0016010B, 0x0115000C, 0, 0x48000000,
                                   0x0016000C, 0x0016010B, 0x0016010B,
                                   0x0115000C, 0x0016000C, 0x55555555});
}

TEST_CASE("Text box frame refusal does not emit malformed geometry",
          "[text-box-frame]") {
  FrameQueue foreground;
  FrameQueue shadow;
  VS::Menu::FrameRequest request;
  request.part = 16;
  CHECK(VS::Menu::DrawTextBoxFrame(foreground, shadow, kTriangle, request)
            .error() == VS::Menu::FrameDecodeError::kSelectedPart);
  CHECK(foreground.packets.empty());
  CHECK(shadow.packets.empty());
  request.part = 0;
  foreground.capacity = 15;
  CHECK(VS::Menu::DrawTextBoxFrame(foreground, shadow, kTriangle, request)
            .error() == VS::Menu::FrameDecodeError::kCapacity);
  CHECK(foreground.words == 14);
  CHECK(shadow.packets.empty());
}

TEST_CASE(
    "Missing encoded frame parts preserve the original base shape fallback",
    "[text-box-frame]") {
  const VS::Menu::FrameRequest base{10, 20, 256, 256, 0, 128, 0};
  FrameQueue expected_foreground;
  FrameQueue expected_shadow;
  REQUIRE(VS::Menu::DrawTextBoxFrame(expected_foreground, expected_shadow,
                                     kTriangle, base));
  for (std::size_t part = 1; part < 16; ++part) {
    FrameQueue foreground;
    FrameQueue shadow;
    auto request = base;
    request.part = part;
    REQUIRE(VS::Menu::DrawTextBoxFrame(foreground, shadow, kTriangle, request));
    CHECK(foreground.packets == expected_foreground.packets);
    CHECK(shadow.packets == expected_shadow.packets);
  }
}

TEST_CASE("Frame vertex storage may include an unused alignment slot",
          "[text-box-frame]") {
  const std::array<std::uint8_t, 16> padded{4, 0,   3,  1,  0, 0, 255, 0,
                                            0, 255, 99, 99, 0, 1, 2,   255};
  REQUIRE(VS::Menu::DecodeTextBoxFrame(padded));
  FrameQueue foreground;
  FrameQueue shadow;
  FrameQueue expected_foreground;
  FrameQueue expected_shadow;
  REQUIRE(VS::Menu::DrawTextBoxFrame(foreground, shadow, padded, {}));
  REQUIRE(VS::Menu::DrawTextBoxFrame(expected_foreground, expected_shadow,
                                     kTriangle, {}));
  CHECK(foreground.packets == expected_foreground.packets);
  CHECK(shadow.packets == expected_shadow.packets);
}

TEST_CASE("Frame part faces may share transformed extra storage slots",
          "[text-box-frame]") {
  const std::array<std::uint8_t, 28> shared{
      6,   2, 4,   0,   0, 1,   1, 1,  2,  3,  1, 0, 0, 0,
      255, 0, 255, 255, 0, 255, 0, 32, 32, 32, 0, 4, 5, 1};
  REQUIRE(VS::Menu::DecodeTextBoxFrame(shared));
  FrameQueue foreground;
  FrameQueue shadow;
  const VS::Menu::FrameRequest request{10, 20, 256, 256, 0, 128, 1};
  REQUIRE(VS::Menu::DrawTextBoxFrame(foreground, shadow, shared, request));
  REQUIRE(foreground.packets.size() == 8);
  CHECK(foreground.packets[6][6] == 0x0034002A);
}

TEST_CASE("Text box frame queues respect every foreground capacity boundary",
          "[text-box-frame]") {
  for (std::size_t capacity = 0; capacity <= 26; ++capacity) {
    FrameQueue foreground;
    FrameQueue shadow;
    foreground.capacity = capacity;
    const auto result =
        VS::Menu::DrawTextBoxFrame(foreground, shadow, kTriangle, {});
    CHECK(foreground.words <= capacity);
    CHECK(result.has_value() == (capacity == 26));
    if (!result) {
      CHECK(result.error() == VS::Menu::FrameDecodeError::kCapacity);
    }
  }
  for (std::size_t capacity = 0; capacity <= 13; ++capacity) {
    FrameQueue foreground;
    FrameQueue shadow;
    shadow.capacity = capacity;
    const auto result =
        VS::Menu::DrawTextBoxFrame(foreground, shadow, kTriangle, {});
    CHECK(shadow.words <= capacity);
    CHECK(result.has_value() == (capacity == 13));
  }
}

TEST_CASE("Text box frame faces cannot leave initialized vertex storage",
          "[text-box-frame]") {
  std::vector<std::uint8_t> bytes{6, 2,   4, 0,   0, 1,   1,   1, 2,   3,  1,
                                  1, 0,   0, 255, 0, 255, 255, 0, 255, 0,  32,
                                  0, 224, 0, 4,   1, 255, 2,   5, 3,   255};
  const std::array<std::uint8_t, 14> unwritten_base{3, 0, 2,   1, 0, 0, 255,
                                                    0, 0, 255, 0, 1, 2, 255};
  CHECK(VS::Menu::DecodeTextBoxFrame(unwritten_base).error() ==
        VS::Menu::FrameDecodeError::kFace);
  REQUIRE(VS::Menu::DecodeTextBoxFrame(bytes));
  bytes[25] = 6;
  CHECK(VS::Menu::DecodeTextBoxFrame(bytes).error() ==
        VS::Menu::FrameDecodeError::kFace);
  FrameQueue foreground;
  FrameQueue shadow;
  CHECK_FALSE(VS::Menu::DrawTextBoxFrame(foreground, shadow, bytes, {}));
  CHECK(foreground.packets.empty());
  CHECK(shadow.packets.empty());
  bytes[25] = 4;
  bytes[0] = 5;
  CHECK(VS::Menu::DecodeTextBoxFrame(bytes).error() ==
        VS::Menu::FrameDecodeError::kVertexCount);
}

TEST_CASE("Text box frame packets match the original valid renderer",
          "[text-box-frame]") {
  LiveFrameFixture original;
  std::seed_seq seed{315};
  std::mt19937 random(seed);
  std::uint64_t original_hash = 14695981039346656037ULL;
  for (int sample = 0; sample < 100; ++sample) {
    for (std::uint8_t alignment = 0; alignment < 4; ++alignment) {
      for (std::size_t part = 0; part < 3; ++part) {
        std::vector<std::uint8_t> bytes{
            8,   2, 4,   1,   0, 1,   2, 1,  2,  3,  2, 1,   0,   0,
            255, 0, 255, 255, 0, 255, 0, 32, 32, 32, 0, 224, 224, 224,
            0,   1, 2,   3,   0, 4,   5, 1,  2,  6,  7, 3};
        if ((sample & 1) != 0) {
          bytes[31] = 255;
        }
        auto request = RandomFrameRequest(random);
        request.alignment = alignment;
        request.part = part;
        CompareFramePackets(original, bytes, request);
        original_hash =
            HashFramePackets(original_hash, original.Draw(bytes, request));
      }
    }
  }
  CHECK(original_hash == 0x9C1CB765277A2800ULL);
}

TEST_CASE("Live text box frames refuse untracked style bytes",
          "[text-box-frame]") {
  LiveFrameFixture frame;
  CHECK(frame.DrawSource(kTriangle.data(), {}).empty());
  VS::Menu::DrawLiveTextBoxFrame(-1);
  VS::Menu::DrawLiveTextBoxFrame(8);
  CHECK(frame.DrawSource(nullptr, {}).empty());
}

TEST_CASE("Live text box frames bound every primitive write",
          "[text-box-frame]") {
  LiveFrameFixture frame;
  std::vector<std::uint8_t> bytes(kTriangle.begin(), kTriangle.end());
  auto* source = static_cast<std::uint8_t*>(
      VS::Core::Memory::AllocateLiveFromHighEnd(bytes.size()));
  REQUIRE(source != nullptr);
  std::ranges::copy(bytes, source);
  for (std::size_t capacity = 0; capacity < 40; ++capacity) {
    const auto packets = frame.DrawSource(source, {}, 0, capacity);
    std::size_t emitted_words = 0;
    for (const auto& packet : packets) {
      emitted_words += packet.size();
    }
    CHECK(emitted_words <= capacity);
  }
  VS::Core::Memory::FreeLiveFromHighEnd(source);
}

TEST_CASE("Live text box frames consume retained native VM leases",
          "[text-box-frame][event-vm-live]") {
  namespace event = VS::Event;
  FrameVmScope vm;
  LiveFrameFixture frame;
  const auto owner = std::make_shared<const std::vector<std::uint8_t>>(
      kTriangle.begin(), kTriangle.end());
  BindNativeFrame(owner);
  const auto original = frame.DrawSource(nullptr, {}, 9);
  REQUIRE_FALSE(original.empty());
  const auto replacement =
      std::make_shared<const std::vector<std::uint8_t>>(4, 0);
  REQUIRE(event::MountLiveScriptSource(event::ScriptRegion::kEventScript,
                                       replacement));
  CHECK(frame.DrawSource(nullptr, {}, 9) == original);
  BindNativeFrame(replacement);
  CHECK(frame.DrawSource(nullptr, {}, 9).empty());
}
