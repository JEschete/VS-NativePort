#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstring>
#include <span>
#include <type_traits>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Actors/ActorRecord.hpp"
#include "Actors/ModelDrawingLive.hpp"
#include "Actors/ModelRenderingLiveState.hpp"
#include "Actors/Movement.hpp"
#include "Actors/ShpModel.hpp"
#include "Actors/SkeletonLiveState.hpp"
#include "Content/ModuleData.hpp"
#include "Field/ActorCollisionLive.hpp"
#include "Field/Gimmicks.hpp"
#include "Field/Mpd.hpp"
#include "Legacy/Psx.hpp"
#include "Render/Camera.hpp"
#include "Render/Display.hpp"
#include "Render/Frame.hpp"
#include "Render/ModelDraw.hpp"
#include "Render/ModelGeometryLive.hpp"
#include "Render/ModelWorkspace.hpp"
#include "Render/Render.hpp"
#include "Render/UiPrimitives.hpp"

using VS::Core::Gte::MATRIX;
using VS::Core::Gte::SetGeomOffset;
using VS::Core::Gte::SetGeomScreen;
using VS::Core::Gte::SVECTOR;

static_assert(std::is_same_v<decltype(&VS::Render::DrawLiveActorGroundShadow),
                             void (*)(VS::Actors::ActorRecord&, const MATRIX&,
                                      const SVECTOR&)>);

namespace {
struct ActorShadowFixture {
  VS::Render::FrameScratch saved_frame = VS::Render::FrameScratchpad();
  VS::Render::ModelScratchpad saved_workspace = *VS::Render::ModelWorkspace();
  VS::Actors::ModelRenderContext saved_context = VS::Actors::modelRenderContext;
  VS::Actors::SkeletonPose saved_pose = VS::Actors::skeletonPose;
  VS::Field::RoomDataRecord saved_room = VS::Field::roomData;
  int saved_framebuffer = VS::Render::frameBuf;
  void* saved_buffer = VS::Render::legacyPrimitiveBuffers[0];
  std::uint16_t saved_platform = VS::Actors::supportingPlatformLayer;
  VS::Field::CollisionProbe saved_probe = VS::Field::LiveCollisionProbe();
  decltype(VS::Actors::supportingGimmickIndex) saved_gimmick_index =
      VS::Actors::supportingGimmickIndex;
  decltype(VS::Field::gimmickInstances) saved_gimmicks =
      VS::Field::gimmickInstances;
  std::array<unsigned int, 18> saved_vertices{};
  std::array<unsigned int, 24> saved_faces{};
  std::vector<std::uint32_t> storage = std::vector<std::uint32_t>(
      0x20000 / sizeof(std::uint32_t) + 2, 0xDEADBEEF);
  std::array<std::uint32_t, 2048> ordering{};
  std::array<unsigned int, 32> model_bytes{};
  VS::Actors::ActorRecord actor{};
  MATRIX camera{};
  alignas(
      VS::Field::MpdCollisionHeader) std::array<unsigned char, 188> terrain{};
  std::array<VS::Field::MpdTileCell, 9> cells{};

  ActorShadowFixture() {
    std::copy_n(VS::Content::battleShadowVertices, saved_vertices.size(),
                saved_vertices.begin());
    std::copy_n(VS::Content::battleShadowFaces, saved_faces.size(),
                saved_faces.begin());
    SetMesh();
    VS::Field::roomData = {};
    VS::Actors::supportingPlatformLayer = 0;
    VS::Render::frameBuf = 0;
    VS::Render::legacyPrimitiveBuffers[0] = storage.data() + 1;
    VS::Render::FrameScratchpad() = {};
    camera.m[0][0] = 4096;
    camera.m[1][2] = -4096;
    camera.m[2][1] = 4096;
    camera.t[2] = 1024;
    VS::Render::FrameScratchpad().view_matrix = camera;
    *VS::Render::ModelWorkspace() = {};
    VS::Actors::modelRenderContext = {};
    actor.instance.is_visible = -1;
    actor.instance.sound_enabled = 1;
    actor.instance.bone_count = 1;
    actor.instance.vertex_group_count = 1;
    actor.instance.scale_x = actor.instance.scale_y = actor.instance.scale_z =
        actor.scale = 4096;
    actor.shadow_radius = 64;
    actor.shadow_blend.radius_expand_rate =
        actor.shadow_blend.radius_shrink_rate = 8192;
    actor.blend_pose.scale_keys[0] = {64, 64, 64, 0};
    actor.model_data_size = sizeof model_bytes;
    auto* model = reinterpret_cast<VS::Actors::ShpModel*>(model_bytes.data());
    actor.instance.model = model;
    model->group_section = 96;
    model->vertex_section = 100;
    model->polygon_section = 108;
    const std::array<std::uint16_t, 2> group{0, 1};
    std::memcpy(reinterpret_cast<unsigned char*>(model) + 96, group.data(),
                sizeof group);
    VS::Core::Gte::Reset();
    SetGeomOffset(160, 120);
    SetGeomScreen(256);
    ResetPackets();
  }

