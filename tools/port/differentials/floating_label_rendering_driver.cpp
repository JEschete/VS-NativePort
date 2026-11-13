#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "Battle/FloatingLabelRendering.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Render/GameFont.hpp"
#include "Render/UiPackets.hpp"
#include "Render/UiTexturedQuads.hpp"

namespace {
namespace b = VS::Battle;
namespace r = VS::Render;
constexpr std::uint32_t kModuleBase = 0x80068800;
constexpr std::uint32_t kEnd = 0xFFFFFF;
constexpr std::uint32_t kHeadLengthMask = 0xFF000000;
constexpr std::uint32_t kSeedStride = 0x9E3779B9;
constexpr std::size_t kLabelWords = 6;
constexpr std::size_t kWordBytes = 4;
constexpr std::uint32_t kByteMask = 0xFF;
constexpr std::uint32_t kHalfMask = 0xFFFF;
constexpr int kByteBits = 8;
constexpr int kHalfBits = 16;
constexpr int kTopByte = 24;
constexpr std::uint32_t kCaptionMap = 0x800E861C;
constexpr std::uint32_t kCaptionAtlas = 0x80068FAC;
constexpr std::uint32_t kHalfHeights = 0x800E8634;
constexpr std::uint32_t kLockedPieces = 0x800E8640;
constexpr std::uint32_t kFontWidths = 0x800EB7B4;
constexpr std::uint32_t kFontTrim = 0x800EB708;
constexpr std::uint32_t kFontIndices = 0x800EB7D4;
constexpr std::size_t kWidthBytes = 32;
constexpr std::size_t kTrimBytes = 172;
constexpr std::size_t kIndexBytes = 60;
constexpr std::size_t kRectBytes = 8;
constexpr std::uint32_t kFirstCaption = 3;
constexpr std::size_t kCaptionKinds = 6;
constexpr std::size_t kCaptionRects = 33;
constexpr std::size_t kHeightSteps = 5;
constexpr std::size_t kPieceCount = 4;

class Image {
 public:
  explicit Image(const char* path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) std::exit(2);
    bytes_.assign(std::istreambuf_iterator<char>(stream),
                  std::istreambuf_iterator<char>());
  }
  [[nodiscard]] std::span<const std::uint8_t> Bytes(std::uint32_t address,
                                                    std::size_t size) const {
    const auto offset = static_cast<std::size_t>(address - kModuleBase);
    if (address < kModuleBase || offset > bytes_.size() ||
        size > bytes_.size() - offset) {
      std::exit(3);
    }
    return std::span(bytes_).subspan(offset, size);
  }
  [[nodiscard]] std::uint32_t Word(std::uint32_t address) const {
    std::uint32_t value = 0;
    const auto bytes = Bytes(address, kWordBytes);
    for (std::size_t index = 0; index < bytes.size(); ++index) {
      value |= static_cast<std::uint32_t>(bytes[index]) << (index * kByteBits);
    }
    return value;
  }
  [[nodiscard]] std::int16_t Half(std::uint32_t address) const {
    const auto bytes = Bytes(address, 2);
    return static_cast<std::int16_t>(
        bytes.front() |
        (static_cast<std::uint32_t>(bytes.back()) << kByteBits));
  }
  [[nodiscard]] b::FloatingLabelAtlasRect Rect(std::uint32_t address) const {
    return {Half(address), Half(address + 2), Half(address + 4),
            Half(address + 6)};
  }

 private:
  std::vector<std::uint8_t> bytes_;
};

class Queue final : public r::PacketQueue {
 public:
  Queue() {
    constexpr std::array<std::uint32_t, 4> kPrefix{0x03FFFFFF, 0x60ABCDEF,
                                                   0x000B000A, 0x00020001};
    packets_.emplace_back(kPrefix.begin(), kPrefix.end());
  }
  [[nodiscard]] std::uint32_t ChainLink() const override { return head_; }
  bool Emit(std::span<const std::uint32_t> packet) override {
    packets_.emplace_back(packet.begin(), packet.end());
    head_ = static_cast<std::uint32_t>(packets_.size());
    words_ += packet.size();
    return true;
  }
  [[nodiscard]] std::size_t Words() const { return words_; }
  [[nodiscard]] std::vector<std::uint32_t> Traversal() const {
    std::vector<std::uint32_t> words;
    auto handle = head_;
    for (std::size_t steps = 0; handle != kEnd; ++steps) {
      if (handle == 0 || handle > packets_.size() || steps > packets_.size()) {
        std::exit(4);
      }
      const auto& packet = packets_[handle - 1];
      words.push_back(packet.front() & kHeadLengthMask);
      words.insert(words.end(), packet.begin() + 1, packet.end());
      handle = packet.front() & r::kPacketLinkMask;
    }
    return words;
  }

 private:
  std::vector<std::vector<std::uint32_t>> packets_;
  std::uint32_t head_ = 1;
  std::size_t words_ = 0;
};

