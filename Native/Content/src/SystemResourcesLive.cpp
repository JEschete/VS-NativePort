// Hand-maintained (tools/port/owned.csv).
// Live binding for the SYSTEM.DAT loader (SystemResources.hpp).
#include "Content/SystemResourcesLive.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

#include "Content/CdDriveLive.hpp"
#include "Content/SystemResources.hpp"
#include "Core/Diagnostics.h"
#include "Core/Memory/HeapLive.hpp"
#include "Render/PaletteAnimationLive.hpp"

extern "C" {
#include "Content/DiscLayout.h"
#include "Legacy/Psx.h"
#include "Menu/Framework/Framework.h"
#include "SystemResources.h"
}

namespace VS::Content {
namespace {
constexpr std::size_t SectionOffsetAt(SystemDatSection section) {
  return static_cast<std::size_t>(section) * sizeof(std::uint32_t);
}

static_assert(kSystemDatSector == VS_SYSTEM_DAT_LBA);
static_assert(kSystemDatBytes == VS_SYSTEM_DAT_SIZE);
static_assert(sizeof(Content_SystemDatHeader) ==
              kSystemDatSections * sizeof(std::uint32_t));
static_assert(offsetof(Content_SystemDatHeader, image0Offset) ==
              SectionOffsetAt(SystemDatSection::kImage0));
static_assert(offsetof(Content_SystemDatHeader, paletteOffset) ==
              SectionOffsetAt(SystemDatSection::kPalette));
static_assert(offsetof(Content_SystemDatHeader, fixedImageOffset) ==
              SectionOffsetAt(SystemDatSection::kFixedImage));
static_assert(offsetof(Content_SystemDatHeader, interfaceDataOffset) ==
              SectionOffsetAt(SystemDatSection::kInterface));
static_assert(sizeof(RECT) == sizeof(VramRect));

class LiveSystemResourcesHost final : public SystemResourcesHost {
 public:
  std::span<std::byte> AcquireBuffer(std::size_t bytes) override {
    void* const buffer = Core::Memory::AllocateLiveFromHighEnd(bytes);
    if (buffer == nullptr) return {};
    return {static_cast<std::byte*>(buffer), bytes};
  }
  void ReleaseBuffer(std::span<std::byte> buffer) override {
    Core::Memory::FreeLiveFromHighEnd(buffer.data());
  }
  void ReadFile(std::int32_t sector,
                std::span<std::byte> destination) override {
    ReadLiveDiscFileBlocking(sector, static_cast<int>(destination.size()),
                             destination.data());
  }
  void UploadImage(const VramRect& rect,
                   std::span<const std::byte> pixels) override {
    RECT area{.x = rect.x, .y = rect.y, .w = rect.w, .h = rect.h};
    // LoadImage only reads the pixels.
    LoadImage(&area,
              reinterpret_cast<u_long*>(const_cast<std::byte*>(pixels.data())));
  }
  void LoadClutColors(std::span<const std::byte> colors, std::uint32_t row,
                      std::uint32_t first, std::uint32_t count) override {
    Render::LoadLiveClutColors(reinterpret_cast<const u_short*>(colors.data()),
                               row, first, count);
  }
  void InitializeGameInterface(std::span<const std::byte> data) override {
    // The menu only uploads the interface images from it.
    Menu_Framework_InitGameInterface(const_cast<std::byte*>(data.data()));
  }
};
}  // namespace

void LoadLiveSystemResources() {
  LiveSystemResourcesHost host;
  const SystemResourcesLoad load = LoadSystemResources(host);
  if (!load.buffered) {
    Core_Warnf("content",
               "LoadSystemResources: no game heap room for SYSTEM.DAT (%u "
               "bytes); nothing loaded",
               static_cast<unsigned>(kSystemDatBytes));
  } else if (load.refused_sections != 0) {
    Core_Warnf("content",
               "LoadSystemResources: skipped SYSTEM.DAT sections outside the "
               "file (mask 0x%02X)",
               static_cast<unsigned>(load.refused_sections));
  }
}

}  // namespace VS::Content
