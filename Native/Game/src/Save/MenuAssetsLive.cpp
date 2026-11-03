// Hand-maintained (tools/port/owned.csv).
// Live binding for the memory-card asset load (MenuAssets.cpp): the disc
// queue, low-end heap, texture pages and the screens' text tables.
#include "Save/MenuAssetsLive.hpp"

#include "Core/Memory/HeapLive.hpp"

extern "C" {
#include "Boot/Lifecycle.h"
#include "CdDrive.h"
#include "Content/DiscLayout.h"
#include "Core/Diagnostics.h"
#include "DiscQueue.h"
#include "Legacy/Psx.h"
#include "Menu/Files.h"
#include "Render/TextureAnimation.h"
#include "Render/Textures.h"
#include "Save/MemoryCard/GameFiles.h"
#include "Save/MemoryCard/Resources.h"
#include "Title/SaveCard.h"
#include "Title/SaveFiles.h"
}

namespace VS::Save::MemoryCard {
namespace {
constexpr unsigned kSaveImageCount = 3;
constexpr unsigned kSaveTexturePage = 0x1D;
constexpr int kSaveTextureOwner = 6;
// The save images upload as one 224x256 16-bit rectangle at (800, 256).
constexpr int kImageX = 800;
constexpr int kImageY = 256;
constexpr int kImageWidth = 224;
constexpr int kImageHeight = 256;
constexpr unsigned kImageBytes = VS_SPMCIMG_BIN_SIZE;
static_assert(kImageWidth * kImageHeight * 2 == kImageBytes);

// GPU rectangles pack x/width in the low halfword and y/height in the high.
constexpr int kHalfwordBits = 16;
constexpr int kHalfwordMask = 0xFFFF;
constexpr int PackedPair(int low, int high) {
  return (high << kHalfwordBits) | (low & kHalfwordMask);
}
constexpr unsigned kTextBytes = VS_MCDATA_BIN_SIZE + VS_MCMAN_BIN_SIZE;
constexpr auto kResourceOffset = sizeof(Save_GameSaveImage) * kSaveImageCount;

// The save images and the text resources share the image buffer once the
// image has been uploaded; both must stay inside it (#264).
static_assert(kResourceOffset + sizeof(Save_CardUiResources) <= kImageBytes);
static_assert(kTextBytes <= sizeof(Save_CardUiResources));

MenuAssetLoad live_load;
Content_DiscQueueSlot* live_request = nullptr;

const char* MisuseName(MenuAssetMisuse misuse) {
  switch (misuse) {
    case MenuAssetMisuse::kAlreadyActive:
      return "begin while a load is active";
    case MenuAssetMisuse::kNotStarted:
      return "step with no load";
    case MenuAssetMisuse::kReleaseWhileLoading:
      return "release while a disc read targets the buffer";
    case MenuAssetMisuse::kNotLoaded:
      return "release with nothing loaded";
  }
  return "unknown";
}

void Request(unsigned lba, unsigned size, void* target) {
  Content_DiscFile file;
  file.lba = static_cast<int>(lba);
  file.size = static_cast<int>(size);
  live_request = Content_AllocateDiscRequest(&file);
  Content_EnqueueDiscRequest(live_request, target);
}

class LiveMenuAssets final : public MenuAssetHost {
 public:
  void ReserveTexturePages() override {
    Render_ReserveTexturePageRange(kSaveTexturePage, kSaveImageCount,
                                   kSaveTextureOwner);
  }
  void RetireTexturePages() override {
    Render_RetireTexturePageRange(kSaveTexturePage, kSaveImageCount,
                                  kSaveTextureOwner);
  }
  void AllocateAssets(MenuAssetOwner owner) override {
    Title_memoryCardUiResource = static_cast<char*>(
        VS::Core::Memory::AllocateLiveFromLowEnd(kImageBytes));
    Save_MemoryCard_uiResources = reinterpret_cast<Save_CardUiResources*>(
        Title_memoryCardUiResource + kResourceOffset);
    if (owner == MenuAssetOwner::kDataMenu) {
      Menu_Files_textTable = Save_MemoryCard_uiResources->textTable;
    } else {
      Title_saveMenuText = Save_MemoryCard_uiResources->textTable;
    }
    Title_saveFileSummaries = Save_MemoryCard_uiResources->fileHeaders;
    Title_cardDirectoryEntryBuffer =
        &Save_MemoryCard_uiResources->directoryEntry;
  }
  void FreeAssets() override {
    VS::Core::Memory::FreeLiveFromLowEnd(Title_memoryCardUiResource);
    Title_memoryCardUiResource = nullptr;
    Save_MemoryCard_uiResources = nullptr;
  }
  void RequestImage() override {
    Request(VS_SPMCIMG_BIN_LBA, kImageBytes, Title_memoryCardUiResource);
  }
  void RequestText() override {
    Request(VS_MCDATA_BIN_LBA, kTextBytes, Save_MemoryCard_uiResources);
  }
  bool RequestLoaded() const override {
    return live_request != nullptr &&
           live_request->state == Content_DiscRequestState_Loaded;
  }
  void ReleaseRequest() override {
    Content_ReleaseDiscRequest(live_request);
    live_request = nullptr;
  }
  void UploadImage() override {
    Render_UploadPackedRectangle(PackedPair(kImageX, kImageY),
                                 Title_memoryCardUiResource,
                                 PackedPair(kImageWidth, kImageHeight));
  }
  void SetSoftResetEnabled(bool enabled) override {
    Boot_SetSoftResetEnabled(enabled ? 1 : 0);
  }
  void ReportMisuse(MenuAssetMisuse misuse) override {
    Core_Errorf("save.assets", "Memory-card assets: %s (phase %d)",
                MisuseName(misuse), static_cast<int>(live_load.phase));
  }
};
}  // namespace

int StepLiveMenuResources(int init) {
  LiveMenuAssets host;
  return InitializeMenuResourcesStep(live_load, host, init != 0) ? 1 : 0;
}

void ReleaseLiveMenuResources() {
  LiveMenuAssets host;
  ReleaseMenuResources(live_load, host);
}

int StepLiveTitleResources(int init) {
  LiveMenuAssets host;
  return InitializeResourcesStep(live_load, host, init != 0) ? 1 : 0;
}

void ReleaseLiveTitleResources() {
  LiveMenuAssets host;
  ReleaseResources(live_load, host);
}

MenuAssetPhase LiveMenuAssetPhase() { return live_load.phase; }
}  // namespace VS::Save::MemoryCard