class World final : public b::FloatingLabelRenderWorld {
 public:
  World(const Image& image, std::uint32_t seed) : image_(image), seed_(seed) {}
  Queue quads;
  Queue text;
  std::optional<b::FloatingLabelAtlasRect> ValueCaption(
      std::uint8_t raw_mode) override {
    const auto index = static_cast<std::uint32_t>(raw_mode) - kFirstCaption;
    if (index >= kCaptionKinds) return std::nullopt;
    const auto rectangle = image_.Word(kCaptionMap + index * kWordBytes);
    if (rectangle >= kCaptionRects) return std::nullopt;
    return image_.Rect(kCaptionAtlas + rectangle * kRectBytes);
  }
  std::optional<std::int16_t> LockedHalfHeight(std::int32_t step) override {
    if (step < 0 || static_cast<std::size_t>(step) >= kHeightSteps) {
      return std::nullopt;
    }
    return image_.Half(kHalfHeights + static_cast<std::uint32_t>(step) * 2);
  }
  std::optional<b::FloatingLabelAtlasRect> LockedPiece(
      std::size_t piece) override {
    if (piece >= kPieceCount) return std::nullopt;
    return image_.Rect(kLockedPieces +
                       static_cast<std::uint32_t>(piece * kRectBytes));
  }
  bool DrawQuad(const b::FloatingLabelQuad& quad) override {
    r::UiTexturedQuadPacket storage{};
    for (std::size_t index = 0; index < storage.size(); ++index) {
      storage[index] =
          seed_ ^
          (static_cast<std::uint32_t>(quads.Words() + index) * kSeedStride);
    }
    return quads.Emit(
        r::BuildUiTexturedQuadPacket(quad, storage, quads.ChainLink()));
  }
  bool DrawText(std::string_view value, b::FloatingLabelScreenPoint origin,
                std::uint32_t color) override {
    const r::CompactFontTables font{image_.Bytes(kFontWidths, kWidthBytes),
                                    image_.Bytes(kFontTrim, kTrimBytes),
                                    image_.Bytes(kFontIndices, kIndexBytes)};
    const auto position = static_cast<std::int32_t>(
        (static_cast<std::uint32_t>(origin.x) & kHalfMask) |
        (static_cast<std::uint32_t>(origin.y) << kHalfBits));
    return r::DrawCompactText(text, font, value, position, color);
  }

 private:
  const Image& image_;
  std::uint32_t seed_;
};

void Print(std::span<const std::uint32_t> values) {
  std::cout << ' ' << values.size();
  for (const auto value : values) std::cout << ' ' << value;
}

void Evaluate(const Image& image, std::span<const std::int64_t> inputs) {
  constexpr std::size_t kInputState = 4;
  constexpr std::size_t kInputSeed = kInputState + kLabelWords;
  const auto x = static_cast<std::int32_t>(inputs[1]);
  const auto y = static_cast<std::int32_t>(inputs[2]);
  const auto third = static_cast<std::uint32_t>(inputs[kInputState + 2]);
  const auto fourth = static_cast<std::uint32_t>(inputs[kInputState + 3]);
  const b::FloatingLabelAppearance label{
      static_cast<std::uint8_t>(third >> kTopByte),
      static_cast<std::uint16_t>(fourth),
      static_cast<std::uint8_t>((fourth >> kHalfBits) & kByteMask),
      static_cast<std::uint8_t>(fourth >> kTopByte)};
  World world(image, static_cast<std::uint32_t>(inputs[kInputSeed]));
  bool success = false;
  std::int32_t width = 0;
  switch (inputs[0]) {
    case 0: {
      const auto result = b::DrawValueLabelCaption(world, {x, y}, label);
      success = result.has_value();
      if (result) width = result.value();
      break;
    }
    case 1:
      success = b::DrawLockedFloatingLabel(world, {x, y}, label);
      break;
    case 2:
      success = b::DrawStatusRemovalMarker(
          world, {x, y}, static_cast<std::int32_t>(inputs[3]));
      break;
    case 3:
      success = b::DrawAbilityTimingResult(world, {x, y}, label);
      break;
    case 4:
      success = b::DrawChainCount(world, {x, y}, label);
      break;
    case 5:
      success = b::DrawAbilityTimingCue(
          world, {VS::Core::Math::WrapSigned16(static_cast<std::uint32_t>(x)),
                  VS::Core::Math::WrapSigned16(static_cast<std::uint32_t>(y))});
      break;
    default:
      std::exit(5);
  }
  std::cout << static_cast<int>(success) << ' ' << width << ' '
            << world.quads.Words() + world.text.Words();
  for (std::size_t index = 0; index < kLabelWords; ++index) {
    std::cout << ' ' << static_cast<std::uint32_t>(inputs[kInputState + index]);
  }
  Print(world.quads.Traversal());
  Print(world.text.Traversal());
  std::cout << '\n';
}
}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) return 1;
  const Image image(argv[1]);
  constexpr std::size_t kInputs = 11;
  std::array<std::int64_t, kInputs> inputs{};
  while (std::cin >> inputs[0]) {
    for (std::size_t index = 1; index < inputs.size(); ++index) {
      if (!(std::cin >> inputs[index])) return 6;
    }
    Evaluate(image, inputs);
  }
}
