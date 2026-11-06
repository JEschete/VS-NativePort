#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Content/ModuleData.h"
#include "Field/Background.hpp"
#include "Field/BackgroundLive.hpp"
#include "Field/Environment.hpp"
#include "Legacy/Psx.h"
#include "TestSupport/ChannelWarnings.hpp"

extern "C" {
#include "Field/BackgroundLiveState.h"
}

// D_80068C1C, loaded from BATTLE.PRG by Content.
extern "C" unsigned int Content_battleSkyDome[166];

namespace {
namespace fx = VS::Field;
using Calls = std::vector<std::string>;
using VS::test_support::ChannelWarnings;

constexpr std::int32_t kOne = 4096;

template <typename Id>
std::uintptr_t Raw(Id id) {
  return static_cast<std::uintptr_t>(id);
}

// The ids name only kNone; the fakes hand out opaque values for the rest.
constexpr fx::BackgroundReadId ReadId(std::uintptr_t id) {
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  return static_cast<fx::BackgroundReadId>(id);
}

constexpr fx::BackgroundBufferId BufferId(std::uintptr_t id) {
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  return static_cast<fx::BackgroundBufferId>(id);
}

constexpr fx::WeatherParticlesId ParticlesId(std::uintptr_t id) {
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  return static_cast<fx::WeatherParticlesId>(id);
}

// Catch2's REQUIRE throws, but clang-tidy cannot see that; the explicit throw
// lets bugprone-unchecked-optional-access prove the accesses through it.
template <typename T>
T& Required(std::optional<T>& value) {
  REQUIRE(value.has_value());
  if (!value) {
    throw std::logic_error("REQUIRE did not stop the test");
  }
  return *value;
}

struct DomeDraw {
  fx::CameraVector position;
  fx::CameraVector angles;
  bool scale_compensated = false;

