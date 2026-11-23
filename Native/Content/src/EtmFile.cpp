// Strict ETM effect texture sheet decoding and encoding (EtmFile.hpp).
#include "Content/EtmFile.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <format>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "Content/ImageFiles.hpp"
#include "Content/NativeImage.hpp"

namespace VS::Content {
namespace {
constexpr std::size_t kHeightAt = 2;
constexpr std::size_t kHalfwordBytes = 2;
constexpr unsigned kByteBits = 8;

std::uint16_t HalfwordAt(std::span<const std::byte> data, std::size_t at) {
  return static_cast<std::uint16_t>(
      std::to_integer<std::uint16_t>(data[at]) |
      (std::to_integer<std::uint16_t>(data[at + 1]) << kByteBits));
}

void PutHalfword(std::vector<std::byte>& out, std::uint16_t value) {
  out.push_back(static_cast<std::byte>(value));
  out.push_back(static_cast<std::byte>(value >> kByteBits));
}

std::expected<void, std::string> CheckHeight(std::size_t height) {
  if (height == 0 || height > kEtmMaxHeight) {
    return std::unexpected(
        std::format("the sheet is {} lines high; it must be 1 to {}", height,
                    kEtmMaxHeight));
  }
  return {};
}

// Whether `document` is a region of an ETM sheet: an indexed image one sheet
// line wide, starting on a line after the header.
std::expected<void, std::string> CheckRegion(const ImageDocument& document) {
  const NativeImage& image = document.image;
  if (!IsIndexed(image.depth) ||
      image.width != kEtmLineWords * PixelsPerHalfword(image.depth)) {
    return std::unexpected(
        std::format("{} is not an indexed image one sheet line ({} halfwords) "
                    "wide",
                    document.id, kEtmLineWords));
  }
  const std::uint64_t offset = document.provenance.offset;
  if (offset < kEtmHeaderBytes ||
      (offset - kEtmHeaderBytes) % kEtmLineBytes != 0) {
    return std::unexpected(std::format(
        "{} starts at byte 0x{:X}, which is not the start of a sheet line",
        document.id, offset));
  }
  if (auto valid = ValidateImage(image); !valid) {
    return std::unexpected(std::format("{}: {}", document.id, valid.error()));
  }
  return {};
}
}  // namespace

std::expected<EtmSheet, std::string> DecodeEtmFile(
    std::span<const std::byte> data) {
  if (data.size() < kEtmHeaderBytes) {
    return std::unexpected(std::format("the file is {} bytes; the header is {}",
                                       data.size(), kEtmHeaderBytes));
  }
  if (const std::uint16_t marker = HalfwordAt(data, 0); marker != kEtmMarker) {
    return std::unexpected(std::format("the marker is 0x{:04X}, not 0x{:04X}",
                                       marker, kEtmMarker));
  }
  const std::size_t height = HalfwordAt(data, kHeightAt);
  if (auto checked = CheckHeight(height); !checked) {
    return std::unexpected(checked.error());
  }
  const std::size_t expected = kEtmHeaderBytes + (height * kEtmLineBytes);
  if (data.size() != expected) {
    return std::unexpected(
        std::format("the file is {} bytes; a sheet {} lines high is {}",
                    data.size(), height, expected));
  }
  EtmSheet sheet;
  sheet.words.reserve(height * kEtmLineWords);
  for (std::size_t at = kEtmHeaderBytes; at < data.size();
       at += kHalfwordBytes) {
    sheet.words.push_back(HalfwordAt(data, at));
  }
  return sheet;
}

std::expected<std::vector<std::byte>, std::string> EncodeEtmFile(
    const EtmSheet& sheet) {
  if (sheet.words.size() % kEtmLineWords != 0) {
    return std::unexpected(
        std::format("the sheet holds {} halfwords, which is not whole lines "
                    "of {}",
                    sheet.words.size(), kEtmLineWords));
  }
  if (auto checked = CheckHeight(sheet.Height()); !checked) {
    return std::unexpected(checked.error());
  }
  std::vector<std::byte> out;
  out.reserve(kEtmHeaderBytes + (sheet.words.size() * kHalfwordBytes));
  PutHalfword(out, kEtmMarker);
  PutHalfword(out, static_cast<std::uint16_t>(sheet.Height()));
  for (const std::uint16_t word : sheet.words) PutHalfword(out, word);
  return out;
}

std::expected<EtmSheet, std::string> EtmSheetOfImages(
    std::span<const ImageDocument> images, std::string_view source) {
  std::vector<const ImageDocument*> regions;
  regions.reserve(images.size());
  for (const ImageDocument& document : images) {
    if (document.provenance.source != source) {
      return std::unexpected(std::format("{} is from {}, not {}", document.id,
                                         document.provenance.source, source));
    }
    if (auto region = CheckRegion(document); !region) {
      return std::unexpected(region.error());
    }
    regions.push_back(&document);
  }
  if (regions.empty()) {
    return std::unexpected(std::format("{} has no region images", source));
  }
  std::ranges::sort(regions, {}, [](const ImageDocument* document) {
    return document->provenance.offset;
  });
  EtmSheet sheet;
  for (const ImageDocument* document : regions) {
    const std::uint64_t next =
        kEtmHeaderBytes + (sheet.words.size() * kHalfwordBytes);
    if (document->provenance.offset != next) {
      return std::unexpected(std::format(
          "{} starts at byte 0x{:X}; the regions before it end at 0x{:X}",
          document->id, document->provenance.offset, next));
    }
    const std::vector<std::uint16_t> words = PackedWords(document->image);
    sheet.words.insert(sheet.words.end(), words.begin(), words.end());
  }
  if (auto checked = CheckHeight(sheet.Height()); !checked) {
    return std::unexpected(std::format("{}: {}", source, checked.error()));
  }
  return sheet;
}

}  // namespace VS::Content
