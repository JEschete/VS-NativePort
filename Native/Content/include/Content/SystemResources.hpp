#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstddef>
#include <cstdint>
#include <span>

// SYSTEM.DAT: the battle interface's images, palette and menu data, read
// once when the battle program starts. The file opens with eight
// little-endian u32 offsets from its start (decomp System_dat_h, INITBTL.PRG
// 18.c:23):
//   0, 1, 3, 4, 5  an image: i16 x, y, w, h, then w*h 16-bit pixels
//   2              a palette: i16 x (the colour count), i16, then colours
//   6              a fixed image: i16 w, h, then pixels, placed at (768, 240)
//   7              the menu interface data
namespace VS::Content {

inline constexpr std::int32_t kSystemDatSector = 1387;
inline constexpr std::uint32_t kSystemDatBytes = 188416;
inline constexpr std::size_t kSystemDatSections = 8;
// The interface data the battle menu uploads from its section: two 64x224
// and one 64x158 16-bit images (Menu_Framework_InitGameInterface).
inline constexpr std::size_t kSystemDatInterfaceBytes = 0x12F00;

enum class SystemDatSection : std::uint8_t {
  kImage0,
  kImage1,
  kPalette,
  kImage2,
  kImage3,
  kImage4,
  kFixedImage,
  kInterface,
};

struct VramRect {
  std::int16_t x = 0;
  std::int16_t y = 0;
  std::int16_t w = 0;
  std::int16_t h = 0;
};

// The heap, drive, VRAM and menu services the loader uses.
class SystemResourcesHost {
 public:
  SystemResourcesHost() = default;
  SystemResourcesHost(const SystemResourcesHost&) = delete;
  SystemResourcesHost& operator=(const SystemResourcesHost&) = delete;
  SystemResourcesHost(SystemResourcesHost&&) = delete;
  SystemResourcesHost& operator=(SystemResourcesHost&&) = delete;
  virtual ~SystemResourcesHost() = default;

  // A high-end game heap block of `bytes`; empty when the heap has no room.
  virtual std::span<std::byte> AcquireBuffer(std::size_t bytes) = 0;
  virtual void ReleaseBuffer(std::span<std::byte> buffer) = 0;
  // A blocking read of `destination.size()` bytes from `sector`.
  virtual void ReadFile(std::int32_t sector,
                        std::span<std::byte> destination) = 0;
  // `pixels` holds rect.w * rect.h 16-bit pixels.
  virtual void UploadImage(const VramRect& rect,
                           std::span<const std::byte> pixels) = 0;
  // `colors` holds `count` 16-bit colours for palette row `row`.
  virtual void LoadClutColors(std::span<const std::byte> colors,
                              std::uint32_t row, std::uint32_t first,
                              std::uint32_t count) = 0;
  // The interface data, from its offset to the end of the file (at least
  // kSystemDatInterfaceBytes).
  virtual void InitializeGameInterface(std::span<const std::byte> data) = 0;
};

struct SystemResourcesLoad {
  // False when the heap had no room for the file; nothing was read.
  bool buffered = false;
  // Bit n set: section n (SystemDatSection) lay outside the file and was
  // skipped.
  std::uint8_t refused_sections = 0;
};

// Reads SYSTEM.DAT into a heap block, uploads its images and palette, hands
// the interface data to the menu and frees the block.
SystemResourcesLoad LoadSystemResources(SystemResourcesHost& host);

}  // namespace VS::Content
