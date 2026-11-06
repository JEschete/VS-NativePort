#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>

// The memory-card screens' font, icon and text assets: one image load, one text
// load, then ready until released. The load state is explicit, and misuse of
// the step and release order is reported instead of reaching a released disc
// request or freeing a buffer the disc queue still writes (#264).
namespace VS::Save::MemoryCard {
// The Data menu also reserves texture pages for the image; the title screens
// draw from pages they own.
enum class MenuAssetOwner : std::uint8_t { kTitle, kDataMenu };

enum class MenuAssetPhase : std::uint8_t {
  kIdle,
  // The asset buffer was refused; each step retries it before the image is
  // requested (#301).
  kAllocating,
  kLoadingImage,
  kQueueText,
  kLoadingText,
  kReady,
};

enum class MenuAssetMisuse : std::uint8_t {
  // A load was begun while one is active; the active load continues.
  kAlreadyActive,
  // A step was requested with no load active.
  kNotStarted,
  // A release was requested while a disc request still targets the buffer;
  // the buffer is kept so the read cannot land in freed memory.
  kReleaseWhileLoading,
  // A release was requested with nothing loaded.
  kNotLoaded,
  // The asset buffer could not be allocated; nothing is queued and the load
  // retries the allocation on each step (#301).
  kAllocationRefused,
};

struct MenuAssetLoad {
  MenuAssetPhase phase = MenuAssetPhase::kIdle;
  MenuAssetOwner owner = MenuAssetOwner::kTitle;
};

class MenuAssetHost {
 public:
  virtual ~MenuAssetHost() = default;
  virtual void ReserveTexturePages() = 0;
  virtual void RetireTexturePages() = 0;
  // Allocates the asset buffer and points the owner's text table, file
  // summaries and directory entry into it; false when the buffer is refused,
  // with every view left unchanged.
  [[nodiscard]] virtual bool AllocateAssets(MenuAssetOwner owner) = 0;
  virtual void FreeAssets() = 0;
  virtual void RequestImage() = 0;
  virtual void RequestText() = 0;
  [[nodiscard]] virtual bool RequestLoaded() const = 0;
  virtual void ReleaseRequest() = 0;
  virtual void UploadImage() = 0;
  virtual void SetSoftResetEnabled(bool enabled) = 0;
  virtual void ReportMisuse(MenuAssetMisuse misuse) = 0;
};

// _initMemcard (MENU7.PRG/260.c:1080): `begin` starts the load and returns
// false; later steps return true once the text is loaded and keep returning
// true until the assets are released.
bool InitializeMenuResourcesStep(MenuAssetLoad& load, MenuAssetHost& host,
                                 bool begin);
// _shutdownMemcard (MENU7.PRG/260.c:1161).
void ReleaseMenuResources(MenuAssetLoad& load, MenuAssetHost& host);
// _initMemcard (TITLE.PRG/22C.c:940).
bool InitializeResourcesStep(MenuAssetLoad& load, MenuAssetHost& host,
                             bool begin);
// _shutdownMemcard (TITLE.PRG/22C.c:1017).
void ReleaseResources(MenuAssetLoad& load, MenuAssetHost& host);
}  // namespace VS::Save::MemoryCard
