#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ActorAssetsLiveState.h"
#include "Content/ModuleData.h"
#include "Content/OverlayFiles.hpp"
#include "Content/OverlayWorkspace.hpp"
#include "Content/OverlayWorkspaceLive.hpp"
#include "Content/PFile.hpp"
#include "Content/PFileLive.hpp"
#include "Content/SystemResources.hpp"
#include "Core/Memory/Heap.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "OverlayWorkspaceLiveState.h"

extern "C" {
#include "Effects/Package.h"
}

namespace {
namespace Content = VS::Content;
namespace Memory = VS::Core::Memory;

class FakeOverlayWorkspace final : public Content::OverlayWorkspaceHost {
 public:
  [[nodiscard]] std::uint8_t Owner() const override { return owner; }
  void SetOwner(std::uint8_t value) override {
    owner = value;
    ++writes;
  }

  std::uint8_t owner = 0;
  int writes = 0;
};

struct LiveWorkspaceOwner {
  char saved = Content_overlayWorkspaceOwner;
  LiveWorkspaceOwner() { Content_overlayWorkspaceOwner = 0; }
  ~LiveWorkspaceOwner() { Content_overlayWorkspaceOwner = saved; }
  LiveWorkspaceOwner(const LiveWorkspaceOwner&) = delete;
  LiveWorkspaceOwner& operator=(const LiveWorkspaceOwner&) = delete;
  LiveWorkspaceOwner(LiveWorkspaceOwner&&) = delete;
  LiveWorkspaceOwner& operator=(LiveWorkspaceOwner&&) = delete;
};

void PutU16(std::span<std::byte> bytes, std::size_t at, std::uint16_t value) {
  bytes[at] = static_cast<std::byte>(value & 0xFFU);
  bytes[at + 1] = static_cast<std::byte>(value >> 8U);
}

void PutI16(std::span<std::byte> bytes, std::size_t at, std::int16_t value) {
  PutU16(bytes, at, static_cast<std::uint16_t>(value));
}

void PutU32(std::span<std::byte> bytes, std::size_t at, std::uint32_t value) {
  PutU16(bytes, at, static_cast<std::uint16_t>(value & 0xFFFFU));
  PutU16(bytes, at + 2, static_cast<std::uint16_t>(value >> 16U));
}

// Two sections of two tracks: section 0's table at 0x0C, section 1's at
// 0x18; section 1 track 1 names data far past the block.
constexpr std::size_t kTimelineBytes = 0x30;

std::array<std::byte, kTimelineBytes> TimelineBlock() {
  std::array<std::byte, kTimelineBytes> block{};
  PutU32(block, 0, 2);
  PutU16(block, 4, 0x0C);
  PutU16(block, 6, 0x18);
  PutU32(block, 0x0C, 2);
  PutU16(block, 0x12, 0x24);
  PutU16(block, 0x16, 0x28);
  PutU32(block, 0x18, 2);
  PutU16(block, 0x1E, 0x2C);
  PutU16(block, 0x22, 0xFFF0);
  return block;
}

struct UploadedImage {
  Content::VramRect rect;
  std::size_t bytes = 0;
  std::byte first{};
};

class FakeSystemResources final : public Content::SystemResourcesHost {
 public:
  std::span<std::byte> AcquireBuffer(std::size_t bytes) override {
    requested = bytes;
    return storage;
  }
  void ReleaseBuffer(std::span<std::byte> buffer) override {
    released.push_back(buffer.data());
  }
  void ReadFile(std::int32_t sector,
                std::span<std::byte> destination) override {
    read_sector = sector;
    read_bytes = destination.size();
    std::copy_n(file.begin(), std::min(file.size(), destination.size()),
                destination.begin());
  }
  void UploadImage(const Content::VramRect& rect,
                   std::span<const std::byte> pixels) override {
    uploads.push_back({.rect = rect,
                       .bytes = pixels.size(),
                       .first = pixels.empty() ? std::byte{} : pixels[0]});
  }
  void LoadClutColors(std::span<const std::byte> colors, std::uint32_t row,
                      std::uint32_t first, std::uint32_t count) override {
    clut_bytes = colors.size();
    clut_row = row;
    clut_first = first;
    clut_count = count;
    ++clut_loads;
  }
  void InitializeGameInterface(std::span<const std::byte> data) override {
    interface_bytes = data.size();
    interface_first = data.empty() ? std::byte{} : data[0];
    ++interface_loads;
  }

