#include <array>
#include <cstdint>
#include <cstring>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Battle/SphereTerrain.h"
#include "Legacy/Psx.h"

extern "C" {
#include "Battle/Sphere.h"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"
}

namespace {
constexpr u_long kGuard = 0xA1B2C3D4;

struct SliceFixture {
  Battle_TargetSphereSetupView storage{};
  Battle_TargetSphereSetupView* saved_sphere = Battle_targetSphere;
  Render_FrameScratch saved_frame = Render_frameScratch;
  std::array<u_long, 2050> ordering{};
  std::vector<u_long> packets = std::vector<u_long>(65538, kGuard);
  SVECTOR center{64, -64, 64, 0};
  MATRIX shape{{{256, 0, 0}, {0, 256, 0}, {0, 0, 256}}, {0, 0, 0}};

  SliceFixture() {
    Battle_targetSphere = &storage;
    Battle_InitSphere();
    for (int& value : Sphere().terrainHeights) value = Terrain(0, -1024);
    Camera(0);
  }

  ~SliceFixture() {
    Battle_targetSphere = saved_sphere;
    Render_frameScratch = saved_frame;
    Core_Gte_Reset();
  }

  static int Terrain(int floor, int ceiling) {
    const auto packed = (static_cast<std::uint32_t>(ceiling) << 17) |
                        ((static_cast<std::uint32_t>(floor) & 0x7FFF) << 2) | 1;
    int result = 0;
    std::memcpy(&result, &packed, sizeof result);
    return result;
  }

  Battle_TargetSphere& Sphere() {
    return *reinterpret_cast<Battle_TargetSphere*>(&storage);
  }

  void Camera(int yaw) {
    SVECTOR angles{512, static_cast<std::int16_t>(yaw), 0, 0};
    MATRIX camera{};
    Core_Gte_Reset();
    Core_Gte_RotMatrix(&angles, &camera);
    camera.t[2] = 2048;
    Core_Gte_SetRotMatrix(&camera);
    Core_Gte_SetTransMatrix(&camera);
    Core_Gte_SetGeomOffset(160, 112);
    Core_Gte_SetGeomScreen(256);
    Sphere().cameraYaw = static_cast<std::int16_t>(yaw);
  }

  std::vector<u_long> Draw(int scale = 4096) {
    ordering.fill(0xFFFFFF);
    ordering.front() = ordering.back() = kGuard;
    std::fill(packets.begin(), packets.end(), kGuard);
    Render_frameScratch.orderingTable = ordering.data() + 1;
    Render_frameScratch.primitiveCursor = packets.data() + 1;
    Battle_DrawSphereTerrainIntersection(&center, &shape, scale);
    REQUIRE(ordering.front() == kGuard);
    REQUIRE(ordering.back() == kGuard);
    REQUIRE(packets.front() == kGuard);
    REQUIRE(packets.back() == kGuard);
    auto* cursor = static_cast<u_long*>(Render_frameScratch.primitiveCursor);
    const auto count = cursor - packets.data() - 1;
    REQUIRE(count >= 0);
    REQUIRE(count < 65536);
    REQUIRE(count % 5 == 0);
    CheckLinks(cursor, count);
    std::vector<u_long> geometry;
    for (std::ptrdiff_t index = 1; index <= count; index += 5) {
      geometry.push_back(packets[index + 3]);
      geometry.push_back(packets[index + 4]);
    }
    return geometry;
  }

  void CheckLinks(const u_long* cursor, std::ptrdiff_t count) {
    std::size_t linked = 0;
    for (std::size_t depth = 1; depth + 1 < ordering.size(); ++depth) {
      auto* link = &ordering[depth];
      while ((*link & 0xFFFFFF) != 0xFFFFFF) {
        link = static_cast<u_long*>(nextPrim(link));
        REQUIRE(link >= packets.data() + 1);
        REQUIRE(link + 5 <= cursor);
        CheckScanline(link);
        ++linked;
        REQUIRE(linked <= static_cast<std::size_t>(count / 5));
      }
    }
    REQUIRE(linked == static_cast<std::size_t>(count / 5));
  }

