// Hand-maintained (tools/port/owned.csv).
#include "Field/BackgroundLive.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <format>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "Content/BackgroundLayout.hpp"
#include "Content/ConvertedImages.hpp"
#include "Content/DataStore.hpp"
#include "Content/DiscQueue.hpp"
#include "Content/ImageFiles.hpp"
#include "Content/ModuleData.hpp"
#include "Content/NativeImage.hpp"
#include "Core/Gte.hpp"
#include "Core/Log.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Field/BackgroundLiveState.hpp"
#include "Field/Environment.hpp"
#include "Field/EnvironmentLive.hpp"
#include "Field/Gimmicks.hpp"
#include "Field/Mpd.hpp"
#include "Field/Room.hpp"
#include "Field/RoomRenderingLive.hpp"
#include "Render/PaletteAnimationLive.hpp"
#include "Render/Render.hpp"
#include "Render/RoomDraw.hpp"
#include "Render/RoomDrawLive.hpp"
#include "Render/SpritesLive.hpp"
#include "Render/TexturePagesLive.hpp"
#include "Render/UiPacketsLive.hpp"

using VS::Core::Gte::MATRIX;
using VS::Core::Gte::VECTOR;

using VS::Legacy::AddPrim;
using VS::Legacy::POLY_FT4;
using VS::Legacy::POLY_GT4;
using VS::Legacy::RECT;
using VS::Legacy::setPolyFT4;
using VS::Legacy::setPolyGT4;
using VS::Legacy::setRGB0;
using VS::Legacy::setRGB1;
using VS::Legacy::setRGB2;
using VS::Legacy::setRGB3;
using VS::Legacy::setTile;
using VS::Legacy::setUV4;
using VS::Legacy::setWH;
using VS::Legacy::setXY0;
using VS::Legacy::setXY4;
using VS::Legacy::TILE;

// decomp: D_800F1CE0 (D_800F1CE0@BATTLE/BATTLE.PRG)
VECTOR VS::Field::backgroundReferenceVectors[2];
// decomp: D_800F1D78 (D_800F1D78@BATTLE/BATTLE.PRG)
int VS::Field::backgroundGradientColors
    [VS::Field::kBackgroundGradientColorCount];
// decomp: D_800F1DB0 (D_800F1DB0@BATTLE/BATTLE.PRG)
// Retained for module metadata only; native background loading has no disc
// request.
VS::Content::DiscQueueSlot* VS::Field::backgroundLayoutDiscRequest;
// decomp: D_800F1DB4 (D_800F1DB4@BATTLE/BATTLE.PRG)
int16_t VS::Field::backgroundOffsetX;
// decomp: D_800F1DB6 (D_800F1DB6@BATTLE/BATTLE.PRG)
int16_t VS::Field::backgroundOffsetY;
// decomp: D_800F1DB8 (D_800F1DB8@BATTLE/BATTLE.PRG)
int16_t VS::Field::backgroundParallaxScaleXFixed12;
// decomp: D_800F1DBA (D_800F1DBA@BATTLE/BATTLE.PRG)
int16_t VS::Field::backgroundParallaxScaleYFixed12;
// decomp: D_800F1DBE (D_800F1DBE@BATTLE/BATTLE.PRG)
int16_t VS::Field::legacyBackgroundTextureReservationId;
// decomp: D_800F1DC6 (D_800F1DC6@BATTLE/BATTLE.PRG)
char VS::Field::isSolidBackground;
// decomp: D_800F1DC8 (D_800F1DC8@BATTLE/BATTLE.PRG)
char VS::Field::legacyBackgroundTexturePage;
// decomp: D_800F1DC9 (D_800F1DC9@BATTLE/BATTLE.PRG)
std::uint8_t VS::Field::isTiledBackgroundEnabled;
// decomp: D_800F1DCC (D_800F1DCC@BATTLE/BATTLE.PRG)
// Retained for module metadata only; nativeBackgroundLayout owns the live
// cells.
VS::Field::BackgroundLayoutRecord* VS::Field::loadedBackgroundLayout;
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