  std::vector<std::byte> storage;
  std::vector<std::byte> file;
  std::size_t requested = 0;
  std::vector<const std::byte*> released;
  std::int32_t read_sector = -1;
  std::size_t read_bytes = 0;
  std::vector<UploadedImage> uploads;
  std::size_t clut_bytes = 0;
  std::uint32_t clut_row = 0;
  std::uint32_t clut_first = 0;
  std::uint32_t clut_count = 0;
  int clut_loads = 0;
  std::size_t interface_bytes = 0;
  std::byte interface_first{};
  int interface_loads = 0;
};

struct ImageHeader {
  std::int16_t x = 0;
  std::int16_t y = 0;
  std::int16_t w = 0;
  std::int16_t h = 0;
};

void PutImage(std::span<std::byte> file, std::size_t at,
              const ImageHeader& header, std::byte fill) {
  PutI16(file, at, header.x);
  PutI16(file, at + 2, header.y);
  PutI16(file, at + 4, header.w);
  PutI16(file, at + 6, header.h);
  const std::size_t pixels = header.w < 0 || header.h < 0
                                 ? 0
                                 : static_cast<std::size_t>(header.w) *
                                       static_cast<std::size_t>(header.h) * 2U;
  const std::size_t end = std::min(file.size(), at + 8 + pixels);
  if (at + 8 < end) {
    std::fill(file.begin() + static_cast<std::ptrdiff_t>(at + 8),
              file.begin() + static_cast<std::ptrdiff_t>(end), fill);
  }
}

constexpr std::size_t kInterfaceAt = 0x200;

// Byte offset of a SYSTEM.DAT section's offset word.
constexpr std::size_t SectionOffsetAt(std::size_t section) {
  return section * sizeof(std::uint32_t);
}

// SYSTEM.DAT with every section in place: three 2-pixel images, a 3-colour
// palette, two more images, a 4x2 fixed image and the interface data.
std::vector<std::byte> SystemDatFile() {
  std::vector<std::byte> file(Content::kSystemDatBytes);
  const std::array<std::uint32_t, Content::kSystemDatSections> offsets{
      0x40, 0x60, 0x80, 0xA0, 0xC0, 0xE0, 0x100, kInterfaceAt};
  for (std::size_t i = 0; i < offsets.size(); ++i) {
    PutU32(file, SectionOffsetAt(i), offsets[i]);
  }
  PutImage(file, 0x40, {.x = 0x10, .y = 0x20, .w = 2, .h = 1}, std::byte{1});
  PutImage(file, 0x60, {.x = 0x30, .y = 0x40, .w = 1, .h = 2}, std::byte{2});
  PutI16(file, 0x80, 3);
  PutImage(file, 0xA0, {.x = 0x50, .y = 0x60, .w = 1, .h = 1}, std::byte{3});
  PutImage(file, 0xC0, {.x = 0x70, .y = 0x80, .w = 1, .h = 1}, std::byte{4});
  PutImage(file, 0xE0, {.x = 0x90, .y = 0xA0, .w = 1, .h = 1}, std::byte{5});
  PutI16(file, 0x100, 4);
  PutI16(file, 0x102, 2);
  std::fill_n(file.begin() + 0x104, 16, std::byte{6});
  file[kInterfaceAt] = std::byte{7};
  return file;
}

// Loads a whole SYSTEM.DAT through `host` into a buffer of the file's size.
Content::SystemResourcesLoad LoadSystemDat(FakeSystemResources& host) {
  host.storage.resize(Content::kSystemDatBytes);
  host.file = SystemDatFile();
  return Content::LoadSystemResources(host);
}

bool SameRect(const Content::VramRect& rect, const ImageHeader& expected) {
  return rect.x == expected.x && rect.y == expected.y && rect.w == expected.w &&
         rect.h == expected.h;
}

constexpr std::array<std::uint32_t, 28> kRetiredBattleEntries{
    0x8007E180u, 0x8007E1C0u, 0x800995B0u, 0x800995E8u, 0x8009967Cu,
    0x80099854u, 0x80099E7Cu, 0x80099FA8u, 0x8009A028u, 0x8009A0B8u,
    0x8009A2E8u, 0x8009A98Cu, 0x8009AA84u, 0x8009AC24u, 0x8009AC84u,
    0x8009AEA0u, 0x8009BD90u, 0x8009BE5Cu, 0x8009C378u, 0x8009C3D8u,
    0x8009CAECu, 0x8009CC20u, 0x8009CFB0u, 0x8009D208u, 0x8009D270u,
    0x800D5550u, 0x800D5564u, 0x800D8200u,
};

constexpr std::array<std::uint32_t, 11> kRetiredMainEntries{
    0x80043FB4u, 0x80044148u, 0x800441C4u, 0x80044224u,
    0x80044310u, 0x8004493Cu, 0x80044B10u, 0x80044B80u,
    0x80044BC4u, 0x80044BF4u, 0x80044C74u,
};
}  // namespace

