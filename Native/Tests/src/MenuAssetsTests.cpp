#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Core/Memory/HeapLive.hpp"
#include "Render/TexturePagesLive.hpp"
#include "Save/MenuAssets.hpp"
#include "Save/MenuAssetsLive.hpp"

extern "C" {
#include "Boot/GameLoop.h"
#include "DiscQueue.h"
#include "Legacy/Psx.h"
#include "Menu/FilesLiveState.h"
#include "Save/MemoryCard/Resources.h"
#include "Title/SaveCard.h"
#include "Title/SaveFiles.h"
}

namespace {
using VS::Save::MemoryCard::InitializeMenuResourcesStep;
using VS::Save::MemoryCard::InitializeResourcesStep;
using VS::Save::MemoryCard::MenuAssetHost;
using VS::Save::MemoryCard::MenuAssetLoad;
using VS::Save::MemoryCard::MenuAssetMisuse;
using VS::Save::MemoryCard::MenuAssetOwner;
using VS::Save::MemoryCard::MenuAssetPhase;
using VS::Save::MemoryCard::ReleaseMenuResources;
using VS::Save::MemoryCard::ReleaseResources;

class RecordingAssets final : public MenuAssetHost {
 public:
  std::vector<std::string> calls;
  std::vector<MenuAssetMisuse> misuse;
  bool loaded = false;
  bool allocates = true;

  void ReserveTexturePages() override { calls.emplace_back("reserve"); }
  void RetireTexturePages() override { calls.emplace_back("retire"); }
  bool AllocateAssets(MenuAssetOwner owner) override {
    calls.emplace_back(owner == MenuAssetOwner::kDataMenu ? "allocate:menu"
                                                          : "allocate:title");
    return allocates;
  }
  void FreeAssets() override { calls.emplace_back("free"); }
  void RequestImage() override { calls.emplace_back("request:image"); }
  void RequestText() override { calls.emplace_back("request:text"); }
  bool RequestLoaded() const override { return loaded; }
  void ReleaseRequest() override { calls.emplace_back("release-request"); }
  void UploadImage() override { calls.emplace_back("upload"); }
  void SetSoftResetEnabled(bool enabled) override {
    calls.emplace_back(enabled ? "soft-reset:on" : "soft-reset:off");
  }
  void ReportMisuse(MenuAssetMisuse kind) override { misuse.push_back(kind); }
};

// Runs a begun load to completion with each disc read arriving after one
// pending step.
void LoadThroughText(MenuAssetLoad& load, RecordingAssets& host, bool menu) {
  const auto step = [&] {
    return menu ? InitializeMenuResourcesStep(load, host, false)
                : InitializeResourcesStep(load, host, false);
  };
  host.loaded = false;
  REQUIRE_FALSE(step());
  host.loaded = true;
  REQUIRE_FALSE(step());
  REQUIRE(load.phase == MenuAssetPhase::kQueueText);
  host.loaded = false;
  REQUIRE_FALSE(step());
  REQUIRE(load.phase == MenuAssetPhase::kLoadingText);
  host.loaded = true;
  REQUIRE(step());
}
}  // namespace

TEST_CASE("Data-menu asset load reserves pages and loads image then text",
          "[menu-assets]") {
  MenuAssetLoad load;
  RecordingAssets host;
  REQUIRE_FALSE(InitializeMenuResourcesStep(load, host, true));
  CHECK(load.phase == MenuAssetPhase::kLoadingImage);
  CHECK(load.owner == MenuAssetOwner::kDataMenu);
  LoadThroughText(load, host, true);
  CHECK(load.phase == MenuAssetPhase::kReady);
  ReleaseMenuResources(load, host);
  CHECK(load.phase == MenuAssetPhase::kIdle);
  CHECK(host.calls ==
        std::vector<std::string>{"reserve", "allocate:menu", "request:image",
                                 "release-request", "upload", "request:text",
                                 "release-request", "soft-reset:off",
                                 "soft-reset:on", "free", "retire"});
  CHECK(host.misuse.empty());
}

TEST_CASE("Title asset load leaves texture pages alone", "[menu-assets]") {
  MenuAssetLoad load;
  RecordingAssets host;
  REQUIRE_FALSE(InitializeResourcesStep(load, host, true));
  CHECK(load.owner == MenuAssetOwner::kTitle);
  LoadThroughText(load, host, false);
  ReleaseResources(load, host);
  CHECK(host.calls == std::vector<std::string>{
                          "allocate:title", "request:image", "release-request",
                          "upload", "request:text", "release-request",
                          "soft-reset:off", "soft-reset:on", "free"});
  CHECK(host.misuse.empty());
}

