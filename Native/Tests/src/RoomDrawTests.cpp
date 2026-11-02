#include <algorithm>
#include <array>
#include <bit>
#include <climits>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Content/ModuleData.h"
#include "Core/Math/ScalarMath.hpp"
#include "Legacy/Psx.h"
#include "Render/RoomDraw.hpp"
#include "Render/RoomDrawLive.hpp"

extern "C" {
#include "Core/Gte.h"
#include "Field/Room.h"
#include "Render/Display.h"
#include "Render/Frame.h"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"
}

namespace {
namespace Render = VS::Render;
namespace Math = VS::Core::Math;

constexpr std::size_t kWordBytes = sizeof(std::uint32_t);
constexpr auto kTriangleBytes = Render::kRoomTrianglePacketWords * kWordBytes;
constexpr auto kQuadBytes = Render::kRoomQuadPacketWords * kWordBytes;

Render::RoomView IdentityView(std::int32_t depth) {
  Render::RoomView view;
  for (std::size_t axis = 0; axis < Render::kRoomAxes; ++axis) {
    view.rotation.m[axis][axis] = Render::kRoomUnitScale;
  }
  view.translation.z = depth;
  return view;
}

// A geometry prefix: the stored group count and `count` zeroed records.
std::vector<std::byte> GroupRecords(std::uint32_t count) {
  std::vector<std::byte> bytes(sizeof count +
                               count * Render::kRoomGroupRecordBytes);
  std::memcpy(bytes.data(), &count, sizeof count);
  return bytes;
}

// Reads stored record `index`, failing the test when it is out of range.
Render::RoomGroup GroupAt(std::span<const std::byte> bytes, std::size_t index) {
  const auto group = Render::ReadRoomGroup(bytes, index);
  REQUIRE(group.has_value());
  return group.value_or(Render::RoomGroup{});
}

// Group 1 of a two-group prefix with every field distinct and signed.
Render::RoomGroup SampleGroup() {
  Render::RoomGroup group;
  group.flags = 0x80001400;
  group.bounds = {{{-1, 2}, {-3, 4}, {-5, 6}}};
  group.angles = {7, -8, 9};
  group.polygon_offset = 0x1234;
  for (std::size_t row = 0; row < 3; ++row) {
    for (std::size_t column = 0; column < 3; ++column) {
      group.view.rotation.m[row][column] =
          static_cast<std::int16_t>(row * 3 + column - 4);
    }
  }
  group.view.translation = {-100, 200, -300};
  return group;
}

// A camera 30 units back at (10, 20) rolled a quarter turn.
Render::RoomCamera RolledCamera() {
  Render::RoomCamera camera;
  camera.view = IdentityView(30);
  camera.view.translation.x = 10;
  camera.view.translation.y = 20;
  camera.roll = 1024;
  return camera;
}

// The GTE with a 256 projection distance centred on (160, 120), starting
// from an identity view 1024 units in front of the camera.
class GteTransformer final : public Render::RoomTransformer {
 public:
  GteTransformer() {
    Core_Gte_Reset();
    Core_Gte_SetGeomScreen(256);
    Core_Gte_SetGeomOffset(160, 120);
    LoadView(IdentityView(1024));
  }
  ~GteTransformer() override { Core_Gte_Reset(); }
  GteTransformer(const GteTransformer&) = delete;
  GteTransformer& operator=(const GteTransformer&) = delete;
  GteTransformer(GteTransformer&&) = delete;
  GteTransformer& operator=(GteTransformer&&) = delete;

  void LoadView(const Render::RoomView& view) override {
    MATRIX matrix{};
    Math::StoreMatrix(matrix, view.rotation);
    matrix.t[0] = view.translation.x;
    matrix.t[1] = view.translation.y;
    matrix.t[2] = view.translation.z;
    Core_Gte_SetRotMatrix(&matrix);
    Core_Gte_SetTransMatrix(&matrix);
  }
  Render::RoomProjectedPoint Project(
      const Render::RoomPosition& position) override {
    SVECTOR input{position.x, position.y, position.z, 0};
    Core_GteWord screen = 0;
    Core_GteWord fog = 0;
    Core_GteWord flags = 0;
    const Core_GteWord depth =
        Core_Gte_RotTransPers(&input, &screen, &fog, &flags);
    return {static_cast<std::int32_t>(screen),
            static_cast<std::int32_t>(depth)};
  }
  Math::Vec3 Transform(const Render::RoomPosition& position) override {
    SVECTOR input{position.x, position.y, position.z, 0};
    VECTOR output{};
    Core_GteWord flags = 0;
    Core_Gte_RotTrans(&input, &output, &flags);
    return Math::VectorOf(output);
  }
  [[nodiscard]] Render::RoomScreenOffset ScreenOffset() const override {
    Core_GteWord x = 0;
    Core_GteWord y = 0;
    Core_Gte_GetGeomOffset(&x, &y);
    return {static_cast<std::int32_t>(x), static_cast<std::int32_t>(y)};
  }
  void SetScreenOffset(const Render::RoomScreenOffset& offset) override {
    Core_Gte_SetGeomOffset(offset.x, offset.y);
  }
};

struct RecordedPacket {
  std::vector<std::uint32_t> words;
  std::uint32_t depth = 0;
};

// Records every accepted packet; refuses one that would exceed the capacity.
class RecordingQueue final : public Render::RoomPacketQueue {
 public:
  explicit RecordingQueue(std::size_t capacity_bytes = 0x20000)
      : capacity_(capacity_bytes) {}

  bool Emit(std::span<const std::uint32_t> packet,
            std::uint32_t depth) override {
    if (packet.size_bytes() > capacity_ - used_) return false;
    used_ += packet.size_bytes();
    packets.push_back({{packet.begin(), packet.end()}, depth});
    return true;
  }

  [[nodiscard]] std::size_t Used() const { return used_; }

  std::vector<RecordedPacket> packets;

