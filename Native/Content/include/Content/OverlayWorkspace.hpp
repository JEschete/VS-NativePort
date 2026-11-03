#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>

// The shared overlay workspace: one loader or menu at a time may claim the
// overlay area for its data. Owners identify themselves by a byte tag (2 for
// the effect package loader, 6 for the screen effects and the menu
// framework); 0 means the workspace is free.
namespace VS::Content {

// The stored owner tag (decomp D_800F18F8, BATTLE.PRG 146C.c). The live
// binding (OverlayWorkspaceLive.hpp) keeps it in the legacy byte.
class OverlayWorkspaceHost {
 public:
  OverlayWorkspaceHost() = default;
  OverlayWorkspaceHost(const OverlayWorkspaceHost&) = delete;
  OverlayWorkspaceHost& operator=(const OverlayWorkspaceHost&) = delete;
  OverlayWorkspaceHost(OverlayWorkspaceHost&&) = delete;
  OverlayWorkspaceHost& operator=(OverlayWorkspaceHost&&) = delete;
  virtual ~OverlayWorkspaceHost() = default;

  [[nodiscard]] virtual std::uint8_t Owner() const = 0;
  virtual void SetOwner(std::uint8_t owner) = 0;
};

// Claims the workspace for `owner_tag` (its low byte is stored, as the
// original's byte store did). Returns false, and reports diagnostic 0x66,
// while another owner holds it.
bool AcquireOverlayWorkspace(OverlayWorkspaceHost& host,
                             std::int32_t owner_tag);

// Frees the workspace when `owner_tag` equals the stored byte tag (the whole
// word is compared, so a tag outside 0-255 never matches). Otherwise returns
// false and reports diagnostic 0x67.
bool ReleaseOverlayWorkspace(OverlayWorkspaceHost& host,
                             std::int32_t owner_tag);

}  // namespace VS::Content
