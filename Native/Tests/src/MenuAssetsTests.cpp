#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Save/MenuAssets.hpp"

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

  void ReserveTexturePages() override { calls.emplace_back("reserve"); }
  void RetireTexturePages() override { calls.emplace_back("retire"); }
  void AllocateAssets(MenuAssetOwner owner) override {
    calls.emplace_back(owner == MenuAssetOwner::kDataMenu ? "allocate:menu"
                                                          : "allocate:title");
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