  friend bool operator==(const DomeDraw&, const DomeDraw&) = default;
};

class FakeBackgroundHost final : public fx::BackgroundHost {
 public:
  [[nodiscard]] std::optional<fx::RoomEnvironment> Environment() override {
    return environment;
  }
  void StoreWeatherArea(const fx::WeatherArea& area) override {
    stored_area = area;
  }
  [[nodiscard]] std::optional<fx::CollisionMapExtent> CollisionMapSize()
      override {
    return map;
  }
  void StoreSkyDomeColors(const fx::SkyDomeColors& colors) override {
    dome = colors;
  }
  [[nodiscard]] fx::ViewCamera FrameCamera() override { return camera; }
  [[nodiscard]] fx::RoomTint ActiveRoomTint() override { return tint; }
  void AddFlatTexturedQuad(const fx::TexturedQuad& quad) override {
    flat_quads.push_back(quad);
  }
  void AddShadedTexturedQuad(const fx::TexturedQuad& quad) override {
    shaded_quads.push_back(quad);
  }
  void AddFill(const fx::FillRect& fill) override { fills.push_back(fill); }
  void DrawSkyDome(const fx::CameraVector& position,
                   const fx::CameraVector& angles,
                   bool scale_compensated) override {
    dome_draws.push_back({.position = position,
                          .angles = angles,
                          .scale_compensated = scale_compensated});
  }
  [[nodiscard]] fx::CameraVector ReferenceRotation() override {
    return reference_rotation;
  }
  [[nodiscard]] std::optional<fx::BackgroundLayout> Layout(
      fx::BackgroundBufferId /*layout*/) override {
    return layout;
  }
  [[nodiscard]] std::optional<fx::TiledBackgroundSection> TiledBackground()
      override {
    return section;
  }
  [[nodiscard]] bool ReserveTexturePages(int first_page, int page_count,
                                         int owner) override {
    calls.push_back(std::format("ReserveTexturePages {} {} {}", first_page,
                                page_count, owner));
    return reserve_ok;
  }
  void RetireTexturePages(int first_page, int page_count, int owner) override {
    calls.push_back(std::format("RetireTexturePages {} {} {}", first_page,
                                page_count, owner));
  }
  [[nodiscard]] fx::BackgroundBufferId AllocateHighBuffer(
      std::int32_t bytes) override {
    calls.push_back(std::format("AllocateHighBuffer {}", bytes));
    if (refuse_buffer) return fx::BackgroundBufferId::kNone;
    return BufferId(next_buffer++);
  }
  void FreeHighBuffer(fx::BackgroundBufferId buffer) override {
    calls.push_back(std::format("FreeHighBuffer {}", Raw(buffer)));
  }
  [[nodiscard]] fx::BackgroundReadId AllocateRead(
      const fx::BackgroundFile& file) override {
    calls.push_back(std::format("AllocateRead {}", file.lba));
    if (refuse_reads) return fx::BackgroundReadId::kNone;
    return ReadId(static_cast<std::uintptr_t>(file.lba));
  }
  void EnqueueRead(fx::BackgroundReadId read,
                   fx::BackgroundBufferId destination) override {
    calls.push_back(
        std::format("EnqueueRead {} {}", Raw(read), Raw(destination)));
  }
  [[nodiscard]] fx::BackgroundReadId CompleteRefusedRead(
      const fx::BackgroundFile& file,
      fx::BackgroundBufferId destination) override {
    calls.push_back(
        std::format("CompleteRefusedRead {} {}", file.lba, Raw(destination)));
    return ReadId(static_cast<std::uintptr_t>(file.lba) + 1000);
  }
  [[nodiscard]] bool ReadLoaded(fx::BackgroundReadId read) override {
    return std::ranges::find(loaded, read) != loaded.end();
  }
  void ReleaseRead(fx::BackgroundReadId read) override {
    calls.push_back(std::format("ReleaseRead {}", Raw(read)));
  }
  void StampTimId(fx::BackgroundBufferId buffer, std::uint32_t id) override {
    calls.push_back(std::format("StampTimId {} {}", Raw(buffer), id));
  }
  [[nodiscard]] fx::BackgroundTim ParseTim(
      fx::BackgroundBufferId /*buffer*/) override {
    return tim;
  }
  void UploadTimPixels(fx::BackgroundBufferId buffer,
                       const fx::TextureRect& rect) override {
    calls.push_back(std::format("UploadTimPixels {} {} {} {} {}", Raw(buffer),
                                rect.x, rect.y, rect.width, rect.height));
  }
  void UploadTimClut(fx::BackgroundBufferId buffer, int row, int first_color,
                     int count) override {
    calls.push_back(std::format("UploadTimClut {} {} {} {}", Raw(buffer), row,
                                first_color, count));
  }

  std::optional<fx::RoomEnvironment> environment = fx::RoomEnvironment{};
  std::optional<fx::WeatherArea> stored_area;
  std::optional<fx::CollisionMapExtent> map;
  std::optional<fx::SkyDomeColors> dome;
  fx::ViewCamera camera{
      .position = {}, .look_at = {}, .angles = {}, .far_clip = 0x1000};
  fx::RoomTint tint;
  std::vector<fx::TexturedQuad> flat_quads;
  std::vector<fx::TexturedQuad> shaded_quads;
  std::vector<fx::FillRect> fills;
  std::vector<DomeDraw> dome_draws;
  fx::CameraVector reference_rotation{7, 8, 9};
  std::optional<fx::BackgroundLayout> layout;
  std::optional<fx::TiledBackgroundSection> section;
  bool reserve_ok = true;
  bool refuse_buffer = false;
  bool refuse_reads = false;
  std::vector<fx::BackgroundReadId> loaded;
  fx::BackgroundTim tim;
  std::uintptr_t next_buffer = 1;
  Calls calls;
};

fx::TiledBackgroundSection Section() {
  return {.texture_page_count = 3,
          .texture = {.lba = 100, .size = 0x801},
          .layout = {.lba = 200, .size = 0x1000}};
}

fx::BackgroundTile Tile(fx::TextureRect texture) {
  fx::BackgroundTile tile{.texture = texture};
  tile.corners[0] = {.red = 1, .green = 2, .blue = 3, .code = 0};
  tile.corners[1] = {.red = 4, .green = 5, .blue = 6, .code = 1};
  tile.corners[2] = {.red = 7, .green = 8, .blue = 9, .code = 0};
  tile.corners[3] = {.red = 10, .green = 11, .blue = 12, .code = 0};
  return tile;
}

constexpr fx::FillRect kBackdropFill{
    .color = {},
    .position = {.x = 0, .y = 0},
    .width = 320,
    .height = 240,
};

// A room whose background and weather InitializeRoomBackground has reset.
struct InitializedRoom {
  FakeBackgroundHost host;
  fx::RoomBackgroundState state{.tiled_enabled = 1,
                                .texture_read = ReadId(1),
                                .layout_read = ReadId(2),
                                .layout = BufferId(3),
                                .tim_buffer = BufferId(4)};
  fx::RoomWeatherState weather{.ambient_sound_id = 21,
                               .kind = fx::kRainWeather,
                               .variation_enabled = 0,
                               .particles = ParticlesId(5)};

