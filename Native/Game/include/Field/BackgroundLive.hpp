#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <optional>
#include <string>
#include <vector>

#include "Field/Background.hpp"
#include "Legacy/Psx.hpp"

// Live binding for the room background (Background.hpp) over the copied
// background globals (BackgroundLiveState.hpp), the room record, the frame
// scratch camera, typed native layouts, converted images and the frame's
// primitive buffer. Each entry loads the globals into a RoomBackgroundState,
// runs the typed core and stores them back. The entries keep the original
// call shapes (BATTLE.PRG 146C.c:17431-17723) for the generated room,
// script and weather callers; a null record is refused with a diagnostic.
namespace VS::Field {
// The background globals as a typed value, and back.
[[nodiscard]] RoomBackgroundState LoadLiveRoomBackgroundState();
void StoreLiveRoomBackgroundState(const RoomBackgroundState& state);

// vs_battle_roomData.environment's background and weather fields, absent
// outside a room.
[[nodiscard]] std::optional<RoomEnvironment> LiveRoomEnvironment();

// The host over the live systems. A packet the frame's primitive buffer or
// ordering table cannot take is dropped with a diagnostic; the original
// wrote past the buffer.
class LiveBackgroundHost final : public BackgroundHost {
 public:
  [[nodiscard]] std::optional<RoomEnvironment> Environment() override;
  void StoreWeatherArea(const WeatherArea& area) override;
  [[nodiscard]] std::optional<CollisionMapExtent> CollisionMapSize() override;
  void StoreSkyDomeColors(const SkyDomeColors& colors) override;
  [[nodiscard]] ViewCamera FrameCamera() override;
  [[nodiscard]] RoomTint ActiveRoomTint() override;
  void AddFlatTexturedQuad(const TexturedQuad& quad) override;
  void AddShadedTexturedQuad(const TexturedQuad& quad) override;
  void AddFill(const FillRect& fill) override;
  void DrawSkyDome(const CameraVector& position, const CameraVector& angles,
                   bool scale_compensated) override;
  [[nodiscard]] CameraVector ReferenceRotation() override;
  [[nodiscard]] std::expected<std::shared_ptr<const OwnedBackgroundLayout>,
                              std::string>
  LoadBackgroundLayout(const TiledBackgroundSection& section) override;
  [[nodiscard]] std::optional<TiledBackgroundSection> TiledBackground()
      override;
  [[nodiscard]] bool ReserveTexturePages(std::uint8_t& first_page,
                                         int page_count, int owner) override;
  void RetireTexturePages(int first_page, int page_count, int owner) override;
  void UploadBackgroundPixels(std::span<const std::uint16_t> words,
                              const TextureRect& rect) override;
  void UploadBackgroundPalette(std::span<const std::uint16_t> words,
                               int row) override;
};

// func_8008E88C (146C.c:17431).
void LiveInitializeRoomBackground();
// func_8008E938 (146C.c:17453).
void LiveInitializeRoomBackgroundColors();
// func_8008EAE4 (146C.c:17517).
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void LiveSetBackgroundParallaxScale(int parallax_x, int parallax_y);
// func_8008EAF8 (146C.c:17523).
void LiveSetTiledBackgroundEnabled(int enabled);
// func_8008EB24 (146C.c:17531): the five colours last set.
[[nodiscard]] int* LiveRoomBackgroundColors();
// func_8008EB30 (146C.c:17533): reads five colour words.
void LiveSetRoomBackgroundColors(const int* gradient_colors);
// func_8008EC48 (146C.c:17571); a null override uses the room's turn.
void LiveDrawRoomBackground(const VS::Core::Gte::VECTOR* rotation_override);
// func_8008EFCC (146C.c:17638).
int LiveQueueTiledBackgroundLoad(int texture_page);
// func_8008F0FC (146C.c:17680).
int LiveFinishTiledBackgroundLoad();
// func_8008F234 (146C.c:17710).
int LiveReleaseTiledBackground();
}  // namespace VS::Field
