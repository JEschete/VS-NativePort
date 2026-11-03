// Hand-maintained (tools/port/owned.csv).
// Live binding for the overlay workspace rules (OverlayWorkspace.hpp): the
// owner tag is the legacy byte Content_overlayWorkspaceOwner.
#include "Content/OverlayWorkspaceLive.hpp"

#include <cstdint>

#include "Content/OverlayWorkspace.hpp"
#include "OverlayWorkspaceLiveState.h"

extern "C" {
// decomp: D_800F18F8 (D_800F18F8@BATTLE/BATTLE.PRG)
char Content_overlayWorkspaceOwner;
}

namespace VS::Content {
namespace {
static_assert(sizeof Content_overlayWorkspaceOwner == 1);

class LiveOverlayWorkspace final : public OverlayWorkspaceHost {
 public:
  [[nodiscard]] std::uint8_t Owner() const override {
    return static_cast<std::uint8_t>(Content_overlayWorkspaceOwner);
  }
  void SetOwner(std::uint8_t owner) override {
    Content_overlayWorkspaceOwner = static_cast<char>(owner);
  }
};
}  // namespace

int AcquireLiveOverlayWorkspace(int owner_tag) {
  LiveOverlayWorkspace host;
  return AcquireOverlayWorkspace(host, owner_tag) ? 1 : 0;
}

int ReleaseLiveOverlayWorkspace(int owner_tag) {
  LiveOverlayWorkspace host;
  return ReleaseOverlayWorkspace(host, owner_tag) ? 1 : 0;
}

}  // namespace VS::Content