  static void CheckScanline(const u_long* link) {
    REQUIRE(link[0] >> 24 == 4);
    REQUIRE(link[1] == 0xE1000220);
    const int y = static_cast<std::int16_t>(link[3] >> 16);
    REQUIRE(y >= 0);
    REQUIRE(y < 224);
    REQUIRE(link[2] == (0x42000000U | static_cast<unsigned>(y / 2) << 8));
    REQUIRE(link[3] >> 16 == link[4] >> 16);
    REQUIRE(static_cast<std::int16_t>(link[3]) <
            static_cast<std::int16_t>(link[4]));
  }
};

int Coverage(const std::vector<u_long>& lines) {
  int pixels = 0;
  for (std::size_t index = 0; index < lines.size(); index += 2) {
    pixels += static_cast<std::int16_t>(lines[index + 1]) -
              static_cast<std::int16_t>(lines[index]);
  }
  return pixels;
}
}  // namespace

TEST_CASE("Spell elevation slice uses visible native-linked scanlines",
          "[render][sphere][terrain]") {
  SliceFixture fixture;
  for (int yaw : {0, 1024, 2048, 3072}) {
    CAPTURE(yaw);
    fixture.Camera(yaw);
    const auto full = fixture.Draw();
    REQUIRE(Coverage(full) > 300);
    REQUIRE(fixture.Draw() == full);
    const auto half = fixture.Draw(2048);
    REQUIRE(Coverage(half) > 0);
    REQUIRE(Coverage(half) < Coverage(full) / 2);
    REQUIRE(fixture.Draw(0).empty());
    fixture.center.vy -= 32;
    REQUIRE(fixture.Draw() != full);
    fixture.center.vy += 32;
    REQUIRE(fixture.Draw() == full);
  }
}

TEST_CASE("Spell elevation slice respects terrain and diagonal corners",
          "[render][sphere][terrain]") {
  SliceFixture fixture;
  for (int yaw : {0, 1024, 2048, 3072}) {
    CAPTURE(yaw);
    fixture.Camera(yaw);
    const auto full = fixture.Draw();
    fixture.Sphere().terrainHeights[40] = SliceFixture::Terrain(-128, -1024);
    const auto occluded = fixture.Draw();
    REQUIRE(Coverage(occluded) < Coverage(full));
    fixture.Sphere().terrainHeights[40] = SliceFixture::Terrain(0, -1024);
    REQUIRE(fixture.Draw() == full);
    for (unsigned char direction : {6, 12, 18, 24}) {
      CAPTURE(direction);
      fixture.Sphere().terrainRowFlags[40] = direction;
      REQUIRE(Coverage(fixture.Draw()) < Coverage(full));
    }
    fixture.Sphere().terrainRowFlags[40] = 0;
    REQUIRE(fixture.Draw() == full);
  }
  for (int& value : fixture.Sphere().terrainHeights) {
    value = SliceFixture::Terrain(-128, -1024);
  }
  REQUIRE(fixture.Draw().empty());
}

TEST_CASE("Spell elevation clipping stays within screen and depth bounds",
          "[render][sphere][terrain]") {
  SliceFixture fixture;
  MATRIX camera{{{4096, 0, 0}, {0, 2896, -2896}, {0, 2896, 2896}},
                {0, 0, 2048}};
  Core_Gte_SetRotMatrix(&camera);
  Core_Gte_SetTransMatrix(&camera);
  Core_Gte_SetGeomOffset(160, -200);
  REQUIRE(fixture.Draw().empty());
  Core_Gte_SetGeomOffset(160, 400);
  REQUIRE(fixture.Draw().empty());
  Core_Gte_SetGeomOffset(160, 0);
  REQUIRE(!fixture.Draw().empty());
  Core_Gte_SetGeomOffset(160, 224);
  REQUIRE(!fixture.Draw().empty());
  camera.t[2] = 10000;
  Core_Gte_SetTransMatrix(&camera);
  Core_Gte_SetGeomOffset(160, 112);
  REQUIRE(fixture.Draw().empty());
}

TEST_CASE("Spell elevation clipping handles stepped terrain across camera yaw",
          "[render][sphere][terrain]") {
  SliceFixture fixture;
  for (int index = 0; index < 81; ++index) {
    fixture.Sphere().terrainHeights[index] =
        SliceFixture::Terrain(index % 3 == 0 ? -128 : 0, -1024);
    fixture.Sphere().terrainRowFlags[index] =
        index % 7 == 0 ? static_cast<unsigned char>((index % 4 + 1) * 6) : 0;
  }
  for (int yaw = 0; yaw < 4096; yaw += 128) {
    CAPTURE(yaw);
    fixture.Camera(yaw);
    fixture.center.vy = -64;
    fixture.Draw();
    fixture.center.vy = -160;
    fixture.Draw();
  }
}
