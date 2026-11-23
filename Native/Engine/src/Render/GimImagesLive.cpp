// Hand-maintained (tools/port/owned.csv).
// Live binding for the event scripts' GIM images (GimImages.cpp): the high-end
// heap, the disc queue, VRAM, the texture-page table and the frame's
// ordering tables.
#include "Render/GimImagesLive.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <type_traits>

#include "Content/CdDriveLive.hpp"
#include "Content/DiscLayout.hpp"
#include "Content/DiscQueue.hpp"
#include "Core/Diagnostics.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Core/RandomLive.hpp"
#include "Render/Display.hpp"
#include "Render/PaletteAnimationLive.hpp"
#include "Render/TexturePagesLive.hpp"
#include "Render/UiPacketsLive.hpp"

using VS::Content::disc::Locate;

namespace VS::Render {
namespace {
// The original's block held three 0x3F8-byte gim_t layers (5BF94.c:2355);
// the set keeps that heap footprint.
constexpr std::size_t kOriginalBlockBytes = 0xBE8;
static_assert(sizeof(GimImageSet) <= kOriginalBlockBytes);
static_assert(std::is_trivially_destructible_v<GimImageSet>,
              "the set is freed without running a destructor");
constexpr int kSectorShift = 11;
constexpr std::uint32_t kClutRow = 3;
constexpr int kHalfwordBits = 16;
constexpr std::uint32_t kHalfwordMask = 0xFFFF;
constexpr std::uint32_t kMaxSectors = 0xFF;

// _gimLbas (BATTLE.PRG 0x800EB608): each image's sector and length. The
// original packed them as lba << 8 | sectors; the sector count is a byte.
struct GimFile {
  std::uint32_t lba = 0;
  std::uint32_t sectors = 0;
};
constexpr GimFile File(VS::Content::disc::FileExtent file) {
  return {.lba = static_cast<std::uint32_t>(file.lba),
          .sectors = static_cast<std::uint32_t>(file.size) >> kSectorShift};
}
constexpr GimFile kStaff13 = File(Locate("GIM/STAFF013.GIM"));
constexpr std::array<GimFile, kGimImageCount> kGimFiles = {
    File(Locate("GIM/STAFF011.GIM")),
    File(Locate("GIM/STAFF001.GIM")),
    File(Locate("GIM/STAFF002.GIM")),
    File(Locate("GIM/STAFF003.GIM")),
    File(Locate("GIM/STAFF004.GIM")),
    File(Locate("GIM/STAFF005.GIM")),
    File(Locate("GIM/STAFF006.GIM")),
    File(Locate("GIM/STAFF007.GIM")),
    File(Locate("GIM/STAFF008.GIM")),
    File(Locate("GIM/GAMEOVER.GIM")),
    File(Locate("GIM/STAFF009.GIM")),
    File(Locate("GIM/STAFF010.GIM")),
    File(Locate("GIM/STAFF012.GIM")),
    File(Locate("GIM/ENKEI.GIM")),
    File(Locate("GIM/LOGO_G22.GIM")),
    kStaff13,
    File(Locate("GIM/1142.GIM")),
    File(Locate("GIM/N_LEA.GIM")),
    File(Locate("GIM/LOGO_28.GIM")),
    File(Locate("GIM/LOGO_29.GIM")),
    File(Locate("GIM/LOGO_30.GIM")),
    File(Locate("GIM/LOGO_31.GIM")),
    File(Locate("GIM/VKP_2.GIM")),
    File(Locate("GIM/VKP_1.GIM")),
    File(Locate("GIM/VKP_0.GIM")),
    File(Locate("GIM/JYO.GIM")),
    File(Locate("GIM/HEADQUAT.GIM")),
    File(Locate("GIM/DEMOENK.GIM")),
    File(Locate("GIM/DADDY.GIM")),
    File(Locate("GIM/BLACK.GIM")),
    File(Locate("GIM/ENKEI2.GIM")),
    File(Locate("GIM/JO.GIM")),
    File(Locate("GIM/SUNRISE.GIM")),
    File(Locate("GIM/MAIN03.GIM")),
    File(Locate("GIM/EPI_INF.GIM")),
    File(Locate("GIM/EPILOGUE.GIM")),
    kStaff13,
    kStaff13,
    kStaff13,
    kStaff13,
    kStaff13,
    kStaff13,
    kStaff13,
    kStaff13,
    kStaff13,
    kStaff13,
    kStaff13,
    kStaff13,
    kStaff13,
    kStaff13,
    kStaff13,
    File(Locate("GIM/EPI_E01B.GIM")),
    File(Locate("GIM/EV_MASK.GIM")),
    File(Locate("GIM/019XXX01.GIM")),
    File(Locate("GIM/DEMO_001.GIM")),
    File(Locate("GIM/DEMO_002.GIM")),
    File(Locate("GIM/DEMO_003.GIM")),
    File(Locate("GIM/DEMO_004.GIM")),
    File(Locate("GIM/DEMO_005.GIM")),
    File(Locate("GIM/DEMO_006.GIM")),
    File(Locate("GIM/DEMO_007.GIM")),
    File(Locate("GIM/DEMO_008.GIM")),
    File(Locate("GIM/SQUARE.GIM")),
    File(Locate("GIM/DEMOFIN.GIM")),
};
static_assert(std::ranges::all_of(kGimFiles, [](const GimFile& file) {
  return file.sectors != 0 && file.sectors <= kMaxSectors;
}));

GimImageSet* liveSet = nullptr;
std::uint16_t* liveFile = nullptr;
std::size_t liveFileWords = 0;
VS::Content::DiscQueueSlot* liveRequest = nullptr;
std::uint64_t refusedRequests = 0;

const char* RefusalName(GimRefusal refusal) {
  switch (refusal) {
    case GimRefusal::kUnknownImage:
      return "unknown image";
    case GimRefusal::kNoHeap:
      return "no heap block";
    case GimRefusal::kNoDiscRequest:
      return "no disc request slot";
    case GimRefusal::kLayerOutOfRange:
      return "layer past the third";
    case GimRefusal::kRoutineOutOfRange:
      return "unknown draw routine";
    case GimRefusal::kTileTableTooLarge:
      return "tile grid past the tile table";
    case GimRefusal::kFileTooShort:
      return "file shorter than its layout";
    case GimRefusal::kTooManyPages:
      return "more texture pages than the set holds";
    case GimRefusal::kPagesUnavailable:
      return "texture pages not lent";
    case GimRefusal::kNoSet:
      return "no image set";
    case GimRefusal::kNoOrderingEntry:
      return "ordering-table entry outside the frame's table";
  }
  return "unknown";
}

void Report(GimRefusal refusal) {
  if (std::has_single_bit(++refusedRequests)) {
    VS::Core::Errorf("render",
                     "GIM image request refused: %s (%" PRIu64 " refused)",
                     RefusalName(refusal), refusedRequests);
  }
}

class LiveGimHost final : public GimHost {
 public:
  [[nodiscard]] bool FileReadFinished() override {
    return liveRequest != nullptr &&
           liveRequest->state == VS::Content::DiscRequestState::kLoaded;
  }
  void ReleaseFileRead() override {
    if (liveRequest != nullptr) {
      Content::ReleaseLiveDiscRequest(liveRequest);
      liveRequest = nullptr;
    }
  }
  [[nodiscard]] std::span<const std::uint16_t> FileWords() override {
    if (liveFile == nullptr) {
      return {};
    }
    return {liveFile, liveFileWords};
  }
  void FreeFile() override {
    if (liveFile != nullptr) {
      Core::Memory::FreeLiveFromHighEnd(liveFile);
      liveFile = nullptr;
      liveFileWords = 0;
    }
  }
  void UploadImage(const ScreenRect& area,
                   std::span<const std::uint16_t> pixels) override {
    const std::size_t needed = static_cast<std::size_t>(area.width) *
                               static_cast<std::size_t>(area.height);
    if (pixels.size() < needed) {
      Report(GimRefusal::kFileTooShort);
      return;
    }
    const auto low = [](std::int16_t value) {
      return static_cast<std::uint32_t>(static_cast<std::uint16_t>(value));
    };
    UploadLiveImageRectangle(
        static_cast<int>(low(area.x) | (low(area.y) << kHalfwordBits)),
        pixels.data(),
        static_cast<int>((low(area.width) & kHalfwordMask) |
                         (low(area.height) << kHalfwordBits)));
  }
  void LoadClutColors(std::span<const std::uint16_t> colors) override {
    LoadLiveClutRow(kClutRow, 0, colors);
  }
  [[nodiscard]] bool ReservePages(std::uint32_t first,
                                  std::uint32_t count) override {
    return ReserveLiveTexturePageRange(first, count, kGimTextureOwner) != 0;
  }
  void RetirePages(std::uint32_t first, std::uint32_t count) override {
    RetireLiveTexturePageRange(first, count, kGimTextureOwner);
  }
  [[nodiscard]] std::uint32_t RandomBits(int bit_count) override {
    return Core::NextLiveInterfaceRandomBits(bit_count);
  }
  void Refuse(GimRefusal refusal) override { Report(refusal); }
};

[[nodiscard]] std::optional<std::size_t> LayerIndex(int layer) {
  if (layer < 0 || static_cast<std::size_t>(layer) >= kGimLayerCount) {
    Report(GimRefusal::kLayerOutOfRange);
    return std::nullopt;
  }
  return static_cast<std::size_t>(layer);
}

[[nodiscard]] GimPlacement Placement(int offset_x, int offset_y,
                                     int scale_and_order, int brightness) {
  return {.offset_x = static_cast<std::int16_t>(offset_x),
          .offset_y = static_cast<std::int16_t>(offset_y),
          .scale_and_order = static_cast<std::int16_t>(scale_and_order),
          .brightness = static_cast<std::uint8_t>(brightness)};
}
}  // namespace

void QueueLiveGimImage(int image_id, int first_page) {
  if (liveSet != nullptr) {
    return;
  }
  if (image_id < 0 || image_id >= kGimImageCount) {
    Report(GimRefusal::kUnknownImage);
    return;
  }
  const GimFile& file = kGimFiles[static_cast<std::size_t>(image_id)];
  void* block = Core::Memory::AllocateLiveFromHighEnd(kOriginalBlockBytes);
  if (block == nullptr) {
    Report(GimRefusal::kNoHeap);
    return;
  }
  const std::size_t bytes = std::size_t{file.sectors} << kSectorShift;
  void* data = Core::Memory::AllocateLiveFromHighEnd(bytes);
  if (data == nullptr) {
    Core::Memory::FreeLiveFromHighEnd(block);
    Report(GimRefusal::kNoHeap);
    return;
  }
  const VS::Content::DiscFile disc_file{.lba = static_cast<int>(file.lba),
                                        .size = static_cast<int>(bytes)};
  VS::Content::DiscQueueSlot* request =
      Content::AllocateLiveDiscRequest(&disc_file);
  if (request == nullptr) {
    Core::Memory::FreeLiveFromHighEnd(data);
    Core::Memory::FreeLiveFromHighEnd(block);
    Report(GimRefusal::kNoDiscRequest);
    return;
  }
  liveSet = std::construct_at(static_cast<GimImageSet*>(block),
                              MakeGimImageSet(image_id, first_page));
  liveFile = static_cast<std::uint16_t*>(data);
  liveFileWords = bytes / sizeof(std::uint16_t);
  liveRequest = request;
  Content::EnqueueLiveDiscRequest(liveRequest, liveFile);
}

void ReleaseLiveGimImages() {
  if (liveSet == nullptr) {
    return;
  }
  LiveGimHost host;
  if (ReleaseGimImageSet(*liveSet, host)) {
    Core::Memory::FreeLiveFromHighEnd(liveSet);
    liveSet = nullptr;
  }
}

void UpdateLiveGimImages() {
  if (liveSet == nullptr) {
    return;
  }
  LiveGimHost host;
  if (!UpdateGimImageSet(*liveSet, host)) {
    return;
  }
  for (std::size_t layer = 0; layer < kGimLayerCount; ++layer) {
    if (!liveSet->layers[layer].draw_enabled) {
      continue;
    }
    const GimOrderingEntry entry = GimLayerOrderingEntry(*liveSet, layer);
    std::uint32_t* chain = LiveFrameOrderingEntry(
        entry.scene ? LiveOrderingBase::kScene : LiveOrderingBase::kOverlay,
        entry.entry);
    if (chain == nullptr) {
      Report(GimRefusal::kNoOrderingEntry);
      continue;
    }
    LivePacketQueue queue(chain);
    DrawGimLayer(queue, *liveSet, layer, frameBuf);
  }
}

void ForgetLiveGimImages() {
  liveSet = nullptr;
  liveFile = nullptr;
  liveFileWords = 0;
  liveRequest = nullptr;
}

bool LiveGimImagesPresent() { return liveSet != nullptr; }

int LiveGimImageLoading() {
  if (liveSet == nullptr) {
    Report(GimRefusal::kNoSet);
    return 0;
  }
  return liveSet->layers[0].loading ? 1 : 0;
}

void ConfigureLiveGimLayer(int layer, int routine, int offset_x, int offset_y,
                           int scale_and_order, int brightness) {
  if (liveSet == nullptr) {
    return;
  }
  const std::optional<std::size_t> index = LayerIndex(layer);
  if (!index.has_value()) {
    return;
  }
  if (routine < 0 || routine >= kGimDrawRoutineCount) {
    Report(GimRefusal::kRoutineOutOfRange);
    return;
  }
  LiveGimHost host;
  ConfigureGimLayer(
      *liveSet, *index,
      {.routine = static_cast<GimDrawRoutine>(routine),
       .placement = Placement(offset_x, offset_y, scale_and_order, brightness)},
      host);
}

std::optional<GimPlacement> LiveGimLayerPlacement(int layer) {
  if (liveSet == nullptr) {
    return std::nullopt;
  }
  const std::optional<std::size_t> index = LayerIndex(layer);
  if (!index.has_value()) {
    return std::nullopt;
  }
  return liveSet->layers[*index].placement;
}

void PlaceLiveGimLayer(int layer, int offset_x, int offset_y,
                       int scale_and_order, int brightness) {
  if (liveSet == nullptr) {
    return;
  }
  const std::optional<std::size_t> index = LayerIndex(layer);
  if (index.has_value()) {
    liveSet->layers[*index].placement =
        Placement(offset_x, offset_y, scale_and_order, brightness);
  }
}

const GimImageSet* LiveGimImages() { return liveSet; }

std::uint64_t RefusedLiveGimRequestCount() { return refusedRequests; }
}  // namespace VS::Render