// VS::Content::battleSkyDome is the complete D_80068C1C definition from
// BATTLE.PRG:0x41C, loaded by Content.
static_assert(VS::Content::kBattleSkyDomeWords == kSkyDomeWords);
static_assert(sizeof(VS::Content::battleSkyDome) == VS::Render::kSkyDomeBytes);
static_assert(VS::Render::kSkyDomeVertexBytes ==
              kSkyDomeVertexWords * sizeof(unsigned int));
static_assert(VS::Field::kSkyDomeVertexCount ==
              VS::Render::kSkyDomeVertexCount);
static_assert(sizeof(VS::Field::BackgroundLayoutRecord) == kLayoutHeaderBytes);
static_assert(sizeof(VS::Field::BackgroundTileRecord) == kLayoutTileBytes);
static_assert(sizeof(VS::Field::MpdEnvironment) == kEnvironmentBytes);
static_assert(offsetof(VS::Field::MpdEnvironment, background_rotation) ==
              kEnvironmentRotationAt);
static_assert(offsetof(VS::Field::MpdEnvironment, weather_kind) ==
              kEnvironmentWeatherKindAt);
static_assert(offsetof(VS::Field::MpdEnvironment, wind_strength) ==
              kEnvironmentWindStrengthAt);
static_assert(offsetof(VS::Field::MpdEnvironment, weather_area) ==
              kEnvironmentWeatherAreaAt);
static_assert(sizeof(VS::Content::DiscFile) == kDiscFileBytes);
static_assert(sizeof(POLY_FT4) % sizeof(std::uint32_t) == 0);
static_assert(sizeof(POLY_GT4) % sizeof(std::uint32_t) == 0);
static_assert(sizeof(TILE) % sizeof(std::uint32_t) == 0);

