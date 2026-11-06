// Hand-maintained (tools/port/owned.csv).
#include "Save/MenuAssets.hpp"

namespace VS::Save::MemoryCard {
namespace {
// The image is requested only into an allocated buffer (#301).
bool Allocate(MenuAssetLoad& load, MenuAssetHost& host) {
  if (!host.AllocateAssets(load.owner)) return false;
  host.RequestImage();
  load.phase = MenuAssetPhase::kLoadingImage;
  return true;
}

bool Begin(MenuAssetLoad& load, MenuAssetHost& host, MenuAssetOwner owner) {
  if (load.phase != MenuAssetPhase::kIdle) {
    host.ReportMisuse(MenuAssetMisuse::kAlreadyActive);
    return false;
  }
  if (owner == MenuAssetOwner::kDataMenu) host.ReserveTexturePages();
  load.owner = owner;
  load.phase = MenuAssetPhase::kAllocating;
  if (!Allocate(load, host)) {
    host.ReportMisuse(MenuAssetMisuse::kAllocationRefused);
  }
  return false;
}

// MENU7.PRG/260.c:1080 and TITLE.PRG/22C.c:940 _initMemcard: the image upload,
// then the MCDATA/MCMAN text read into the same buffer. The card BIOS event
// setup of the original is not part of the native load.
bool Step(MenuAssetLoad& load, MenuAssetHost& host) {
  switch (load.phase) {
    case MenuAssetPhase::kIdle:
      host.ReportMisuse(MenuAssetMisuse::kNotStarted);
      return false;
    case MenuAssetPhase::kAllocating:
      // The refusal was reported once when the load began.
      static_cast<void>(Allocate(load, host));
      return false;
    case MenuAssetPhase::kLoadingImage:
      if (host.RequestLoaded()) {
        host.ReleaseRequest();
        host.UploadImage();
        load.phase = MenuAssetPhase::kQueueText;
      }
      return false;
    case MenuAssetPhase::kQueueText:
      host.RequestText();
      load.phase = MenuAssetPhase::kLoadingText;
      [[fallthrough]];
    case MenuAssetPhase::kLoadingText:
      if (!host.RequestLoaded()) return false;
      host.ReleaseRequest();
      host.SetSoftResetEnabled(false);
      load.phase = MenuAssetPhase::kReady;
      return true;
    case MenuAssetPhase::kReady:
      // The request was released when the text arrived; its slot may now
      // belong to another loader (#264).
      return true;
  }
  return false;
}

// MENU7.PRG/260.c:1161 and TITLE.PRG/22C.c:1017 _shutdownMemcard.
void Release(MenuAssetLoad& load, MenuAssetHost& host) {
  if (load.phase == MenuAssetPhase::kIdle) {
    host.ReportMisuse(MenuAssetMisuse::kNotLoaded);
    return;
  }
  if (load.phase == MenuAssetPhase::kAllocating) {
    // No buffer was allocated and no read queued (#301).
    if (load.owner == MenuAssetOwner::kDataMenu) host.RetireTexturePages();
    load.phase = MenuAssetPhase::kIdle;
    return;
  }
  if (load.phase != MenuAssetPhase::kReady) {
    host.ReportMisuse(MenuAssetMisuse::kReleaseWhileLoading);
    return;
  }
  host.SetSoftResetEnabled(true);
  host.FreeAssets();
  if (load.owner == MenuAssetOwner::kDataMenu) host.RetireTexturePages();
  load.phase = MenuAssetPhase::kIdle;
}

bool InitializeStep(MenuAssetLoad& load, MenuAssetHost& host, bool begin,
                    MenuAssetOwner owner) {
  return begin ? Begin(load, host, owner) : Step(load, host);
}
}  // namespace

bool InitializeMenuResourcesStep(MenuAssetLoad& load, MenuAssetHost& host,
                                 bool begin) {
  return InitializeStep(load, host, begin, MenuAssetOwner::kDataMenu);
}

void ReleaseMenuResources(MenuAssetLoad& load, MenuAssetHost& host) {
  Release(load, host);
}

bool InitializeResourcesStep(MenuAssetLoad& load, MenuAssetHost& host,
                             bool begin) {
  return InitializeStep(load, host, begin, MenuAssetOwner::kTitle);
}

void ReleaseResources(MenuAssetLoad& load, MenuAssetHost& host) {
  Release(load, host);
}
}  // namespace VS::Save::MemoryCard