  ~ActorShadowFixture() {
    std::copy(saved_vertices.begin(), saved_vertices.end(),
              VS::Content::battleShadowVertices);
    std::copy(saved_faces.begin(), saved_faces.end(),
              VS::Content::battleShadowFaces);
    VS::Field::roomData = saved_room;
    VS::Actors::supportingPlatformLayer = saved_platform;
    VS::Actors::supportingGimmickIndex = saved_gimmick_index;
    VS::Field::LiveCollisionProbe() = saved_probe;
    std::copy(saved_gimmicks.begin(), saved_gimmicks.end(),
              VS::Field::gimmickInstances.begin());
    VS::Render::FrameScratchpad() = saved_frame;
    *VS::Render::ModelWorkspace() = saved_workspace;
    VS::Actors::modelRenderContext = saved_context;
    VS::Actors::skeletonPose = saved_pose;
    VS::Render::frameBuf = saved_framebuffer;
    VS::Render::legacyPrimitiveBuffers[0] = saved_buffer;
    VS::Core::Gte::Reset();
  }

  static void SetMesh() {
    constexpr std::array<int, 8> kX{64, 45, 0, -45, -64, -45, 0, 45};
    constexpr std::array<int, 8> kZ{0, 45, 64, 45, 0, -45, -64, -45};
    std::fill_n(VS::Content::battleShadowVertices, 18, 0);
    const auto face = [](unsigned a, unsigned b, unsigned c) {
      return a * 4 | (b * 4 << 8) | (c * 4 << 16);
    };
    for (unsigned index = 0; index < 8; ++index) {
      const unsigned next = (index + 1) % 8;
      for (unsigned ring = 0; ring < 2; ++ring) {
        const int scale = static_cast<int>(ring + 1);
        VS::Content::battleShadowVertices[1 + ring * 8 + index] =
            static_cast<std::uint16_t>(kX[index] * scale) |
            (static_cast<unsigned>(
                 static_cast<std::uint16_t>(kZ[index] * scale))
             << 16);
      }
      VS::Content::battleShadowFaces[index] = face(0, 1 + index, 1 + next);
      VS::Content::battleShadowFaces[8 + index] =
          face(1 + index, 9 + index, 1 + next);
      VS::Content::battleShadowFaces[16 + index] =
          face(1 + next, 9 + index, 9 + next);
    }
  }

  void ResetPackets() {
    std::fill(ordering.begin(), ordering.end(), 0x00FFFFFF);
    std::fill(storage.begin(), storage.end(), 0xDEADBEEF);
    VS::Render::FrameScratchpad().ordering_table = ordering.data();
    VS::Render::FrameScratchpad().primitive_cursor = storage.data() + 1;
  }

  std::size_t PacketCount() const {
    return (static_cast<std::uint32_t*>(
                VS::Render::FrameScratchpad().primitive_cursor) -
            (storage.data() + 1)) /
           8;
  }

  const std::uint32_t* Packet(unsigned index) const {
    return storage.data() + 1 + std::size_t{index} * 8;
  }

  void Draw() {
    VS::Render::DrawLiveActorGroundShadow(
        actor, camera, VS::Render::ModelWorkspace()->root_offset);
  }