 private:
  std::size_t capacity_;
  std::size_t used_ = 0;
};

std::int16_t ScreenX(std::uint32_t screen) {
  return static_cast<std::int16_t>(screen);
}
std::int16_t ScreenY(std::uint32_t screen) {
  return static_cast<std::int16_t>(screen >> 16);
}

// --- Packed coordinates --------------------------------------------------

using PackedCoordinates = std::array<std::byte, Render::kRoomCoordinateBytes>;

PackedCoordinates Coordinates() {
  PackedCoordinates data{};
  constexpr std::array<std::int16_t, 3> kBase{32760, -32760, -123};
  std::memcpy(data.data(), kBase.data(), sizeof kBase);
  return data;
}

Render::RoomPosition ExpectedPosition(const PackedCoordinates& data,
                                      std::uint32_t shift, std::size_t vertex) {
  std::array<std::int16_t, 3> expected{};
  const auto scale = std::int64_t{1} << (shift % 32);
  for (std::size_t axis = 0; axis < expected.size(); ++axis) {
    const auto word = static_cast<std::uint16_t>(
        std::to_integer<unsigned>(data[axis * 2]) |
        (std::to_integer<unsigned>(data[axis * 2 + 1]) << 8));
    const auto base = std::bit_cast<std::int16_t>(word);
    int delta = 0;
    if (vertex != 0) {
      const int value = std::to_integer<int>(data[6 + (vertex - 1) * 3 + axis]);
      delta = value < 128 ? value : value - 256;
    }
    const auto result = static_cast<std::uint16_t>(base + delta * scale);
    expected[axis] = std::bit_cast<std::int16_t>(result);
  }
  return {expected[0], expected[1], expected[2]};
}

void CheckCoordinates(const PackedCoordinates& data, std::uint32_t shift) {
  const auto positions = Render::DecodeRoomPolygonVertices(data, shift);
  for (std::size_t vertex = 0; vertex < positions.size(); ++vertex) {
    CAPTURE(shift, vertex);
    CHECK(positions[vertex] == ExpectedPosition(data, shift, vertex));
  }
}

PackedCoordinates MixedDeltas() {
  auto data = Coordinates();
  constexpr std::array<unsigned char, 6> kDeltas{127, 128, 1, 255, 3, 128};
  for (std::size_t index = 0; index < kDeltas.size(); ++index) {
    data[6 + index] = std::byte{kDeltas[index]};
  }
  return data;
}

// --- Subdivision ---------------------------------------------------------

std::array<Render::RoomVertex, 4> SquareVertices() {
  return {{{{-128, -128, 0}, 0x102030, 0x2010},
           {{128, -128, 0}, 0x708090, 0x4030},
           {{-128, 128, 0}, 0xD0E0F0, 0x6050},
           {{128, 128, 0}, 0x305070, 0x8070}}};
}

constexpr Render::RoomPolygonMaterial kMaterial{0, 0x3D, 0x1234, 0x15};

Render::RoomPolygonResult Subdivide(bool quad, int levels,
                                    const Render::RoomPolygonMaterial& material,
                                    const Render::RoomClipState& clip,
                                    Render::RoomTransformer& transformer,
                                    Render::RoomPacketQueue& queue) {
  const auto vertices = SquareVertices();
  if (quad) {
    return Render::SubdivideRoomQuad(std::span(vertices), levels, material,
                                     clip, transformer, queue);
  }
  return Render::SubdivideRoomTriangle(
      std::span(vertices).first<Render::kRoomTriangleCorners>(), levels,
      material, clip, transformer, queue);
}

void CheckPacketMaterial(const RecordedPacket& packet) {
  const bool quad = packet.words.size() == Render::kRoomQuadPacketWords;
  REQUIRE((quad || packet.words.size() == Render::kRoomTrianglePacketWords));
  CHECK(packet.words[0] == 0);
  CHECK((packet.words[1] >> 24) == (quad ? 0x3Cu : 0x34u));
  CHECK((packet.words[3] >> 16) == 0x1234);
  CHECK((packet.words[6] >> 16) == 0x15);
}

std::array<int, 2> CountPackets(const RecordingQueue& queue) {
  int triangles = 0;
  int quads = 0;
  for (const auto& packet : queue.packets) {
    CheckPacketMaterial(packet);
    packet.words.size() == Render::kRoomQuadPacketWords ? ++quads : ++triangles;
  }
  return {triangles, quads};
}

void CheckLevel(bool quad, int levels) {
  CAPTURE(quad, levels);
  constexpr std::array<int, 4> kTriangles{1, 7, 31, 127};
  constexpr std::array<int, 4> kQuadLeaves{1, 4, 16, 64};
  constexpr std::array<int, 4> kEdgeTriangles{0, 4, 20, 84};
  const auto level = static_cast<std::size_t>(levels);
  const int triangles = quad ? kEdgeTriangles[level] : kTriangles[level];
  const int quads = quad ? kQuadLeaves[level] : 0;
  GteTransformer transformer;
  RecordingQueue queue;
  const auto result =
      Subdivide(quad, levels, kMaterial, {}, transformer, queue);
  CHECK(result.status == Render::RoomDrawStatus::kDrawn);
  CHECK(result.emitted == static_cast<std::uint32_t>(triangles + quads));
  CHECK(queue.Used() == triangles * kTriangleBytes + quads * kQuadBytes);
  const std::array expected{triangles, quads};
  CHECK(CountPackets(queue) == expected);
}

// One triangle record and one quad record of the square, deltas stored at
// a coordinate shift of three.
std::vector<std::byte> Batch() {
  const auto vertices = SquareVertices();
  std::vector<std::byte> bytes(8 + Render::kRoomTriangleRecordBytes +
                               Render::kRoomQuadRecordBytes);
  constexpr std::array<std::uint32_t, 2> kCounts{1, 1};
  std::memcpy(bytes.data(), kCounts.data(), sizeof kCounts);
  constexpr std::array<std::size_t, 4> kColors{12, 16, 20, 36};
  constexpr std::array<std::size_t, 4> kU{24, 28, 19, 35};
  constexpr std::array<std::size_t, 4> kV{25, 29, 23, 39};
  for (bool quad : {false, true}) {
    auto* record = bytes.data() + (quad ? 40 : 8);
    std::memcpy(record, &vertices[0].position, sizeof vertices[0].position);
    for (std::size_t corner = 0; corner < (quad ? 4u : 3u); ++corner) {
      const auto& vertex = vertices[corner];
      if (corner != 0) {
        const std::size_t offset = corner == 3 ? 32 : 6 + (corner - 1) * 3;
        record[offset] = static_cast<std::byte>(
            (vertex.position.x - vertices[0].position.x) / 8);
        record[offset + 1] = static_cast<std::byte>(
            (vertex.position.y - vertices[0].position.y) / 8);
        record[offset + 2] = static_cast<std::byte>(
            (vertex.position.z - vertices[0].position.z) / 8);
      }
      std::memcpy(record + kColors[corner], &vertex.color, 3);
      record[kU[corner]] = static_cast<std::byte>(vertex.uv);
      record[kV[corner]] = static_cast<std::byte>(vertex.uv >> 8);
    }
    record[15] = std::byte{static_cast<unsigned char>(quad ? 0x3D : 0x35)};
    const auto clut = static_cast<std::uint16_t>(kMaterial.clut);
    const auto page = static_cast<std::uint16_t>(kMaterial.page);
    std::memcpy(record + 26, &clut, sizeof clut);
    std::memcpy(record + 30, &page, sizeof page);
  }
  return bytes;
}

void CheckBatchTriangle(const RecordingQueue& queue) {
  REQUIRE_FALSE(queue.packets.empty());
  const auto& triangle = queue.packets.front().words;
  REQUIRE(triangle.size() == Render::kRoomTrianglePacketWords);
  CHECK(triangle[2] == (128u | (88u << 16)));
  CHECK(triangle[5] == (192u | (88u << 16)));
  CHECK(triangle[8] == (128u | (152u << 16)));
  CHECK(triangle[3] == 0x12342010);
  CHECK(triangle[6] == 0x00154030);
}

// A one-group geometry section: the count, one group record and the batch.
std::vector<std::byte> Geometry(std::uint32_t flags) {
  const auto batch = Batch();
  std::vector<std::byte> bytes(sizeof(std::uint32_t) +
                               Render::kRoomGroupRecordBytes + batch.size());
  const std::uint32_t count = 1;
  std::memcpy(bytes.data(), &count, sizeof count);
  Render::RoomGroup group;
  group.flags = flags;
  group.polygon_offset = sizeof(std::uint32_t) + Render::kRoomGroupRecordBytes;
  group.view = IdentityView(1024);
  REQUIRE(Render::WriteRoomGroup(bytes, 0, group));
  std::ranges::copy(batch, bytes.begin() + group.polygon_offset);
  return bytes;
}

// --- Screen visibility and midpoints -------------------------------------

struct ScreenPoint {
  std::int16_t x;
  std::int16_t y;
};

bool ReferenceVisibility(const std::array<ScreenPoint, 4>& points,
                         const Render::RoomScreenBounds& clip) {
  const auto all = [&](auto predicate) {
    return std::ranges::all_of(points, predicate);
  };
  return !all([&](ScreenPoint p) { return p.y >= clip.bottom; }) &&
         !all([&](ScreenPoint p) { return p.y < clip.top; }) &&
         !all([&](ScreenPoint p) { return p.x < clip.left; }) &&
         !all([&](ScreenPoint p) { return p.x >= clip.right; });
}

void CheckVisibility(const std::array<ScreenPoint, 4>& points,
                     const Render::RoomScreenBounds& clip) {
  Render::RoomScreenBounds bounds{INT_MAX, INT_MAX, INT_MIN, INT_MIN};
  for (const auto point : points) {
    bounds.left = std::min<std::int32_t>(bounds.left, point.x);
    bounds.right = std::max<std::int32_t>(bounds.right, point.x);
    bounds.top = std::min<std::int32_t>(bounds.top, point.y);
    bounds.bottom = std::max<std::int32_t>(bounds.bottom, point.y);
  }
  CHECK(Render::IsRoomPolygonOnScreen(bounds, clip) ==
        ReferenceVisibility(points, clip));
}

std::uint32_t NextSample(std::uint32_t& state) {
  state = state * 1664525u + 1013904223u;
  return state;
}

std::int16_t Halfword(std::uint32_t bits) {
  return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(bits));
}