TEST_CASE("Overlay workspace claims refuse a busy workspace",
          "[content-slice][content][workspace]") {
  FakeOverlayWorkspace host;
  CHECK(Content::AcquireOverlayWorkspace(host, 2));
  CHECK(host.owner == 2);
  CHECK_FALSE(Content::AcquireOverlayWorkspace(host, 6));
  CHECK(host.owner == 2);
  CHECK(host.writes == 1);
}

TEST_CASE(
    "Overlay workspace releases refuse a foreign owner and a free workspace",
    "[content-slice][content][workspace]") {
  FakeOverlayWorkspace host;
  REQUIRE(Content::AcquireOverlayWorkspace(host, 2));
  CHECK_FALSE(Content::ReleaseOverlayWorkspace(host, 6));
  CHECK(host.owner == 2);
  CHECK(Content::ReleaseOverlayWorkspace(host, 2));
  CHECK(host.owner == 0);
  CHECK(host.writes == 2);
  CHECK_FALSE(Content::ReleaseOverlayWorkspace(host, 2));
  CHECK(host.writes == 2);
}

TEST_CASE("Overlay workspace owners keep the low byte of the tag",
          "[content-slice][content][workspace]") {
  FakeOverlayWorkspace host;
  // sb stores the low byte; the release compares it (lbu) with the whole tag.
  CHECK(Content::AcquireOverlayWorkspace(host, 0x106));
  CHECK(host.owner == 6);
  CHECK_FALSE(Content::ReleaseOverlayWorkspace(host, 0x106));
  CHECK(Content::ReleaseOverlayWorkspace(host, 6));
  // A tag whose low byte is 0 claims nothing.
  CHECK(Content::AcquireOverlayWorkspace(host, 0x100));
  CHECK(host.owner == 0);
  CHECK(Content::AcquireOverlayWorkspace(host, 3));
}

TEST_CASE("Overlay workspace releases compare the stored byte with the tag",
          "[content-slice][content][workspace]") {
  FakeOverlayWorkspace host;
  CHECK(Content::AcquireOverlayWorkspace(host, -1));
  CHECK(host.owner == 0xFF);
  CHECK_FALSE(Content::ReleaseOverlayWorkspace(host, -1));
  CHECK(Content::ReleaseOverlayWorkspace(host, 0xFF));
  CHECK(host.owner == 0);
}

TEST_CASE("Live overlay workspace binding stores the claimed byte",
          "[content-slice][content][workspace]") {
  LiveWorkspaceOwner state;
  CHECK(Content::AcquireLiveOverlayWorkspace(0x1FF) == 1);
  CHECK(static_cast<std::uint8_t>(Content_overlayWorkspaceOwner) == 0xFF);
  CHECK(Content::AcquireLiveOverlayWorkspace(4) == 0);
  CHECK(Content::ReleaseLiveOverlayWorkspace(-1) == 0);
  CHECK(Content::ReleaseLiveOverlayWorkspace(0xFF) == 1);
  CHECK(Content_overlayWorkspaceOwner == 0);
}

TEST_CASE("Main menu overlay reads start at its fixed disc sector",
          "[content-slice][content][overlay-files]") {
  CHECK(Content::MainMenuOverlayDiscSector() == 0x14C08);
  CHECK(Content::kMainMenuOverlaySector == 85000);
}

TEST_CASE("PFile timeline sections resolve block-relative table offsets",
          "[content-slice][content][timeline]") {
  const auto block = TimelineBlock();
  const std::span<const std::byte> bytes(block);
  CHECK(Content::PFileTimelineSection(bytes, 0) == std::size_t{0x0C});
  CHECK(Content::PFileTimelineSection(bytes, 1) == std::size_t{0x18});
  // The original does not check the section count.
  CHECK(Content::PFileTimelineSection(bytes, 2) == std::size_t{0});
  CHECK_FALSE(Content::PFileTimelineSection(bytes, 22).has_value());
  CHECK_FALSE(Content::PFileTimelineSection(
                  bytes, std::numeric_limits<std::size_t>::max())
                  .has_value());
  CHECK_FALSE(Content::PFileTimelineSection(bytes.first(3), 0).has_value());
  CHECK_FALSE(Content::PFileTimelineSection(bytes.first(0x0C), 0).has_value());
  CHECK(Content::PFileTimelineSection(bytes.first(0x0D), 0) ==
        std::size_t{0x0C});
}

