// Hand-maintained (tools/port/owned.csv).
// Live binding for the gazette camera and panels (GazetteCamera.hpp):
// implements the panel target over the frame's primitive buffer and scene
// ordering table, and the overview host over the frame camera, the GTE
// projection offset, the compact font and the actor preview.
#include "Menu/Score/GazetteCameraLive.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "Core/Log.hpp"
#include "Core/Math/Matrix.hpp"
#include "Core/Math/MatrixLive.hpp"
#include "Core/Scratch.hpp"
#include "Menu/Framework/ActorPreviewLive.hpp"
#include "Menu/Score/GazetteCamera.hpp"
#include "Menu/Score/Records.hpp"
#include "Menu/Score/ScoreScreen.hpp"
#include "Render/Display.hpp"
#include "Render/GameFontLive.hpp"
#include "Render/Render.hpp"
#include "Render/RoomDrawLive.hpp"
#include "Render/SpritesLive.hpp"
#include "Render/UiPackets.hpp"
#include "Render/UiPacketsLive.hpp"
#include "Render/UiPrimitives.hpp"

namespace VS::Menu::Score {
namespace {
constexpr std::size_t kTagWords = 1;
static_assert(sizeof(u_long) == sizeof(std::uint32_t));
static_assert(sizeof(VECTOR) == kCameraVectorWords * sizeof(std::int32_t));

std::uint64_t refusals = 0;

void Refuse(std::string_view what) {
  if (std::has_single_bit(++refusals)) {
    VS::Core::LogWarn("menu.score", "gazette camera: {} ({} refusals)", what,
                      refusals);
  }
}

u_long* OrderingEntry(std::int32_t slot) {
  return Render::LiveFrameOrderingEntry(Render::LiveOrderingBase::kScene, slot);
}

// Links each panel packet in front of its scene ordering-table entry or,
// while tallying, only adds up the packets' words and checks their entries,
// so a call that does not fit is refused before anything is linked.
class LivePanelTarget final : public PanelTarget {
 public:
  enum class Mode : std::uint8_t { kTally, kLink };

  explicit LivePanelTarget(Mode mode) : mode_(mode) {}

  void DrawLine(std::int32_t slot, const PanelLine& line) override {
    Queue(slot, PackPanelLine(line));
  }
  void DrawFlatQuad(std::int32_t slot, const PanelFlatQuad& quad) override {
    Queue(slot, PackPanelFlatQuad(quad));
  }
  void DrawShadedQuad(std::int32_t slot, const PanelShadedQuad& quad) override {
    Queue(slot, PackPanelShadedQuad(quad));
  }
  void SetTexturePage(std::int32_t slot, std::uint16_t texture_page) override {
    Queue(slot, PackPanelTexturePage(texture_page));
  }

  [[nodiscard]] std::size_t TalliedWords() const { return tallied_words_; }
  [[nodiscard]] bool MissedEntry() const { return missed_entry_; }

 private:
  template <std::size_t Words>
  void Queue(std::int32_t slot, const std::array<std::uint32_t, Words>& body) {
    u_long* const entry = OrderingEntry(slot);
    if (mode_ == Mode::kTally) {
      tallied_words_ += kTagWords + Words;
      missed_entry_ = missed_entry_ || entry == nullptr;
      return;
    }
    Render::LivePacketQueue queue(entry);
    std::array<std::uint32_t, kTagWords + Words> packet{
        Render::PackTagWord(Words, queue.ChainLink())};
    std::ranges::copy(body, packet.begin() + kTagWords);
    queue.Emit(packet);
  }