void CheckScreenSample(std::uint32_t& state) {
  std::array<std::int32_t, 4> wide{};
  std::array<std::int32_t, 4> wrapped{};
  for (std::size_t index = 0; index < wide.size(); ++index) {
    const auto bits = NextSample(state);
    wide[index] = std::bit_cast<std::int32_t>(bits);
    wrapped[index] = Halfword(bits);
  }
  Render::RoomClipState clip;
  Render::SetRoomPolygonScreenBounds(clip,
                                     {wide[0], wide[1], wide[2], wide[3]});
  const Render::RoomScreenBounds expected_clip{wrapped[0], wrapped[1],
                                               wrapped[2], wrapped[3]};
  CHECK(clip.screen == expected_clip);
  std::array<ScreenPoint, 4> points{};
  for (auto& point : points) {
    point.x = Halfword(NextSample(state) >> 16);
    point.y = Halfword(NextSample(state) >> 16);
  }
  CheckVisibility(points, clip.screen);
}

Render::RoomVertex ReferenceMidpoint(const Render::RoomVertex& first,
                                     const Render::RoomVertex& second) {
  const auto floor_half = [](int a, int b) {
    return static_cast<std::int16_t>(std::floor((a + b) / 2.0));
  };
  Render::RoomVertex result;
  result.position = {floor_half(first.position.x, second.position.x),
                     floor_half(first.position.y, second.position.y),
                     floor_half(first.position.z, second.position.z)};
  for (unsigned shift = 0; shift < 24; shift += 8) {
    result.color |=
        (((first.color >> shift & 255) + (second.color >> shift & 255)) / 2)
        << shift;
  }
  result.uv = static_cast<std::uint16_t>(
      ((first.uv & 255) + (second.uv & 255)) / 2 +
      (((first.uv >> 8) + (second.uv >> 8)) / 2) * 256);
  return result;
}

void CheckMidpointSample(std::uint32_t bits) {
  Render::RoomVertex first;
  Render::RoomVertex second;
  first.position.x = Halfword(bits);
  first.position.y = Halfword(bits ^ 0x8000u);
  first.position.z = first.position.x;
  second.position.x = INT16_MAX;
  second.position.y = INT16_MIN;
  second.position.z = Halfword(bits * 7 + 13);
  const auto low = bits & 255;
  const auto high = bits >> 8;
  first.color = 0x19000000u | low | ((255 - low) << 8) | (low << 16);
  second.color = 0xE2000000u | high | (high << 8) | ((255 - high) << 16);
  first.uv = static_cast<std::uint16_t>(high | ((255 - high) << 8));
  second.uv = static_cast<std::uint16_t>(low | (low << 8));
  CHECK(Render::InterpolateRoomVertexMidpoint(first, second) ==
        ReferenceMidpoint(first, second));
  CHECK(Render::InterpolateRoomVertexMidpoint(first, first).position ==
        first.position);
}

// --- Tint ----------------------------------------------------------------

std::uint32_t ReferenceTint(std::uint32_t color, std::int16_t strength,
                            std::int16_t red, std::int16_t green,
                            std::int16_t blue) {
  const std::array offsets{red, green, blue};
  const double luminance = (5.0 * (color & 255) + 9.0 * ((color >> 8) & 255) +
                            2.0 * ((color >> 16) & 255)) /
                           16.0;
  const double coefficient = strength / 4096.0;
  std::uint32_t result = color & 0xFF000000u;
  for (std::size_t channel = 0; channel < offsets.size(); ++channel) {
    const auto shift = channel * 8;
    const double input = (color >> shift) & 255;
    const double faded = std::clamp(
        std::floor(input + (luminance - input) * coefficient), 0.0, 255.0);
    result |= static_cast<std::uint32_t>(
                  std::clamp(faded + offsets[channel], 0.0, 255.0))
              << shift;
  }
  return result;
}

std::uint32_t Tint(std::uint32_t color, std::int16_t strength, std::int16_t red,
                   std::int16_t green, std::int16_t blue) {
  return Render::TintRoomPrimitiveColor(color, {strength, red, green, blue});
}

// A GT3 then a GT4 room packet starting at word 1, each followed by the two
// padding words flag 2 describes.
struct TintPackets {
  std::array<std::uint32_t, 40> words{};
  std::size_t extra;
  std::size_t quad;
  std::size_t end;

  explicit TintPackets(std::uint32_t flags)
      : extra((flags & 2) != 0 ? 2 : 0),
        quad(11 + extra),
        end(quad + 13 + extra) {
    words.fill(0xB6B6B6B6);
    words[1] = 0x09000012;
    words[quad] = 0x0C000034;
    for (std::uint32_t vertex = 0; vertex < 3; ++vertex) {
      words[2 + std::size_t{vertex} * 3] = 0x34102030u + (vertex << 24);
    }
    for (std::uint32_t vertex = 0; vertex < 4; ++vertex) {
      words[quad + 1 + std::size_t{vertex} * 3] = 0x3C102030u + (vertex << 24);
    }
  }

  [[nodiscard]] std::span<std::uint32_t> Range() {
    return std::span(words).subspan(1, end - 1);
  }

  [[nodiscard]] auto Expected(bool include_quad = true) const {
    auto expected = words;
    for (std::uint32_t vertex = 0; vertex < 3; ++vertex) {
      expected[2 + std::size_t{vertex} * 3] = 0x3418242Cu + (vertex << 24);
    }
    if (include_quad) {
      for (std::uint32_t vertex = 0; vertex < 4; ++vertex) {
        expected[quad + 1 + std::size_t{vertex} * 3] =
            0x3C18242Cu + (vertex << 24);
      }
    }
    return expected;
  }
};

constexpr Render::RoomTint kTint{-1, 1, 2, 0};

// --- Sky -----------------------------------------------------------------

struct SkyVertex {
  SVECTOR position{};
  std::uint32_t color = 0;
};
struct SkyDome {
  std::array<SkyVertex, Render::kSkyDomeVertexCount> vertices{};
  std::array<std::array<std::uint8_t, 4>, Render::kSkyDomeFaceCount> faces{};
};
static_assert(sizeof(SkyDome) == Render::kSkyDomeBytes);

SkyDome OneFaceSky() {
  SkyDome sky;
  for (auto& vertex : sky.vertices) vertex.position.vz = -100;
  sky.vertices[0] = {{-20, -20, 100, 0}, 0x0000FF};
  sky.vertices[1] = {{20, -20, 100, 0}, 0x00FF00};
  sky.vertices[2] = {{0, 20, 100, 0}, 0xFF0000};
  for (auto& face : sky.faces) face = {3, 3, 3, 0};
  sky.faces[0] = {0, 1, 2, 0};
  return sky;
}