  InitializedRoom() { fx::InitializeRoomBackground(host, state, weather); }
};

// A tiled background with both reads queued and its TIM buffer held.
struct TiledLoad {
  FakeBackgroundHost host;
  fx::RoomBackgroundState state{.texture_page = 0x15,
                                .texture_read = ReadId(100),
                                .layout_read = ReadId(200),
                                .layout = BufferId(2),
                                .tim_buffer = BufferId(1)};

  TiledLoad() {
    host.tim = {
        .pixels =
            fx::TextureRect{.x = 0x47, .y = 0x1C3, .width = 16, .height = 32},
        .has_clut = true};
  }
};
}  // namespace

TEST_CASE("Room background colours spread over the dome's rings",
          "[field-camera][field-background]") {
  FakeBackgroundHost host;
  fx::RoomBackgroundState state;
  const fx::GradientColors colors{0x01112233, 0x445566, 0x778899, 0xAABBCC,
                                  0xDDEEFF};
  fx::SetRoomBackgroundColors(host, state, colors);
  REQUIRE(host.dome.has_value());
  const fx::SkyDomeColors& dome = *host.dome;
  CHECK(dome[0] == 0x30112233U);
  for (std::size_t vertex = 1; vertex < 9; ++vertex) {
    CHECK(dome.at(vertex) == 0x30445566U);
  }
  for (std::size_t vertex = 9; vertex < 25; ++vertex) {
    CHECK(dome.at(vertex) == 0x30778899U);
  }
  for (std::size_t vertex = 25; vertex < 33; ++vertex) {
    CHECK(dome.at(vertex) == 0x30AABBCCU);
  }
  CHECK(dome[33] == 0x30DDEEFFU);
  CHECK(fx::RoomBackgroundColors(state) == colors);
}

TEST_CASE("Room background chooses the solid backdrop for one colour",
          "[field-camera][field-background]") {
  FakeBackgroundHost host;
  fx::RoomBackgroundState state;
  state.offset_x = 5;
  state.offset_y = -5;
  Required(host.environment).background_colors.fill(0x123456);
  fx::InitializeRoomBackgroundColors(host, state);
  CHECK(state.solid == 1);
  CHECK_FALSE(host.dome.has_value());
  CHECK(state.offset_x == 0);
  CHECK(state.offset_y == 0);
  CHECK(state.parallax_x == fx::kUnitParallax);
  CHECK(state.parallax_y == fx::kUnitParallax);
}

TEST_CASE("Room background draws a black room's dome black",
          "[field-camera][field-background]") {
  FakeBackgroundHost host;
  fx::RoomBackgroundState state{.solid = 1};
  Required(host.environment).background_colors.fill(0);
  fx::InitializeRoomBackgroundColors(host, state);
  CHECK(state.solid == 0);
  CHECK(std::ranges::all_of(Required(host.dome), [](std::uint32_t color) {
    return color == 0x30000000U;
  }));
}

