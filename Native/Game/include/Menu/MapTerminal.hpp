#pragma once

#include <cstdint>
#include <optional>
#include <span>

#include "Core/Math/Matrix.hpp"
#include "Menu/MapLoading.hpp"

namespace VS::Menu::map {
// MENU5.PRG 4D8.c:905-971, 1788-1832, 1880-1963.
struct MapCamera {
  MapAngles position;
  Core::Math::Transform3 transform;
};

[[nodiscard]] MapCamera ApplyCamera(MapAngles angles, std::int32_t distance,
                                    MapAngles previous_position);

struct FramebufferStrip {
  std::int16_t x = 0;
  std::int16_t y = 0;
  std::int16_t width = 0;
  std::int16_t height = 0;
  std::uint8_t texture_y = 0;
  bool raw_texture = false;
  // The second Paling strip deliberately leaves its old CLUT untouched.
  bool clear_clut = true;

  friend bool operator==(const FramebufferStrip&,
                         const FramebufferStrip&) = default;
};

class FramebufferHost {
 public:
  virtual ~FramebufferHost() = default;
  virtual bool TexturePage(std::int32_t slot, std::uint16_t page) = 0;
  virtual bool Strip(std::int32_t slot, const FramebufferStrip& strip) = 0;
};

[[nodiscard]] bool DarkenBackground(FramebufferHost& host, bool back_buffer);
[[nodiscard]] bool DrawPalingDistortion(FramebufferHost& host, bool back_buffer,
                                        std::int32_t frame_counter);

class MapFrameHost {
 public:
  virtual ~MapFrameHost() = default;
  virtual void DrawControls() = 0;
  virtual bool EnableMaskWrite() = 0;
  virtual void DrawDistortion() = 0;
  virtual void SetPalingCaption() = 0;
  virtual void ApplyView() = 0;
  virtual void Darken() = 0;
  virtual void DrawRooms() = 0;
};

[[nodiscard]] bool DrawFrame(MapFrameHost& host, bool jammed,
                             bool leaving_jammed_scene);

// Original controller bits, not device-specific receive-buffer records.
enum class MapButton : std::uint32_t {
  kL2 = 1,
  kR2 = 2,
  kL1 = 4,
  kR1 = 8,
  kTriangle = 0x10,
  kCircle = 0x20,
  kCross = 0x40,
  kSelect = 0x100,
  kStart = 0x800,
  kUp = 0x1000,
  kRight = 0x2000,
  kDown = 0x4000,
  kLeft = 0x8000,
};

inline constexpr std::uint8_t kMapStickCenter = 128;

struct MapInput {
  std::uint32_t pressed = 0;
  std::uint32_t held = 0;
  std::uint32_t repeated = 0;
  std::uint8_t left_x = kMapStickCenter;
  std::uint8_t left_y = kMapStickCenter;
  std::uint8_t right_x = kMapStickCenter;
  std::uint8_t right_y = kMapStickCenter;
  std::uint16_t field_yaw = 0;
};

struct MapMenuState {
  SceneMapView view;
  std::int32_t selected_map = 0;
  std::int32_t label_y = 0;
};

class MapMenuHost {
 public:
  virtual ~MapMenuHost() = default;
  virtual void SetFarColorBlack() = 0;
  virtual void SetBackgroundPriority(std::int32_t slot,
                                     std::int32_t brightness) = 0;
  virtual void SetVisible(bool visible) = 0;
  virtual void PlaySound(std::int32_t volume) = 0;
  virtual bool InitializeRoomText() = 0;
  virtual bool ShowRoomName(std::int32_t room) = 0;
  virtual void DismissRoomText() = 0;
  virtual std::int32_t PollAreas(bool initialize) = 0;
  virtual void ShowAreaName(std::int32_t scene, bool current) = 0;
  virtual bool BeginSceneRead(std::int32_t scene, DiscFileRange file) = 0;
  virtual std::int32_t SceneReadState() = 0;
  // Releases the read, replaces the old buffer, resolves names and visits,
  // and scales by four. The caller then chooses and centers the room.
  virtual bool InstallSceneRead(std::int32_t scene, DiscFileRange file) = 0;
  virtual bool FreeScene() = 0;
  virtual std::int32_t InitialRoom() = 0;
  virtual void CenterOnRoom(std::int32_t room) = 0;
  virtual std::int32_t SelectRoom(std::int32_t direction,
                                  std::int32_t current) = 0;
  virtual void SetCenterTarget(std::int32_t room) = 0;
  virtual void DrawFrame() = 0;
  virtual void AnimateCentering() = 0;
};

// F04151, _menuManager, MENU5.PRG 4D8.c:523-893. None is a native refusal
// (invalid scene/selection or failed resource operation), never completion.
[[nodiscard]] std::optional<std::int32_t> UpdateMapMenu(
    MapMenuHost& host, MapMenuState& state, const MapInput& input,
    std::span<const std::uint8_t> paling,
    std::span<const std::int32_t> scene_rows,
    std::span<const DiscFileRange> files);

struct MapScreenState {
  std::int32_t load_delay = 0;
  std::int32_t scene_loaded = 0;
  std::int32_t menu_result = 0;

  friend bool operator==(const MapScreenState&,
                         const MapScreenState&) = default;
};

class MapScreenHost {
 public:
  virtual ~MapScreenHost() = default;
  virtual bool CaptureBackground() = 0;
  virtual std::int32_t LoadBackground(bool begin) = 0;
  virtual std::int32_t LoadScene() = 0;
  virtual void FadeBackground(bool fade) = 0;
  virtual void SetFieldRendering(bool enabled) = 0;
  virtual void SetHudHidden(bool hidden) = 0;
  virtual void SetHudSlide(std::int32_t offset) = 0;
  virtual std::int32_t MenuRowsIdle() = 0;
  virtual std::int32_t UpdateMenu() = 0;
  virtual void RestoreMenuPriority() = 0;
  virtual void ClearCommand() = 0;
  virtual void ClearRows() = 0;
  virtual bool RestoreBackground() = 0;
  virtual bool ReleaseBackground() = 0;
  virtual void SetDestination(std::uint8_t menu) = 0;
  virtual void ShowMenu() = 0;
};

// F04149, vs_menu5_exec, MENU5.PRG 4D8.c:240-367.
[[nodiscard]] std::optional<std::int32_t> UpdateScreen(
    MapScreenHost& host, MapScreenState& state, std::uint8_t& step,
    std::int32_t background_fade_step);
}  // namespace VS::Menu::map
