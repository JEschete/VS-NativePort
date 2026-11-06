#include <array>
#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Field/RoomRenderingLive.hpp"
#include "Legacy/Render/LibGpu.h"
#include "Render/Device.hpp"
#include "Render/PageSpace.hpp"
#include "Render/RoomDraw.hpp"
#include "Render/RoomDrawLive.hpp"

extern "C" {
#include "Core/Scratch.h"
#include "Field/CameraLiveState.h"
#include "Field/Gimmicks.h"
#include "Field/RoomRenderingLiveState.h"
#include "Render/Render.h"
}

extern "C" void* Render_legacyPrimitiveBuffers[2];
extern "C" int Render_frameBuf;

namespace {

struct Packet {
  u_long tag;
  u_long body[3];
};

std::vector<const void*> Walk(u_long* head) {
  std::vector<const void*> out;
  for (void* p = head; p != nullptr && out.size() < 64;
       p = isendprim(p) ? nullptr : nextPrim(p)) {
    out.push_back(p);
  }
  return out;
}

constexpr auto kRoomGroupBytes = VS::Render::kRoomGroupRecordBytes;
using RoomGroupRecord = std::array<std::byte, kRoomGroupBytes>;

template <typename Geometry>
std::span<std::byte> GeometryBytes(Geometry& geometry) {
  return std::as_writable_bytes(std::span(&geometry, 1));
}

template <typename Geometry>
VS::Render::RoomGroup GroupOf(Geometry& geometry, std::size_t index) {
  const auto group = VS::Render::ReadRoomGroup(GeometryBytes(geometry), index);
  REQUIRE(group.has_value());
  return group.value_or(VS::Render::RoomGroup{});
}

template <typename Geometry>
void StoreGroup(Geometry& geometry, std::size_t index,
                const VS::Render::RoomGroup& group) {
  REQUIRE(VS::Render::WriteRoomGroup(GeometryBytes(geometry), index, group));
}

}  // namespace

TEST_CASE("Ordering-table links survive 64-bit addresses", "[legacy][gpu]") {
  // Two packets exactly 16 MiB apart share their low 24 address bits: a raw
  // 24-bit link cannot tell them apart.
  std::vector<unsigned char> memory((16u << 20) + 2 * sizeof(Packet));
  auto* a = reinterpret_cast<Packet*>(memory.data());
  auto* b = reinterpret_cast<Packet*>(memory.data() + (16u << 20));
  std::memset(a, 0, sizeof *a);
  std::memset(b, 0, sizeof *b);
  setlen(a, 3);
  setlen(b, 3);

  u_long ot[4];
  ClearOTagR(ot, 4);
  addPrim(&ot[2], a);
  addPrim(&ot[1], b);

  // reverse table: drawing starts at the last entry and follows each link down
  // to entry 0
  const auto order = Walk(&ot[3]);
  REQUIRE(order.size() == 6);
  CHECK(order[0] == &ot[3]);
  CHECK(order[1] == &ot[2]);
  CHECK(order[2] == a);
  CHECK(order[3] == &ot[1]);
  CHECK(order[4] == b);
  CHECK(order[5] == &ot[0]);
  CHECK(LegacyGpu_Handle(a) != LegacyGpu_Handle(b));
  CHECK(LegacyGpu_WalkList(&ot[3]) == 6);
}

TEST_CASE("setaddr accepts a packet pointer or a copied link word",
          "[legacy][gpu]") {
  Packet p{}, q{};
  setaddr(&p, &q);                        // pointer: registered as a handle
  setaddr(&q, getaddr(&p) | 0x05000000);  // link word (length bits included)
                                          // copied from another packet
  CHECK(nextPrim(&p) == &q);
  CHECK(getaddr(&q) == getaddr(&p));
  termPrim(&q);
  CHECK(isendprim(&q));
  CHECK(nextPrim(&q) == nullptr);
}