TEST_CASE("Room weather area turns into world units",
          "[field-camera][field-background]") {
  FakeBackgroundHost host;
  fx::RoomBackgroundState state;
  host.map = fx::CollisionMapExtent{.width = 10, .height = 6};
  fx::InitializeRoomBackgroundColors(host, state);
  CHECK(host.stored_area ==
        fx::WeatherArea{
            .origin_x = 0, .origin_z = 0, .extent_x = 5120, .extent_z = 3072});
  host.map.reset();
  fx::InitializeRoomBackgroundColors(host, state);
  CHECK(host.stored_area ==
        fx::WeatherArea{
            .origin_x = 0, .origin_z = 0, .extent_x = 4096, .extent_z = 4096});
  Required(host.environment).weather_area = {
      .origin_x = 1, .origin_z = 2, .extent_x = 5, .extent_z = 7};
  fx::InitializeRoomBackgroundColors(host, state);
  CHECK(host.stored_area == fx::WeatherArea{.origin_x = 128,
                                            .origin_z = 256,
                                            .extent_x = 512,
                                            .extent_z = 640});

  host.environment.reset();
  host.stored_area.reset();
  host.dome.reset();
  state.parallax_x = 1;
  fx::InitializeRoomBackgroundColors(host, state);
  CHECK(state.parallax_x == fx::kUnitParallax);
  CHECK_FALSE(host.stored_area.has_value());
  CHECK_FALSE(host.dome.has_value());
}

TEST_CASE("Room background initialization clears the loads",
          "[field-camera][field-background]") {
  const InitializedRoom room;
  CHECK(room.state.texture_read == fx::BackgroundReadId::kNone);
  CHECK(room.state.layout_read == fx::BackgroundReadId::kNone);
  CHECK(room.state.layout == fx::BackgroundBufferId::kNone);
  CHECK(room.state.tim_buffer == fx::BackgroundBufferId::kNone);
  CHECK(room.state.tiled_enabled == 0);
  CHECK(room.host.stored_area.has_value());
  CHECK(room.host.calls.empty());
}

TEST_CASE("Room background initialization resets the weather",
          "[field-camera][field-background]") {
  const InitializedRoom room;
  CHECK(room.weather.particles == fx::WeatherParticlesId::kNone);
  CHECK(room.weather.ambient_sound_id == fx::kNoAmbientSound);
  CHECK(room.weather.kind == fx::kNoWeather);
  CHECK(room.weather.variation_enabled == 1);
}

TEST_CASE("Solid backdrop covers the screen in grey or the room tint",
          "[field-camera][field-background]") {
  FakeBackgroundHost host;
  const fx::RoomBackgroundState state{.solid = 1};
  fx::DrawRoomBackground(host, state, std::nullopt);
  REQUIRE(host.flat_quads.size() == 1);
  const fx::TexturedQuad& quad = host.flat_quads[0];
  CHECK(quad.corners == std::array<fx::ScreenPoint, 4>{
                            {{0, 0}, {320, 0}, {0, 240}, {320, 240}}});
  CHECK(quad.texels == std::array<fx::TexelPoint, 4>{
                           {{248, 248}, {249, 248}, {248, 249}, {249, 249}}});
  CHECK(quad.texture_page == 0x11);
  CHECK(quad.clut == 0x38B1);
  CHECK(quad.colors[0] == fx::PrimitiveColor{128, 128, 128});
  CHECK(host.dome_draws.empty());
  host.tint = {.red = 10, .green = -40, .blue = 127, .is_active = 1};
  fx::DrawRoomBackground(host, state, std::nullopt);
  REQUIRE(host.flat_quads.size() == 2);
  CHECK(host.flat_quads[1].colors[0] == fx::PrimitiveColor{168, 0, 255});
}

