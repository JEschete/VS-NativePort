// Hand-maintained (tools/port/owned.csv).
#include "Field/BackgroundLive.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <string_view>
#include <type_traits>
#include <vector>

#include "Content/CdDriveLive.hpp"
#include "Core/Log.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Field/Environment.hpp"
#include "Field/EnvironmentLive.hpp"
#include "Field/RoomRenderingLive.hpp"
#include "Render/PaletteAnimationLive.hpp"
#include "Render/RoomDraw.hpp"
#include "Render/RoomDrawLive.hpp"
#include "Render/SpritesLive.hpp"
#include "Render/TexturePagesLive.hpp"
#include "Render/TimImageLive.hpp"
#include "Render/UiPacketsLive.hpp"

extern "C" {
#include "Core/Gte.h"
#include "Core/Scratch.h"
#include "DiscQueue.h"
#include "Field/BackgroundLiveState.h"
#include "Field/Gimmicks.h"
#include "Field/Mpd.h"
#include "Field/Room.h"
#include "Render/Render.h"

// decomp: D_800F1CE0 (D_800F1CE0@BATTLE/BATTLE.PRG)
VECTOR Field_backgroundReferenceVectors[2];
// decomp: D_800F1D78 (D_800F1D78@BATTLE/BATTLE.PRG)
int Field_backgroundGradientColors[kFieldBackgroundGradientColorCount];
// decomp: D_800F1DAC (D_800F1DAC@BATTLE/BATTLE.PRG)
Content_DiscQueueSlot* Field_backgroundTextureDiscRequest;
// decomp: D_800F1DB0 (D_800F1DB0@BATTLE/BATTLE.PRG)
Content_DiscQueueSlot* Field_backgroundLayoutDiscRequest;
// decomp: D_800F1DB4 (D_800F1DB4@BATTLE/BATTLE.PRG)
int16_t Field_backgroundOffsetX;
// decomp: D_800F1DB6 (D_800F1DB6@BATTLE/BATTLE.PRG)
int16_t Field_backgroundOffsetY;
// decomp: D_800F1DB8 (D_800F1DB8@BATTLE/BATTLE.PRG)
int16_t Field_backgroundParallaxScaleXFixed12;
// decomp: D_800F1DBA (D_800F1DBA@BATTLE/BATTLE.PRG)
int16_t Field_backgroundParallaxScaleYFixed12;
// decomp: D_800F1DBE (D_800F1DBE@BATTLE/BATTLE.PRG)
int16_t Field_legacyBackgroundTextureReservationId;
// decomp: D_800F1DC6 (D_800F1DC6@BATTLE/BATTLE.PRG)
char Field_isSolidBackground;
// decomp: D_800F1DC8 (D_800F1DC8@BATTLE/BATTLE.PRG)
char Field_legacyBackgroundTexturePage;
// decomp: D_800F1DC9 (D_800F1DC9@BATTLE/BATTLE.PRG)
u_char Field_isTiledBackgroundEnabled;
// decomp: D_800F1DCC (D_800F1DCC@BATTLE/BATTLE.PRG)
Field_BackgroundLayout* Field_loadedBackgroundLayout;
// decomp: D_800F1DD0 (D_800F1DD0@BATTLE/BATTLE.PRG)
u_long* Field_legacyBackgroundTimBuffer;
}

namespace {
// D_80068C1C: 34 twelve-byte vertices, the third word of each its colour, and
// 64 four-byte faces (146C.c:17537).
constexpr std::size_t kSkyDomeWords = 166;
constexpr std::size_t kSkyDomeVertexWords = 3;
constexpr std::size_t kSkyDomeColorWord = 2;

// The original's layouts (146C.h:136, 2842C.h:9-16 and main.h:24).
constexpr std::size_t kLayoutHeaderBytes = 0xC;
constexpr std::size_t kLayoutTileBytes = 0x18;
constexpr std::size_t kEnvironmentBytes = 0x58;
constexpr std::size_t kEnvironmentRotationAt = 0x14;
constexpr std::size_t kEnvironmentWeatherKindAt = 0x34;
constexpr std::size_t kEnvironmentWindStrengthAt = 0x42;
constexpr std::size_t kEnvironmentWeatherAreaAt = 0x48;
constexpr std::size_t kDiscFileBytes = 8;

// func_8008EFCC: the tiled background section's page count word, then its
// TIM file at byte 0x40 and its layout file at byte 0x84.
constexpr std::size_t kTiledTextureFileAt = 0x40;
constexpr std::size_t kTiledLayoutFileAt = 0x84;
constexpr std::size_t kTiledSectionBytes = kTiledLayoutFileAt + kDiscFileBytes;
}  // namespace