TEST_CASE("Asset load finishes when the text read completes at once",
          "[menu-assets]") {
  MenuAssetLoad load;
  RecordingAssets host;
  REQUIRE_FALSE(InitializeResourcesStep(load, host, true));
  host.loaded = true;
  REQUIRE_FALSE(InitializeResourcesStep(load, host, false));
  CHECK(load.phase == MenuAssetPhase::kQueueText);
  // The queue step falls through to the text check: a read that completes
  // immediately finishes the load in the same step.
  REQUIRE(InitializeResourcesStep(load, host, false));
  CHECK(load.phase == MenuAssetPhase::kReady);
}

TEST_CASE("Ready asset load repeats without touching the disc queue",
          "[menu-assets]") {
  MenuAssetLoad load;
  RecordingAssets host;
  REQUIRE_FALSE(InitializeResourcesStep(load, host, true));
  LoadThroughText(load, host, false);
  const auto calls = host.calls.size();
  host.loaded = false;
  CHECK(InitializeResourcesStep(load, host, false));
  CHECK(InitializeResourcesStep(load, host, false));
  CHECK(host.calls.size() == calls);
  CHECK(host.misuse.empty());
}

TEST_CASE("Asset step before begin reports and does nothing", "[menu-assets]") {
  MenuAssetLoad load;
  RecordingAssets host;
  host.loaded = true;
  CHECK_FALSE(InitializeMenuResourcesStep(load, host, false));
  CHECK(load.phase == MenuAssetPhase::kIdle);
  CHECK(host.calls.empty());
  CHECK(host.misuse == std::vector{MenuAssetMisuse::kNotStarted});
}

TEST_CASE("Asset begin while active keeps the active load", "[menu-assets]") {
  MenuAssetLoad load;
  RecordingAssets host;
  REQUIRE_FALSE(InitializeMenuResourcesStep(load, host, true));
  const auto calls = host.calls.size();
  CHECK_FALSE(InitializeResourcesStep(load, host, true));
  CHECK(host.calls.size() == calls);
  CHECK(load.owner == MenuAssetOwner::kDataMenu);
  CHECK(load.phase == MenuAssetPhase::kLoadingImage);
  CHECK(host.misuse == std::vector{MenuAssetMisuse::kAlreadyActive});
}

namespace {
// Begins a Data-menu load and leaves it waiting for the text read.
void LoadUntilTextPending(MenuAssetLoad& load, RecordingAssets& host) {
  REQUIRE_FALSE(InitializeMenuResourcesStep(load, host, true));
  host.loaded = true;
  REQUIRE_FALSE(InitializeMenuResourcesStep(load, host, false));
  host.loaded = false;
  REQUIRE_FALSE(InitializeMenuResourcesStep(load, host, false));
  REQUIRE(load.phase == MenuAssetPhase::kLoadingText);
}
}  // namespace

TEST_CASE("Asset release while a read is pending keeps the buffer",
          "[menu-assets]") {
  MenuAssetLoad load;
  RecordingAssets host;
  LoadUntilTextPending(load, host);
  const auto calls = host.calls.size();
  ReleaseMenuResources(load, host);
  CHECK(host.calls.size() == calls);
  CHECK(load.phase == MenuAssetPhase::kLoadingText);
  CHECK(host.misuse == std::vector{MenuAssetMisuse::kReleaseWhileLoading});
}

TEST_CASE("Asset load refused a release still completes and releases",
          "[menu-assets]") {
  MenuAssetLoad load;
  RecordingAssets host;
  LoadUntilTextPending(load, host);
  ReleaseMenuResources(load, host);
  host.loaded = true;
  REQUIRE(InitializeMenuResourcesStep(load, host, false));
  ReleaseMenuResources(load, host);
  CHECK(load.phase == MenuAssetPhase::kIdle);
  CHECK(host.calls.back() == "retire");
}

TEST_CASE("Asset release with nothing loaded is reported", "[menu-assets]") {
  MenuAssetLoad load;
  RecordingAssets host;
  ReleaseResources(load, host);
  CHECK(host.calls.empty());
  CHECK(host.misuse == std::vector{MenuAssetMisuse::kNotLoaded});
}

TEST_CASE("Asset load can begin again after release", "[menu-assets]") {
  MenuAssetLoad load;
  RecordingAssets host;
  REQUIRE_FALSE(InitializeMenuResourcesStep(load, host, true));
  LoadThroughText(load, host, true);
  ReleaseMenuResources(load, host);
  host.calls.clear();
  REQUIRE_FALSE(InitializeResourcesStep(load, host, true));
  CHECK(load.owner == MenuAssetOwner::kTitle);
  CHECK(host.calls ==
        std::vector<std::string>{"allocate:title", "request:image"});
  CHECK(host.misuse.empty());
}

