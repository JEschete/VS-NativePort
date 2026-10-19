#include <array>
#include <cstring>
#include <limits>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Legacy/Render/LibGpu.h"
#include "Render/Device.hpp"
#include "Render/PageSpace.hpp"
#include "Render/RoomDraw.h"

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
    Render_RoomGeometryGroup group{};
    unsigned triangles = 1;
    unsigned quads = 0;
    unsigned char polygon[32]{};
  } geometry;
  geometry.group.polygonOffset = 4 + sizeof geometry.group;
  REQUIRE(Render_ValidateRoomGeometry(&geometry, sizeof geometry));
  CHECK_FALSE(Render_ValidateRoomGeometry(&geometry, sizeof geometry - 1));
  geometry.triangles = 0xFFFFFFFFu;
  CHECK_FALSE(Render_ValidateRoomGeometry(&geometry, sizeof geometry));
  geometry.triangles = 1;
  geometry.group.polygonOffset = 0;
  CHECK_FALSE(Render_ValidateRoomGeometry(&geometry, sizeof geometry));
  CHECK_FALSE(Render_ValidateRoomGeometry(nullptr, 0));
}

TEST_CASE("Room transforms use native camera layout and camera-facing groups",
          "[render][room]") {
  struct {
    unsigned count = 2;
    Render_RoomGeometryGroup groups[2]{};
  } geometry;
  auto* frame = static_cast<Render_FrameScratch*>(Core_ScratchBase());
  const auto saved = *frame;
  frame->viewMatrix = {};
  for (int axis = 0; axis < 3; ++axis) {
    frame->viewMatrix.m[axis][axis] = 4096;
    frame->viewMatrix.t[axis] = 10 * (axis + 1);
    geometry.groups[0].bounds[axis][0] = static_cast<short>(100 * (axis + 1));
  }
  frame->camera.farClip = 4096;
  frame->camera.angles.vz = 1024;
  geometry.groups[1].flags = 0x400;
  Render_UpdateRoomGroupTransforms(&geometry);
  CHECK(geometry.groups[0].view.t[0] == 110);
  CHECK(geometry.groups[0].view.t[1] == 220);
  CHECK(geometry.groups[0].view.t[2] == 330);
  CHECK(geometry.groups[0].view.m[0][0] == 4096);
  CHECK(geometry.groups[1].view.m[0][0] == 0);
  CHECK(geometry.groups[1].view.m[0][1] == -4096);
  CHECK(geometry.groups[1].view.m[1][0] == 4096);
  CHECK(geometry.groups[1].view.m[2][2] == 4096);
  frame->camera.farClip = 8192;
  for (int axis = 0; axis < 3; ++axis) {
    frame->viewMatrix.m[axis][axis] = 8192;
  }
  VECTOR position{100, 200, 300, 0}, angles{};
  MATRIX output{};
  Render_BuildScaleCompensatedRoomView(&position, &angles, &output);
  CHECK(output.m[0][0] == 4096);
  CHECK(output.t[0] == 110);
  *frame = saved;
}

TEST_CASE("Room polygons preserve UV order and bound native frame packets",
          "[render][room]") {
  struct {
    unsigned count = 1;
    Render_RoomGeometryGroup group{};
    unsigned triangles = 1;
    unsigned quads = 0;
    unsigned char polygon[40]{};
  } geometry;
  geometry.group.flags = 0x800;
  geometry.group.polygonOffset = 4 + sizeof geometry.group;
  for (int axis = 0; axis < 3; ++axis) geometry.group.view.m[axis][axis] = 4096;
  geometry.group.view.t[2] = 1024;
  const short anchor[3]{-64, -64, 0};
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
  Render_SetRoomPolygonNearClip(8);
  Render_SetRoomTextureLodDepth(512);
  Render_DrawRoomGeometry(&geometry, 0);
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
  frame->primitiveCursor = buffer;
  Render_SetRoomTextureLodDepth(128);
  Render_DrawRoomGeometry(&geometry, 0);
  CHECK(triangle->u2 == 25);
  CHECK(triangle->v2 == 30);
  CHECK(triangle->tpage == 0x14);
  frame->primitiveCursor = buffer;
  Render_SetRoomTextureLodDepth(512);
  geometry.group.flags |= 0x6000;
  Render_DrawRoomGeometry(&geometry, 1);
  CHECK(frame->primitiveCursor == buffer + 7 * sizeof(POLY_GT3));
  CHECK(triangle->u1 == 20);
  CHECK(triangle->v1 == 30);
  CHECK(triangle->u2 == 30);
  CHECK(triangle->v2 == 40);
  geometry.group.flags &= ~0x6000u;
  frame->primitiveCursor = buffer;
  geometry.triangles = 0;
  geometry.quads = 1;
  geometry.polygon[15] = 0x3D;
  Render_DrawRoomGeometry(&geometry, 0);
  auto* quad = reinterpret_cast<POLY_GT4*>(buffer);
  CHECK(frame->primitiveCursor == buffer + sizeof(POLY_GT4));
  CHECK(quad->u3 == 70);
  CHECK(quad->v3 == 80);
  frame->primitiveCursor = buffer;
  geometry.group.flags |= 0x6000;
  Render_DrawRoomGeometry(&geometry, 1);
  CHECK(frame->primitiveCursor ==
        buffer + 4 * sizeof(POLY_GT4) + 4 * sizeof(POLY_GT3));
  CHECK(quad->u1 == 20);
  CHECK(quad->v1 == 30);
  CHECK(quad->u2 == 30);
  CHECK(quad->v2 == 40);
  CHECK(quad->u3 == 40);
  CHECK(quad->v3 == 50);
  frame->primitiveCursor = buffer + 0x20000 - 4;
  Render_DrawRoomGeometry(&geometry, 0);
  CHECK(frame->primitiveCursor == buffer + 0x20000 - 4);
  CHECK(memory.front() == 0xA5);
  CHECK(memory.back() == 0xA5);
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
  Render_DrawSkyDome(&sky);
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
