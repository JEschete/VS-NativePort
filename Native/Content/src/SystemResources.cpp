// Hand-maintained (tools/port/owned.csv).
#include "Content/SystemResources.hpp"  // NOLINT(build/include): typed API

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace VS::Content {
namespace {
constexpr std::size_t kOffsetBytes = 4;
constexpr std::size_t kBitsPerByte = 8;
constexpr std::size_t kRectBytes = 8;
constexpr std::size_t kPaletteColorsAt = 4;
constexpr std::size_t kFixedImagePixelsAt = 4;
constexpr std::size_t kPixelBytes = 2;
constexpr std::uint32_t kInterfacePaletteRow = 0xF;
constexpr VramRect kFixedImagePlacement{.x = 0x300, .y = 0xF0};

std::uint32_t ReadWord(std::span<const std::byte> bytes, std::size_t at) {
  std::uint32_t value = 0;
  for (std::size_t i = 0; i < kOffsetBytes; ++i) {
    value |= std::uint32_t{std::to_integer<std::uint8_t>(bytes[at + i])}
             << (kBitsPerByte * i);
  }
  return value;
}

std::int16_t ReadHalf(std::span<const std::byte> bytes, std::size_t at) {
  return static_cast<std::int16_t>(
      std::to_integer<std::uint8_t>(bytes[at]) |
      (std::to_integer<std::uint8_t>(bytes[at + 1]) << kBitsPerByte));
}

class Loader {
 public:
  Loader(SystemResourcesHost& host, std::span<const std::byte> file)
      : host_(host), file_(file) {}

  // The section's bytes from its offset, holding at least `minimum`.
  std::optional<std::span<const std::byte>> Section(SystemDatSection section,
                                                    std::size_t minimum) {
    const std::size_t at = ReadWord(file_, Index(section) * kOffsetBytes);
    if (at > file_.size() || file_.size() - at < minimum) {
      Refuse(section);
      return std::nullopt;
    }
    return file_.subspan(at);
  }

  // The w*h pixels after a header at `pixels_at`, if the section holds them.
  void Upload(SystemDatSection section, std::span<const std::byte> bytes,
              const VramRect& rect, std::size_t pixels_at) {
    if (rect.w < 0 || rect.h < 0) {
      Refuse(section);
      return;
    }
    const std::size_t pixel_bytes = static_cast<std::size_t>(rect.w) *
                                    static_cast<std::size_t>(rect.h) *
                                    kPixelBytes;
    if (bytes.size() - pixels_at < pixel_bytes) {
      Refuse(section);
      return;
    }
    host_.UploadImage(rect, bytes.subspan(pixels_at, pixel_bytes));
  }

  void Image(SystemDatSection section) {
    const auto bytes = Section(section, kRectBytes);
    if (!bytes) return;
    const VramRect rect{.x = ReadHalf(*bytes, 0),
                        .y = ReadHalf(*bytes, 2),
                        .w = ReadHalf(*bytes, 4),
                        .h = ReadHalf(*bytes, 6)};
    Upload(section, *bytes, rect, kRectBytes);
  }

  void FixedImage() {
    const auto bytes =
        Section(SystemDatSection::kFixedImage, kFixedImagePixelsAt);
    if (!bytes) return;
    VramRect rect = kFixedImagePlacement;
    rect.w = ReadHalf(*bytes, 0);
    rect.h = ReadHalf(*bytes, 2);
    Upload(SystemDatSection::kFixedImage, *bytes, rect, kFixedImagePixelsAt);
  }

  void Palette() {
    const auto bytes = Section(SystemDatSection::kPalette, kPaletteColorsAt);
    if (!bytes) return;
    // lh: the colour count is the header's signed x.
    const std::int16_t count = ReadHalf(*bytes, 0);
    if (count < 0 || (bytes->size() - kPaletteColorsAt) / kPixelBytes <
                         static_cast<std::size_t>(count)) {
      Refuse(SystemDatSection::kPalette);
      return;
    }
    host_.LoadClutColors(
        bytes->subspan(kPaletteColorsAt,
                       static_cast<std::size_t>(count) * kPixelBytes),
        kInterfacePaletteRow, 0, static_cast<std::uint32_t>(count));
  }

  void Interface() {
    if (const auto bytes =
            Section(SystemDatSection::kInterface, kSystemDatInterfaceBytes)) {
      host_.InitializeGameInterface(*bytes);
    }
  }

  [[nodiscard]] std::uint8_t Refused() const { return refused_; }

 private:
  static std::size_t Index(SystemDatSection section) {
    return static_cast<std::size_t>(section);
  }

  void Refuse(SystemDatSection section) {
    refused_ = static_cast<std::uint8_t>(refused_ | (1U << Index(section)));
  }

  SystemResourcesHost& host_;
  std::span<const std::byte> file_;
  std::uint8_t refused_ = 0;
};
}  // namespace

// decomp: INITBTL.PRG _loadSystemDat (src/BATTLE/INITBTL.PRG/18.c:258)
SystemResourcesLoad LoadSystemResources(SystemResourcesHost& host) {
  const std::span<std::byte> buffer = host.AcquireBuffer(kSystemDatBytes);
  if (buffer.size() < kSystemDatBytes) {
    if (!buffer.empty()) host.ReleaseBuffer(buffer);
    return {};
  }
  host.ReadFile(kSystemDatSector, buffer.first(kSystemDatBytes));
  Loader loader(host, buffer.first(kSystemDatBytes));
  loader.Image(SystemDatSection::kImage0);
  loader.Image(SystemDatSection::kImage1);
  loader.Palette();
  loader.Image(SystemDatSection::kImage2);
  loader.Image(SystemDatSection::kImage3);
  loader.Image(SystemDatSection::kImage4);
  loader.FixedImage();
  loader.Interface();
  host.ReleaseBuffer(buffer);
  return {.buffered = true, .refused_sections = loader.Refused()};
}

}  // namespace VS::Content