TEST_CASE("Sky dome follows the camera at the zoomed far clip",
          "[field-camera][field-background]") {
  FakeBackgroundHost host;
  const fx::RoomBackgroundState state;
  host.camera.position = {100 * kOne, -50 * kOne, (7 * kOne) + 123};
  host.camera.far_clip = 0x2000;
  Required(host.environment).background_rotation = {4, 5, 6};
  fx::DrawRoomBackground(host, state, fx::CameraVector{1, 2, 3});
  fx::DrawRoomBackground(host, state, std::nullopt);
  host.environment.reset();
  host.camera.far_clip = 0x1000;
  fx::DrawRoomBackground(host, state, std::nullopt);
  CHECK(host.dome_draws ==
        std::vector<DomeDraw>{{{200, -100, 14}, {1, 2, 3}, true},
                              {{200, -100, 14}, {4, 5, 6}, true},
                              {{100, -50, 7}, {7, 8, 9}, false}});
  CHECK(host.flat_quads.empty());
}

TEST_CASE("Tiled background refuses an unreadable layout",
          "[field-camera][field-background]") {
  ChannelWarnings warnings{"field.background"};
  FakeBackgroundHost host;
  const fx::RoomBackgroundState state{.tiled_enabled = 1,
                                      .layout = BufferId(1)};
  fx::DrawRoomBackground(host, state, std::nullopt);
  CHECK(host.fills.empty());
  CHECK(host.dome_draws.empty());
  REQUIRE(warnings.messages.size() == 1);
  CHECK(warnings.messages[0] ==
        "DrawRoomBackground: the tiled layout cannot be read; no background "
        "is drawn");

  // Without a layout the tiled flag falls through to the dome.
  const fx::RoomBackgroundState unloaded{.tiled_enabled = 1};
  fx::DrawRoomBackground(host, unloaded, std::nullopt);
  CHECK(host.dome_draws.size() == 1);
}

TEST_CASE("Tiled background draws only a sized layout",
          "[field-camera][field-background]") {
  ChannelWarnings warnings{"field.background"};
  FakeBackgroundHost host;
  const fx::RoomBackgroundState state{.tiled_enabled = 1,
                                      .layout = BufferId(1)};
  const std::vector<fx::BackgroundTile> tiles(std::size_t{32} * 32);
  for (const std::int32_t width : {0, 32}) {
    host.layout =
        fx::BackgroundLayout{.width = width, .height = 1, .tiles = tiles};
    fx::DrawRoomBackground(host, state, std::nullopt);
  }
  CHECK(host.fills.empty());
  CHECK(host.dome_draws.empty());
  host.layout = fx::BackgroundLayout{
      .width = 1, .height = 1, .tiles = std::span(tiles).first(1)};
  fx::DrawRoomBackground(host, state, std::nullopt);
  CHECK(host.fills == std::vector<fx::FillRect>{kBackdropFill});
  CHECK(host.shaded_quads.empty());
  CHECK(warnings.messages.empty());
}

TEST_CASE("Background cells draw across the visible strip",
          "[field-camera][field-background]") {
  FakeBackgroundHost host;
  const std::array<fx::BackgroundTile, 1> tiles{
      Tile({.x = 8, .y = 16, .width = 32, .height = 32})};
  fx::DrawRoomBackgroundTiles(host, {.width = 1, .height = 1, .tiles = tiles},
                              0, 0, 0x10);
  REQUIRE(host.shaded_quads.size() == 15);
  const fx::TexturedQuad& first = host.shaded_quads[0];
  CHECK(first.corners == std::array<fx::ScreenPoint, 4>{
                             {{-64, 0}, {-32, 0}, {-64, 32}, {-32, 32}}});
  CHECK(first.texels ==
        std::array<fx::TexelPoint, 4>{{{8, 16}, {40, 16}, {8, 48}, {40, 48}}});
  CHECK(first.colors == std::array<fx::PrimitiveColor, 4>{
                            {{1, 2, 3}, {4, 5, 6}, {7, 8, 9}, {10, 11, 12}}});
  CHECK(first.texture_page == 0x90);
  CHECK(first.clut == 0x3BB0);
  CHECK(host.shaded_quads[14].corners[0] == fx::ScreenPoint{384, 0});
  CHECK(host.fills == std::vector<fx::FillRect>{kBackdropFill});
}