namespace VS::Field {
namespace {
namespace Math = Core::Math;
namespace Content = VS::Content;

constexpr std::string_view kChannel = "field.background";
// The farthest scene ordering entry (orderingTable + 0x1FFC).
constexpr int kFarthestOrderingSlot = 0x7FF;
constexpr std::size_t kBackgroundPaletteEntries = 256;

std::uint64_t refusedCalls = 0;
std::uint64_t droppedPackets = 0;
std::uint64_t refusedSections = 0;
VS::Field::BackgroundFile queuedBackgroundTextureFile;
std::uint8_t backgroundTextureUploaded;
std::shared_ptr<const OwnedBackgroundLayout> nativeBackgroundLayout;

// True on the 1st, 2nd, 4th, 8th... occurrence, to keep repeated
// diagnostics readable.
[[nodiscard]] bool ShouldReport(std::uint64_t& count) {
  return std::has_single_bit(++count);
}

[[nodiscard]] VS::Render::FrameScratch& Scratch() {
  return VS::Render::FrameScratchpad();
}

[[nodiscard]] ViewCamera ViewCameraOf(const VS::Render::Camera& camera) {
  return {.position = Math::VectorOf(camera.position),
          .look_at = Math::VectorOf(camera.look_at),
          .angles = Math::VectorOf(camera.angles),
          .far_clip = camera.far_clip};
}

[[nodiscard]] BackgroundFile BackgroundFileAt(std::span<const std::byte> bytes,
                                              std::size_t offset) {
  VS::Content::DiscFile disc{};
  std::memcpy(&disc, bytes.subspan(offset, sizeof(disc)).data(), sizeof(disc));
  return {.lba = disc.lba, .size = disc.size};
}

[[nodiscard]] BackgroundTile TileOf(const Content::BackgroundCell& tile) {
  BackgroundTile result{
      .texture = {.x = static_cast<std::int16_t>(tile.left),
                  .y = static_cast<std::int16_t>(tile.top),
                  .width = static_cast<std::int16_t>(tile.right - tile.left),
                  .height = static_cast<std::int16_t>(tile.bottom - tile.top)}};
  for (std::size_t corner = 0; corner < result.corners.size(); ++corner) {
    const auto& color = tile.colors.at(corner);
    result.corners.at(corner) = {
        .red = static_cast<std::uint8_t>(color.red),
        .green = static_cast<std::uint8_t>(color.green),
        .blue = static_cast<std::uint8_t>(color.blue),
        .code = static_cast<std::uint8_t>(corner == 1 ? 1 : 0)};
  }
  return result;
}

// Writes a packet into the frame's primitive buffer and links it at the
// farthest entry, as the original's addPrim(ot + 0x1FFC) did.
template <typename Packet>
void LinkPacket(const Packet& packet, std::string_view kind) {
  constexpr std::size_t kWords = sizeof(Packet) / sizeof(std::uint32_t);
  std::uint32_t* const entry = Render::LiveFrameOrderingEntry(
      Render::LiveOrderingBase::kScene, kFarthestOrderingSlot);
  std::uint32_t* const slot =
      entry != nullptr ? Render::ReserveLivePrimitiveWords(kWords) : nullptr;
  if (slot == nullptr) {
    if (ShouldReport(droppedPackets)) {
      VS::Core::LogWarn(kChannel,
                        "background {} has no ordering entry or buffer room; "
                        "dropped ({} drops)",
                        kind, droppedPackets);
    }
    return;
  }
  std::memcpy(slot, &packet, sizeof(Packet));
  AddPrim(entry, slot);
  Scratch().primitive_cursor = slot + kWords;
}

template <typename Primitive>
void SetCorners(Primitive& primitive, const TexturedQuad& quad) {
  const auto& xy = quad.corners;
  const auto& uv = quad.texels;
  setXY4(&primitive, xy[0].x, xy[0].y, xy[1].x, xy[1].y, xy[2].x, xy[2].y,
         xy.back().x, xy.back().y);
  setUV4(&primitive, uv[0].u, uv[0].v, uv[1].u, uv[1].v, uv[2].u, uv[2].v,
         uv.back().u, uv.back().v);
  primitive.tpage = quad.texture_page;
  primitive.clut = quad.clut;
}

void RefuseNull(std::string_view entry) {
  if (ShouldReport(refusedCalls)) {
    VS::Core::LogWarn(kChannel,
                      "{} was given a null record; the call is refused", entry);
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

std::expected<Content::ImageDocument, std::string> LoadBackgroundImage(
    const Content::DataStore& store, const BackgroundFile& file) {
  if (file.lba <= 0 || file.size <= 0) {
    return std::unexpected(
        std::format("room background source has invalid LBA {} and size {}",
                    file.lba, file.size));
  }
  auto source = Content::FindConvertedSource(
      store, {.lba = static_cast<std::uint32_t>(file.lba),
              .rounded_size = static_cast<std::uint64_t>(file.size),
              .extension = ".TIM",
              .converter = "tim-png"});
  if (!source) {
    return std::unexpected("room background: " + source.error());
  }
  auto document = Content::LoadConvertedImage(store, *source);
  if (!document) return std::unexpected(document.error());
  if (!document->provenance.tim) {
    return std::unexpected(
        std::format("{} has no TIM provenance", source->entry->path));
  }
  const Content::TimOrigin& tim = *document->provenance.tim;
  if (document->image.depth != Content::ImageDepth::kIndexed8 ||
      document->image.width != (std::uint32_t{tim.image.width} * 2) ||
      document->image.height != tim.image.height ||
      document->image.palettes.size() != 1 ||
      document->image.PaletteEntries() != kBackgroundPaletteEntries ||
      document->material !=
          Content::ImageMaterial{
              .zero_word = Content::ZeroWord::kTransparent,
              .semi_transparency = Content::SemiTransparency::kOff}) {
    return std::unexpected(std::format(
        "{} is not a supported 8-bit background image", source->entry->path));
  }
  return document;
}
}  // namespace

RoomBackgroundState LoadLiveRoomBackgroundState() {
  RoomBackgroundState state{
      .offset_x = backgroundOffsetX,
      .offset_y = backgroundOffsetY,
      .parallax_x = backgroundParallaxScaleXFixed12,
      .parallax_y = backgroundParallaxScaleYFixed12,
      .texture_page_count = legacyBackgroundTextureReservationId,
      .solid = static_cast<std::uint8_t>(isSolidBackground),
      .texture_page = static_cast<std::uint8_t>(legacyBackgroundTexturePage),
      .tiled_enabled = isTiledBackgroundEnabled,
      .texture_file = queuedBackgroundTextureFile,
      .texture_uploaded = backgroundTextureUploaded,
      .layout = nativeBackgroundLayout};
  std::ranges::copy(backgroundGradientColors, state.gradient_colors.begin());
  return state;
}

void StoreLiveRoomBackgroundState(const RoomBackgroundState& state) {
  std::ranges::copy(state.gradient_colors, backgroundGradientColors);
  backgroundOffsetX = state.offset_x;
  backgroundOffsetY = state.offset_y;
  backgroundParallaxScaleXFixed12 = state.parallax_x;
  backgroundParallaxScaleYFixed12 = state.parallax_y;
  legacyBackgroundTextureReservationId = state.texture_page_count;
  isSolidBackground = static_cast<char>(state.solid);
  legacyBackgroundTexturePage = static_cast<char>(state.texture_page);
  isTiledBackgroundEnabled = state.tiled_enabled;
  queuedBackgroundTextureFile = state.texture_file;
  backgroundTextureUploaded = state.texture_uploaded;
  nativeBackgroundLayout = state.layout;
}

std::optional<RoomEnvironment> LiveRoomEnvironment() {
  const VS::Field::MpdEnvironment* const record =
      VS::Field::roomData.environment;
  if (record == nullptr) return std::nullopt;
  RoomEnvironment environment{
      .background_rotation = Math::VectorOf(record->background_rotation),
      .wind_direction = record->wind_direction,
      .wind_direction_limit = record->wind_direction_limit,
      .wind_direction_interval = record->wind_direction_interval,
      .weather_intensity = record->weather_intensity,
      .weather_intensity_limit = record->weather_intensity_limit,
      .weather_intensity_interval = record->weather_intensity_interval,
      .wind_strength = record->wind_strength,
      .wind_strength_limit = record->wind_strength_limit,
      .wind_strength_interval = record->wind_strength_interval,
      .weather_area = {.origin_x = record->weather_area[0].halves[0],
                       .origin_z = record->weather_area[0].halves[1],
                       .extent_x = record->weather_area[1].halves[0],
                       .extent_z = record->weather_area[1].halves[1]}};
  std::ranges::copy(record->background_colors,
                    environment.background_colors.begin());
  return environment;
}

std::optional<RoomEnvironment> LiveBackgroundHost::Environment() {
  return LiveRoomEnvironment();
}

void LiveBackgroundHost::StoreWeatherArea(const WeatherArea& area) {
  VS::Field::MpdEnvironment* const record = VS::Field::roomData.environment;
  if (record == nullptr) return;
  record->weather_area[0].halves[0] = area.origin_x;
  record->weather_area[0].halves[1] = area.origin_z;
  record->weather_area[1].halves[0] = area.extent_x;
  record->weather_area[1].halves[1] = area.extent_z;
}

std::optional<CollisionMapExtent> LiveBackgroundHost::CollisionMapSize() {
  const VS::Field::MpdCollisionHeader* const map =
      VS::Field::roomData.collision_map;
  if (map == nullptr) return std::nullopt;
  return CollisionMapExtent{.width = map->width, .height = map->height};
}

void LiveBackgroundHost::StoreSkyDomeColors(const SkyDomeColors& colors) {
  const std::span dome(VS::Content::battleSkyDome);
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
  setRGB3(&primitive, colors.back().red, colors.back().green,
          colors.back().blue);
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
  // The original loaded this view into the GTE for func_8009820C; no later
  // draw reads it back (#51 audit), so it travels as an argument instead.
  Render::DrawLiveSkyDome(VS::Content::battleSkyDome,
                          sizeof VS::Content::battleSkyDome, view);
}

CameraVector LiveBackgroundHost::ReferenceRotation() {
  return Math::VectorOf(backgroundReferenceVectors[0]);
}

std::expected<std::shared_ptr<const OwnedBackgroundLayout>, std::string>
LiveBackgroundHost::LoadBackgroundLayout(
    const TiledBackgroundSection& section) {
  const auto* store = Content::DataStore::Get();
  if (store == nullptr) {
    return std::unexpected("no extracted-data store is open");
  }
  if (section.layout.lba <= 0 || section.layout.size <= 0) {
    return std::unexpected(
        "room background layout has invalid source coordinates");
  }
  auto source = Content::FindConvertedSource(
      *store, {.lba = static_cast<std::uint32_t>(section.layout.lba),
               .rounded_size = static_cast<std::uint64_t>(section.layout.size),
               .extension = ".FAR",
               .converter = Content::kFarConverterName});
  if (!source) return std::unexpected(source.error());
  auto image = LoadBackgroundImage(*store, section.texture);
  if (!image) return std::unexpected(image.error());
  auto document = Content::LoadConvertedBackground(*store, *source, *image);
  if (!document) return std::unexpected(document.error());
  auto layout = std::make_shared<OwnedBackgroundLayout>();
  layout->width = static_cast<std::int32_t>(document->columns);
  layout->height = static_cast<std::int32_t>(document->rows);
  for (const auto& cell : document->cells) {
    layout->tiles.push_back(TileOf(cell));
  }
  Core::LogInfo(kChannel, "loaded native layout {} ({}x{}, image {})",
                document->id, document->columns, document->rows,
                document->image_id);
  return layout;
}

std::optional<TiledBackgroundSection> LiveBackgroundHost::TiledBackground() {
  const std::uint16_t* const words = VS::Field::roomData.tiled_background;
  if (words == nullptr) return std::nullopt;
  const std::size_t bytes = VS::Field::roomData.header.tiled_background_length;
  if (bytes < kTiledSectionBytes) {
    if (ShouldReport(refusedSections)) {
      VS::Core::LogWarn(
          kChannel,
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

std::expected<BackgroundTextureUpload, std::string>
LiveBackgroundHost::LoadBackgroundTexture(const BackgroundFile& source) {
  const Content::DataStore* const store = Content::DataStore::Get();
  if (store == nullptr) {
    return std::unexpected(
        "no extracted-data store is open for the room background");
  }
  auto document = LoadBackgroundImage(*store, source);
  if (!document) return std::unexpected(document.error());

  const Content::TimOrigin& tim = *document->provenance.tim;
  return BackgroundTextureUpload{
      .source_rect = {.x = static_cast<std::int16_t>(tim.image.x),
                      .y = static_cast<std::int16_t>(tim.image.y),
                      .width = static_cast<std::int16_t>(tim.image.width),
                      .height = static_cast<std::int16_t>(tim.image.height)},
      .pixel_words = Content::PackedWords(document->image),
      .palette_words = document->image.palettes.front()};
}

void LiveBackgroundHost::UploadBackgroundPixels(
    std::span<const std::uint16_t> words, const TextureRect& rect) {
  std::vector<std::uint32_t> packed((words.size() + 1) / 2);
  for (std::size_t i = 0; i < words.size(); i += 2) {
    const std::uint32_t high =
        i + 1 < words.size() ? std::uint32_t{words[i + 1]} << 16 : 0;
    packed[i / 2] = std::uint32_t{words[i]} | high;
  }
  RECT target{.x = rect.x, .y = rect.y, .w = rect.width, .h = rect.height};
  LoadImage(&target, packed.data());
}

void LiveBackgroundHost::UploadBackgroundPalette(
    std::span<const std::uint16_t> words, int row) {
  Render::LoadLiveClutColors(words.data(), static_cast<std::uint32_t>(row), 0,
                             static_cast<std::uint32_t>(words.size()));
}

// decomp: BATTLE.PRG func_8008E88C (src/BATTLE/BATTLE.PRG/146C.c:17310)
void LiveInitializeRoomBackground() {
  RoomWeatherState weather = LoadLiveRoomWeatherState();
  RunLive([&weather](BackgroundHost& host, RoomBackgroundState& state) {
    InitializeRoomBackground(host, state, weather);
  });
  StoreLiveRoomWeatherState(weather);
}

// decomp: BATTLE.PRG func_8008E938 (src/BATTLE/BATTLE.PRG/146C.c:17332)
void LiveInitializeRoomBackgroundColors() {
  RunLive([](BackgroundHost& host, RoomBackgroundState& state) {
    InitializeRoomBackgroundColors(host, state);
  });
}

// decomp: BATTLE.PRG func_8008EAE4 (src/BATTLE/BATTLE.PRG/146C.c:17394)
void LiveSetBackgroundParallaxScale(int parallax_x, int parallax_y) {
  RunLive([parallax_x, parallax_y](BackgroundHost& /*host*/,
                                   RoomBackgroundState& state) {
    SetBackgroundParallaxScale(state, parallax_x, parallax_y);
  });
}

// decomp: BATTLE.PRG func_8008EAF8 (src/BATTLE/BATTLE.PRG/146C.c:17400)
void LiveSetTiledBackgroundEnabled(int enabled) {
  RunLive([enabled](BackgroundHost& /*host*/, RoomBackgroundState& state) {
    SetTiledBackgroundEnabled(state, enabled);
  });
}

// decomp: BATTLE.PRG func_8008EB24 (src/BATTLE/BATTLE.PRG/146C.c:17408)
int* LiveRoomBackgroundColors() { return backgroundGradientColors; }

// decomp: BATTLE.PRG func_8008EB30 (src/BATTLE/BATTLE.PRG/146C.c:17410)
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

// decomp: BATTLE.PRG func_8008EC48 (src/BATTLE/BATTLE.PRG/146C.c:17448)
void LiveDrawRoomBackground(const VECTOR* rotation_override) {
  std::optional<CameraVector> rotation;
  if (rotation_override != nullptr) {
    rotation = Math::VectorOf(*rotation_override);
  }
  RunLive([&rotation](BackgroundHost& host, RoomBackgroundState& state) {
    DrawRoomBackground(host, state, rotation);
  });
}

// decomp: BATTLE.PRG func_8008EFCC (src/BATTLE/BATTLE.PRG/146C.c:17515)
int LiveQueueTiledBackgroundLoad(int texture_page) {
  return RunLive(
      [texture_page](BackgroundHost& host, RoomBackgroundState& state) {
        return QueueTiledBackgroundLoad(host, state, texture_page);
      });
}

// decomp: BATTLE.PRG func_8008F0FC (src/BATTLE/BATTLE.PRG/146C.c:17557)
int LiveFinishTiledBackgroundLoad() {
  return RunLive([](BackgroundHost& host, RoomBackgroundState& state) {
    return FinishTiledBackgroundLoad(host, state);
  });
}

// decomp: BATTLE.PRG func_8008F234 (src/BATTLE/BATTLE.PRG/146C.c:17587)
int LiveReleaseTiledBackground() {
  return RunLive([](BackgroundHost& host, RoomBackgroundState& state) {
    return ReleaseTiledBackground(host, state);
  });
}
}  // namespace VS::Field