// --- Live frame ----------------------------------------------------------

// A frame buffer and ordering table the Live binding draws into.
struct LiveFrame {
  Render_FrameScratch saved_frame = Render_frameScratch;
  int saved_framebuffer = Render_frameBuf;
  void* saved_buffer = Render_legacyPrimitiveBuffers[0];
  Render::RoomClipState saved_clip = Render::LiveRoomClipState();
  std::vector<std::uint32_t> memory = std::vector<std::uint32_t>(
      0x20000 / sizeof(std::uint32_t) + 2, 0xA5A5A5A5);
  std::array<u_long, 2048> ordering{};

  LiveFrame() {
    Render_frameScratch = {};
    Render_frameBuf = 0;
    Render_legacyPrimitiveBuffers[0] = Buffer();
    Render_frameScratch.primitiveCursor = Buffer();
    Render_frameScratch.orderingTable = ordering.data();
    ClearOTagR(ordering.data(), static_cast<int>(ordering.size()));
    Render::LiveRoomClipState() = {};
    Core_Gte_Reset();
    Core_Gte_SetGeomScreen(256);
    Core_Gte_SetGeomOffset(160, 120);
  }
  ~LiveFrame() {
    Render_frameScratch = saved_frame;
    Render_frameBuf = saved_framebuffer;
    Render_legacyPrimitiveBuffers[0] = saved_buffer;
    Render::LiveRoomClipState() = saved_clip;
    Core_Gte_Reset();
  }
  LiveFrame(const LiveFrame&) = delete;
  LiveFrame& operator=(const LiveFrame&) = delete;
  LiveFrame(LiveFrame&&) = delete;
  LiveFrame& operator=(LiveFrame&&) = delete;

  std::uint32_t* Buffer() { return memory.data() + 1; }
  [[nodiscard]] std::size_t Used() const {
    return static_cast<std::size_t>(
        static_cast<const std::byte*>(Render_frameScratch.primitiveCursor) -
        reinterpret_cast<const std::byte*>(memory.data() + 1));
  }
};
}  // namespace

TEST_CASE("Room group records round-trip every field at the original layout",
          "[render][room]") {
  auto bytes = GroupRecords(2);
  const auto group = SampleGroup();
  REQUIRE(Render::WriteRoomGroup(bytes, 1, group));
  const auto read = GroupAt(bytes, 1);
  CHECK(read.flags == group.flags);
  CHECK(read.bounds == group.bounds);
  CHECK(std::array<std::int32_t, 3>{read.angles.x, read.angles.y,
                                    read.angles.z} == std::array{7, -8, 9});
  CHECK(read.polygon_offset == 0x1234);
  CHECK(read.view == group.view);
  const Render::RoomView view = IdentityView(77);
  REQUIRE(Render::WriteRoomGroupView(bytes, 0, view));
  CHECK(GroupAt(bytes, 0).view == view);
}

TEST_CASE("Room group records refuse indexes past the stored count or span",
          "[render][room]") {
  auto bytes = GroupRecords(2);
  CHECK_FALSE(Render::ReadRoomGroup(bytes, 2).has_value());
  CHECK_FALSE(Render::WriteRoomGroup(bytes, 2, SampleGroup()));
  CHECK_FALSE(Render::WriteRoomGroupView(bytes, 2, IdentityView(1)));
  CHECK_FALSE(Render::ReadRoomGroup(std::span(bytes).first(bytes.size() - 1), 1)
                  .has_value());
  CHECK_FALSE(Render::ReadRoomGroup({}, 0).has_value());
}

TEST_CASE("Room group records keep the original byte offsets",
          "[render][room]") {
  auto bytes = GroupRecords(2);
  REQUIRE(Render::WriteRoomGroup(bytes, 1, SampleGroup()));
  const auto* record = bytes.data() + 4 + Render::kRoomGroupRecordBytes;
  std::uint32_t flags = 0;
  std::int16_t first_bound = 0;
  std::int32_t angle_y = 0;
  std::uint32_t polygon_offset = 0;
  std::int16_t last_rotation = 0;
  std::int32_t translation_z = 0;
  std::memcpy(&flags, record, sizeof flags);
  std::memcpy(&first_bound, record + 4, sizeof first_bound);
  std::memcpy(&angle_y, record + 20, sizeof angle_y);
  std::memcpy(&polygon_offset, record + 28, sizeof polygon_offset);
  std::memcpy(&last_rotation, record + 48, sizeof last_rotation);
  std::memcpy(&translation_z, record + 60, sizeof translation_z);
  CHECK(flags == 0x80001400);
  CHECK(first_bound == -1);
  CHECK(angle_y == -8);
  CHECK(polygon_offset == 0x1234);
  CHECK(last_rotation == 4);
  CHECK(translation_z == -300);
}

TEST_CASE("Room geometry validation bounds counts, groups and polygon spans",
          "[render][room]") {
  auto geometry = Geometry(0);
  CHECK(Render::ValidateRoomGeometry(geometry));
  CHECK_FALSE(Render::ValidateRoomGeometry(
      std::span(geometry).first(geometry.size() - 1)));
  CHECK_FALSE(Render::ValidateRoomGeometry({}));
  auto group = GroupAt(geometry, 0);
  group.polygon_offset = 0;
  REQUIRE(Render::WriteRoomGroup(geometry, 0, group));
  CHECK_FALSE(Render::ValidateRoomGeometry(geometry));
  group.polygon_offset = UINT32_MAX;
  REQUIRE(Render::WriteRoomGroup(geometry, 0, group));
  CHECK_FALSE(Render::ValidateRoomGeometry(geometry));
  geometry = Geometry(0);
  const std::uint32_t too_many = 0xFFFFFFFFu;
  std::memcpy(geometry.data() + 4 + Render::kRoomGroupRecordBytes, &too_many,
              sizeof too_many);
  CHECK_FALSE(Render::ValidateRoomGeometry(geometry));
}

TEST_CASE("Room transforms follow the camera and face camera-facing groups",
          "[render][room]") {
  auto bytes = GroupRecords(2);
  Render::RoomGroup placed;
  for (std::size_t axis = 0; axis < 3; ++axis) {
    placed.bounds[axis][0] = static_cast<std::int16_t>(100 * (axis + 1));
  }
  Render::RoomGroup facing;
  facing.flags = 0x400;
  REQUIRE(Render::WriteRoomGroup(bytes, 0, placed));
  REQUIRE(Render::WriteRoomGroup(bytes, 1, facing));
  const auto camera = RolledCamera();
  REQUIRE(Render::UpdateRoomGroupTransforms(bytes, camera));
  Render::RoomView expected_placed = IdentityView(330);
  expected_placed.translation.x = 110;
  expected_placed.translation.y = 220;
  CHECK(GroupAt(bytes, 0).view == expected_placed);
  // A quarter roll about the view axis, keeping the group's own position.
  Render::RoomView expected_facing = camera.view;
  expected_facing.rotation.m = {{{0, -4096, 0}, {4096, 0, 0}, {0, 0, 4096}}};
  CHECK(GroupAt(bytes, 1).view == expected_facing);
}

TEST_CASE("Room transforms keep stored views and refuse truncated groups",
          "[render][room]") {
  auto bytes = GroupRecords(1);
  Render::RoomGroup kept;
  kept.flags = 0x100;
  kept.view = IdentityView(4321);
  REQUIRE(Render::WriteRoomGroup(bytes, 0, kept));
  const auto camera = RolledCamera();
  REQUIRE(Render::UpdateRoomGroupTransforms(bytes, camera));
  CHECK(GroupAt(bytes, 0).view == IdentityView(4321));
  CHECK_FALSE(Render::UpdateRoomGroupTransforms(
      std::span(bytes).first(bytes.size() - 1), camera));
}