namespace {
// Begins a load whose asset buffer is refused (#301).
void BeginRefused(MenuAssetLoad& load, RecordingAssets& host, bool menu) {
  host.allocates = false;
  const bool ready = menu ? InitializeMenuResourcesStep(load, host, true)
                          : InitializeResourcesStep(load, host, true);
  REQUIRE_FALSE(ready);
  REQUIRE(load.phase == MenuAssetPhase::kAllocating);
  REQUIRE(host.misuse == std::vector{MenuAssetMisuse::kAllocationRefused});
}
}  // namespace

TEST_CASE("Refused asset buffer queues nothing and retries on each step",
          "[menu-assets]") {
  MenuAssetLoad load;
  RecordingAssets host;
  BeginRefused(load, host, true);
  REQUIRE_FALSE(InitializeMenuResourcesStep(load, host, false));
  CHECK(load.phase == MenuAssetPhase::kAllocating);
  host.allocates = true;
  REQUIRE_FALSE(InitializeMenuResourcesStep(load, host, false));
  CHECK(load.phase == MenuAssetPhase::kLoadingImage);
  CHECK(host.calls == std::vector<std::string>{"reserve", "allocate:menu",
                                               "allocate:menu", "allocate:menu",
                                               "request:image"});
  LoadThroughText(load, host, true);
  CHECK(host.misuse.size() == 1);
}

TEST_CASE("Data-menu release after a refused asset buffer retires its pages",
          "[menu-assets]") {
  MenuAssetLoad load;
  RecordingAssets host;
  BeginRefused(load, host, true);
  ReleaseMenuResources(load, host);
  CHECK(load.phase == MenuAssetPhase::kIdle);
  CHECK(host.calls ==
        std::vector<std::string>{"reserve", "allocate:menu", "retire"});
  CHECK(host.misuse.size() == 1);
}

TEST_CASE("Title release after a refused asset buffer frees nothing",
          "[menu-assets]") {
  MenuAssetLoad load;
  RecordingAssets host;
  BeginRefused(load, host, false);
  ReleaseResources(load, host);
  CHECK(load.phase == MenuAssetPhase::kIdle);
  CHECK(host.calls == std::vector<std::string>{"allocate:title"});
  host.allocates = true;
  REQUIRE_FALSE(InitializeResourcesStep(load, host, true));
  CHECK(load.phase == MenuAssetPhase::kLoadingImage);
  CHECK(host.misuse.size() == 1);
}

namespace {
using VS::Save::MemoryCard::LiveMenuAssetPhase;

// The save image's VRAM rectangle (MenuAssetsLive.cpp UploadImage).
constexpr std::int16_t kImageX = 800;
constexpr std::int16_t kImageY = 256;
constexpr std::int16_t kImageWidth = 224;
constexpr std::int16_t kImageHeight = 256;
constexpr int kImagePixels = kImageWidth * kImageHeight;
// Room for the 114,688-byte save image and its heap block bookkeeping.
constexpr std::size_t kArenaBytes = std::size_t{16} * 8192;

struct alignas(16) LiveArena {
  std::array<std::byte, kArenaBytes> bytes{};
};

// Swaps in a test heap, an idle disc queue and freshly initialized texture
// pages, and restores them, the soft-reset flag, the asset views and the save
// image's VRAM afterwards.
class LiveAssetFixture {
 public:
  LiveAssetFixture() : heap_(arena_->bytes) {
    VS::Render::InitializeLiveTexturePages();
    std::memcpy(saved_slots_.data(), Content_discRequestSlots,
                sizeof Content_discRequestSlots);
    std::memset(Content_discRequestSlots, 0, sizeof Content_discRequestSlots);
    Content_discRequestCounts = Content_DiscRequestCounts{};
    Content_discDriveState.state =
        static_cast<char>(Content_DiscDriveState_Idle);
    setRECT(&image_, kImageX, kImageY, kImageWidth, kImageHeight);
    StoreImage(&image_, reinterpret_cast<u_long*>(saved_vram_.data()));
    DrawSync(0);
  }
  ~LiveAssetFixture() {
    VS::Render::LiveTexturePages() = saved_pages_;
    LoadImage(&image_, reinterpret_cast<u_long*>(saved_vram_.data()));
    DrawSync(0);
    Menu_Files_textTable = saved_menu_text_;
    Title_saveMenuText = saved_title_text_;
    Title_saveFileSummaries = saved_summaries_;
    Title_cardDirectoryEntryBuffer = saved_directory_;
    Boot_isSoftResetEnabled = saved_soft_reset_;
    std::memcpy(Content_discRequestSlots, saved_slots_.data(),
                sizeof Content_discRequestSlots);
    Content_discRequestCounts = saved_counts_;
    Content_discDriveState = saved_drive_;
  }
  LiveAssetFixture(const LiveAssetFixture&) = delete;
  LiveAssetFixture& operator=(const LiveAssetFixture&) = delete;
  LiveAssetFixture(LiveAssetFixture&&) = delete;
  LiveAssetFixture& operator=(LiveAssetFixture&&) = delete;

