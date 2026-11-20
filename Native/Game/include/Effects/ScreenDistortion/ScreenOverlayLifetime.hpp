#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>

#include "Effects/ScreenDistortion/RadialWarp.hpp"
#include "Effects/ScreenDistortion/ScreenOverlays.hpp"

namespace VS::Content {
struct DiscQueueSlot;
}  // namespace VS::Content

// The lifetime of the SCREFF2 screen overlay controller (BATTLE.PRG): attach
// it and start reading SCREFF2 into the secondary overlay slot, poll the read
// each frame until it lands, draw the enabled overlays, then detach it, retire
// the feedback pages and restore the defaults. The cores reach the disc queue,
// overlay workspace and overlay draws only through the ports below
// (ScreenOverlaysLive.cpp binds them).
namespace VS::Effects::ScreenDistortion {
// The overlay workspace owner tag SCREFF2 holds while attached.
inline constexpr int kScreenOverlayWorkspaceOwner = 6;
// The radial warp scale that leaves the frame unwarped (1.0).
inline constexpr std::uint16_t kNeutralRadialWarpScale = 0x1000;

// The controller (D_800EB9B4_t) and whether D_800EB9B4 points at it.
struct ScreenOverlayLifetime {
  bool attached{};
  // +0: nonzero while the SCREFF2 read is pending.
  std::uint8_t loading{};
  ScreenOverlayControls controls{};
  // +0xC: the SCREFF2 read request; kept after it is released.
  VS::Content::DiscQueueSlot* read{};
};

// The disc queue, overlay workspace and post-load wait the lifetime uses.
class ScreenOverlayLoader {
 public:
  ScreenOverlayLoader() = default;
  ScreenOverlayLoader(const ScreenOverlayLoader&) = delete;
  ScreenOverlayLoader& operator=(const ScreenOverlayLoader&) = delete;
  ScreenOverlayLoader(ScreenOverlayLoader&&) = delete;
  ScreenOverlayLoader& operator=(ScreenOverlayLoader&&) = delete;
  virtual ~ScreenOverlayLoader() = default;

  virtual void AcquireWorkspace(int owner_tag) = 0;
  virtual void ReleaseWorkspace(int owner_tag) = 0;
  // A request for SCREFF2.PRG, or null when the queue refused it.
  [[nodiscard]] virtual VS::Content::DiscQueueSlot* AllocateOverlayRead() = 0;
  // Queues the request into the secondary overlay slot.
  virtual void EnqueueOverlayRead(VS::Content::DiscQueueSlot* read) = 0;
  // Reads SCREFF2.PRG without a request after a refusal and returns the
  // completed record that polls and releases accept.
  [[nodiscard]] virtual VS::Content::DiscQueueSlot*
  CompleteRefusedOverlayRead() = 0;
  [[nodiscard]] virtual bool OverlayReadLoaded(
      const VS::Content::DiscQueueSlot* read) = 0;
  virtual void ReleaseOverlayRead(VS::Content::DiscQueueSlot* read) = 0;
  virtual void WaitForOverlayReady() = 0;
};

// The SCREFF2 overlay draws (func_800F9BD8, func_800F986C, func_800F9FB8).
class ScreenOverlayPainter {
 public:
  ScreenOverlayPainter() = default;
  ScreenOverlayPainter(const ScreenOverlayPainter&) = delete;
  ScreenOverlayPainter& operator=(const ScreenOverlayPainter&) = delete;
  ScreenOverlayPainter(ScreenOverlayPainter&&) = delete;
  ScreenOverlayPainter& operator=(ScreenOverlayPainter&&) = delete;
  virtual ~ScreenOverlayPainter() = default;

  virtual void WarpPolygons(RadialWarpSettings settings) = 0;
  virtual void DrawFeedbackTiles(std::uint16_t feedback_layer) = 0;
  virtual void DrawVignette() = 0;
};

// func_800CA97C: detached, every controller word cleared, the warp scale 1.0.
void ResetScreenOverlay(ScreenOverlayLifetime& lifetime) noexcept;

// _loadScreff2: a nonzero `begin_load` marks the controller loading and
// queues the SCREFF2 read (reading it at once when the queue is full); zero
// polls the read and, once it has landed, releases it, clears loading and
// waits for the overlay. The original read through the controller without a
// check; callers keep it attached.
void LoadScreenOverlay(ScreenOverlayLifetime& lifetime, std::int32_t begin_load,
                       ScreenOverlayLoader& loader);

// func_800CB708: nothing while detached; polls the read while loading;
// otherwise warps when the warp words are not (1.0, 0), then draws the
// feedback tiles for a nonzero layer and the vignette when enabled.
void DrawScreenOverlay(ScreenOverlayLifetime& lifetime,
                       ScreenOverlayLoader& loader,
                       ScreenOverlayPainter& painter);

// func_800CB79C: when detached, attaches, takes the overlay workspace and
// starts the SCREFF2 read. Attached controllers are left alone.
void AcquireScreenOverlay(ScreenOverlayLifetime& lifetime,
                          ScreenOverlayLoader& loader);

// func_800CB7DC: when attached, returns the overlay workspace, retires the
// feedback pages when the pages-reserved byte is exactly 1, then resets. A
// pending read is neither cancelled nor released, as in the original.
void ReleaseScreenOverlay(ScreenOverlayLifetime& lifetime,
                          ScreenOverlayLoader& loader,
                          FeedbackTexturePages& pages);
}  // namespace VS::Effects::ScreenDistortion