TEST_CASE("Scale-compensated room views divide out the camera scale",
          "[render][room]") {
  Render::RoomCamera camera;
  camera.view = IdentityView(30);
  camera.view.translation.x = 10;
  camera.view.translation.y = 20;
  camera.scale = 8192;
  for (std::size_t axis = 0; axis < 3; ++axis) {
    camera.view.rotation.m[axis][axis] = 8192;
  }
  const auto view =
      Render::BuildScaleCompensatedRoomView(camera, {100, 200, 300}, {});
  CHECK(view.rotation.m[0][0] == 4096);
  CHECK(view.translation.x == 110);
  camera.scale = 0;
  CHECK(Render::BuildScaleCompensatedRoomView(camera, {100, 200, 300}, {}) ==
        Render::RoomView{});
}

TEST_CASE("Packed room decoding covers every signed delta and masked shift",
          "[render][room-decode]") {
  for (std::uint32_t shift = 0; shift < 32; ++shift) {
    for (unsigned byte = 0; byte < 256; ++byte) {
      for (std::size_t slot = 0; slot < 6; ++slot) {
        auto data = Coordinates();
        data[6 + slot] = static_cast<std::byte>(byte);
        CheckCoordinates(data, shift);
      }
    }
  }
}

TEST_CASE("Room coordinate shift ignores high bits of the original register",
          "[render][room-decode]") {
  const auto data = MixedDeltas();
  for (std::uint32_t shift : {32u, 35u, 63u, 0xA5A50003u, UINT32_MAX}) {
    CheckCoordinates(data, shift);
  }
}

TEST_CASE("Room projection decodes positions, screens, depths and winding",
          "[render][room-projection]") {
  GteTransformer transformer;
  PackedCoordinates data{};
  constexpr std::array<std::int16_t, 3> kBase{-128, -128, 0};
  std::memcpy(data.data(), kBase.data(), sizeof kBase);
  data[6] = std::byte{32};
  data[10] = std::byte{32};
  auto projection = Render::ProjectAndDecodeRoomPolygon(data, 3, transformer);
  constexpr std::array<std::array<int, 2>, 3> kScreen{
      {{128, 88}, {192, 88}, {128, 152}}};
  std::array<std::int32_t, 3> screens{};
  for (std::size_t index = 0; index < 3; ++index) {
    CAPTURE(index);
    CHECK(projection.positions[index] == ExpectedPosition(data, 3, index));
    const auto screen =
        static_cast<std::uint32_t>(projection.points[index].screen);
    CHECK(ScreenX(screen) == kScreen[index][0]);
    CHECK(ScreenY(screen) == kScreen[index][1]);
    CHECK(projection.points[index].depth == 256);
    screens[index] = projection.points[index].screen;
  }
  CHECK(projection.winding == 4096);
  CHECK(Render::RoomPolygonWinding(screens) == 4096);
  std::swap(data[6], data[9]);
  std::swap(data[7], data[10]);
  projection = Render::ProjectAndDecodeRoomPolygon(data, 3, transformer);
  CHECK(projection.winding == -4096);
}

// Links one packet at `depth` with the signed `bias` in the flags' high half
// and checks the result against the original near, far and slot rules.
void CheckDepthLink(std::int32_t depth, int bias, int far_depth, bool quad) {
  CAPTURE(depth, bias, far_depth, quad);
  Render::RoomClipState clip;
  Render::SetRoomPolygonFarClip(clip, far_depth);
  const auto biased = std::int64_t{depth} + bias;
  const bool expected =
      depth >= 8 && biased >= 8 && biased < far_depth && biased < 2047;
  const auto flags =
      (static_cast<std::uint32_t>(static_cast<std::uint16_t>(bias)) << 16) |
      0xA5A5u;
  std::vector<std::uint32_t> packet(
      quad ? Render::kRoomQuadPacketWords : Render::kRoomTrianglePacketWords,
      0x5A5A5A5A);
  RecordingQueue queue;
  const auto status =
      Render::LinkRoomPolygonByDepth(packet, depth, flags, clip, queue);
  CHECK(Render::ResolveRoomPolygonDepth(depth, flags, clip).has_value() ==
        expected);
  if (!expected) {
    CHECK(status == Render::RoomLinkStatus::kRejected);
    CHECK(queue.packets.empty());
    return;
  }
  CHECK(status == Render::RoomLinkStatus::kLinked);
  REQUIRE(queue.packets.size() == 1);
  CHECK(queue.packets[0].depth == biased);
  CHECK(queue.packets[0].words == packet);
}

TEST_CASE("Room depth links retain near, far and signed-bias boundaries",
          "[render][room-link]") {
  constexpr std::array<std::int32_t, 10> kDepths{
      INT32_MIN, -1, 0, 7, 8, 9, 2046, 2047, 16383, INT32_MAX};
  constexpr std::array<int, 7> kBias{-32768, -8, -1, 0, 1, 8, 32767};
  constexpr std::array<int, 6> kFar{-1, 0, 8, 256, 2047, 32767};
  for (const std::int32_t depth : kDepths) {
    for (const int bias : kBias) {
      for (const int far_depth : kFar) {
        CheckDepthLink(depth, bias, far_depth, false);
        CheckDepthLink(depth, bias, far_depth, true);
      }
    }
  }
}

TEST_CASE("Room links refuse malformed packets and full queues",
          "[render][room-link]") {
  Render::RoomClipState clip;
  RecordingQueue queue;
  for (const std::size_t words : {0u, 9u, 11u, 12u, 14u}) {
    const std::vector<std::uint32_t> packet(words);
    CHECK(Render::LinkRoomPolygonByDepth(packet, 256, 0, clip, queue) ==
          Render::RoomLinkStatus::kInvalidPacket);
  }
  CHECK(queue.packets.empty());
  const std::vector<std::uint32_t> triangle(Render::kRoomTrianglePacketWords);
  RecordingQueue full(kTriangleBytes - 1);
  CHECK(Render::LinkRoomPolygonByDepth(triangle, 256, 0, clip, full) ==
        Render::RoomLinkStatus::kBufferFull);
  CHECK(full.packets.empty());
}

TEST_CASE("Room links reject negative slots under a negative near clip",
          "[render][room-link]") {
  Render::RoomClipState clip;
  RecordingQueue queue;
  const std::vector<std::uint32_t> triangle(Render::kRoomTrianglePacketWords);
  Render::SetRoomPolygonNearClip(clip, -32768);
  CHECK(Render::LinkRoomPolygonByDepth(triangle, 0, 0xFFFF0000u, clip, queue) ==
        Render::RoomLinkStatus::kRejected);
  CHECK(queue.packets.empty());
  CHECK(Render::LinkRoomPolygonByDepth(triangle, 255, 0, clip, queue) ==
        Render::RoomLinkStatus::kLinked);
  CHECK(Render::LinkRoomPolygonByDepth(triangle, 256, 0, clip, queue) ==
        Render::RoomLinkStatus::kLinked);
  REQUIRE(queue.packets.size() == 2);
  CHECK(queue.packets[1].depth == 256);
}

TEST_CASE("Room clip setters keep the original signed-halfword registers",
          "[render][room-link]") {
  Render::RoomClipState clip;
  Render::SetRoomPolygonNearClip(clip, 0x10008);
  Render::SetRoomPolygonFarClip(clip, 0x8000);
  Render::SetRoomTextureLodDepth(clip, 0x1FFFF);
  Render::SetRoomPolygonScreenBounds(clip, {0x10090, 0x10068, 0x100B0, -1});
  CHECK(clip.near_depth == 8);
  CHECK(clip.far_depth == -32768);
  CHECK(clip.lod_depth == UINT32_MAX);
  CHECK(clip.screen == Render::RoomScreenBounds{0x90, 0x68, 0xB0, -1});
}

