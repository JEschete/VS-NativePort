#include <array>
#include <cstdint>
#include <limits>
#include <random>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Menu/TextBoxFrames.hpp"

extern "C" {
#include "Event/ScriptVm.h"
#include "Menu/TextBox.h"
#include "Render/Display.h"
#include "Render/Frame.h"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"
}

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

using Triangle = std::array<std::uint8_t, 14>;
const Triangle kTriangle = {3, 0, 3, 1, 0, 0, 255, 0, 0, 255, 0, 1, 2, 255};

class OriginalFrameFixture {
 public:
  OriginalFrameFixture()
      : saved_box_(Menu_textBoxes[0]),
        saved_style_(Event_textBoxFrameStyles[0]),
        saved_table_(Menu_activeTextBoxOrderingTable),
        saved_frame_(Render_frameScratch),
        slot_(Render_frameBuf & 1),
        saved_buffer_(Render_legacyPrimitiveBuffers[slot_]),
        storage_(kPrimitiveWords) {
    Menu_activeTextBoxOrderingTable = ordering_.data();
    Render_legacyPrimitiveBuffers[slot_] = storage_.data();
  }

  ~OriginalFrameFixture() {
    Menu_textBoxes[0] = saved_box_;
    Event_textBoxFrameStyles[0] = saved_style_;
    Menu_activeTextBoxOrderingTable = saved_table_;
    Render_frameScratch = saved_frame_;
    Render_legacyPrimitiveBuffers[slot_] = saved_buffer_;
  }

  std::vector<std::vector<std::uint32_t>> Draw(
      std::vector<std::uint8_t>& bytes, const VS::Menu::FrameRequest& request) {
    Menu_textBoxes[0] = {};
    auto& box = Menu_textBoxes[0];
    box.flags.frameShape = 0;
    box.flags.framePart = static_cast<unsigned>(request.part);
    box.flags.alignment = request.alignment;
    box.frameX = static_cast<std::int16_t>(request.x);
    box.frameY = static_cast<std::int16_t>(request.y);
    box.frameWidth = static_cast<std::int16_t>(request.width);
    box.frameHeight = static_cast<std::int16_t>(request.height);
    box.brightness = request.brightness;
    Event_textBoxFrameStyles[0] = bytes.data();
    ordering_.fill(0xFFFFFF);
    Render_frameScratch.primitiveCursor = storage_.data();
    Menu_DrawTextBoxFrame(0);
    std::vector<std::vector<std::uint32_t>> packets;
    const auto* end =
        static_cast<const std::uint32_t*>(Render_frameScratch.primitiveCursor);
    for (const auto* cursor = storage_.data(); cursor < end;) {
      const auto words = (*cursor >> 24) + 1;
      REQUIRE(words <= static_cast<std::size_t>(end - cursor));
      packets.emplace_back(cursor, cursor + words);
      packets.back()[0] &= 0xFF000000;
      cursor += words;
    }
    return packets;
  }

 private:
  static constexpr std::size_t kPrimitiveWords =
      0x20000 / sizeof(std::uint32_t);
  Menu_TextBoxState saved_box_;
  unsigned char* saved_style_;
  decltype(Menu_activeTextBoxOrderingTable) saved_table_;
  Render_FrameScratch saved_frame_;
  int slot_;
  void* saved_buffer_;
  std::vector<std::uint32_t> storage_;
  std::array<u_long, 4> ordering_{};
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

void CompareFramePackets(OriginalFrameFixture& original,
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

}  // namespace

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
  request.part = 15;
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

TEST_CASE("Text box frame parts cannot borrow vertices from another part",
          "[text-box-frame]") {
  std::vector<std::uint8_t> bytes{6, 2,   4, 0,   0, 1,   1,   1, 2,   3,  1,
                                  1, 0,   0, 255, 0, 255, 255, 0, 255, 0,  32,
                                  0, 224, 0, 4,   1, 255, 2,   5, 3,   255};
  REQUIRE(VS::Menu::DecodeTextBoxFrame(bytes));
  bytes[25] = 5;
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
  OriginalFrameFixture original;
  std::seed_seq seed{315};
  std::mt19937 random(seed);
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
      }
    }
  }
}