// Complete D_80068C1C definition from BATTLE.PRG:0x41C, loaded by Content.
extern "C" unsigned int Content_battleSkyDome[kSkyDomeWords];

static_assert(sizeof(Content_battleSkyDome) == VS::Render::kSkyDomeBytes);
static_assert(VS::Render::kSkyDomeVertexBytes ==
              kSkyDomeVertexWords * sizeof(unsigned int));
static_assert(VS::Field::kSkyDomeVertexCount ==
              VS::Render::kSkyDomeVertexCount);
static_assert(sizeof(Field_BackgroundLayout) == kLayoutHeaderBytes);
static_assert(sizeof(Field_BackgroundTile) == kLayoutTileBytes);
static_assert(sizeof(Field_MpdEnvironment) == kEnvironmentBytes);
static_assert(offsetof(Field_MpdEnvironment, backgroundRotation) ==
              kEnvironmentRotationAt);
static_assert(offsetof(Field_MpdEnvironment, weatherKind) ==
              kEnvironmentWeatherKindAt);
static_assert(offsetof(Field_MpdEnvironment, windStrength) ==
              kEnvironmentWindStrengthAt);
static_assert(offsetof(Field_MpdEnvironment, weatherArea) ==
              kEnvironmentWeatherAreaAt);
static_assert(sizeof(Content_DiscFile) == kDiscFileBytes);
static_assert(sizeof(POLY_FT4) % sizeof(u_long) == 0);
static_assert(sizeof(POLY_GT4) % sizeof(u_long) == 0);
static_assert(sizeof(TILE) % sizeof(u_long) == 0);