TEST_CASE("PFile timeline tracks resolve data offsets inside the block only",
          "[content-slice][content][timeline]") {
  const auto block = TimelineBlock();
  const std::span<const std::byte> bytes(block);
  CHECK(Content::PFileTimelineTrackData(bytes, 0, 0) == std::size_t{0x24});
  CHECK(Content::PFileTimelineTrackData(bytes, 0, 1) == std::size_t{0x28});
  CHECK(Content::PFileTimelineTrackData(bytes, 1, 0) == std::size_t{0x2C});
  CHECK_FALSE(Content::PFileTimelineTrackData(bytes, 1, 1).has_value());
  // The original does not check the track count either.
  CHECK(Content::PFileTimelineTrackData(bytes, 0, 2) == std::size_t{0});
  CHECK_FALSE(Content::PFileTimelineTrackData(bytes, 1, 5).has_value());
  CHECK_FALSE(Content::PFileTimelineTrackData(
                  bytes, 0, std::numeric_limits<std::size_t>::max())
                  .has_value());
  CHECK_FALSE(Content::PFileTimelineTrackData(bytes, 22, 0).has_value());
}

TEST_CASE("Live timeline lookups bound the block by its game heap payload",
          "[content-slice][content][timeline]") {
  alignas(Memory::kHeapUnitBytes)
      std::array<std::byte, Memory::kHeapUnitBytes * 64>
          arena{};
  Memory::ScopedLiveGameHeap heap{arena};
  void* const payload = Memory::AllocateLiveFromHighEnd(kTimelineBytes);
  REQUIRE(payload != nullptr);
  const auto block = TimelineBlock();
  std::memcpy(payload, block.data(), block.size());
  auto* const timeline = static_cast<Effects_PackageBlock3*>(payload);
  auto* const base = static_cast<std::byte*>(payload);
  CHECK(Content::PFileLiveTimelineSection(timeline, 1) == base + 0x18);
  CHECK(Content::PFileLiveTimelineTrackData(timeline, 0, 1) == base + 0x28);
  CHECK(Content::PFileLiveTimelineTrackData(timeline, 1, 1) == nullptr);
  CHECK(Content::PFileLiveTimelineSection(timeline, -1) == nullptr);
  CHECK(Content::PFileLiveTimelineTrackData(timeline, 0, -1) == nullptr);
  CHECK(Content::PFileLiveTimelineSection(nullptr, 0) == nullptr);
  auto local = TimelineBlock();
  CHECK(Content::PFileLiveTimelineSection(
            reinterpret_cast<Effects_PackageBlock3*>(local.data()), 0) ==
        nullptr);
  Memory::FreeLiveFromHighEnd(payload);
}

TEST_CASE("System resources read SYSTEM.DAT into a whole file buffer",
          "[content-slice][content][system-resources]") {
  FakeSystemResources host;
  static_cast<void>(LoadSystemDat(host));
  CHECK(host.requested == Content::kSystemDatBytes);
  CHECK(host.read_sector == 1387);
  CHECK(host.read_bytes == 188416);
  CHECK(host.uploads.size() == 6);
}

TEST_CASE("System resources upload the SYSTEM.DAT images in order",
          "[content-slice][content][system-resources]") {
  FakeSystemResources host;
  static_cast<void>(LoadSystemDat(host));
  REQUIRE(host.uploads.size() == 6);
  CHECK(SameRect(host.uploads[0].rect, {.x = 0x10, .y = 0x20, .w = 2, .h = 1}));
  CHECK(host.uploads[0].bytes == 4);
  CHECK(host.uploads[0].first == std::byte{1});
  CHECK(SameRect(host.uploads[1].rect, {.x = 0x30, .y = 0x40, .w = 1, .h = 2}));
  CHECK(SameRect(host.uploads[2].rect, {.x = 0x50, .y = 0x60, .w = 1, .h = 1}));
  CHECK(host.uploads[4].first == std::byte{5});
}

TEST_CASE("System resources upload the fixed SYSTEM.DAT image last",
          "[content-slice][content][system-resources]") {
  FakeSystemResources host;
  static_cast<void>(LoadSystemDat(host));
  REQUIRE(host.uploads.size() == 6);
  CHECK(
      SameRect(host.uploads[5].rect, {.x = 0x300, .y = 0xF0, .w = 4, .h = 2}));
  CHECK(host.uploads[5].bytes == 16);
  CHECK(host.uploads[5].first == std::byte{6});
}