TEST_CASE("Packet traversal rejects cycles immediately", "[legacy][gpu]") {
  Packet first{}, second{};
  setaddr(&first, &second);
  setaddr(&second, &first);
  CHECK(LegacyGpu_WalkList(&first) == -1);
  termPrim(&second);
  CHECK(LegacyGpu_WalkList(&first) == 2);
}

TEST_CASE("Packet decoder validates complete polygon line and sprite commands",
          "[render][gpu]") {
  VS::Render::PageSpace pages;
  const unsigned cases[][2] = {{0x20, 4}, {0x28, 5}, {0x24, 7}, {0x2C, 9},
                               {0x30, 6}, {0x38, 8}, {0x34, 9}, {0x3C, 12},
                               {0x40, 3}, {0x50, 4}, {0x60, 3}, {0x64, 4},
                               {0x68, 2}, {0x6C, 3}, {0x74, 3}, {0x7C, 3}};
  for (const auto& entry : cases) {
    std::array<VS::u32, 12> words{};
    words[0] = entry[0] << 24;
    for (unsigned length = 1; length < entry[1]; ++length) {
      CAPTURE(entry[0], length);
      CHECK(pages.Execute(std::span(words).first(length)) == 0);
    }
    CHECK(pages.Execute(std::span(words).first(entry[1])) == entry[1]);
  }
}

TEST_CASE(
    "Framebuffer feedback mask packets set and clear the stored blend bit",
    "[render][gpu][framebuffer]") {
  VS::Render::PageSpace pages;
  DR_STP packet{};
  SetDrawStp(&packet, 1);
  CHECK((packet.tag >> 24) == 2);
  CHECK(packet.code[0] == 0xE6000001u);
  CHECK(packet.code[1] == 0);
  pages.Execute(std::span(reinterpret_cast<const VS::u32*>(packet.code), 2));
  const std::array<VS::u32, 2> red{0x680000FFu, 0};
  pages.Execute(red);
  CHECK(pages.At(0, 0) == 0x801F);
  SetDrawStp(&packet, 0);
  CHECK(packet.code[0] == 0xE6000000u);
  pages.Execute(std::span(reinterpret_cast<const VS::u32*>(packet.code), 2));
  const std::array<VS::u32, 2> blue{0x68FF0000u, 1};
  pages.Execute(blue);
  CHECK(pages.At(1, 0) == 0x7C00);
}

TEST_CASE("Room geometry validates group pointers and polygon extents",
          "[render][room]") {
  struct {
    unsigned count = 1;
    RoomGroupRecord group{};
    unsigned triangles = 1;
    unsigned quads = 0;
    unsigned char polygon[32]{};
  } geometry;
  auto group = GroupOf(geometry, 0);
  group.polygon_offset = 4 + sizeof geometry.group;
  StoreGroup(geometry, 0, group);
  REQUIRE(VS::Render::ValidateLiveRoomGeometry(&geometry, sizeof geometry));
  CHECK_FALSE(
      VS::Render::ValidateLiveRoomGeometry(&geometry, sizeof geometry - 1));
  geometry.triangles = 0xFFFFFFFFu;
  CHECK_FALSE(VS::Render::ValidateLiveRoomGeometry(&geometry, sizeof geometry));
  geometry.triangles = 1;
  group.polygon_offset = 0;
  StoreGroup(geometry, 0, group);
  CHECK_FALSE(VS::Render::ValidateLiveRoomGeometry(&geometry, sizeof geometry));
  CHECK_FALSE(VS::Render::ValidateLiveRoomGeometry(nullptr, 0));
}