 private:
  std::unique_ptr<LiveArena> arena_ = std::make_unique<LiveArena>();
  VS::Core::Memory::ScopedLiveGameHeap heap_;
  VS::Render::TexturePageTable saved_pages_ = VS::Render::LiveTexturePages();
  std::array<Content_DiscQueueSlot, 32> saved_slots_{};
  Content_DiscRequestCounts saved_counts_ = Content_discRequestCounts;
  Content_DiscDrive saved_drive_ = Content_discDriveState;
  int saved_soft_reset_ = Boot_isSoftResetEnabled;
  u_short* saved_menu_text_ = Menu_Files_textTable;
  u_short* saved_title_text_ = Title_saveMenuText;
  Save_CardFileInfoRecord* saved_summaries_ = Title_saveFileSummaries;
  DIRENTRY* saved_directory_ = Title_cardDirectoryEntryBuffer;
  RECT image_{};
  std::vector<std::uint16_t> saved_vram_ =
      std::vector<std::uint16_t>(std::size_t{kImagePixels});
};

// Marks the one request the load queued as read.
void CompletePendingRead() {
  auto* const end = std::end(Content_discRequestSlots);
  auto* const pending =
      std::find_if(std::begin(Content_discRequestSlots), end,
                   [](const Content_DiscQueueSlot& slot) {
                     return slot.state != Content_DiscRequestState_Free;
                   });
  REQUIRE(pending != end);
  pending->state = static_cast<std::int16_t>(Content_DiscRequestState_Loaded);
}

// Runs a live load through the image read, leaving the text read to queue.
void LoadLiveImage(int (*step)(int)) {
  REQUIRE(LiveMenuAssetPhase() == MenuAssetPhase::kIdle);
  REQUIRE(step(1) == 0);
  CompletePendingRead();
  REQUIRE(step(0) == 0);
  REQUIRE(LiveMenuAssetPhase() == MenuAssetPhase::kQueueText);
}

// Runs a live load from the queued text read until ready; the summary and
// directory views then point into the loaded block.
void LoadLiveText(int (*step)(int)) {
  REQUIRE(step(0) == 0);
  REQUIRE(LiveMenuAssetPhase() == MenuAssetPhase::kLoadingText);
  CompletePendingRead();
  REQUIRE(step(0) == 1);
  REQUIRE(LiveMenuAssetPhase() == MenuAssetPhase::kReady);
  REQUIRE(Save_MemoryCard_uiResources != nullptr);
  REQUIRE(Title_saveFileSummaries == Save_MemoryCard_uiResources->fileHeaders);
  REQUIRE(Title_cardDirectoryEntryBuffer ==
          &Save_MemoryCard_uiResources->directoryEntry);
}

// Runs a live load through the image and text reads.
void LoadLive(int (*step)(int)) {
  LoadLiveImage(step);
  LoadLiveText(step);
}

// The asset views a refused buffer must leave unchanged (#301).
struct AssetViews {
  char* image = Title_memoryCardUiResource;
  Save_CardUiResources* resources = Save_MemoryCard_uiResources;
  u_short* menu_text = Menu_Files_textTable;
  u_short* title_text = Title_saveMenuText;
  Save_CardFileInfoRecord* summaries = Title_saveFileSummaries;
  DIRENTRY* directory = Title_cardDirectoryEntryBuffer;

  bool operator==(const AssetViews&) const = default;
};

// Holds the low end of the test heap so the 114,688-byte image buffer is
// refused until released.
class HeapBlocker {
 public:
  HeapBlocker()
      : block_(VS::Core::Memory::AllocateLiveFromLowEnd(kBlockerBytes)) {
    REQUIRE(block_ != nullptr);
  }
  ~HeapBlocker() { Release(); }
  HeapBlocker(const HeapBlocker&) = delete;
  HeapBlocker& operator=(const HeapBlocker&) = delete;
  HeapBlocker(HeapBlocker&&) = delete;
  HeapBlocker& operator=(HeapBlocker&&) = delete;

