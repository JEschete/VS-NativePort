// Hand-maintained (tools/port/owned.csv).
#include "Content/OverlayWorkspace.hpp"

#include <cstdint>

#include "Core/Runtime.hpp"

namespace VS::Content {
namespace {
constexpr int kWorkspaceBusyDiagnostic = 0x66;
constexpr int kWorkspaceNotOwnedDiagnostic = 0x67;
}  // namespace

// decomp: BATTLE.PRG func_8007E180 (src/BATTLE/BATTLE.PRG/146C.c:9562)
bool AcquireOverlayWorkspace(OverlayWorkspaceHost& host,
                             std::int32_t owner_tag) {
  if (host.Owner() != 0) {
    Core::IgnoreDiagnostic(kWorkspaceBusyDiagnostic, 0);
    return false;
  }
  host.SetOwner(static_cast<std::uint8_t>(owner_tag));
  return true;
}

// decomp: BATTLE.PRG func_8007E1C0 (src/BATTLE/BATTLE.PRG/146C.c:9572)
bool ReleaseOverlayWorkspace(OverlayWorkspaceHost& host,
                             std::int32_t owner_tag) {
  if (std::int32_t{host.Owner()} != owner_tag) {
    Core::IgnoreDiagnostic(kWorkspaceNotOwnedDiagnostic, 0);
    return false;
  }
  host.SetOwner(0);
  return true;
}

}  // namespace VS::Content