TEST_CASE("Room transforms use native camera layout and camera-facing groups",
          "[render][room]") {
  struct {
    unsigned count = 2;
    std::array<RoomGroupRecord, 2> groups{};
  } geometry;
  auto* frame = static_cast<Render_FrameScratch*>(Core_ScratchBase());
  const auto saved = *frame;
  frame->viewMatrix = {};
  auto first = GroupOf(geometry, 0);
  auto second = GroupOf(geometry, 1);
  for (int axis = 0; axis < 3; ++axis) {
    frame->viewMatrix.m[axis][axis] = 4096;
    frame->viewMatrix.t[axis] = 10 * (axis + 1);
    first.bounds[axis][0] = static_cast<std::int16_t>(100 * (axis + 1));
  }
  frame->camera.farClip = 4096;
  frame->camera.angles.vz = 1024;
  second.flags = 0x400;
  StoreGroup(geometry, 0, first);
  StoreGroup(geometry, 1, second);
  REQUIRE(
      VS::Render::UpdateLiveRoomGroupTransforms(&geometry, sizeof geometry));
  first = GroupOf(geometry, 0);
  second = GroupOf(geometry, 1);
  CHECK(first.view.translation == VS::Core::Math::Vec3{110, 220, 330});
  CHECK(first.view.rotation.m[0][0] == 4096);
  CHECK(second.view.rotation.m[0][0] == 0);
  CHECK(second.view.rotation.m[0][1] == -4096);
  CHECK(second.view.rotation.m[1][0] == 4096);
  CHECK(second.view.rotation.m[2][2] == 4096);
  CHECK_FALSE(VS::Render::UpdateLiveRoomGroupTransforms(&geometry,
                                                        sizeof geometry - 1));
  frame->camera.farClip = 8192;
  for (int axis = 0; axis < 3; ++axis) {
    frame->viewMatrix.m[axis][axis] = 8192;
  }
  VECTOR position{100, 200, 300, 0}, angles{};
  MATRIX output{};
  VS::Render::BuildLiveScaleCompensatedRoomView(position, angles, output);
  CHECK(output.m[0][0] == 4096);
  CHECK(output.t[0] == 110);
  *frame = saved;
}

