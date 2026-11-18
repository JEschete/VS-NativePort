// Hand-maintained (tools/port/owned.csv).
// Live binding for the overlay workspace rules (OverlayWorkspace.hpp): the
// owner tag is the legacy byte VS::Content::overlayWorkspaceOwner.
#include "Content/OverlayWorkspaceLive.hpp"

#include <cstdint>

#include "Content/OverlayWorkspace.hpp"

// decomp: D_800F18F8 (D_800F18F8@BATTLE/BATTLE.PRG)
char VS::Content::overlayWorkspaceOwner;

namespace VS::Content {
namespace {
static_assert(sizeof overlayWorkspaceOwner == 1);

class LiveOverlayWorkspace final : public OverlayWorkspaceHost {
 public:
  [[nodiscard]] std::uint8_t Owner() const override {
    return static_cast<std::uint8_t>(overlayWorkspaceOwner);
  }
  void SetOwner(std::uint8_t owner) override {
    overlayWorkspaceOwner = static_cast<char>(owner);
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