  void SetTerrain(bool along_z, bool steep) {
    const VS::Field::MpdCollisionHeader header{
        3, 3, static_cast<std::int16_t>(steep ? 64 : 32), 9};
    std::memcpy(terrain.data(), &header, sizeof header);
    for (unsigned tile = 0; tile < 9; ++tile) {
      const auto descriptor = static_cast<std::uint16_t>(tile);
      for (unsigned layer = 0; layer < 2; ++layer) {
        std::memcpy(terrain.data() + 8 + (std::size_t{layer} * 9 + tile) * 2,
                    &descriptor, sizeof descriptor);
      }
      for (unsigned z = 0; z < 4; ++z) {
        for (unsigned x = 0; x < 4; ++x) {
          const unsigned sample =
              along_z ? (tile / 3) * 4 + z : (tile % 3) * 4 + x;
          terrain[44 + tile * 16 + z * 4 + x] =
              static_cast<unsigned char>(8 + sample * (steep ? 4 : 1));
        }
      }
    }
    VS::Field::roomData.collision_map =
        reinterpret_cast<VS::Field::MpdCollisionHeader*>(terrain.data());
    VS::Field::roomData.tile_cells = cells.data();
    VS::Field::roomData.header.collision_map_length = terrain.size();
    VS::Field::roomData.header.tile_cells_length = sizeof cells;
    std::fill_n(VS::Field::gimmickInstances.begin(), saved_gimmicks.size(),
                nullptr);
    const std::int16_t coordinate = steep ? 176 : 160;
    actor.instance.position = {coordinate, 0, coordinate, 0};
    actor.shadow_radius = steep ? 32 : 64;
    VS::Render::ModelWorkspace()->root_offset = {
        coordinate, static_cast<std::int16_t>(steep ? 64 : 56), coordinate, 0};
    camera.t[0] = -coordinate;
    camera.t[1] = coordinate;
  }
};

void CheckShadowBands(const ActorShadowFixture& fixture, unsigned light) {
  const unsigned half = (light / 2) * 0x010101;
  const std::array<unsigned, 9> expected{0x32000000 | light * 0x010101,
                                         half,
                                         half,
                                         0x32000000 | half,
                                         0,
                                         0,
                                         0x32000000,
                                         0x32000000 | half,
                                         0x32000000 | half};
  std::array<unsigned, 9> actual{};
  for (unsigned band = 0; band < 3; ++band) {
    for (unsigned corner = 0; corner < 3; ++corner) {
      actual[band * 3 + corner] = fixture.Packet(band * 8)[2 + corner * 2];
    }
  }
  CHECK(actual == expected);
}

int FloorDivide(int value, int divisor) {
  return value / divisor - (value < 0 && value % divisor != 0);
}

std::int16_t ReferenceScale(unsigned radius, int height, std::int16_t model,
                            std::uint16_t blend) {
  const unsigned extent = static_cast<unsigned>(
      std::max(20, static_cast<int>(radius) + FloorDivide(height, 8)));
  const auto multiply_low = [](std::uint32_t first, std::uint32_t second) {
    return static_cast<std::uint32_t>(std::uint64_t{first} * second);
  };
  const std::uint32_t sign_extended =
      blend < 0x8000 ? blend : 0xFFFF0000u | blend;
  const auto first =
      multiply_low(extent * 128, static_cast<std::uint16_t>(model)) / 4096;
  const auto second = multiply_low(first, sign_extended / 2) / 4096;
  return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(second));
}
struct ShadowScaleCase {
  unsigned radius;
  int height;
  std::int16_t model;
  std::uint16_t blend;
};

void CheckScaleCase(ActorShadowFixture& fixture, const ShadowScaleCase& input) {
  fixture.ResetPackets();
  fixture.actor.shadow_radius = static_cast<std::uint16_t>(input.radius);
  fixture.actor.instance.scale_x = fixture.actor.instance.scale_y = input.model;
  fixture.actor.shadow_blend.radius_expand_rate =
      fixture.actor.shadow_blend.radius_shrink_rate = input.blend;
  VS::Render::ModelWorkspace()->root_offset.vy =
      static_cast<std::int16_t>(input.height);
  fixture.Draw();
  REQUIRE(fixture.PacketCount() == 24);
  const auto scale =
      ReferenceScale(input.radius, input.height, input.model, input.blend);
  const auto x = 160 + FloorDivide(FloorDivide(scale * 64, 4096), 4);
  CHECK(static_cast<std::int16_t>(fixture.Packet(0)[5]) == x);
}
}  // namespace