TEST_CASE("Shared subdivision preserves every source-valid packet count",
          "[render][room-subdivision]") {
  for (const bool quad : {false, true}) {
    for (int levels = 0; levels <= 3; ++levels) CheckLevel(quad, levels);
  }
}

TEST_CASE("Room subdivision refuses levels outside the original recursion",
          "[render][room-subdivision]") {
  for (const bool quad : {false, true}) {
    for (const int levels : {-1, 4, INT32_MAX}) {
      GteTransformer transformer;
      RecordingQueue queue;
      const auto result =
          Subdivide(quad, levels, kMaterial, {}, transformer, queue);
      CHECK(result.status == Render::RoomDrawStatus::kInvalidLevels);
      CHECK(result.emitted == 0);
      CHECK(queue.packets.empty());
    }
  }
}

TEST_CASE("Room subdivision emits no partial packet at a capacity boundary",
          "[render][room-subdivision]") {
  for (const bool quad : {false, true}) {
    const std::size_t full =
        quad ? 4 * kQuadBytes + 4 * kTriangleBytes : 7 * kTriangleBytes;
    GteTransformer transformer;
    RecordingQueue queue(full - 1);
    const auto result = Subdivide(quad, 1, kMaterial, {}, transformer, queue);
    CHECK(result.status == Render::RoomDrawStatus::kBufferFull);
    CHECK(queue.Used() == full - kTriangleBytes);
    RecordingQueue empty(0);
    CHECK(Subdivide(quad, 0, kMaterial, {}, transformer, empty).status ==
          Render::RoomDrawStatus::kBufferFull);
    CHECK(empty.Used() == 0);
  }
}

TEST_CASE("Room subdivision retains source midpoint position color and UV",
          "[render][room-subdivision]") {
  GteTransformer transformer;
  RecordingQueue queue;
  REQUIRE(Subdivide(false, 1, kMaterial, {}, transformer, queue).emitted == 7);
  const auto& first = queue.packets.front().words;
  using Screen = std::pair<std::int16_t, std::int16_t>;
  const std::array<Screen, 3> screens{
      Screen{ScreenX(first[2]), ScreenY(first[2])},
      Screen{ScreenX(first[5]), ScreenY(first[5])},
      Screen{ScreenX(first[8]), ScreenY(first[8])}};
  CHECK(screens == std::array<Screen, 3>{Screen{128, 88}, Screen{160, 88},
                                         Screen{128, 120}});
  CHECK((first[4] & 0xFFFFFF) == 0x405060);
  CHECK((first[1] & 0xFFFFFF) == 0x102030);
  CHECK((first[6] & 0xFFFF) == (32u | (48u << 8)));
  CHECK((first[9] & 0xFFFF) == (48u | (64u << 8)));
}

TEST_CASE("Room subdivision preserves far rejection and signed depth bias",
          "[render][room-subdivision]") {
  for (const bool quad : {false, true}) {
    GteTransformer transformer;
    Render::RoomClipState clip;
    Render::SetRoomPolygonFarClip(clip, 256);
    RecordingQueue rejected;
    CHECK(Subdivide(quad, 1, kMaterial, clip, transformer, rejected).emitted ==
          0);
    CHECK(rejected.packets.empty());
    auto biased = kMaterial;
    biased.flags = 0xFFFF0000;
    RecordingQueue accepted;
    CHECK(Subdivide(quad, 1, biased, clip, transformer, accepted).emitted ==
          (quad ? 8u : 7u));
    CHECK(std::ranges::all_of(accepted.packets, [](const auto& packet) {
      return packet.depth == 255;
    }));
  }
}

TEST_CASE("Room subdivision rejects off-screen leaves without emitting",
          "[render][room-subdivision]") {
  GteTransformer transformer;
  Render::SetProjectionScreenOffset(transformer, {1000, 1000});
  RecordingQueue queue;
  CHECK(Subdivide(false, 3, kMaterial, {}, transformer, queue).emitted == 0);
  CHECK(Subdivide(true, 3, kMaterial, {}, transformer, queue).emitted == 0);
  CHECK(queue.packets.empty());
}

TEST_CASE("Room batches emit typed packets with native materials",
          "[render][room-batch]") {
  GteTransformer transformer;
  RecordingQueue queue;
  const auto batch = Batch();
  const auto result =
      Render::DrawRoomGeometryPolygons(batch, 0, 0, {}, transformer, queue);
  CHECK(result.status == Render::RoomDrawStatus::kDrawn);
  CHECK(result.emitted == 2);
  CHECK(queue.Used() == kTriangleBytes + kQuadBytes);
  const std::array expected{1, 1};
  CHECK(CountPackets(queue) == expected);
  CheckBatchTriangle(queue);
}

TEST_CASE("Room batches preserve capacity and exact final-record boundaries",
          "[render][room-batch]") {
  const auto batch = Batch();
  for (const std::size_t capacity : {0u, 39u, 40u, 91u, 92u}) {
    CAPTURE(capacity);
    GteTransformer transformer;
    RecordingQueue queue(capacity);
    const auto result =
        Render::DrawRoomGeometryPolygons(batch, 0, 0, {}, transformer, queue);
    CHECK(result.status == (capacity == 92
                                ? Render::RoomDrawStatus::kDrawn
                                : Render::RoomDrawStatus::kBufferFull));
    CHECK(queue.Used() == (capacity < 40 ? 0u : capacity < 92 ? 40u : 92u));
  }
  GteTransformer transformer;
  RecordingQueue queue(0);
  const std::array<std::uint32_t, 2> empty{};
  const auto result = Render::DrawRoomGeometryPolygons(
      std::as_bytes(std::span(empty)), 0, 0, {}, transformer, queue);
  CHECK(result.status == Render::RoomDrawStatus::kDrawn);
  CHECK(result.emitted == 0);
}

TEST_CASE("Room batches refuse truncated or oversized polygon records",
          "[render][room-batch]") {
  GteTransformer transformer;
  RecordingQueue queue;
  const auto batch = Batch();
  const std::span bytes(batch);
  CHECK(Render::DrawRoomGeometryPolygons(bytes.first(bytes.size() - 1), 0, 0,
                                         {}, transformer, queue)
            .status == Render::RoomDrawStatus::kInvalidGeometry);
  CHECK(Render::DrawRoomGeometryPolygons(bytes.first(4), 0, 0, {}, transformer,
                                         queue)
            .status == Render::RoomDrawStatus::kInvalidGeometry);
  auto oversized = batch;
  const std::uint32_t count = 0x08000000;
  std::memcpy(oversized.data(), &count, sizeof count);
  CHECK(
      Render::DrawRoomGeometryPolygons(oversized, 0, 0, {}, transformer, queue)
          .status == Render::RoomDrawStatus::kInvalidGeometry);
  CHECK(queue.packets.empty());
}