TEST_CASE("Background cells without a texture are skipped",
          "[field-camera][field-background]") {
  FakeBackgroundHost host;
  const std::array<fx::BackgroundTile, 1> empty{};
  fx::DrawRoomBackgroundTiles(host, {.width = 1, .height = 1, .tiles = empty},
                              0, 0, 0x10);
  CHECK(host.shaded_quads.empty());
  CHECK(host.fills.size() == 1);
}

TEST_CASE("Background cells refuse a layout missing cells",
          "[field-camera][field-background]") {
  ChannelWarnings warnings{"field.background"};
  ChannelWarnings environment_warnings{"field.environment"};
  FakeBackgroundHost host;
  const std::array<fx::BackgroundTile, 3> tiles{
      Tile({.x = 0, .y = 0, .width = 32, .height = 32}),
      Tile({.x = 0, .y = 0, .width = 32, .height = 32}),
      Tile({.x = 0, .y = 0, .width = 32, .height = 32})};
  fx::DrawRoomBackgroundTiles(host, {.width = 2, .height = 2, .tiles = tiles},
                              0, 0, 0);
  CHECK(host.shaded_quads.empty());
  CHECK(host.fills == std::vector<fx::FillRect>{kBackdropFill});
  CHECK(warnings.messages.empty());
  REQUIRE(environment_warnings.messages.size() == 1);
  CHECK(environment_warnings.messages[0] ==
        "DrawRoomBackgroundTiles: a 2x2 layout has 3 cells; only the "
        "backdrop fill is drawn");
}

TEST_CASE("Tiled background queues its texture pages and reads",
          "[field-camera][field-background]") {
  FakeBackgroundHost host;
  host.section = Section();
  fx::RoomBackgroundState state;
  CHECK(fx::QueueTiledBackgroundLoad(host, state, 0x15) == 1);
  CHECK(state.texture_page == 0x15);
  CHECK(state.texture_page_count == 3);
  CHECK(Raw(state.tim_buffer) == 1);
  CHECK(Raw(state.layout) == 2);
  CHECK(Raw(state.texture_read) == 100);
  CHECK(Raw(state.layout_read) == 200);
  CHECK(host.calls ==
        Calls{"ReserveTexturePages 21 3 5", "AllocateHighBuffer 4096",
              "AllocateHighBuffer 4096", "AllocateRead 100", "AllocateRead 200",
              "EnqueueRead 100 1", "EnqueueRead 200 2"});
}

TEST_CASE("Tiled background completes refused reads at once",
          "[field-camera][field-background]") {
  FakeBackgroundHost refused;
  refused.section = Section();
  refused.refuse_reads = true;
  fx::RoomBackgroundState refused_state;
  CHECK(fx::QueueTiledBackgroundLoad(refused, refused_state, 0x15) == 1);
  CHECK(Raw(refused_state.texture_read) == 1100);
  CHECK(Raw(refused_state.layout_read) == 1200);
  CHECK(refused.calls ==
        Calls{"ReserveTexturePages 21 3 5", "AllocateHighBuffer 4096",
              "AllocateHighBuffer 4096", "AllocateRead 100", "AllocateRead 200",
              "CompleteRefusedRead 100 1", "CompleteRefusedRead 200 2"});
}

