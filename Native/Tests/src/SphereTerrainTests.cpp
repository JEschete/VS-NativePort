#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Battle/Ai/PlanningWorkspace.hpp"
#include "Battle/SphereTerrain.h"
#include "Battle/TargetPages.h"
#include "Legacy/Psx.h"

extern "C" {
#include "Battle/Action.h"
#include "Battle/ActionFlow.h"
#include "Battle/ActionResolution.h"
#include "Battle/Sphere.h"
#include "Field/Interactions.h"
#include "Input/Controller.h"
#include "Render/Render.h"
#include "Render/Text/CompactFont.h"
#include "Render/UiPrimitives.h"
}

namespace {
constexpr u_long kGuard = 0xA1B2C3D4;

struct SliceFixture {
  Battle_TargetSphereSetupView storage{};
  Battle_TargetSphereSetupView* saved_sphere = Battle_targetSphere;
  Render_FrameScratch saved_frame = Render_frameScratch;
  u_int saved_control = Field_controlMode;
  u_int saved_action_mode = Battle_actionFlowMode;
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
    Battle_ResetTargetPages();
    Battle_targetSphere = saved_sphere;
    Render_frameScratch = saved_frame;
    Field_controlMode = saved_control;
    Battle_actionFlowMode = saved_action_mode;
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

struct SphereLabelFixture {
  SliceFixture slice;
  std::array<u_char, 92> saved_widths =
      std::to_array(Render_compactGlyphWidths);
  std::array<u_char, 60> saved_indices =
      std::to_array(Render_compactGlyphIndices);
  std::array<u_char, 264> saved_trim =
      std::to_array(Render_compactGlyphLeftTrim);
  std::array<u_long, 2200> ordering{};

  SphereLabelFixture() {
    std::fill_n(Render_compactGlyphWidths, saved_widths.size(), 0x66);
    std::fill_n(Render_compactGlyphIndices, saved_indices.size(), 0);
    std::fill_n(Render_compactGlyphLeftTrim, saved_trim.size(), 0);
    Render_frameScratch = {};
    for (int axis = 0; axis < 3; ++axis) {
      Render_frameScratch.viewMatrix.m[axis][axis] = 4096;
    }
    Render_frameScratch.viewMatrix.t[2] = 2048;
    Render_frameScratch.orderingTable = ordering.data() + 64;
    Render_frameScratch.overlayOrderingTable = ordering.data() + 32;
    Render_frameScratch.primitiveCursor = slice.packets.data() + 1;
    ClearOTagR(ordering.data(), static_cast<int>(ordering.size()));
  }

  ~SphereLabelFixture() {
    std::copy(saved_widths.begin(), saved_widths.end(),
              Render_compactGlyphWidths);
    std::copy(saved_indices.begin(), saved_indices.end(),
              Render_compactGlyphIndices);
    std::copy(saved_trim.begin(), saved_trim.end(),
              Render_compactGlyphLeftTrim);
  }

  void Labels(int count) {
    static char text[] = "TARGET";
    for (int index = 0; index < count; ++index) {
      auto& label = slice.Sphere().labels[index];
      label.state = 1;
      label.depth = 1;
      label.screenX = static_cast<int16_t>(index % 2 ? 248 : -4);
      label.offsetY = static_cast<char>(index % 8 * 11);
      label.ringSize = 20;
      label.worldPosition = {1083, -650, 2008, 0};
      label.text = text;
    }
  }
};

struct ExtendedPageFixture : SliceFixture {
  std::array<Battle_TargetPreview, 40> entries{};