TEST_CASE("Actor drawing submits the original gated ground-shadow pass",
          "[render][actor-shadow]") {
  ActorShadowFixture fixture;
  const bool body_visible = GENERATE(false, true);
  if (!body_visible) {
    // Hide only the body vertex behind the camera, not the actor's ground.
    const SVECTOR vertex{0, -2048, 0, 0};
    std::memcpy(
        reinterpret_cast<unsigned char*>(fixture.model_bytes.data()) + 100,
        &vertex, sizeof vertex);
  }
  VS::Actors::DrawLiveActorModel(fixture.actor, fixture.camera, 0);
  CHECK(fixture.PacketCount() == 24);
  CHECK(static_cast<bool>(fixture.actor.instance.is_culled) != body_visible);
  CHECK(fixture.actor.sound_position.vx == 160);
  CHECK(fixture.actor.sound_position.vy == 120);
  CHECK(fixture.actor.sound_position.vz == 1024);
  CHECK(fixture.storage.front() == 0xDEADBEEF);
  CHECK(fixture.storage.back() == 0xDEADBEEF);
}

TEST_CASE(
    "Actor shadow production gates preserve hidden and companion behavior",
    "[render][actor-shadow]") {
  ActorShadowFixture fixture;
  const int gate = GENERATE(0, 1, 2, 3);
  char companion = 0;
  if (gate == 0) fixture.actor.instance.sound_enabled = 0;
  if (gate == 1) companion = 1;
  if (gate == 2) fixture.actor.instance.is_visible = 0;
  if (gate == 3) fixture.actor.instance.position.vy = 256;
  VS::Actors::DrawLiveActorModel(fixture.actor, fixture.camera, companion);
  CHECK(fixture.PacketCount() == 0);
  CHECK(fixture.actor.sound_position.vz == 0);
}

TEST_CASE(
    "Actor shadow packets retain subtractive mode bands and nearest depth",
    "[render][actor-shadow]") {
  ActorShadowFixture fixture;
  fixture.actor.shadow_blend.color = 0x808080;
  fixture.Draw();
  REQUIRE(fixture.PacketCount() == 24);
  CheckShadowBands(fixture, 192);
  for (unsigned face = 0; face < 24; ++face) {
    const auto* packet = fixture.Packet(face);
    CHECK(packet[0] >> 24 == 7);
    CHECK(packet[1] == 0xE1000640);
    CHECK(packet[2] >> 24 == 0x32);
  }
  CHECK((fixture.Packet(0)[0] & 0xFFFFFF) == 0xFFFFFF);
  for (unsigned bucket = 0; bucket < fixture.ordering.size(); ++bucket) {
    if (bucket == 254) {
      CHECK(fixture.ordering[bucket] != 0xFFFFFF);
    } else {
      CHECK(fixture.ordering[bucket] == 0xFFFFFF);
    }
  }
  CHECK(fixture.storage[1 + 24 * 8] == 0xDEADBEEF);
}

TEST_CASE(
    "Actor shadow color retains signed fade and the full upper color word",
    "[render][actor-shadow]") {
  ActorShadowFixture fixture;
  for (const std::uint32_t rgb :
       {0u, 0x808080u, 0xFFFFFFu, 0x01204060u, 0xFF0A0B0Cu}) {
    for (unsigned fade = 0; fade <= UINT8_MAX; ++fade) {
      fixture.ResetPackets();
      fixture.actor.shadow_blend.color = std::bit_cast<std::int32_t>(rgb);
      fixture.actor.fade_red = static_cast<std::uint8_t>(fade);
      const int signed_fade =
          fade < 128 ? static_cast<int>(fade) : static_cast<int>(fade) - 256;
      const auto luminance =
          ((rgb % 256) + ((rgb / 256) % 256) * 2 + rgb / 65536) / 4;
      const int light = std::clamp(
          static_cast<int>(luminance) + 64 - signed_fade * 4, 0, 255);
      fixture.Draw();
      REQUIRE(fixture.PacketCount() == 24);
      CheckShadowBands(fixture, static_cast<unsigned>(light));
    }
  }
}

TEST_CASE(
    "Actor shadow scale keeps original low-word products and halfword inputs",
    "[render][actor-shadow]") {
  ActorShadowFixture fixture;
  for (const auto radius : {0, 19, 20, 64, 65535}) {
    for (const auto height : {-32768, 0, 80, 32767}) {
      for (const auto model : {-1, 4096, 32767}) {
        for (const auto blend : {0, 4096, 8192, 32767, 32768, 65535}) {
          CheckScaleCase(fixture, {static_cast<unsigned>(radius), height,
                                   static_cast<std::int16_t>(model),
                                   static_cast<std::uint16_t>(blend)});
        }
      }
    }
  }
}

