#pragma once
// Hand-maintained (tools/port/owned.csv).

// Live binding for the overlay workspace rules (OverlayWorkspace.hpp) over the
// legacy owner byte Content_overlayWorkspaceOwner. Generated callers keep the
// original int owner tag and 1/0 result.
namespace VS::Content {

int AcquireLiveOverlayWorkspace(int owner_tag);
int ReleaseLiveOverlayWorkspace(int owner_tag);

}  // namespace VS::Content