TEST_CASE("Room batches retain subdivision clipping and texture LOD",
          "[render][room-batch]") {
  GteTransformer transformer;
  const auto batch = Batch();
  RecordingQueue subdivided;
  CHECK(Render::DrawRoomGeometryPolygons(batch, 3u << 13, 1, {}, transformer,
                                         subdivided)
            .emitted == 15);
  const std::array expected{11, 4};
  CHECK(CountPackets(subdivided) == expected);
  transformer.LoadView(IdentityView(2048));
  Render::RoomClipState clip;
  Render::SetRoomTextureLodDepth(clip, 512);
  RecordingQueue distant;
  CHECK(
      Render::DrawRoomGeometryPolygons(batch, 0, 0, clip, transformer, distant)
          .emitted == 2);
  const auto& triangle = distant.packets.front().words;
  CHECK(triangle[3] == 0x12341008);
  CHECK(triangle[6] == 0x00142018);
  Render::SetRoomPolygonFarClip(clip, 512);
  RecordingQueue clipped;
  CHECK(
      Render::DrawRoomGeometryPolygons(batch, 0, 0, clip, transformer, clipped)
          .emitted == 0);
  CHECK(clipped.packets.empty());
}

TEST_CASE("Room geometry skips hidden, alternate-pass and unfaced groups",
          "[render][room]") {
  struct Case {
    std::uint32_t flags;
    Render::RoomDrawPass pass;
    std::size_t packets;
  };
  constexpr std::array kCases{
      Case{0, {0, 0}, 2},      Case{0x100, {0, 0}, 0},
      Case{0x1000, {0, 0}, 0}, Case{0x8000, {0, 0}, 2},
      Case{0x8000, {1, 0}, 0}, Case{0, {0, 1}, 0},
      Case{1, {0, 1}, 2},      Case{0, {0, 0x10001}, 2},
  };
  for (const auto& entry : kCases) {
    CAPTURE(entry.flags, entry.pass.mode, entry.pass.view_facing);
    GteTransformer transformer;
    transformer.LoadView(IdentityView(9999));
    RecordingQueue queue;
    const auto geometry = Geometry(entry.flags);
    CHECK(Render::DrawRoomGeometry(geometry, entry.pass, {}, transformer,
                                   queue) == Render::RoomDrawStatus::kDrawn);
    CHECK(queue.packets.size() == entry.packets);
  }
  GteTransformer transformer;
  RecordingQueue queue;
  const auto geometry = Geometry(0);
  CHECK(Render::DrawRoomGeometry(std::span(geometry).first(geometry.size() - 1),
                                 {}, {}, transformer, queue) ==
        Render::RoomDrawStatus::kInvalidGeometry);
  RecordingQueue full(kTriangleBytes);
  CHECK(Render::DrawRoomGeometry(geometry, {}, {}, transformer, full) ==
        Render::RoomDrawStatus::kBufferFull);
  CHECK(full.packets.size() == 1);
}

TEST_CASE("Room visibility matches the original per-vertex rejection",
          "[render][room-screen]") {
  std::uint32_t state = 0x80098014u;
  for (unsigned sample = 0; sample <= UINT16_MAX; ++sample) {
    CheckScreenSample(state);
  }
}

TEST_CASE("Room visibility keeps inclusive origins and exclusive extents",
          "[render][room-screen]") {
  const Render::RoomScreenBounds clip{0, 0, 320, 224};
  constexpr std::array<ScreenPoint, 10> kPoints{{{0, 0},
                                                 {319, 223},
                                                 {320, 223},
                                                 {319, 224},
                                                 {-1, 0},
                                                 {0, -1},
                                                 {INT16_MIN, 0},
                                                 {INT16_MAX, 0},
                                                 {0, INT16_MIN},
                                                 {0, INT16_MAX}}};
  for (const auto point : kPoints) {
    std::array<ScreenPoint, 4> points{};
    points.fill(point);
    CheckVisibility(points, clip);
  }
  CheckVisibility({{{-1, -1}, {320, -1}, {-1, 224}, {320, 224}}}, clip);
  CHECK(Render::IsRoomPolygonOnScreen({0, 0, 319, 223}, clip));
  CHECK_FALSE(Render::IsRoomPolygonOnScreen({320, 0, 320, 223}, clip));
  CHECK_FALSE(Render::IsRoomPolygonOnScreen({10, 10, 5, 20}, clip));
}

TEST_CASE("Room vertex midpoints floor signed positions and every byte pair",
          "[render][room-midpoint]") {
  for (std::uint32_t bits = 0; bits <= UINT16_MAX; ++bits) {
    CheckMidpointSample(bits);
  }
  const Render::RoomVertex first{
      {-1, INT16_MAX, INT16_MIN}, 0xAA0000FFu, 0xFF00u};
  const Render::RoomVertex second{
      {0, INT16_MIN, INT16_MAX}, 0x55FF0000u, 0x00FFu};
  const auto midpoint = Render::InterpolateRoomVertexMidpoint(first, second);
  CHECK(midpoint.position == Render::RoomPosition{-1, -1, -1});
  CHECK(midpoint.color == 0x007F007Fu);
  CHECK(midpoint.uv == 0x7F7Fu);
}

TEST_CASE("Room color tint preserves grayscale weights and clamp order",
          "[render][room-tint]") {
  CHECK(Tint(0xAA10C020, 4096, 0, 0, 0) == 0xAA787878);
  CHECK(Tint(0xAA10C020, 2048, 0, 0, 0) == 0xAA449C4C);
  CHECK(Tint(0xC700FF00, -32768, 40, -40, 0) == 0xC700D728);
  CHECK(Tint(0xC7000000, 0, -512, 508, 0) == 0xC700FF00);
  CHECK(Tint(0xC7FFFFFF, 0, 508, -512, 0) == 0xC7FF00FF);
}

TEST_CASE("Room color tint matches every signed16 strength",
          "[render][room-tint]") {
  for (std::uint32_t word = 0; word <= UINT16_MAX; ++word) {
    const auto strength = Halfword(word);
    for (const auto color : {0xC700FF00u, 0xA310C020u}) {
      CHECK(Tint(color, strength, 40, -40, 8) ==
            ReferenceTint(color, strength, 40, -40, 8));
    }
  }
}

TEST_CASE("Room tint transforms GT3 and GT4 payloads without changing tags",
          "[render][room-tint]") {
  const auto flags = GENERATE(0u, 1u, 2u, 3u);
  TintPackets packets(flags);
  const auto expected = packets.Expected();
  CHECK(Render::TintRoomGeometryPrimitives(packets.Range(), kTint, flags) ==
        Render::RoomTintStatus::kTinted);
  CHECK(packets.words == expected);
}

TEST_CASE("Room tint postpass uses signed offsets and its strength",
          "[render][room-tint]") {
  const auto flags = GENERATE(0u, 2u);
  TintPackets packets(flags);
  auto expected = packets.words;
  for (std::size_t vertex = 0; vertex < 3; ++vertex) {
    const auto index = 2 + vertex * 3;
    expected[index] = ReferenceTint(expected[index], 2048, 508, -512, 12);
  }
  for (std::size_t vertex = 0; vertex < 4; ++vertex) {
    const auto index = packets.quad + 1 + vertex * 3;
    expected[index] = ReferenceTint(expected[index], 2048, 508, -512, 12);
  }
  CHECK(Render::TintRoomGeometryPrimitives(packets.Range(),
                                           {127, -128, 3, 2048}, flags) ==
        Render::RoomTintStatus::kTinted);
  CHECK(packets.words == expected);
}

TEST_CASE("Room tint stops at foreign packets and refuses truncated ranges",
          "[render][room-tint]") {
  TintPackets foreign(0);
  foreign.words[foreign.quad] = 0x08000034;
  const auto stopped = foreign.Expected(false);
  CHECK(Render::TintRoomGeometryPrimitives(foreign.Range(), kTint, 0) ==
        Render::RoomTintStatus::kStoppedAtForeignPacket);
  CHECK(foreign.words == stopped);
  TintPackets truncated(0);
  const auto untouched = truncated.words;
  CHECK(Render::TintRoomGeometryPrimitives(
            truncated.Range().first(truncated.Range().size() - 1), kTint, 0) ==
        Render::RoomTintStatus::kTruncated);
  CHECK(truncated.words == untouched);
  CHECK(Render::TintRoomGeometryPrimitives({}, kTint, 0) ==
        Render::RoomTintStatus::kTinted);
  TintPackets zero(0);
  const auto unchanged = zero.words;
  CHECK(Render::TintRoomGeometryPrimitives(zero.Range(), {}, 0) ==
        Render::RoomTintStatus::kTinted);
  CHECK(zero.words == unchanged);
}

