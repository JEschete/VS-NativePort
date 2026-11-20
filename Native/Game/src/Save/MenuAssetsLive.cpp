// Hand-maintained (tools/port/owned.csv).
// Live binding for the memory-card asset load (MenuAssets.cpp): the disc
// queue, low-end heap, texture pages and the screens' text tables.
#include "Save/MenuAssetsLive.hpp"

#include "Boot/SessionLive.hpp"
#include "Content/CdDriveLive.hpp"
#include "Content/DiscLayout.h"
#include "Content/DiscQueue.hpp"
#include "Core/Diagnostics.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Legacy/Psx.h"
#include "Menu/FilesLiveState.hpp"
#include "Render/TexturePagesLive.hpp"
#include "Render/UiPacketsLive.hpp"
#include "Save/MemoryCard/CardResources.hpp"
#include "Save/MemoryCard/SaveImage.hpp"
#include "Title/SaveCard.hpp"
#include "Title/SaveFiles.hpp"

// decomp: _mcData (_mcData@src/TITLE/TITLE.PRG/22C.c)
VS::Save::CardUiResources* VS::Save::MemoryCard::uiResources;

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
constexpr auto kResourceOffset = sizeof(GameSaveImage) * kSaveImageCount;

// The save images and the text resources share the image buffer once the
// image has been uploaded; both must stay inside it (#264).
static_assert(kResourceOffset + sizeof(VS::Save::CardUiResources) <=
              kImageBytes);
static_assert(kTextBytes <= sizeof(VS::Save::CardUiResources));

MenuAssetLoad live_load;
VS::Content::DiscQueueSlot* live_request = nullptr;

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
    case MenuAssetMisuse::kAllocationRefused:
      return "asset buffer refused; retrying on each step";
  }
  return "unknown";
}

void Request(unsigned lba, unsigned size, void* target) {
  VS::Content::DiscFile file;
  file.lba = static_cast<int>(lba);
  file.size = static_cast<int>(size);
  live_request = VS::Content::AllocateLiveDiscRequest(&file);
  if (live_request != nullptr) {
    VS::Content::EnqueueLiveDiscRequest(live_request, target);
  } else {
    // #282: the queue was full; read the file without it.
    live_request = VS::Content::CompleteLiveRefusedDiscRequest(&file, target);
  }
}

class LiveMenuAssets final : public MenuAssetHost {
 public:
  void ReserveTexturePages() override {
    VS::Render::ReserveLiveTexturePageRange(kSaveTexturePage, kSaveImageCount,
                                            kSaveTextureOwner);
  }
  void RetireTexturePages() override {
    VS::Render::RetireLiveTexturePageRange(kSaveTexturePage, kSaveImageCount,
                                           kSaveTextureOwner);
  }
  bool AllocateAssets(MenuAssetOwner owner) override {
    auto* const buffer = static_cast<char*>(
        VS::Core::Memory::AllocateLiveFromLowEnd(kImageBytes));
    // #301: a refused buffer derives no views.
    if (buffer == nullptr) return false;
    VS::Title::memoryCardUiResource = buffer;
    VS::Save::MemoryCard::uiResources =
        reinterpret_cast<VS::Save::CardUiResources*>(
            VS::Title::memoryCardUiResource + kResourceOffset);
    if (owner == MenuAssetOwner::kDataMenu) {
      VS::Menu::Files::textTable =
          VS::Save::MemoryCard::uiResources->text_table;
    } else {
      VS::Title::saveMenuText = VS::Save::MemoryCard::uiResources->text_table;
    }
    VS::Title::saveFileSummaries =
        VS::Save::MemoryCard::uiResources->file_headers;
    VS::Title::cardDirectoryEntryBuffer =
        &VS::Save::MemoryCard::uiResources->directory_entry;
    return true;
  }
  void FreeAssets() override {
    VS::Core::Memory::FreeLiveFromLowEnd(VS::Title::memoryCardUiResource);
    VS::Title::memoryCardUiResource = nullptr;
    VS::Save::MemoryCard::uiResources = nullptr;
    // #300: every view AllocateAssets derived pointed into the freed block, so
    // clear them all for the readers' null guards to refuse later reads.
    VS::Menu::Files::textTable = nullptr;
    VS::Title::saveMenuText = nullptr;
    VS::Title::saveFileSummaries = nullptr;
    VS::Title::cardDirectoryEntryBuffer = nullptr;
  }
  void RequestImage() override {
    Request(VS_SPMCIMG_BIN_LBA, kImageBytes, VS::Title::memoryCardUiResource);
  }
  void RequestText() override {
    Request(VS_MCDATA_BIN_LBA, kTextBytes, VS::Save::MemoryCard::uiResources);
  }
  bool RequestLoaded() const override {
    return live_request != nullptr &&
           live_request->state == VS::Content::DiscRequestState::kLoaded;
  }
  void ReleaseRequest() override {
    VS::Content::ReleaseLiveDiscRequest(live_request);
    live_request = nullptr;
  }
  void UploadImage() override {
    VS::Render::UploadLiveImageRectangle(PackedPair(kImageX, kImageY),
                                         VS::Title::memoryCardUiResource,
                                         PackedPair(kImageWidth, kImageHeight));
  }
  void SetSoftResetEnabled(bool enabled) override {
    VS::Boot::SetSoftResetEnabledLive(enabled ? 1 : 0);
  }
  void ReportMisuse(MenuAssetMisuse misuse) override {
    VS::Core::Errorf("save.assets", "Memory-card assets: %s (phase %d)",
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