TEST_CASE("Room polygons preserve UV order and bound native frame packets",
          "[render][room]") {
  struct {
    unsigned count = 1;
    RoomGroupRecord group_record{};
    unsigned triangles = 1;
    unsigned quads = 0;
    unsigned char polygon[40]{};
  } geometry;
  VS::Render::RoomGroup group;
  group.flags = 0x800;
  group.polygon_offset = 4 + sizeof geometry.group_record;
  for (int axis = 0; axis < 3; ++axis) {
    group.view.rotation.m[axis][axis] = 4096;
  }
  group.view.translation.z = 1024;
  const auto draw = [&](int mode) {
    StoreGroup(geometry, 0, group);
    return VS::Render::DrawLiveRoomGeometry(&geometry, sizeof geometry, mode);
  };
  auto& room_clip = VS::Render::LiveRoomClipState();
  const auto saved_clip = room_clip;
  const std::int16_t anchor[3]{-64, -64, 0};
  std::memcpy(geometry.polygon, anchor, sizeof anchor);
  geometry.polygon[6] = geometry.polygon[10] = 127;
  geometry.polygon[32] = geometry.polygon[33] = 127;
  for (unsigned offset : {12u, 16u, 20u, 36u}) {
    geometry.polygon[offset] = geometry.polygon[offset + 1] =
        geometry.polygon[offset + 2] = 128;
  }
  geometry.polygon[15] = 0x35;
  geometry.polygon[24] = 10;
  geometry.polygon[25] = 20;
  geometry.polygon[28] = 30;
  geometry.polygon[29] = 40;
  geometry.polygon[19] = 50;
  geometry.polygon[23] = 60;
  geometry.polygon[35] = 70;
  geometry.polygon[39] = 80;
  geometry.polygon[26] = 0x34;
  geometry.polygon[27] = 0x12;
  geometry.polygon[30] = 0x15;
  std::vector<unsigned char> memory(0x20000 + 16, 0xA5);
  auto* buffer = memory.data() + 8;
  std::array<u_long, 2048> ordering{};
  auto* frame = static_cast<Render_FrameScratch*>(Core_ScratchBase());
  const auto saved = *frame;
  void* savedBuffer = Render_legacyPrimitiveBuffers[0];
  const int savedFrame = Render_frameBuf;
  Render_legacyPrimitiveBuffers[0] = buffer;
  Render_frameBuf = 0;
  *frame = {};
  frame->primitiveCursor = buffer;
  frame->orderingTable = ordering.data();
  Core_Gte_Reset();
  Core_Gte_SetGeomOffset(160, 120);
  Core_Gte_SetGeomScreen(256);
  ClearOTagR(ordering.data(), static_cast<int>(ordering.size()));
  VS::Render::SetRoomPolygonNearClip(room_clip, 8);
  VS::Render::SetRoomTextureLodDepth(room_clip, 512);
  const auto saved_room = Field_roomData;
  const auto saved_tint = Field_roomTint;
  const auto saved_mode = Field_roomGeometryDrawMode;
  const auto saved_start = Field_roomPrimitiveRangeStart;
  const auto saved_end = Field_roomPrimitiveRangeEnd;
  const auto saved_field_view = frame->viewMatrix;
  for (int axis = 0; axis < 3; ++axis) {
    frame->viewMatrix.m[axis][axis] = 4096;
  }
  frame->viewMatrix.t[2] = 1024;
  Field_roomData.header.geometryLength = sizeof geometry;
  Field_roomGeometryDrawMode = 0;
  Render_ColorCode tint{1, 2, 3, 0};
  VS::Field::LiveSetRoomTint(&tint, 0);
  StoreGroup(geometry, 0, group);
  VS::Field::LiveDrawRoomGeometry(&geometry);
  auto* tinted = reinterpret_cast<POLY_GT3*>(buffer);
  CHECK(Field_roomPrimitiveRangeStart == buffer);
  CHECK(Field_roomPrimitiveRangeEnd == buffer + sizeof(POLY_GT3));
  CHECK(tinted->r0 == 132);
  CHECK(tinted->g1 == 136);
  CHECK(tinted->b2 == 140);
  Field_roomData = saved_room;
  Field_roomTint = saved_tint;
  Field_roomGeometryDrawMode = saved_mode;
  Field_roomPrimitiveRangeStart = saved_start;
  Field_roomPrimitiveRangeEnd = saved_end;
  frame->viewMatrix = saved_field_view;
  ClearOTagR(ordering.data(), static_cast<int>(ordering.size()));
  frame->primitiveCursor = buffer;
  draw(0);
  auto* triangle = reinterpret_cast<POLY_GT3*>(buffer);
  CHECK(frame->primitiveCursor == buffer + sizeof(POLY_GT3));
  CHECK(nextPrim(&ordering[256]) == triangle);
  CHECK(triangle->code == 0x34);
  CHECK(triangle->u0 == 10);
  CHECK(triangle->v1 == 40);
  CHECK(triangle->u2 == 50);
  CHECK(triangle->v2 == 60);
  CHECK(triangle->clut == 0x1234);
  CHECK(triangle->tpage == 0x15);
  for (unsigned flags : {0u, 0x800u}) {
    group.flags = flags;
    ClearOTagR(ordering.data(), static_cast<int>(ordering.size()));
    frame->primitiveCursor = buffer;
    draw(0);
    const std::array<std::int16_t, 6> coordinates{triangle->x0, triangle->y0,
                                                  triangle->x1, triangle->y1,
                                                  triangle->x2, triangle->y2};
    const std::array<std::int16_t, 6> expected =
        flags == 0 ? std::array<std::int16_t, 6>{144, 104, 398, 104, 144, 358}
                   : std::array<std::int16_t, 6>{144, 104, 175, 104, 144, 135};
    CHECK(coordinates == expected);
  }
  group.flags = 0x800;
  VS::Render::SetRoomTextureLodDepth(room_clip, 256);
  for (int bias : {-1, 0, 1}) {
    group.flags =
        0x800u |
        (static_cast<unsigned>(static_cast<std::uint16_t>(bias)) << 16);
    ClearOTagR(ordering.data(), static_cast<int>(ordering.size()));
    frame->primitiveCursor = buffer;
    draw(0);
    CHECK(triangle->u2 == (bias < 0 ? 50 : 25));
    CHECK(triangle->tpage == (bias < 0 ? 0x15 : 0x14));
  }
  group.flags = 0x800;
  frame->primitiveCursor = buffer;
  VS::Render::SetRoomTextureLodDepth(room_clip, 128);
  draw(0);
  CHECK(triangle->u2 == 25);
  CHECK(triangle->v2 == 30);
  CHECK(triangle->tpage == 0x14);
  frame->primitiveCursor = buffer;
  VS::Render::SetRoomTextureLodDepth(room_clip, 512);
  group.flags |= 0x6000;
  draw(1);
  CHECK(frame->primitiveCursor == buffer + 7 * sizeof(POLY_GT3));
  CHECK(triangle->u1 == 20);
  CHECK(triangle->v1 == 30);
  CHECK(triangle->u2 == 30);
  CHECK(triangle->v2 == 40);
  group.flags &= ~0x6000u;
  frame->primitiveCursor = buffer;
  geometry.triangles = 0;
  geometry.quads = 1;
  geometry.polygon[15] = 0x3D;
  draw(0);
  auto* quad = reinterpret_cast<POLY_GT4*>(buffer);
  CHECK(frame->primitiveCursor == buffer + sizeof(POLY_GT4));
  CHECK(quad->u3 == 70);
  CHECK(quad->v3 == 80);
  frame->primitiveCursor = buffer;
  group.flags |= 0x6000;
  draw(1);
  CHECK(frame->primitiveCursor ==
        buffer + 4 * sizeof(POLY_GT4) + 4 * sizeof(POLY_GT3));
  CHECK(quad->u1 == 20);
  CHECK(quad->v1 == 30);
  CHECK(quad->u2 == 30);
  CHECK(quad->v2 == 40);
  CHECK(quad->u3 == 40);
  CHECK(quad->v3 == 50);
  frame->primitiveCursor = buffer + 0x20000 - 4;
  draw(0);
  CHECK(frame->primitiveCursor == buffer + 0x20000 - 4);
  CHECK(memory.front() == 0xA5);
  CHECK(memory.back() == 0xA5);
  struct FarDepthCase {
    int requested;
    int signed_halfword;
  };
  constexpr std::array kFarDepths{
      FarDepthCase{255, 255},     FarDepthCase{256, 256},
      FarDepthCase{257, 257},     FarDepthCase{258, 258},
      FarDepthCase{-1, -1},       FarDepthCase{0x10100, 256},
      FarDepthCase{0x10101, 257}, FarDepthCase{0x8000, -32768},
      FarDepthCase{32767, 32767}};
  for (bool is_quad : {false, true}) {
    geometry.triangles = is_quad ? 0 : 1;
    geometry.quads = is_quad ? 1 : 0;
    geometry.polygon[15] = is_quad ? 0x3D : 0x35;
    for (int mode : {0, 1}) {
      const std::size_t emitted =
          mode == 0 ? (is_quad ? sizeof(POLY_GT4) : sizeof(POLY_GT3))
                    : (is_quad ? 4 * sizeof(POLY_GT4) + 4 * sizeof(POLY_GT3)
                               : 7 * sizeof(POLY_GT3));
      for (int bias : {-1, 0, 1}) {
        group.flags =
            0x800u | (mode ? 0x6000u : 0) |
            (static_cast<unsigned>(static_cast<std::uint16_t>(bias)) << 16);
        for (const auto& cutoff : kFarDepths) {
          ClearOTagR(ordering.data(), static_cast<int>(ordering.size()));
          frame->primitiveCursor = buffer;
          VS::Render::SetRoomPolygonFarClip(room_clip, cutoff.requested);
          draw(mode);
          const std::size_t bytes =
              256 + bias < cutoff.signed_halfword ? emitted : 0;
          CHECK(frame->primitiveCursor == buffer + bytes);
        }
      }
      group.flags = 0x800u | (mode ? 0x6000u : 0) | (2047u << 16);
      ClearOTagR(ordering.data(), static_cast<int>(ordering.size()));
      frame->primitiveCursor = buffer;
      VS::Render::SetRoomPolygonFarClip(room_clip, 32767);
      draw(mode);
      CHECK(frame->primitiveCursor == buffer);
    }
  }
  VS::Render::SetRoomPolygonFarClip(room_clip, 2047);
  group.view.translation.y = 480;
  for (bool is_quad : {false, true}) {
    geometry.triangles = is_quad ? 0 : 1;
    geometry.quads = is_quad ? 1 : 0;
    geometry.polygon[15] = is_quad ? 0x3D : 0x35;
    for (int mode : {0, 1}) {
      group.flags = 0x800u | (mode ? 0x6000u : 0);
      ClearOTagR(ordering.data(), static_cast<int>(ordering.size()));
      frame->primitiveCursor = buffer;
      draw(mode);
      CHECK(frame->primitiveCursor == buffer);
    }
  }
  group.view.translation.y = 0;
  enum class ClipResult { kHidden, kOverlap, kFull };
  struct ClipCase {
    int left;
    int top;
    int right;
    int bottom;
    ClipResult expected;
  };
  constexpr std::array kClipCases{
      ClipCase{0, 0, 320, 224, ClipResult::kFull},
      ClipCase{144, 104, 176, 136, ClipResult::kFull},
      ClipCase{175, 0, 320, 224, ClipResult::kOverlap},
      ClipCase{176, 0, 320, 224, ClipResult::kHidden},
      ClipCase{0, 0, 144, 224, ClipResult::kHidden},
      ClipCase{0, 0, 145, 224, ClipResult::kOverlap},
      ClipCase{0, 135, 320, 224, ClipResult::kOverlap},
      ClipCase{0, 136, 320, 224, ClipResult::kHidden},
      ClipCase{0, 0, 320, 104, ClipResult::kHidden},
      ClipCase{0, 0, 320, 105, ClipResult::kOverlap},
      ClipCase{0x10090, 0x10068, 0x100B0, 0x10088, ClipResult::kFull},
      ClipCase{0x8000, 0x8000, 32767, 32767, ClipResult::kFull},
      ClipCase{0, 0, 0x8000, 224, ClipResult::kHidden},
      ClipCase{0, 0, 320, 0x8000, ClipResult::kHidden}};
  for (bool is_quad : {false, true}) {
    geometry.triangles = is_quad ? 0 : 1;
    geometry.quads = is_quad ? 1 : 0;
    geometry.polygon[15] = is_quad ? 0x3D : 0x35;
    for (int mode : {0, 1}) {
      group.flags = 0x800u | (mode ? 0x6000u : 0);
      const std::size_t full_bytes =
          mode == 0 ? (is_quad ? sizeof(POLY_GT4) : sizeof(POLY_GT3))
                    : (is_quad ? 4 * sizeof(POLY_GT4) + 4 * sizeof(POLY_GT3)
                               : 7 * sizeof(POLY_GT3));
      for (const auto& clip : kClipCases) {
        VS::Render::SetRoomPolygonScreenBounds(
            room_clip, {clip.left, clip.top, clip.right, clip.bottom});
        ClearOTagR(ordering.data(), static_cast<int>(ordering.size()));
        frame->primitiveCursor = buffer;
        draw(mode);
        if (clip.expected == ClipResult::kFull) {
          CHECK(frame->primitiveCursor == buffer + full_bytes);
        } else if (clip.expected == ClipResult::kHidden) {
          CHECK(frame->primitiveCursor == buffer);
        } else {
          const auto* cursor =
              static_cast<unsigned char*>(frame->primitiveCursor);
          CHECK(cursor > buffer);
          CHECK(cursor <= buffer + full_bytes);
        }
      }
      group.view.translation.y = 480;
      VS::Render::SetRoomPolygonScreenBounds(room_clip, {0, 0, 320, 240});
      ClearOTagR(ordering.data(), static_cast<int>(ordering.size()));
      frame->primitiveCursor = buffer;
      draw(mode);
      CHECK(frame->primitiveCursor != buffer);
      group.view.translation.y = 0;
    }
  }
  VS::Render::SetRoomPolygonScreenBounds(room_clip, {0, 0, 320, 224});
  CHECK(memory.front() == 0xA5);
  CHECK(memory.back() == 0xA5);
  room_clip = saved_clip;
  Render_legacyPrimitiveBuffers[0] = savedBuffer;
  Render_frameBuf = savedFrame;
  *frame = saved;
  Core_Gte_Reset();
}