TEST_CASE("Sky dome emits rotated Gouraud faces at the farthest slot",
          "[render][room]") {
  GteTransformer transformer;
  transformer.LoadView(IdentityView(0));
  const auto sky = OneFaceSky();
  const auto bytes = std::as_bytes(std::span(&sky, 1));
  RecordingQueue queue;
  CHECK(Render::DrawSkyDome(bytes, transformer, queue) ==
        Render::RoomDrawStatus::kDrawn);
  REQUIRE(queue.packets.size() == 1);
  const auto& packet = queue.packets.front();
  CHECK(packet.depth == Render::kRoomLastOrderingSlot);
  REQUIRE(packet.words.size() == Render::kSkyDomePacketWords);
  CHECK(ScreenX(packet.words[2]) == 140);
  CHECK(ScreenY(packet.words[2]) == 100);
  CHECK((packet.words[1] & 0xFF) == 255);
}

TEST_CASE("Sky dome refuses truncated, out-of-range and unqueued faces",
          "[render][room]") {
  GteTransformer transformer;
  transformer.LoadView(IdentityView(0));
  const auto sky = OneFaceSky();
  const auto bytes = std::as_bytes(std::span(&sky, 1));
  RecordingQueue refused;
  CHECK(Render::DrawSkyDome(bytes.first(bytes.size() - 1), transformer,
                            refused) ==
        Render::RoomDrawStatus::kInvalidGeometry);
  auto invalid = sky;
  invalid.faces[63][1] = Render::kSkyDomeVertexCount;
  CHECK(Render::DrawSkyDome(std::as_bytes(std::span(&invalid, 1)), transformer,
                            refused) ==
        Render::RoomDrawStatus::kInvalidGeometry);
  CHECK(refused.packets.empty());
  RecordingQueue full(0);
  CHECK(Render::DrawSkyDome(bytes, transformer, full) ==
        Render::RoomDrawStatus::kBufferFull);
}

TEST_CASE("Projection screen offsets round-trip as unsigned halfwords",
          "[render][room]") {
  GteTransformer transformer;
  Render::SetProjectionScreenOffset(transformer, {264, 196});
  CHECK(Render::GetProjectionScreenOffset(transformer) ==
        Render::RoomScreenOffset{264, 196});
  Render::SetProjectionScreenOffset(transformer, {-1, 65536 + 5});
  CHECK(Render::GetProjectionScreenOffset(transformer) ==
        Render::RoomScreenOffset{0xFFFF, 5});
}

TEST_CASE("Live room draws link sized packets into the ordering table",
          "[render][room][live]") {
  LiveFrame frame;
  auto& clip = Render::LiveRoomClipState();
  CHECK(clip.near_depth == 8);
  const auto geometry = Geometry(0);
  REQUIRE(Render::DrawLiveRoomGeometry(geometry.data(), geometry.size(), 0) ==
          Render::RoomDrawStatus::kDrawn);
  REQUIRE(frame.Used() == kTriangleBytes + kQuadBytes);
  auto* triangle = frame.Buffer();
  auto* quad = triangle + Render::kRoomTrianglePacketWords;
  CHECK((triangle[0] >> 24) == 9);
  CHECK((quad[0] >> 24) == 12);
  CHECK(nextPrim(&frame.ordering[256]) == quad);
  CHECK(nextPrim(quad) == triangle);
  CHECK(frame.memory.back() == 0xA5A5A5A5);
}

TEST_CASE("Live room draws refuse geometry overlapping unwritten packets",
          "[render][room][live]") {
  LiveFrame frame;
  const auto geometry = Geometry(0);
  const auto words = (geometry.size() + 3) / 4;
  std::memcpy(frame.Buffer(), geometry.data(), geometry.size());
  for (const std::size_t cursor : {std::size_t{0}, words - 1}) {
    CAPTURE(cursor);
    Render_frameScratch.primitiveCursor = frame.Buffer() + cursor;
    CHECK(Render::DrawLiveRoomGeometry(frame.Buffer(), geometry.size(), 0) ==
          Render::RoomDrawStatus::kInvalidGeometry);
    CHECK(Render_frameScratch.primitiveCursor == frame.Buffer() + cursor);
  }
  Render_frameScratch.primitiveCursor = frame.Buffer() + words;
  CHECK(Render::DrawLiveRoomGeometry(frame.Buffer(), geometry.size(), 0) ==
        Render::RoomDrawStatus::kDrawn);
  CHECK(Render_frameScratch.primitiveCursor ==
        frame.Buffer() + words + Render::kRoomTrianglePacketWords +
            Render::kRoomQuadPacketWords);
}

TEST_CASE("Live room draws queue nothing before the frame is ready",
          "[render][room][live]") {
  LiveFrame frame;
  const auto geometry = Geometry(0);
  Render_frameScratch.orderingTable = nullptr;
  auto* before = Render_frameScratch.primitiveCursor;
  CHECK(Render::DrawLiveRoomGeometry(geometry.data(), geometry.size(), 0) ==
        Render::RoomDrawStatus::kDrawn);
  CHECK(Render_frameScratch.primitiveCursor == before);
}

TEST_CASE("Live room tint is bounded by the current frame buffer",
          "[render][room-tint][live]") {
  LiveFrame frame;
  TintPackets packets(0);
  std::ranges::copy(packets.words, frame.Buffer());
  const Field_RoomTint tint{-1, 1, 2, 0, 0, 1};
  CHECK(Render::TintLiveRoomGeometryPrimitives(
      frame.Buffer() + 1, frame.Buffer() + packets.end, tint, 0));
  CHECK(std::ranges::equal(packets.Expected(),
                           std::span(frame.Buffer(), packets.words.size())));
  const auto outside = packets.words;
  CHECK_FALSE(Render::TintLiveRoomGeometryPrimitives(
      packets.words.data() + 1, packets.words.data() + packets.end, tint, 0));
  CHECK(packets.words == outside);
  auto* bytes = reinterpret_cast<std::byte*>(frame.Buffer());
  CHECK_FALSE(
      Render::TintLiveRoomGeometryPrimitives(bytes + 1, bytes + 9, tint, 0));
  CHECK_FALSE(Render::TintLiveRoomGeometryPrimitives(
      frame.Buffer() + 1, frame.Buffer() + packets.end - 1, tint, 0));
  CHECK(Render::TintLiveRoomGeometryPrimitives(nullptr, nullptr, tint, -1));
}

TEST_CASE("Room draw entries no longer resolve through module addresses",
          "[render][room][redirect-binding]") {
  const std::span symbols(Content_moduleSymbols, Content_moduleSymbolCount);
  constexpr std::array<std::uint32_t, 21> kRetired{
      0x8009723C, 0x80097388, 0x8009797C, 0x80097A10, 0x80097AEC, 0x80097CF0,
      0x80097F70, 0x80098014, 0x800980F8, 0x80098160, 0x8009817C, 0x80098194,
      0x800981B8, 0x8009820C, 0x800983F8, 0x800985AC, 0x80098648, 0x80098D6C,
      0x80098E50, 0x800994D4, 0x800994F4};
  for (const auto address : kRetired) {
    CAPTURE(address);
    CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                            address) == nullptr);
  }
}