TEST_CASE("Tiled background load refusals leave no reads",
          "[field-camera][field-background]") {
  FakeBackgroundHost host;
  fx::RoomBackgroundState state{.texture_read = ReadId(1),
                                .layout_read = ReadId(2)};
  CHECK(fx::QueueTiledBackgroundLoad(host, state, 0x15) == 0);
  CHECK(state.texture_read == fx::BackgroundReadId::kNone);
  CHECK(state.layout_read == fx::BackgroundReadId::kNone);
  CHECK(host.calls.empty());

  host.section = Section();
  host.reserve_ok = false;
  state.texture_read = ReadId(1);
  CHECK(fx::QueueTiledBackgroundLoad(host, state, 0x15) == 0);
  CHECK(state.texture_read == fx::BackgroundReadId::kNone);
  CHECK(host.calls == Calls{"ReserveTexturePages 21 3 5"});

  host.calls.clear();
  host.reserve_ok = true;
  host.refuse_buffer = true;
  CHECK(fx::QueueTiledBackgroundLoad(host, state, 0x15) == 0);
  CHECK(host.calls == Calls{"ReserveTexturePages 21 3 5",
                            "AllocateHighBuffer 4096",
                            "AllocateHighBuffer 4096"});
}

TEST_CASE("Tiled background uploads the TIM once its read lands",
          "[field-camera][field-background]") {
  ChannelWarnings warnings{"field.background"};
  TiledLoad load;
  CHECK(fx::FinishTiledBackgroundLoad(load.host, load.state) == 0);
  CHECK(load.host.calls.empty());

  load.host.loaded = {ReadId(100)};
  CHECK(fx::FinishTiledBackgroundLoad(load.host, load.state) == 0);
  CHECK(load.host.calls ==
        Calls{"StampTimId 1 16", "UploadTimPixels 1 327 451 16 32",
              "UploadTimClut 1 14 0 256", "ReleaseRead 100"});
  CHECK(load.state.texture_read == fx::BackgroundReadId::kNone);
  CHECK(warnings.messages.empty());
}

TEST_CASE("Tiled background frees the TIM once the layout lands",
          "[field-camera][field-background]") {
  ChannelWarnings warnings{"field.background"};
  TiledLoad load;
  load.state.texture_read = fx::BackgroundReadId::kNone;
  load.host.loaded = {ReadId(200)};
  CHECK(fx::FinishTiledBackgroundLoad(load.host, load.state) == 1);
  CHECK(load.host.calls == Calls{"FreeHighBuffer 1", "ReleaseRead 200"});
  CHECK(load.state.tim_buffer == fx::BackgroundBufferId::kNone);
  CHECK(load.state.layout_read == fx::BackgroundReadId::kNone);
  CHECK(Raw(load.state.layout) == 2);

  load.host.calls.clear();
  CHECK(fx::FinishTiledBackgroundLoad(load.host, load.state) == 0);
  CHECK(load.host.calls.empty());
  CHECK(warnings.messages.empty());
}

TEST_CASE("Tiled background skips a TIM read without a buffer",
          "[field-camera][field-background]") {
  ChannelWarnings warnings{"field.background"};
  FakeBackgroundHost host;
  fx::RoomBackgroundState state{.texture_read = ReadId(100)};
  host.loaded = {ReadId(100)};
  host.tim = {.pixels = fx::TextureRect{}, .has_clut = true};
  CHECK(fx::FinishTiledBackgroundLoad(host, state) == 0);
  CHECK(host.calls == Calls{"ReleaseRead 100"});
  CHECK(state.texture_read == fx::BackgroundReadId::kNone);
  REQUIRE(warnings.messages.size() == 1);
  CHECK(warnings.messages[0] ==
        "FinishTiledBackgroundLoad: the texture read finished without a TIM "
        "buffer; nothing is uploaded");
}

TEST_CASE("Tiled background release returns its pages once",
          "[field-camera][field-background]") {
  FakeBackgroundHost host;
  fx::RoomBackgroundState state{.texture_page_count = 3,
                                .texture_page = 0x15,
                                .tiled_enabled = 1,
                                .layout = BufferId(2)};
  CHECK(fx::ReleaseTiledBackground(host, state) == 1);
  CHECK(state.tiled_enabled == 0);
  CHECK(state.layout == fx::BackgroundBufferId::kNone);
  CHECK(host.calls == Calls{"RetireTexturePages 21 3 5", "FreeHighBuffer 2"});
  host.calls.clear();
  CHECK(fx::ReleaseTiledBackground(host, state) == 0);
  CHECK(host.calls.empty());
}

