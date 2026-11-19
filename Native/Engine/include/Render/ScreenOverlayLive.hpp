#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>

#include "Legacy/Psx.h"
#include "Render/ScreenOverlay.hpp"

namespace VS::Render {
struct FramebufferEffectTranslation;
}  // namespace VS::Render

// Live binding for the screen overlays (ScreenOverlay.cpp): the framebuffer
// effect's state and its packet block from the high end of the game heap, and
// the Field transition globals with their fade and wipe packets. Each call
// keeps its original call shape; a null record or ordering table is refused,
// counted and reported instead of dereferenced.
namespace VS::Render {
void DrawLiveScreenTransition(void* ordering_table);
void ResetLiveFramebufferEffect();
// Start-up, over a fresh heap: drops the packet block without freeing it.
void InitializeLiveFramebufferEffect();
// Only after the old frame producer has stopped, before its heap is replaced.
// Submitted commands are synchronous; abandon unsubmitted frame bindings and
// free the effect packets. Refusal preserves both owner and bindings.
[[nodiscard]] bool AbandonLiveFrameBeforeHeapReset();
void DrawLiveFramebufferEffect(void* ordering_table);
void SetLiveFramebufferEffectEnabled(int enabled);
void SetLiveFramebufferEffectRotation(int roll_angle);
void SetLiveFramebufferEffectScale(
    const VS::Render::FramebufferEffectTranslation* scale);
void SetLiveFramebufferEffectTint(const CVECTOR* tint);
void SetLiveFramebufferEffectOffset(
    const VS::Render::FramebufferEffectTranslation* offset);
void SetLiveFramebufferEffectBlendMode(int blend_mode);
void SetLiveFramebufferEffectDepth(u_int ordering_depth);
void SetLiveFramebufferPreviousFrame(int use_previous_frame);
// Writes x, the x scale again as y (the original quirk) and 0.
void ReadLiveFramebufferEffectScale(
    VS::Render::FramebufferEffectTranslation* output);
// Writes r, g and b; cd is left as it was.
void ReadLiveFramebufferEffectTint(CVECTOR* output);
// Writes x and y; the two reserved words are left as they were.
void ReadLiveFramebufferEffectOffset(
    VS::Render::FramebufferEffectTranslation* output);

// True while the effect is starting or drawing (GameLoop.cpp's frame wait).
[[nodiscard]] bool IsLiveFramebufferEffectShowing() noexcept;
[[nodiscard]] const FramebufferEffect& LiveFramebufferEffect() noexcept;
[[nodiscard]] bool HasLiveFramebufferEffectPackets() noexcept;
// Null records and ordering tables, packet blocks the heap could not supply
// and frames drawn without one, since start-up.
[[nodiscard]] std::uint64_t RefusedLiveScreenOverlayCount() noexcept;
}  // namespace VS::Render
