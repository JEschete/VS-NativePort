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

#include "Core/Memory/HeapLive.hpp"
#include "Core/RandomLive.hpp"
#include "Render/PaletteAnimationLive.hpp"
#include "Render/TexturePagesLive.hpp"
#include "Render/UiPacketsLive.hpp"

extern "C" {
#include "CdDrive.h"
#include "Content/DiscLayout.h"
#include "Core/Diagnostics.h"
#include "DiscQueue.h"
#include "Render/Display.h"
}

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
constexpr GimFile File(std::uint32_t lba, std::uint32_t size) {
  return {.lba = lba, .sectors = size >> kSectorShift};
}
constexpr GimFile kStaff13 = File(VS_STAFF013_GIM_LBA, VS_STAFF013_GIM_SIZE);
constexpr std::array<GimFile, kGimImageCount> kGimFiles = {
    File(VS_STAFF011_GIM_LBA, VS_STAFF011_GIM_SIZE),
    File(VS_STAFF001_GIM_LBA, VS_STAFF001_GIM_SIZE),
    File(VS_STAFF002_GIM_LBA, VS_STAFF002_GIM_SIZE),
    File(VS_STAFF003_GIM_LBA, VS_STAFF003_GIM_SIZE),
    File(VS_STAFF004_GIM_LBA, VS_STAFF004_GIM_SIZE),
    File(VS_STAFF005_GIM_LBA, VS_STAFF005_GIM_SIZE),
    File(VS_STAFF006_GIM_LBA, VS_STAFF006_GIM_SIZE),
    File(VS_STAFF007_GIM_LBA, VS_STAFF007_GIM_SIZE),
    File(VS_STAFF008_GIM_LBA, VS_STAFF008_GIM_SIZE),
    File(VS_GAMEOVER_GIM_LBA, VS_GAMEOVER_GIM_SIZE),
    File(VS_STAFF009_GIM_LBA, VS_STAFF009_GIM_SIZE),
    File(VS_STAFF010_GIM_LBA, VS_STAFF010_GIM_SIZE),
    File(VS_STAFF012_GIM_LBA, VS_STAFF012_GIM_SIZE),
    File(VS_ENKEI_GIM_LBA, VS_ENKEI_GIM_SIZE),
    File(VS_LOGO_G22_GIM_LBA, VS_LOGO_G22_GIM_SIZE),
    kStaff13,
    File(VS_1142_GIM_LBA, VS_1142_GIM_SIZE),
    File(VS_N_LEA_GIM_LBA, VS_N_LEA_GIM_SIZE),
    File(VS_LOGO_28_GIM_LBA, VS_LOGO_28_GIM_SIZE),
    File(VS_LOGO_29_GIM_LBA, VS_LOGO_29_GIM_SIZE),
    File(VS_LOGO_30_GIM_LBA, VS_LOGO_30_GIM_SIZE),
    File(VS_LOGO_31_GIM_LBA, VS_LOGO_31_GIM_SIZE),
    File(VS_VKP_2_GIM_LBA, VS_VKP_2_GIM_SIZE),
    File(VS_VKP_1_GIM_LBA, VS_VKP_1_GIM_SIZE),
    File(VS_VKP_0_GIM_LBA, VS_VKP_0_GIM_SIZE),
    File(VS_JYO_GIM_LBA, VS_JYO_GIM_SIZE),
    File(VS_HEADQUAT_GIM_LBA, VS_HEADQUAT_GIM_SIZE),
    File(VS_DEMOENK_GIM_LBA, VS_DEMOENK_GIM_SIZE),
    File(VS_DADDY_GIM_LBA, VS_DADDY_GIM_SIZE),
    File(VS_BLACK_GIM_LBA, VS_BLACK_GIM_SIZE),
    File(VS_ENKEI2_GIM_LBA, VS_ENKEI2_GIM_SIZE),
    File(VS_JO_GIM_LBA, VS_JO_GIM_SIZE),
    File(VS_SUNRISE_GIM_LBA, VS_SUNRISE_GIM_SIZE),
    File(VS_MAIN03_GIM_LBA, VS_MAIN03_GIM_SIZE),
    File(VS_EPI_INF_GIM_LBA, VS_EPI_INF_GIM_SIZE),
    File(VS_EPILOGUE_GIM_LBA, VS_EPILOGUE_GIM_SIZE),
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
    File(VS_EPI_E01B_GIM_LBA, VS_EPI_E01B_GIM_SIZE),
    File(VS_EV_MASK_GIM_LBA, VS_EV_MASK_GIM_SIZE),
    File(VS_019XXX01_GIM_LBA, VS_019XXX01_GIM_SIZE),
    File(VS_DEMO_001_GIM_LBA, VS_DEMO_001_GIM_SIZE),
    File(VS_DEMO_002_GIM_LBA, VS_DEMO_002_GIM_SIZE),
    File(VS_DEMO_003_GIM_LBA, VS_DEMO_003_GIM_SIZE),
    File(VS_DEMO_004_GIM_LBA, VS_DEMO_004_GIM_SIZE),
    File(VS_DEMO_005_GIM_LBA, VS_DEMO_005_GIM_SIZE),
    File(VS_DEMO_006_GIM_LBA, VS_DEMO_006_GIM_SIZE),
    File(VS_DEMO_007_GIM_LBA, VS_DEMO_007_GIM_SIZE),
    File(VS_DEMO_008_GIM_LBA, VS_DEMO_008_GIM_SIZE),
    File(VS_SQUARE_GIM_LBA, VS_SQUARE_GIM_SIZE),
    File(VS_DEMOFIN_GIM_LBA, VS_DEMOFIN_GIM_SIZE),
};
static_assert(std::ranges::all_of(kGimFiles, [](const GimFile& file) {
  return file.sectors != 0 && file.sectors <= kMaxSectors;
}));