TEST_CASE("Sky background emits visible colored triangles behind room geometry",
          "[render][room]") {
  struct SkyVertex {
    SVECTOR position{};
    unsigned color = 0;
  };
  struct {
    SkyVertex vertices[34];
    unsigned char triangles[64][4]{};
  } sky{};
  for (auto& vertex : sky.vertices) vertex.position.vz = -100;
  sky.vertices[0] = {{-20, -20, 100, 0}, 0x0000FF};
  sky.vertices[1] = {{20, -20, 100, 0}, 0x00FF00};
  sky.vertices[2] = {{0, 20, 100, 0}, 0xFF0000};
  for (auto& face : sky.triangles) face[0] = face[1] = face[2] = 3;
  sky.triangles[0][0] = 0;
  sky.triangles[0][1] = 1;
  sky.triangles[0][2] = 2;
  std::vector<unsigned char> buffer(0x20000);
  std::array<u_long, 2048> ordering{};
  auto* frame = static_cast<Render_FrameScratch*>(Core_ScratchBase());
  const auto saved = *frame;
  void* savedBuffer = Render_legacyPrimitiveBuffers[0];
  const int savedIndex = Render_frameBuf;
  Render_frameBuf = 0;
  Render_legacyPrimitiveBuffers[0] = buffer.data();
  frame->primitiveCursor = buffer.data();
  frame->orderingTable = ordering.data();
  ClearOTagR(ordering.data(), 2048);
  Core_Gte_Reset();
  static_assert(sizeof sky == VS::Render::kSkyDomeBytes);
  CHECK(VS::Render::DrawLiveSkyDome(&sky, sizeof sky) ==
        VS::Render::RoomDrawStatus::kDrawn);
  CHECK(VS::Render::DrawLiveSkyDome(&sky, sizeof sky - 1) ==
        VS::Render::RoomDrawStatus::kInvalidGeometry);
  auto* triangle = reinterpret_cast<POLY_G3*>(buffer.data());
  CHECK(frame->primitiveCursor == buffer.data() + sizeof(POLY_G3));
  CHECK(nextPrim(&ordering[2047]) == triangle);
  CHECK(triangle->x0 == 140);
  CHECK(triangle->y0 == 100);
  CHECK(triangle->r0 == 255);
  CHECK(triangle->g1 == 255);
  CHECK(triangle->b2 == 255);
  *frame = saved;
  Render_legacyPrimitiveBuffers[0] = savedBuffer;
  Render_frameBuf = savedIndex;
}

TEST_CASE("GPU frame byte counts reject invalid and overflowing dimensions",
          "[render][upload]") {
  using VS::Render::Device;
  CHECK(Device::FrameByteCount(320, 240) == sizeof(VS::u32) * 320u * 240u);
  CHECK_FALSE(Device::FrameByteCount(0, 240).has_value());
  CHECK_FALSE(Device::FrameByteCount(-1, 1).has_value());
  CHECK_FALSE(Device::FrameByteCount(1, -1).has_value());
  CHECK_FALSE(Device::FrameByteCount(32768, 32768).has_value());
  CHECK_FALSE(Device::FrameByteCount(std::numeric_limits<VS::i32>::max(), 1)
                  .has_value());
  CHECK(Device::FrameByteCount(
            static_cast<VS::i32>(UINT32_MAX / sizeof(VS::u32)), 1) ==
        UINT32_MAX - (sizeof(VS::u32) - 1));
  Device device;
  const VS::u32 pixel = 0;
  CHECK_FALSE(device.PresentImage(&pixel, 32768, 32768));
  CHECK_FALSE(device.PresentImage(nullptr, 1, 1));
}
