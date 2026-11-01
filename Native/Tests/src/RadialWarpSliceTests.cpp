#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Effects/ScreenDistortion/RadialWarp.hpp"

namespace {
namespace warp = VS::Effects::ScreenDistortion;

constexpr std::uint32_t kTriangleCommand = 0x20010203;
constexpr std::uint32_t kModeCommand = 0xE1000640;
constexpr std::uint32_t kLineCommand = 0x40010203;
constexpr std::uint16_t kHalfScale = 2048;
constexpr std::uint16_t kIdentityScale = 4096;
constexpr int kCoordinateBits = 16;

std::uint32_t Point(int x, int y) {
  return static_cast<std::uint16_t>(x) |
         (static_cast<std::uint32_t>(static_cast<std::uint16_t>(y))
          << kCoordinateBits);
}

// A root table whose radius lookup is the true square root, so a half scale
// pulls every vertex halfway toward the screen centre.
std::vector<std::uint8_t> RootTable() {
  constexpr std::size_t kRootOffset = 516;
  constexpr unsigned kRootEntries = 192;
  constexpr unsigned kRootBias = 64;
  constexpr unsigned kRootScale = 262144;
  std::vector<std::uint8_t> table(warp::kRadialWarpRootTableBytes);
  for (std::size_t index = 0; index < kRootEntries; ++index) {
    std::size_t root = 0;
    while ((root + 1) * (root + 1) <= (index + kRootBias) * kRootScale) ++root;
    const std::size_t entry = kRootOffset + (index * 2);
    table[entry] = static_cast<std::uint8_t>(root);
    table[entry + 1] = static_cast<std::uint8_t>(root >> 8);
  }
  return table;
}

class FakePackets final : public warp::PrimitivePacketCursor {
 public:
  explicit FakePackets(std::vector<std::vector<std::uint32_t>> packets)
      : packets_(std::move(packets)) {}

  bool Advance() override { return ++current_ < packets_.size(); }
  std::size_t WordCount() const override { return packets_[current_].size(); }
  std::uint32_t Word(std::size_t index) const override {
    return packets_[current_][index];
  }
  void SetWord(std::size_t index, std::uint32_t value) override {
    packets_[current_][index] = value;
    ++writes_;
  }

  [[nodiscard]] const std::vector<std::uint32_t>& Packet(
      std::size_t index) const {
    return packets_[index];
  }
  [[nodiscard]] unsigned Writes() const { return writes_; }

 private:
  std::vector<std::vector<std::uint32_t>> packets_;
  std::size_t current_ = static_cast<std::size_t>(-1);
  unsigned writes_ = 0;
};

std::vector<std::uint32_t> Triangle() {
  return {kTriangleCommand, Point(32, 64), Point(288, 64), Point(160, 192)};
}
}  // namespace

TEST_CASE("Typed radial warp scales triangle vertices toward the centre",
          "[effects][screen-warp][typed]") {
  const auto table = RootTable();
  FakePackets packets({Triangle()});
  warp::WarpPolygonVertices({.scale = kHalfScale, .growth = 0}, table, packets);
  const auto& triangle = packets.Packet(0);
  CHECK(triangle[0] == kTriangleCommand);
  CHECK(triangle[1] == Point(96, 88));
  CHECK(triangle[2] == Point(224, 88));
  CHECK(triangle[3] == Point(160, 152));
  CHECK(packets.Writes() == 3);
}

TEST_CASE("Typed radial warp skips mode prefixes and visits every packet",
          "[effects][screen-warp][typed]") {
  const auto table = RootTable();
  auto prefixed = Triangle();
  prefixed.insert(prefixed.begin(), kModeCommand);
  FakePackets packets({{}, {kModeCommand}, prefixed, Triangle()});
  warp::WarpPolygonVertices({.scale = kHalfScale, .growth = 0}, table, packets);
  CHECK(packets.Packet(0).empty());
  CHECK(packets.Packet(1) == std::vector<std::uint32_t>{kModeCommand});
  CHECK(packets.Packet(2)[0] == kModeCommand);
  CHECK(packets.Packet(2)[2] == Point(96, 88));
  CHECK(packets.Packet(3)[1] == Point(96, 88));
  CHECK(packets.Writes() == 6);
}

TEST_CASE("Typed radial warp leaves non-polygon draw commands untouched",
          "[effects][screen-warp][typed]") {
  const auto table = RootTable();
  const std::vector<std::uint32_t> line{kLineCommand, Point(0, 0),
                                        Point(320, 240)};
  FakePackets packets({line});
  warp::WarpPolygonVertices({.scale = kHalfScale, .growth = 0}, table, packets);
  CHECK(packets.Packet(0) == line);
  CHECK(packets.Writes() == 0);
}

TEST_CASE("Typed radial warp at identity scale rewrites the same coordinates",
          "[effects][screen-warp][typed]") {
  const auto table = RootTable();
  FakePackets packets({Triangle()});
  warp::WarpPolygonVertices({.scale = kIdentityScale, .growth = 0}, table,
                            packets);
  CHECK(packets.Packet(0) == Triangle());
  CHECK(packets.Writes() == 3);
}