  ExtendedPageFixture() {
    for (std::size_t index = 0; index < entries.size(); ++index) {
      entries[index].actorId = static_cast<char>(2 + index / 5);
      entries[index].limb = static_cast<char>(index % 5);
    }
    Battle_SetSphereTargets(static_cast<int>(entries.size()), entries.data(),
                            0);
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

TEST_CASE("Native action buffers cover every supported actor limb",
          "[combat][targeting][capacity]") {
  constexpr int kActorLimbDomain = 16 * 6;
  CHECK(std::extent_v<decltype(Battle_ActionState::previews)> >=
        kActorLimbDomain);
  CHECK(std::extent_v<decltype(Battle_ActionRequest::targets)> >=
        kActorLimbDomain);
  CHECK(Battle_ValidateTargetPreviewIndex(254) == 254);
  CHECK(Battle_TargetPreviewIndexIsValid(254) == 1);
  CHECK(Battle_TargetPreviewIndexIsValid(255) == 0);
  CHECK(Battle_TargetPreviewIndexIsValid(-1) == 0);
}

TEST_CASE("Extended target pages retain complete actors and absolute indices",
          "[combat][targeting][pages]") {
  ExtendedPageFixture fixture;
  auto first = Battle_GetTargetPageInfo();
  CHECK(first.total == 40);
  CHECK(first.pages == 2);
  CHECK(first.visible == 20);
  CHECK(Battle_MapTargetPageIndex(19) == 19);
  fixture.Sphere().selectedTarget = -1;
  REQUIRE(Battle_AdvanceTargetPage(1) == 1);
  const auto second = Battle_GetTargetPageInfo();
  CHECK(second.first == 20);
  CHECK(second.visible == 20);
  CHECK(fixture.Sphere().targetLists[0] == fixture.entries.data() + 20);
}

TEST_CASE("Minotaur preview index thirty-one stays inside native storage",
          "[combat][targeting][capacity]") {
  using PreviewBuffer = decltype(Battle_ActionState::previews);
  struct GuardedPreviews {
    PreviewBuffer entries{};
    u_long guard = kGuard;
  } storage;
  REQUIRE(std::size(storage.entries) > 31);
  const int index = Battle_ValidateTargetPreviewIndex(31);
  storage.entries[index].position = {1531, -308, 1367, 0};
  CHECK(storage.entries[index].position.vx == 1531);
  CHECK(storage.entries[index].position.vy == -308);
  CHECK(storage.entries[index].position.vz == 1367);
  CHECK(storage.guard == kGuard);
}

TEST_CASE("Six-part target groups retain every candidate beyond one page",
          "[combat][targeting][pages]") {
  SliceFixture fixture;
  std::array<Battle_TargetPreview, 36> entries{};
  for (std::size_t index = 0; index < entries.size(); ++index) {
    entries[index].actorId = static_cast<char>(2 + index / 6);
    entries[index].limb = static_cast<char>(index % 6);
  }
  Battle_SetSphereTargets(static_cast<int>(entries.size()), entries.data(), 0);
  CHECK(Battle_GetTargetPageInfo().total == 36);
  CHECK(Battle_GetTargetPageInfo().visible == 24);
  REQUIRE(Battle_AdvanceTargetPage(1) == 1);
  CHECK(Battle_GetTargetPageInfo().first == 24);
  CHECK(Battle_GetTargetPageInfo().visible == 12);
  CHECK(Battle_MapTargetPageIndex(7) == 31);
  CHECK(Battle_MapTargetPageIndex(11) == 35);
}

TEST_CASE(
    "Extended pages map absolute indices and reverse without losing targets",
    "[combat][targeting][pages]") {
  ExtendedPageFixture fixture;
  REQUIRE(Battle_AdvanceTargetPage(1) == 1);
  CHECK(Battle_MapTargetPageIndex(0) == 20);
  CHECK(Battle_MapTargetPageIndex(19) == 39);
  CHECK(Battle_MapTargetPageIndex(255) == 255);
  fixture.Sphere().selectedTarget = -1;
  REQUIRE(Battle_AdvanceTargetPage(-1) == 1);
  CHECK(Battle_GetTargetPageInfo().first == 0);
}

TEST_CASE("Native action result clearing covers the entire native capacity",
          "[combat][targeting][capacity]") {
  struct GuardedRequest {
    u_long before;
    Battle_ActionRequest request;
    u_long after;
  } storage{};
  storage.before = storage.after = kGuard;
  std::memset(&storage.request, 0xA5, sizeof storage.request);
  Battle_ClearActionResult(&storage.request);
  std::array<unsigned char, sizeof(Battle_ActionRequest)> cleared{};
  CHECK(std::memcmp(&storage.request, cleared.data(), cleared.size()) == 0);
  CHECK(storage.before == kGuard);
  CHECK(storage.after == kGuard);
}

TEST_CASE("AI planning result clearing preserves every adjacent native buffer",
          "[combat][targeting][capacity][ai]") {
  VS::Game::ai::PlanningWorkspace workspace;
  std::memset(&workspace.result, 0xA5, sizeof workspace.result);
  std::memset(workspace.candidates, 0xA5, sizeof workspace.candidates);
  std::memset(workspace.exclusions, 0xA5, sizeof workspace.exclusions);
  std::memset(workspace.fallback, 0xA5, sizeof workspace.fallback);
  Battle_ClearActionResult(&workspace.result);
  std::array<unsigned char, sizeof(Battle_ActionRequest)> cleared{};
  CHECK(std::memcmp(&workspace.result, cleared.data(), cleared.size()) == 0);
  const auto untouched = [](const auto& buffer) {
    const auto* bytes = reinterpret_cast<const unsigned char*>(&buffer);
    return std::all_of(bytes, bytes + sizeof buffer,
                       [](unsigned char value) { return value == 0xA5; });
  };
  CHECK(untouched(workspace.candidates));
  CHECK(untouched(workspace.exclusions));
  CHECK(untouched(workspace.fallback));
}

TEST_CASE("Small target lists preserve the original single visible page",
          "[combat][targeting][pages]") {
  SliceFixture fixture;
  std::array<Battle_TargetPreview, 24> entries{};
  for (std::size_t index = 0; index < entries.size(); ++index) {
    entries[index].actorId = static_cast<char>(index / 6);
  }
  Battle_SetSphereTargets(static_cast<int>(entries.size()), entries.data(), 0);
  CHECK(Battle_GetTargetPageInfo().pages == 1);
  CHECK(Battle_GetTargetPageInfo().visible == 24);
  CHECK(Battle_MapTargetPageIndex(23) == 23);
  CHECK(Battle_AdvanceTargetPage(1) == 0);
  Battle_SetSphereTargets(0, nullptr, 0);
  CHECK(Battle_GetTargetPageInfo().total == 0);
  CHECK(Battle_GetTargetPageInfo().selected == -1);
}

TEST_CASE("Paged target lists own shoulders only during target selection",
          "[combat][targeting][pages]") {
  SliceFixture fixture;
  std::array<Battle_TargetPreview, 40> entries{};
  for (std::size_t index = 0; index < entries.size(); ++index) {
    entries[index].actorId = static_cast<char>(index / 5);
  }
  Battle_SetSphereTargets(static_cast<int>(entries.size()), entries.data(), 0);
  Field_controlMode = 2;
  Battle_actionFlowMode = 1;
  CHECK(Battle_TargetPagesUseShoulders() == 1);
  Battle_actionFlowMode = 2;
  CHECK(Battle_TargetPagesUseShoulders() == 0);
  Battle_actionFlowMode = 3;
  CHECK(Battle_TargetPagesUseShoulders() == 1);
  Field_controlMode = 1;
  CHECK(Battle_TargetPagesUseShoulders() == 0);
}

TEST_CASE("Sphere labels and wireframes preserve the native frame cursor",
          "[render][sphere][labels]") {
  const int labels = GENERATE(0, 1, 8, 24);
  const int kind = GENERATE(0, 1, 3, 5, 7);
  SphereLabelFixture fixture;
  fixture.Labels(labels);
  auto& volume = fixture.slice.Sphere().volumes[0];
  volume.kind = static_cast<u_char>(kind);
  volume.scale.extents[0] = volume.scale.extents[2] = 16;
  volume.scale.extents[1] = 24;
  volume.scale.extents[3] = 32;
  Battle_DrawSphere();
  auto* cursor = static_cast<u_long*>(Render_frameScratch.primitiveCursor);
  REQUIRE(cursor >= fixture.slice.packets.data() + 1);
  REQUIRE(cursor < fixture.slice.packets.data() + fixture.slice.packets.size());
  CHECK(Render_frameScratch.orderingTable == fixture.ordering.data() + 64);
  CHECK(Render_frameScratch.overlayOrderingTable ==
        fixture.ordering.data() + 32);
  CHECK(fixture.slice.packets.front() == kGuard);
  CHECK(fixture.slice.packets.back() == kGuard);
}

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

namespace {
struct AreaActionFixture {
  Battle_ActionState storage{};
  Battle_ActionState* saved = Battle_actionState;

  AreaActionFixture() { Battle_actionState = &storage; }
  ~AreaActionFixture() { Battle_actionState = saved; }
};
}  // namespace

TEST_CASE("NPC area filtering does not require a player targeting sphere",
          "[battle][sphere][area-target]") {
  SliceFixture sphere;
  AreaActionFixture action;
  Battle_targetSphere = nullptr;
  action.storage.areaCenter = {64, -32, 96, 0};
  action.storage.areaShape = 2;
  action.storage.areaKind = 3;
  action.storage.areaX = 4;
  action.storage.areaY = 6;
  action.storage.areaZ = 8;
  Battle_FilterAreaActionTargets();
  CHECK(Battle_targetSphere == nullptr);
  CHECK(action.storage.previewCount == 0);
  CHECK(Battle_actionFlowMode == 3);
  const auto& origin = action.storage.areaOrigin;
  const std::array<int, 3> position{origin.vx, origin.vy, origin.vz};
  CHECK(position == std::array{64, -32, 96});
  const std::array<int, 5> shape{action.storage.areaShape,
                                 action.storage.areaKind, action.storage.areaX,
                                 action.storage.areaY, action.storage.areaZ};
  const std::array expected_shape{2, 3, 4, 6, 8};
  CHECK(shape == expected_shape);
}

TEST_CASE("Player secondary sphere updates preserve complete volume state",
          "[battle][sphere][area-target]") {
  SliceFixture sphere;
  Battle_TargetSphereSpec volume{2, 3, 4, 5, 128, -64, 256};
  sphere.storage.primaryVolume.kind = 7;
  sphere.storage.wireShape = 15;
  Battle_SetSecondarySphereVolume(&volume);
  const auto& actual = sphere.storage.secondaryVolume;
  CHECK(actual.kind == 2);
  CHECK(actual.reserved1 == 3);
  CHECK(actual.reserved2 == 4);
  CHECK(actual.reserved3 == 5);
  CHECK(actual.reserved4 == 128);
  CHECK(actual.reserved8 == -64);
  CHECK(actual.reservedC == 256);
  CHECK(sphere.storage.wireShape == 0);
  CHECK(sphere.storage.primaryVolume.kind == 7);
}