GimImageSet* live_set = nullptr;
std::uint16_t* live_file = nullptr;
std::size_t live_file_words = 0;
Content_DiscQueueSlot* live_request = nullptr;
std::uint64_t refused_requests = 0;

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
  if (std::has_single_bit(++refused_requests)) {
    Core_Errorf("render", "GIM image request refused: %s (%" PRIu64 " refused)",
                RefusalName(refusal), refused_requests);
  }
}

class LiveGimHost final : public GimHost {
 public:
  [[nodiscard]] bool FileReadFinished() override {
    return live_request != nullptr &&
           live_request->state == Content_DiscRequestState_Loaded;
  }
  void ReleaseFileRead() override {
    if (live_request != nullptr) {
      Content_ReleaseDiscRequest(live_request);
      live_request = nullptr;
    }
  }
  [[nodiscard]] std::span<const std::uint16_t> FileWords() override {
    if (live_file == nullptr) {
      return {};
    }
    return {live_file, live_file_words};
  }
  void FreeFile() override {
    if (live_file != nullptr) {
      Core::Memory::FreeLiveFromHighEnd(live_file);
      live_file = nullptr;
      live_file_words = 0;
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
  if (live_set != nullptr) {
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
  const Content_DiscFile disc_file{.lba = static_cast<int>(file.lba),
                                   .size = static_cast<int>(bytes)};
  Content_DiscQueueSlot* request = Content_AllocateDiscRequest(&disc_file);
  if (request == nullptr) {
    Core::Memory::FreeLiveFromHighEnd(data);
    Core::Memory::FreeLiveFromHighEnd(block);
    Report(GimRefusal::kNoDiscRequest);
    return;
  }
  live_set = std::construct_at(static_cast<GimImageSet*>(block),
                               MakeGimImageSet(image_id, first_page));
  live_file = static_cast<std::uint16_t*>(data);
  live_file_words = bytes / sizeof(std::uint16_t);
  live_request = request;
  Content_EnqueueDiscRequest(live_request, live_file);
}

void ReleaseLiveGimImages() {
  if (live_set == nullptr) {
    return;
  }
  LiveGimHost host;
  if (ReleaseGimImageSet(*live_set, host)) {
    Core::Memory::FreeLiveFromHighEnd(live_set);
    live_set = nullptr;
  }
}

void UpdateLiveGimImages() {
  if (live_set == nullptr) {
    return;
  }
  LiveGimHost host;
  if (!UpdateGimImageSet(*live_set, host)) {
    return;
  }
  for (std::size_t layer = 0; layer < kGimLayerCount; ++layer) {
    if (!live_set->layers[layer].draw_enabled) {
      continue;
    }
    const GimOrderingEntry entry = GimLayerOrderingEntry(*live_set, layer);
    u_long* chain = LiveFrameOrderingEntry(
        entry.scene ? LiveOrderingBase::kScene : LiveOrderingBase::kOverlay,
        entry.entry);
    if (chain == nullptr) {
      Report(GimRefusal::kNoOrderingEntry);
      continue;
    }
    LivePacketQueue queue(chain);
    DrawGimLayer(queue, *live_set, layer, Render_frameBuf);
  }
}

void ForgetLiveGimImages() {
  live_set = nullptr;
  live_file = nullptr;
  live_file_words = 0;
  live_request = nullptr;
}

bool LiveGimImagesPresent() { return live_set != nullptr; }

int LiveGimImageLoading() {
  if (live_set == nullptr) {
    Report(GimRefusal::kNoSet);
    return 0;
  }
  return live_set->layers[0].loading ? 1 : 0;
}

void ConfigureLiveGimLayer(int layer, int routine, int offset_x, int offset_y,
                           int scale_and_order, int brightness) {
  if (live_set == nullptr) {
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
      *live_set, *index,
      {.routine = static_cast<GimDrawRoutine>(routine),
       .placement = Placement(offset_x, offset_y, scale_and_order, brightness)},
      host);
}

std::optional<GimPlacement> LiveGimLayerPlacement(int layer) {
  if (live_set == nullptr) {
    return std::nullopt;
  }
  const std::optional<std::size_t> index = LayerIndex(layer);
  if (!index.has_value()) {
    return std::nullopt;
  }
  return live_set->layers[*index].placement;
}

void PlaceLiveGimLayer(int layer, int offset_x, int offset_y,
                       int scale_and_order, int brightness) {
  if (live_set == nullptr) {
    return;
  }
  const std::optional<std::size_t> index = LayerIndex(layer);
  if (index.has_value()) {
    live_set->layers[*index].placement =
        Placement(offset_x, offset_y, scale_and_order, brightness);
  }
}

const GimImageSet* LiveGimImages() { return live_set; }

std::uint64_t RefusedLiveGimRequestCount() { return refused_requests; }
}  // namespace VS::Render