TEST_CASE("System resources load the interface palette",
          "[content-slice][content][system-resources]") {
  FakeSystemResources host;
  const Content::SystemResourcesLoad load = LoadSystemDat(host);
  CHECK(load.buffered);
  CHECK(load.refused_sections == 0);
  CHECK(host.clut_loads == 1);
  CHECK(host.clut_row == 0xF);
  CHECK(host.clut_first == 0);
  CHECK(host.clut_count == 3);
  CHECK(host.clut_bytes == 6);
}

TEST_CASE("System resources pass on the interface and release the buffer",
          "[content-slice][content][system-resources]") {
  FakeSystemResources host;
  static_cast<void>(LoadSystemDat(host));
  CHECK(host.interface_loads == 1);
  CHECK(host.interface_bytes == Content::kSystemDatBytes - kInterfaceAt);
  CHECK(host.interface_first == std::byte{7});
  REQUIRE(host.released.size() == 1);
  CHECK(host.released[0] == host.storage.data());
}

TEST_CASE("System resources refuse sections that run past the file",
          "[content-slice][content][system-resources]") {
  FakeSystemResources host;
  host.storage.resize(Content::kSystemDatBytes);
  host.file = SystemDatFile();
  const std::span<std::byte> file(host.file);
  constexpr std::uint32_t kEnd = Content::kSystemDatBytes;
  PutU32(file, 0, kEnd + 4);
  PutI16(file, 0x64, -1);
  // A 5-pixel image whose last pixel runs past the end, and a 3-colour
  // palette with room for 2.
  PutU32(file, SectionOffsetAt(3), kEnd - 16);
  PutImage(file, kEnd - 16, {.w = 5, .h = 1}, std::byte{8});
  PutU32(file, SectionOffsetAt(2), kEnd - 8);
  PutI16(file, kEnd - 8, 3);
  PutU32(file, SectionOffsetAt(6), kEnd);
  PutU32(file, SectionOffsetAt(7),
         kEnd - Content::kSystemDatInterfaceBytes + 1);
  const Content::SystemResourcesLoad load = Content::LoadSystemResources(host);
  CHECK(load.buffered);
  CHECK(load.refused_sections == 0xCF);
  REQUIRE(host.uploads.size() == 2);
  CHECK(host.uploads[0].first == std::byte{4});
  CHECK(host.uploads[1].first == std::byte{5});
  CHECK(host.clut_loads == 0);
  CHECK(host.interface_loads == 0);
  CHECK(host.released.size() == 1);
}

TEST_CASE("System resources without a whole file buffer read nothing",
          "[content-slice][content][system-resources]") {
  FakeSystemResources host;
  host.storage.resize(100);
  host.file = SystemDatFile();
  Content::SystemResourcesLoad load = Content::LoadSystemResources(host);
  CHECK_FALSE(load.buffered);
  CHECK(load.refused_sections == 0);
  CHECK(host.read_sector == -1);
  CHECK(host.uploads.empty());
  REQUIRE(host.released.size() == 1);
  CHECK(host.released[0] == host.storage.data());
  host.storage.clear();
  host.released.clear();
  load = Content::LoadSystemResources(host);
  CHECK_FALSE(load.buffered);
  CHECK(host.released.empty());
  CHECK(host.read_sector == -1);
}

TEST_CASE("Retired content entries leave no module address bindings",
          "[content-slice][content][module-symbols]") {
  // No module image or dispatch table stores the promoted loaders, so the
  // typed cores are reached only through OWNED_CALL_BINDINGS and the Live
  // adapters (#276 slice content).
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  for (const std::uint32_t address : kRetiredBattleEntries) {
    CAPTURE(address);
    CHECK(Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                        address) == nullptr);
  }
  for (const std::uint32_t address : kRetiredMainEntries) {
    CAPTURE(address);
    CHECK(Content::ResolveModuleAddress(symbols, "SLUS_010.40", address) ==
          nullptr);
  }
  // The globals the Live bindings now own keep their addresses.
  CHECK(Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                      0x800F18F8u) ==
        &Content_overlayWorkspaceOwner);
  CHECK(Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                      0x800E8FC4u) ==
        &Content_assetQueuePeakUsage);
  CHECK(Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                      0x800EC4BCu) != nullptr);
}