namespace VS::Field {
namespace {
namespace Math = Core::Math;
namespace Memory = Core::Memory;

constexpr std::string_view kChannel = "field.background";
// The farthest scene ordering entry (orderingTable + 0x1FFC).
constexpr int kFarthestOrderingSlot = 0x7FF;

std::uint64_t refused_calls = 0;
std::uint64_t dropped_packets = 0;
std::uint64_t refused_sections = 0;

// True on the 1st, 2nd, 4th, 8th... occurrence, to keep repeated
// diagnostics readable.
[[nodiscard]] bool ShouldReport(std::uint64_t& count) {
  return std::has_single_bit(++count);
}

[[nodiscard]] Render_FrameScratch& Scratch() {
  return *static_cast<Render_FrameScratch*>(Core_ScratchBase());
}

// The read and buffer ids name only kNone; each carries the address of a
// disc request, the layout or the TIM buffer.
template <typename Id, typename T>
[[nodiscard]] Id IdOf(T* pointer) {
  const auto address = reinterpret_cast<std::uintptr_t>(pointer);
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  return static_cast<Id>(address);
}

template <typename T, typename Id>
[[nodiscard]] T* PointerOf(Id id) {
  const auto address = static_cast<std::uintptr_t>(id);
  return reinterpret_cast<T*>(address);
}

[[nodiscard]] ViewCamera ViewCameraOf(const Render_Camera& camera) {
  return {.position = Math::VectorOf(camera.position),
          .look_at = Math::VectorOf(camera.lookAt),
          .angles = Math::VectorOf(camera.angles),
          .far_clip = camera.farClip};
}

[[nodiscard]] Content_DiscFile DiscFileOf(const BackgroundFile& file) {
  Content_DiscFile disc{};
  disc.lba = file.lba;
  disc.size = file.size;
  return disc;
}

[[nodiscard]] BackgroundFile BackgroundFileAt(std::span<const std::byte> bytes,
                                              std::size_t offset) {
  Content_DiscFile disc{};
  std::memcpy(&disc, bytes.subspan(offset, sizeof(disc)).data(), sizeof(disc));
  return {.lba = disc.lba, .size = disc.size};
}

[[nodiscard]] BackgroundTile TileOf(const Field_BackgroundTile& tile) {
  BackgroundTile result{.texture = {.x = tile.textureRect.x,
                                    .y = tile.textureRect.y,
                                    .width = tile.textureRect.w,
                                    .height = tile.textureRect.h}};
  for (std::size_t corner = 0; corner < result.corners.size(); ++corner) {
    const CVECTOR& color = tile.cornerColors[corner];
    result.corners.at(corner) = {
        .red = color.r, .green = color.g, .blue = color.b, .code = color.cd};
  }
  return result;
}

// Writes a packet into the frame's primitive buffer and links it at the
// farthest entry, as the original's addPrim(ot + 0x1FFC) did.
template <typename Packet>
void LinkPacket(const Packet& packet, std::string_view kind) {
  constexpr std::size_t kWords = sizeof(Packet) / sizeof(u_long);
  u_long* const entry = Render::LiveFrameOrderingEntry(
      Render::LiveOrderingBase::kScene, kFarthestOrderingSlot);
  u_long* const slot =
      entry != nullptr ? Render::ReserveLivePrimitiveWords(kWords) : nullptr;
  if (slot == nullptr) {
    if (ShouldReport(dropped_packets)) {
      VS_LOG_WARN(kChannel,
                  "background {} has no ordering entry or buffer room; "
                  "dropped ({} drops)",
                  kind, dropped_packets);
    }
    return;
  }
  std::memcpy(slot, &packet, sizeof(Packet));
  AddPrim(entry, slot);
  Scratch().primitiveCursor = slot + kWords;
}

template <typename Primitive>
void SetCorners(Primitive& primitive, const TexturedQuad& quad) {
  const auto& xy = quad.corners;
  const auto& uv = quad.texels;
  setXY4(&primitive, xy[0].x, xy[0].y, xy[1].x, xy[1].y, xy[2].x, xy[2].y,
         xy[3].x, xy[3].y);
  setUV4(&primitive, uv[0].u, uv[0].v, uv[1].u, uv[1].v, uv[2].u, uv[2].v,
         uv[3].u, uv[3].v);
  primitive.tpage = quad.texture_page;
  primitive.clut = quad.clut;
}

void RefuseNull(std::string_view entry) {
  if (ShouldReport(refused_calls)) {
    VS_LOG_WARN(kChannel, "{} was given a null record; the call is refused",
                entry);
  }
}

// Runs a rule over the loaded globals and stores them back before returning
// its result.
template <typename Rule>
auto RunLive(Rule rule) {
  RoomBackgroundState state = LoadLiveRoomBackgroundState();
  LiveBackgroundHost host;
  if constexpr (std::is_void_v<decltype(rule(host, state))>) {
    rule(host, state);
    StoreLiveRoomBackgroundState(state);
  } else {
    auto result = rule(host, state);
    StoreLiveRoomBackgroundState(state);
    return result;
  }
}
}  // namespace

RoomBackgroundState LoadLiveRoomBackgroundState() {
  RoomBackgroundState state{
      .offset_x = Field_backgroundOffsetX,
      .offset_y = Field_backgroundOffsetY,
      .parallax_x = Field_backgroundParallaxScaleXFixed12,
      .parallax_y = Field_backgroundParallaxScaleYFixed12,
      .texture_page_count = Field_legacyBackgroundTextureReservationId,
      .solid = static_cast<std::uint8_t>(Field_isSolidBackground),
      .texture_page =
          static_cast<std::uint8_t>(Field_legacyBackgroundTexturePage),
      .tiled_enabled = Field_isTiledBackgroundEnabled,
      .texture_read =
          IdOf<BackgroundReadId>(Field_backgroundTextureDiscRequest),
      .layout_read = IdOf<BackgroundReadId>(Field_backgroundLayoutDiscRequest),
      .layout = IdOf<BackgroundBufferId>(Field_loadedBackgroundLayout),
      .tim_buffer = IdOf<BackgroundBufferId>(Field_legacyBackgroundTimBuffer)};
  std::ranges::copy(Field_backgroundGradientColors,
                    state.gradient_colors.begin());
  return state;
}

void StoreLiveRoomBackgroundState(const RoomBackgroundState& state) {
  std::ranges::copy(state.gradient_colors, Field_backgroundGradientColors);
  Field_backgroundOffsetX = state.offset_x;
  Field_backgroundOffsetY = state.offset_y;
  Field_backgroundParallaxScaleXFixed12 = state.parallax_x;
  Field_backgroundParallaxScaleYFixed12 = state.parallax_y;
  Field_legacyBackgroundTextureReservationId = state.texture_page_count;
  Field_isSolidBackground = static_cast<char>(state.solid);
  Field_legacyBackgroundTexturePage = static_cast<char>(state.texture_page);
  Field_isTiledBackgroundEnabled = state.tiled_enabled;
  Field_backgroundTextureDiscRequest =
      PointerOf<Content_DiscQueueSlot>(state.texture_read);
  Field_backgroundLayoutDiscRequest =
      PointerOf<Content_DiscQueueSlot>(state.layout_read);
  Field_loadedBackgroundLayout =
      PointerOf<Field_BackgroundLayout>(state.layout);
  Field_legacyBackgroundTimBuffer = PointerOf<u_long>(state.tim_buffer);
}

std::optional<RoomEnvironment> LiveRoomEnvironment() {
  const Field_MpdEnvironment* const record = Field_roomData.environment;
  if (record == nullptr) return std::nullopt;
  RoomEnvironment environment{
      .background_rotation = Math::VectorOf(record->backgroundRotation),
      .wind_direction = record->windDirection,
      .wind_direction_limit = record->windDirectionLimit,
      .wind_direction_interval = record->windDirectionInterval,
      .weather_intensity = record->weatherIntensity,
      .weather_intensity_limit = record->weatherIntensityLimit,
      .weather_intensity_interval = record->weatherIntensityInterval,
      .wind_strength = record->windStrength,
      .wind_strength_limit = record->windStrengthLimit,
      .wind_strength_interval = record->windStrengthInterval,
      .weather_area = {.origin_x = record->weatherArea[0].halves[0],
                       .origin_z = record->weatherArea[0].halves[1],
                       .extent_x = record->weatherArea[1].halves[0],
                       .extent_z = record->weatherArea[1].halves[1]}};
  std::ranges::copy(record->backgroundColors,
                    environment.background_colors.begin());
  return environment;
}

std::optional<RoomEnvironment> LiveBackgroundHost::Environment() {
  return LiveRoomEnvironment();
}

void LiveBackgroundHost::StoreWeatherArea(const WeatherArea& area) {
  Field_MpdEnvironment* const record = Field_roomData.environment;
  if (record == nullptr) return;
  record->weatherArea[0].halves[0] = area.origin_x;
  record->weatherArea[0].halves[1] = area.origin_z;
  record->weatherArea[1].halves[0] = area.extent_x;
  record->weatherArea[1].halves[1] = area.extent_z;
}

std::optional<CollisionMapExtent> LiveBackgroundHost::CollisionMapSize() {
  const Field_MpdCollisionHeader* const map = Field_roomData.collisionMap;
  if (map == nullptr) return std::nullopt;
  return CollisionMapExtent{.width = map->width, .height = map->height};
}

void LiveBackgroundHost::StoreSkyDomeColors(const SkyDomeColors& colors) {
  const std::span dome(Content_battleSkyDome);
  for (std::size_t vertex = 0; vertex < colors.size(); ++vertex) {
    dome[(vertex * kSkyDomeVertexWords) + kSkyDomeColorWord] =
        colors.at(vertex);
  }
}

ViewCamera LiveBackgroundHost::FrameCamera() {
  return ViewCameraOf(Scratch().camera);
}

RoomTint LiveBackgroundHost::ActiveRoomTint() {
  const RoomTintState tint = LiveRoomRenderState().tint;
  return {.red = tint.parameters.red,
          .green = tint.parameters.green,
          .blue = tint.parameters.blue,
          .is_active = static_cast<std::int16_t>(tint.active ? 1 : 0)};
}

void LiveBackgroundHost::AddFlatTexturedQuad(const TexturedQuad& quad) {
  POLY_FT4 primitive{};
  setPolyFT4(&primitive);
  const PrimitiveColor& color = quad.colors.front();
  setRGB0(&primitive, color.red, color.green, color.blue);
  SetCorners(primitive, quad);
  LinkPacket(primitive, "quad");
}

void LiveBackgroundHost::AddShadedTexturedQuad(const TexturedQuad& quad) {
  POLY_GT4 primitive{};
  setPolyGT4(&primitive);
  const auto& colors = quad.colors;
  setRGB0(&primitive, colors[0].red, colors[0].green, colors[0].blue);
  setRGB1(&primitive, colors[1].red, colors[1].green, colors[1].blue);
  setRGB2(&primitive, colors[2].red, colors[2].green, colors[2].blue);
  setRGB3(&primitive, colors[3].red, colors[3].green, colors[3].blue);
  SetCorners(primitive, quad);
  LinkPacket(primitive, "tile");
}

void LiveBackgroundHost::AddFill(const FillRect& fill) {
  TILE primitive{};
  setTile(&primitive);
  setRGB0(&primitive, fill.color.red, fill.color.green, fill.color.blue);
  setXY0(&primitive, fill.position.x, fill.position.y);
  setWH(&primitive, fill.width, fill.height);
  LinkPacket(primitive, "fill");
}

void LiveBackgroundHost::DrawSkyDome(const CameraVector& position,
                                     const CameraVector& angles,
                                     bool scale_compensated) {
  const Render_FrameScratch& scratch = Scratch();
  Core_Gte_SetRotMatrix(&scratch.viewMatrix);
  Core_Gte_SetTransMatrix(&scratch.viewMatrix);
  VECTOR dome_position{};
  VECTOR dome_angles{};
  Math::StoreVector(dome_position, position);
  Math::StoreVector(dome_angles, angles);
  MATRIX view{};
  if (scale_compensated) {
    Render::BuildLiveScaleCompensatedRoomView(dome_position, dome_angles, view);
  } else {
    Render::BuildLiveRoomGeometryViewMatrix(dome_position, dome_angles, view);
  }
  Core_Gte_SetRotMatrix(&view);
  Core_Gte_SetTransMatrix(&view);
  Render::DrawLiveSkyDome(Content_battleSkyDome, sizeof Content_battleSkyDome);
}

CameraVector LiveBackgroundHost::ReferenceRotation() {
  return Math::VectorOf(Field_backgroundReferenceVectors[0]);
}

std::optional<BackgroundLayout> LiveBackgroundHost::Layout(
    BackgroundBufferId layout) {
  const auto* const record = PointerOf<const Field_BackgroundLayout>(layout);
  const std::size_t capacity = Memory::LivePayloadCapacity(record).value_or(0);
  if (capacity < sizeof(Field_BackgroundLayout)) return std::nullopt;
  const std::size_t readable = (capacity - sizeof(Field_BackgroundLayout)) /
                               sizeof(Field_BackgroundTile);
  const auto side = [](std::int32_t value) {
    return static_cast<std::size_t>(std::max(value, 0));
  };
  const std::size_t count =
      std::min(readable, side(record->width) * side(record->height));
  const std::span cells(static_cast<const Field_BackgroundTile*>(record->tiles),
                        count);
  tiles_.clear();
  tiles_.reserve(count);
  for (const Field_BackgroundTile& tile : cells) {
    tiles_.push_back(TileOf(tile));
  }
  return BackgroundLayout{
      .width = record->width, .height = record->height, .tiles = tiles_};
}

std::optional<TiledBackgroundSection> LiveBackgroundHost::TiledBackground() {
  const u_short* const words = Field_roomData.tiledBackground;
  if (words == nullptr) return std::nullopt;
  const std::size_t bytes = Field_roomData.header.tiledBackgroundLength;
  if (bytes < kTiledSectionBytes) {
    if (ShouldReport(refused_sections)) {
      VS_LOG_WARN(kChannel,
                  "the room's tiled background section has {} bytes, fewer "
                  "than the {} its files need; no picture is loaded",
                  bytes, kTiledSectionBytes);
    }
    return std::nullopt;
  }
  const std::span section(reinterpret_cast<const std::byte*>(words), bytes);
  return TiledBackgroundSection{
      .texture_page_count = words[0],
      .texture = BackgroundFileAt(section, kTiledTextureFileAt),
      .layout = BackgroundFileAt(section, kTiledLayoutFileAt)};
}

bool LiveBackgroundHost::ReserveTexturePages(int first_page, int page_count,
                                             int owner) {
  return Render::ReserveLiveTexturePageRange(
             static_cast<std::uint32_t>(first_page),
             static_cast<std::uint32_t>(page_count), owner) != 0;
}

void LiveBackgroundHost::RetireTexturePages(int first_page, int page_count,
                                            int owner) {
  Render::RetireLiveTexturePageRange(static_cast<std::uint32_t>(first_page),
                                     static_cast<std::uint32_t>(page_count),
                                     owner);
}

BackgroundBufferId LiveBackgroundHost::AllocateHighBuffer(std::int32_t bytes) {
  return IdOf<BackgroundBufferId>(Memory::AllocateLiveFromHighEnd(
      static_cast<std::size_t>(static_cast<std::uint32_t>(bytes))));
}

void LiveBackgroundHost::FreeHighBuffer(BackgroundBufferId buffer) {
  Memory::FreeLiveFromHighEnd(PointerOf<const void>(buffer));
}

BackgroundReadId LiveBackgroundHost::AllocateRead(const BackgroundFile& file) {
  const Content_DiscFile disc = DiscFileOf(file);
  return IdOf<BackgroundReadId>(Content::AllocateLiveDiscRequest(&disc));
}

void LiveBackgroundHost::EnqueueRead(BackgroundReadId read,
                                     BackgroundBufferId destination) {
  Content::EnqueueLiveDiscRequest(PointerOf<Content_DiscQueueSlot>(read),
                                  PointerOf<void>(destination));
}

BackgroundReadId LiveBackgroundHost::CompleteRefusedRead(
    const BackgroundFile& file, BackgroundBufferId destination) {
  const Content_DiscFile disc = DiscFileOf(file);
  return IdOf<BackgroundReadId>(Content::CompleteLiveRefusedDiscRequest(
      &disc, PointerOf<void>(destination)));
}

bool LiveBackgroundHost::ReadLoaded(BackgroundReadId read) {
  return PointerOf<const Content_DiscQueueSlot>(read)->state ==
         Content_DiscRequestState_Loaded;
}

void LiveBackgroundHost::ReleaseRead(BackgroundReadId read) {
  Content::ReleaseLiveDiscRequest(PointerOf<Content_DiscQueueSlot>(read));
}

void LiveBackgroundHost::StampTimId(BackgroundBufferId buffer,
                                    std::uint32_t id) {
  *PointerOf<u_long>(buffer) = id;
}

BackgroundTim LiveBackgroundHost::ParseTim(BackgroundBufferId buffer) {
  TIM_IMAGE tim{};
  Render::ParseLiveTim(PointerOf<void>(buffer), &tim);
  BackgroundTim parsed{.pixels = std::nullopt,
                       .has_clut = tim.caddr != nullptr};
  if (tim.paddr != nullptr) {
    const RECT& rect = *tim.prect;
    parsed.pixels = TextureRect{
        .x = rect.x, .y = rect.y, .width = rect.w, .height = rect.h};
  }
  return parsed;
}

void LiveBackgroundHost::UploadTimPixels(BackgroundBufferId buffer,
                                         const TextureRect& rect) {
  TIM_IMAGE tim{};
  Render::ParseLiveTim(PointerOf<void>(buffer), &tim);
  if (tim.paddr == nullptr) return;
  RECT& target = *tim.prect;
  target.x = rect.x;
  target.y = rect.y;
  target.w = rect.width;
  target.h = rect.height;
  LoadImage(tim.prect, tim.paddr);
}

void LiveBackgroundHost::UploadTimClut(BackgroundBufferId buffer, int row,
                                       int first_color, int count) {
  TIM_IMAGE tim{};
  Render::ParseLiveTim(PointerOf<void>(buffer), &tim);
  if (tim.caddr == nullptr) return;
  Render::LoadLiveClutColors(
      reinterpret_cast<const u_short*>(tim.caddr), static_cast<u_int>(row),
      static_cast<u_int>(first_color), static_cast<u_int>(count));
}

// decomp: BATTLE.PRG func_8008E88C (src/BATTLE/BATTLE.PRG/146C.c:17431)
void LiveInitializeRoomBackground() {
  RoomWeatherState weather = LoadLiveRoomWeatherState();
  RunLive([&weather](BackgroundHost& host, RoomBackgroundState& state) {
    InitializeRoomBackground(host, state, weather);
  });
  StoreLiveRoomWeatherState(weather);
}

// decomp: BATTLE.PRG func_8008E938 (src/BATTLE/BATTLE.PRG/146C.c:17453)
void LiveInitializeRoomBackgroundColors() {
  RunLive([](BackgroundHost& host, RoomBackgroundState& state) {
    InitializeRoomBackgroundColors(host, state);
  });
}

// decomp: BATTLE.PRG func_8008EAE4 (src/BATTLE/BATTLE.PRG/146C.c:17517)
void LiveSetBackgroundParallaxScale(int parallax_x, int parallax_y) {
  RunLive([parallax_x, parallax_y](BackgroundHost& /*host*/,
                                   RoomBackgroundState& state) {
    SetBackgroundParallaxScale(state, parallax_x, parallax_y);
  });
}

// decomp: BATTLE.PRG func_8008EAF8 (src/BATTLE/BATTLE.PRG/146C.c:17523)
void LiveSetTiledBackgroundEnabled(int enabled) {
  RunLive([enabled](BackgroundHost& /*host*/, RoomBackgroundState& state) {
    SetTiledBackgroundEnabled(state, enabled);
  });
}

// decomp: BATTLE.PRG func_8008EB24 (src/BATTLE/BATTLE.PRG/146C.c:17531)
int* LiveRoomBackgroundColors() { return Field_backgroundGradientColors; }

// decomp: BATTLE.PRG func_8008EB30 (src/BATTLE/BATTLE.PRG/146C.c:17533)
void LiveSetRoomBackgroundColors(const int* gradient_colors) {
  if (gradient_colors == nullptr) {
    RefuseNull("SetRoomBackgroundColors");
    return;
  }
  GradientColors colors{};
  std::ranges::copy(std::span(gradient_colors, kGradientColorCount),
                    colors.begin());
  RunLive([&colors](BackgroundHost& host, RoomBackgroundState& state) {
    SetRoomBackgroundColors(host, state, colors);
  });
}

// decomp: BATTLE.PRG func_8008EC48 (src/BATTLE/BATTLE.PRG/146C.c:17571)
void LiveDrawRoomBackground(const VECTOR* rotation_override) {
  std::optional<CameraVector> rotation;
  if (rotation_override != nullptr) {
    rotation = Math::VectorOf(*rotation_override);
  }
  RunLive([&rotation](BackgroundHost& host, RoomBackgroundState& state) {
    DrawRoomBackground(host, state, rotation);
  });
}

// decomp: BATTLE.PRG func_8008EFCC (src/BATTLE/BATTLE.PRG/146C.c:17638)
int LiveQueueTiledBackgroundLoad(int texture_page) {
  return RunLive(
      [texture_page](BackgroundHost& host, RoomBackgroundState& state) {
        return QueueTiledBackgroundLoad(host, state, texture_page);
      });
}

// decomp: BATTLE.PRG func_8008F0FC (src/BATTLE/BATTLE.PRG/146C.c:17680)
int LiveFinishTiledBackgroundLoad() {
  return RunLive([](BackgroundHost& host, RoomBackgroundState& state) {
    return FinishTiledBackgroundLoad(host, state);
  });
}

// decomp: BATTLE.PRG func_8008F234 (src/BATTLE/BATTLE.PRG/146C.c:17710)
int LiveReleaseTiledBackground() {
  return RunLive([](BackgroundHost& host, RoomBackgroundState& state) {
    return ReleaseTiledBackground(host, state);
  });
}
}  // namespace VS::Field