TEST_CASE("Background setters keep halfwords and bytes",
          "[field-camera][field-background]") {
  fx::RoomBackgroundState state;
  fx::SetBackgroundParallaxScale(state, 0x12345, -1);
  CHECK(state.parallax_x == 0x2345);
  CHECK(state.parallax_y == -1);
  fx::SetTiledBackgroundEnabled(state, 0x101);
  CHECK(state.tiled_enabled == 1);
  fx::SetRoomBackgroundOffset(state, 0x10005, -3);
  CHECK(state.offset_x == 5);
  CHECK(state.offset_y == -3);
}

TEST_CASE("Live background colours reach the sky dome",
          "[field-camera][field-background]") {
  std::array<unsigned int, 166> saved_dome{};
  std::ranges::copy(Content_battleSkyDome, saved_dome.begin());
  std::array<int, 5> saved_colors{};
  std::ranges::copy(Field_backgroundGradientColors, saved_colors.begin());
  ChannelWarnings warnings{"field.background"};

  const std::array<int, 5> colors{1, 2, 3, 4, 0xABCDEF};
  VS::Field::LiveSetRoomBackgroundColors(colors.data());
  CHECK(Content_battleSkyDome[2] == 0x30000001U);
  CHECK(Content_battleSkyDome[(1 * 3) + 2] == 0x30000002U);
  CHECK(Content_battleSkyDome[(33 * 3) + 2] == 0x30ABCDEFU);
  CHECK(VS::Field::LiveRoomBackgroundColors()[4] == 0xABCDEF);
  for (int call = 0; call < 1024 && warnings.messages.empty(); ++call) {
    VS::Field::LiveSetRoomBackgroundColors(nullptr);
  }

  std::ranges::copy(saved_dome, Content_battleSkyDome);
  std::ranges::copy(saved_colors, Field_backgroundGradientColors);
  REQUIRE_FALSE(warnings.messages.empty());
  CHECK(warnings.messages[0] ==
        "SetRoomBackgroundColors was given a null record; the call is "
        "refused");
}

TEST_CASE("The field camera module symbols are retired",
          "[field-camera][field-background]") {
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  constexpr std::array<std::uint32_t, 57> kRetired{
      0x80069D78U, 0x8006D97CU, 0x8006D9C4U, 0x8006D9FCU, 0x8006DB98U,
      0x8006DEFCU, 0x8006DFE0U, 0x8006E158U, 0x8006E640U, 0x8006EBF8U,
      0x8006EC7CU, 0x8006EF10U, 0x8006EF5CU, 0x8006F204U, 0x800730BCU,
      0x800732ACU, 0x80074580U, 0x800745ECU, 0x80074744U, 0x80074B14U,
      0x80077DF0U, 0x80077EC4U, 0x8007A850U, 0x8007A9DCU, 0x8007AACCU,
      0x8007AAF8U, 0x8007ABECU, 0x8007AC18U, 0x8007AC40U, 0x8007AC6CU,
      0x8007AC94U, 0x8007ACA0U, 0x8007BDD8U, 0x8007CD70U, 0x8007CE74U,
      0x8008E88CU, 0x8008E938U, 0x8008EAE4U, 0x8008EAF8U, 0x8008EB24U,
      0x8008EB30U, 0x8008EC48U, 0x8008EFCCU, 0x8008F0FCU, 0x8008F234U,
      0x80090C2CU, 0x80090EECU, 0x8009121CU, 0x80091314U, 0x80091320U,
      0x8009134CU, 0x80091378U, 0x800913BCU, 0x80091468U, 0x8009147CU,
      0x800918E8U, 0x80068EB4U};
  for (const std::uint32_t address : kRetired) {
    CAPTURE(address);
    CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                            address) == nullptr);
  }
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x80068C1CU) ==
        static_cast<void*>(Content_battleSkyDome));
}