TEST_CASE(
    "Actor shadow winding and depth cutoffs preserve the projected anchor",
    "[render][actor-shadow]") {
  ActorShadowFixture fixture;
  const int depth = GENERATE(-1, 0, 7, 8, 9, 7943, 7944, 65535, 65536);
  fixture.camera.t[2] = depth;
  fixture.Draw();
  CHECK(fixture.PacketCount() == (depth >= 8 && depth < 7944 ? 24 : 0));
  CHECK(static_cast<std::uint16_t>(fixture.actor.sound_position.vz) ==
        std::clamp(depth, 0, 65535));
  CHECK(fixture.actor.sound_position.pad == 0);
}

TEST_CASE("Actor shadow buffer exhaustion cannot overwrite adjacent storage",
          "[render][actor-shadow]") {
  ActorShadowFixture fixture;
  VS::Render::FrameScratchpad().primitive_cursor =
      fixture.storage.data() + 1 + 32768 - 8;
  fixture.Draw();
  CHECK(VS::Render::FrameScratchpad().primitive_cursor ==
        fixture.storage.data() + 1 + 32768);
  CHECK(fixture.storage.back() == 0xDEADBEEF);
  CHECK(fixture.storage.front() == 0xDEADBEEF);
}

TEST_CASE(
    "Actor shadow native bindings expose mesh strides and no C draw entry",
    "[render][actor-shadow][redirect-binding]") {
  ActorShadowFixture fixture;
  const auto symbols = VS::Content::ModuleSymbols();
  // F01268 is typed C++ now; no module-image caller reaches 800B4B14.
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x800B4B14u) == nullptr);
  fixture.Draw();
  CHECK(fixture.PacketCount() == 24);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x800E9A68u) ==
        VS::Content::battleShadowVertices);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x800E9A6Cu) ==
        VS::Content::battleShadowVertices + 1);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x800E9AD8u) ==
        VS::Content::battleShadowFaces + 1);
}

TEST_CASE("Actor shadows follow actual interpolated terrain on both axes",
          "[render][actor-shadow]") {
  ActorShadowFixture fixture;
  const bool along_z = GENERATE(false, true);
  fixture.SetTerrain(along_z, false);
  CHECK(VS::Field::LiveSampleActorSupportHeight(
            &fixture.actor.instance.position, 0, 0, 3) == 56);
  fixture.Draw();
  REQUIRE(fixture.PacketCount() == 24);
  CHECK(fixture.actor.sound_position.vz == 1080);
  CHECK(static_cast<std::int16_t>(fixture.Packet(0)[5]) ==
        (along_z ? 190 : 188));
  CHECK(static_cast<std::int16_t>(fixture.Packet(0)[7]) ==
        (along_z ? 182 : 179));
  CHECK(static_cast<std::int16_t>(fixture.Packet(0)[7] >> 16) ==
        (along_z ? 100 : 97));
}

TEST_CASE("Actor shadows flatten source-rejected steep terrain",
          "[render][actor-shadow]") {
  ActorShadowFixture fixture;
  const bool along_z = GENERATE(false, true);
  fixture.SetTerrain(along_z, true);
  CHECK(VS::Field::LiveSampleActorSupportHeight(
            &fixture.actor.instance.position, 0, 0, 3) == 64);
  fixture.Draw();
  REQUIRE(fixture.PacketCount() == 24);
  CHECK(fixture.actor.sound_position.vz == 1088);
  CHECK(static_cast<std::int16_t>(fixture.Packet(0)[5]) == 175);
  CHECK(static_cast<std::int16_t>(fixture.Packet(0)[7]) == 170);
  CHECK(static_cast<std::int16_t>(fixture.Packet(0)[7] >> 16) == 109);
}

TEST_CASE("Actor shadows reject reversed winding but retain the sound anchor",
          "[render][actor-shadow]") {
  ActorShadowFixture fixture;
  for (auto& face : VS::Content::battleShadowFaces) {
    face = (face & 0xFF0000FFu) | ((face & 0xFF00u) << 8) |
           ((face & 0xFF0000u) >> 8);
  }
  fixture.Draw();
  CHECK(fixture.PacketCount() == 0);
  CHECK(fixture.actor.sound_position.vx == 160);
  CHECK(fixture.actor.sound_position.vy == 120);
  CHECK(fixture.actor.sound_position.vz == 1024);
  CHECK(fixture.storage[1] == 0xDEADBEEF);
}