  Mode mode_;
  std::size_t tallied_words_ = 0;
  bool missed_entry_ = false;
};

// Tallies `draw` (which returns the core's result) and links its packets
// only when the core accepted its inputs and every packet fits the frame.
template <typename Draw>
void DrawChecked(std::string_view refused, Draw draw) {
  LivePanelTarget tally(LivePanelTarget::Mode::kTally);
  std::string_view problem = refused;
  if (draw(tally) != PanelDrawResult::kRefused) {
    if (tally.MissedEntry()) {
      problem = "no frame ordering-table entry";
    } else if (Render::ReserveLivePrimitiveWords(tally.TalliedWords()) ==
               nullptr) {
      problem = "the primitive buffer is full";
    } else {
      LivePanelTarget link(LivePanelTarget::Mode::kLink);
      draw(link);
      return;
    }
  }
  Refuse(problem);
}

void DrawPanel(const PanelRect& rect, std::int32_t gradient) {
  DrawChecked("gradient panel colour outside the palette",
              [&](PanelTarget& target) {
                return DrawGradientPanel(rect, gradient, target);
              });
}

void DrawFooter(std::int32_t top) {
  DrawChecked("footer", [&](PanelTarget& target) {
    DrawFooterBackground(top, target);
    return PanelDrawResult::kDrawn;
  });
}

void DrawEdgeFade() {
  DrawChecked("edge fade", [](PanelTarget& target) {
    DrawViewerEdgeFade(target);
    return PanelDrawResult::kDrawn;
  });
}

CameraVector CameraVectorOf(const VECTOR& vector) {
  return {static_cast<std::int32_t>(vector.vx),
          static_cast<std::int32_t>(vector.vy),
          static_cast<std::int32_t>(vector.vz),
          static_cast<std::int32_t>(vector.pad)};
}

void StoreCameraVector(VECTOR& vector, const CameraVector& words) {
  const auto [x, y, z, pad] = words;
  vector.vx = x;
  vector.vy = y;
  vector.vz = z;
  vector.pad = pad;
}

// The viewer camera from the score screen's settings; the frame camera's
// position and angles take its eye and angles, their pads untouched.
Core::Math::Transform3 BuildAndStoreViewerCamera() {
  const ViewerCamera camera =
      BuildViewerCamera({.pitch = cameraAngles.vx,
                         .yaw = cameraAngles.vy,
                         .roll = cameraAngles.vz,
                         .distance = cameraDistance,
                         .height_offset = cameraHeightOffset});
  VS::Render::Camera& frame_camera = VS::Render::FrameScratchpad().camera;
  frame_camera.position.vx = camera.position.x;
  frame_camera.position.vy = camera.position.y;
  frame_camera.position.vz = camera.position.z;
  frame_camera.angles.vx = camera.angles.x;
  frame_camera.angles.vy = camera.angles.y;
  frame_camera.angles.vz = camera.angles.z;
  return camera.view;
}

class LiveOverviewHost final : public OverviewHost {
 public:
  void DrawViewerEdgeFade() override { DrawEdgeFade(); }
  ProjectionOffset GetProjectionOffset() override {
    const VS::Render::ScreenOffset offset =
        Render::GetLiveProjectionScreenOffset();
    return {.x = offset.x, .y = offset.y};
  }
  void SetProjectionOffset(const ProjectionOffset& offset) override {
    Render::SetLiveProjectionScreenOffset(
        VS::Render::ScreenOffset{.x = offset.x, .y = offset.y});
  }
  Core::Math::Transform3 BuildViewerCamera() override {
    return BuildAndStoreViewerCamera();
  }
  void DrawActorPreview(const Core::Math::Transform3& camera) override {
    MATRIX matrix{};
    Core::Math::StoreTransform(matrix, camera);
    Framework::DrawLiveActorPreview(&matrix);
  }
  void DrawText(std::string_view text, std::uint32_t position,
                std::uint32_t color) override {
    u_long* const chain = OrderingEntry(kPanelSlot);
    if (chain == nullptr) {
      // The compact font would fall back to the overlay table instead.
      Refuse("overview text has no frame ordering-table entry");
      return;
    }
    const std::string terminated(text);
    Render::DrawLiveCompactTextColor(terminated.c_str(),
                                     static_cast<int>(position),
                                     static_cast<int>(color), chain);
  }
  void DrawGradientPanel(const PanelRect& rect,
                         std::int32_t gradient) override {
    DrawPanel(rect, gradient);
  }
  void DrawFooterBackground(std::int32_t top) override { DrawFooter(top); }
};
}  // namespace

// decomp: MENU9.PRG _restoreCamera (src/MENU/MENU9.PRG/238.c:1402)
void RestoreLiveCamera() {
  VS::Render::Camera& frame_camera = VS::Render::FrameScratchpad().camera;
  CameraPose camera{.position = CameraVectorOf(frame_camera.position),
                    .angles = CameraVectorOf(frame_camera.angles)};
  RestoreCamera({.position = CameraVectorOf(cameraPositionBackup),
                 .angles = CameraVectorOf(cameraAnglesBackup)},
                camera);
  StoreCameraVector(frame_camera.position, camera.position);
  StoreCameraVector(frame_camera.angles, camera.angles);
}

// decomp: MENU9.PRG _buildCameraMatrix (src/MENU/MENU9.PRG/238.c:1474)
void BuildLiveViewerCamera(MATRIX* camera_matrix) {
  if (camera_matrix == nullptr) {
    Refuse("null viewer camera matrix");
    return;
  }
  Core::Math::StoreTransform(*camera_matrix, BuildAndStoreViewerCamera());
}

// decomp: MENU9.PRG _renderEnemyInfoBg (src/MENU/MENU9.PRG/238.c:1552)
void DrawLiveEnemyBanner(int x, int y, int width, int height, int gradient) {
  const PanelRect rect{.x = x, .y = y, .width = width, .height = height};
  DrawChecked("enemy banner colour or height outside its bounds",
              [&](PanelTarget& target) {
                return DrawEnemyBanner(rect, gradient, target);
              });
}

// decomp: MENU9.PRG _renderGradientQuad (src/MENU/MENU9.PRG/238.c:1594)
void DrawLiveGradientPanel(int x, int y, int width, int height, int gradient) {
  DrawPanel({.x = x, .y = y, .width = width, .height = height}, gradient);
}

// decomp: MENU9.PRG _renderFooterBg (src/MENU/MENU9.PRG/238.c:1645)
void DrawLiveFooterBackground(int top) { DrawFooter(top); }

// decomp: MENU9.PRG _renderTopMenu (src/MENU/MENU9.PRG/238.c:1679)
void RenderLiveOverview(int animation_step) {
  LiveOverviewHost host;
  RenderOverview(host, animation_step, VS::Menu::Score::score);
}

// decomp: MENU9.PRG _renderScreenEdgeFade (src/MENU/MENU9.PRG/238.c:2146)
void DrawLiveViewerEdgeFade() { DrawEdgeFade(); }

std::uint64_t LiveGazetteCameraRefusals() noexcept { return refusals; }
}  // namespace VS::Menu::Score
