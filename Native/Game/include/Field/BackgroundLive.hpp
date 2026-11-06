#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <optional>
#include <vector>

#include "Field/Background.hpp"
#include "Legacy/Psx.h"

// Live binding for the room background (Background.hpp) over the copied
// background globals (BackgroundLiveState.h), the room record, the frame
// scratch camera, the high-end heap, the disc queue, VRAM and the frame's
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
  [[nodiscard]] std::optional<BackgroundLayout> Layout(
      BackgroundBufferId layout) override;
  [[nodiscard]] std::optional<TiledBackgroundSection> TiledBackground()
      override;
  [[nodiscard]] bool ReserveTexturePages(int first_page, int page_count,
                                         int owner) override;
  void RetireTexturePages(int first_page, int page_count, int owner) override;
  [[nodiscard]] BackgroundBufferId AllocateHighBuffer(
      std::int32_t bytes) override;
  void FreeHighBuffer(BackgroundBufferId buffer) override;
  [[nodiscard]] BackgroundReadId AllocateRead(
      const BackgroundFile& file) override;
  void EnqueueRead(BackgroundReadId read,
                   BackgroundBufferId destination) override;
  [[nodiscard]] BackgroundReadId CompleteRefusedRead(
      const BackgroundFile& file, BackgroundBufferId destination) override;
  [[nodiscard]] bool ReadLoaded(BackgroundReadId read) override;
  void ReleaseRead(BackgroundReadId read) override;
  void StampTimId(BackgroundBufferId buffer, std::uint32_t id) override;
  [[nodiscard]] BackgroundTim ParseTim(BackgroundBufferId buffer) override;
  void UploadTimPixels(BackgroundBufferId buffer,
                       const TextureRect& rect) override;
  void UploadTimClut(BackgroundBufferId buffer, int row, int first_color,
                     int count) override;

 private:
  // The cells Layout read out of the heap buffer, which the layout it
  // returns views.
  std::vector<BackgroundTile> tiles_;
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
void LiveDrawRoomBackground(const VECTOR* rotation_override);
// func_8008EFCC (146C.c:17638).
int LiveQueueTiledBackgroundLoad(int texture_page);
// func_8008F0FC (146C.c:17680).
int LiveFinishTiledBackgroundLoad();
// func_8008F234 (146C.c:17710).
int LiveReleaseTiledBackground();
}  // namespace VS::Field