  void Release() {
    if (block_ == nullptr) return;
    VS::Core::Memory::FreeLiveFromLowEnd(block_);
    block_ = nullptr;
  }

 private:
  static constexpr std::size_t kBlockerBytes = std::size_t{32} * 1024;
  void* block_;
};

bool AnyReadQueued() {
  return std::any_of(std::begin(Content_discRequestSlots),
                     std::end(Content_discRequestSlots),
                     [](const Content_DiscQueueSlot& slot) {
                       return slot.state != Content_DiscRequestState_Free;
                     });
}

// #300: no view the load derived may outlive the freed block.
void CheckViewsCleared() {
  CHECK(LiveMenuAssetPhase() == MenuAssetPhase::kIdle);
  CHECK(Title_memoryCardUiResource == nullptr);
  CHECK(Save_MemoryCard_uiResources == nullptr);
  CHECK(Menu_Files_textTable == nullptr);
  CHECK(Title_saveMenuText == nullptr);
  CHECK(Title_saveFileSummaries == nullptr);
  CHECK(Title_cardDirectoryEntryBuffer == nullptr);
}
}  // namespace

TEST_CASE("Live data-menu asset release clears every view into the block",
          "[menu-assets]") {
  const LiveAssetFixture fixture;
  const auto refused_pages = VS::Render::RefusedLiveTexturePageRequestCount();
  LoadLive(&VS::Save::MemoryCard::StepLiveMenuResources);
  CHECK(Menu_Files_textTable == Save_MemoryCard_uiResources->textTable);
  VS::Save::MemoryCard::ReleaseLiveMenuResources();
  CheckViewsCleared();
  CHECK(VS::Render::RefusedLiveTexturePageRequestCount() == refused_pages);
}

TEST_CASE("Live title asset release clears every view into the block",
          "[menu-assets]") {
  const LiveAssetFixture fixture;
  LoadLive(&VS::Save::MemoryCard::StepLiveTitleResources);
  CHECK(Title_saveMenuText == Save_MemoryCard_uiResources->textTable);
  VS::Save::MemoryCard::ReleaseLiveTitleResources();
  CheckViewsCleared();
}

TEST_CASE("Live refused asset buffer derives no views and queues no read",
          "[menu-assets]") {
  const LiveAssetFixture fixture;
  const HeapBlocker blocker;
  const AssetViews before;
  const auto refused = VS::Core::Memory::RefusedLiveHeapRequestCount();
  REQUIRE(VS::Save::MemoryCard::StepLiveMenuResources(1) == 0);
  REQUIRE(VS::Save::MemoryCard::StepLiveMenuResources(0) == 0);
  CHECK(LiveMenuAssetPhase() == MenuAssetPhase::kAllocating);
  CHECK(VS::Core::Memory::RefusedLiveHeapRequestCount() == refused + 2);
  const AssetViews after;
  CHECK(after == before);
  CHECK_FALSE(AnyReadQueued());
  VS::Save::MemoryCard::ReleaseLiveMenuResources();
  CHECK(LiveMenuAssetPhase() == MenuAssetPhase::kIdle);
}

TEST_CASE("Live asset load retries a refused buffer once the heap has room",
          "[menu-assets]") {
  const LiveAssetFixture fixture;
  HeapBlocker blocker;
  const auto refused_pages = VS::Render::RefusedLiveTexturePageRequestCount();
  REQUIRE(VS::Save::MemoryCard::StepLiveMenuResources(1) == 0);
  blocker.Release();
  REQUIRE(VS::Save::MemoryCard::StepLiveMenuResources(0) == 0);
  REQUIRE(LiveMenuAssetPhase() == MenuAssetPhase::kLoadingImage);
  CompletePendingRead();
  REQUIRE(VS::Save::MemoryCard::StepLiveMenuResources(0) == 0);
  LoadLiveText(&VS::Save::MemoryCard::StepLiveMenuResources);
  VS::Save::MemoryCard::ReleaseLiveMenuResources();
  CheckViewsCleared();
  CHECK(VS::Render::RefusedLiveTexturePageRequestCount() == refused_pages);
}

TEST_CASE("Live release after a refused asset buffer leaves views alone",
          "[menu-assets]") {
  const LiveAssetFixture fixture;
  const HeapBlocker blocker;
  const AssetViews before;
  REQUIRE(VS::Save::MemoryCard::StepLiveTitleResources(1) == 0);
  VS::Save::MemoryCard::ReleaseLiveTitleResources();
  CHECK(LiveMenuAssetPhase() == MenuAssetPhase::kIdle);
  const AssetViews after;
  CHECK(after == before);
  CHECK_FALSE(AnyReadQueued());
}
