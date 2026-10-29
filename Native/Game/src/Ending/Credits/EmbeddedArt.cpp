#include "EmbeddedArt.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <span>

#include "Legacy/Psx.h"

extern "C" {
#include "Core/Diagnostics.h"
#include "Ending/Credits.h"
}

namespace {
constexpr std::size_t kTimCapacity = 72224;
constexpr std::uint32_t kSourceBase = 0x8006FF7C;
constexpr std::uint32_t kTimIdentifier = 0x10;
constexpr std::uint32_t kIndexed4BitWithClut = 8;
constexpr std::uint32_t kIndexed8BitWithClut = 9;
constexpr std::size_t kIllustrationIndex = 7;
constexpr auto kSourceAddresses = std::to_array<std::uint32_t>({
    0x8007005C,
    0x800CF33C,
    0x800D7F7C,
    0x800D975C,
    0x800D837C,
    0x800DB09C,
    0x800DB26C,
    0x8007709C,
    0x80088ABC,
    0x8009A4DC,
    0x800ABEFC,
    0x800BD91C,
});

struct TimStorage {
  Ending_Credits_TimHeader header{};
  std::array<unsigned char, kTimCapacity - sizeof(Ending_Credits_TimHeader)>
      payload{};
};
static_assert(sizeof(TimStorage) == kTimCapacity);

std::array<TimStorage, kSourceAddresses.size()> embedded_art{};
bool prepared = false;

template <typename Integer>
Integer ReadInteger(const unsigned char* source) {
  Integer value{};
  std::memcpy(&value, source, sizeof value);
  return value;
}

std::optional<std::size_t> TimLength(std::span<const unsigned char> source) {
  constexpr std::size_t kHeaderBytes = 8;
  constexpr std::size_t kBlockHeaderBytes = 12;
  if (source.size() < kHeaderBytes ||
      ReadInteger<std::uint32_t>(source.data()) != kTimIdentifier) {
    return std::nullopt;
  }
  const auto mode = ReadInteger<std::uint32_t>(source.data() + 4);
  if (mode != kIndexed4BitWithClut && mode != kIndexed8BitWithClut) {
    return std::nullopt;
  }
  std::size_t cursor = kHeaderBytes;
  for (unsigned block = 0; block < 2; ++block) {
    if (source.size() - cursor < kBlockHeaderBytes) return std::nullopt;
    const auto* header = source.data() + cursor;
    const auto bytes = ReadInteger<std::uint32_t>(header);
    const auto width = ReadInteger<std::uint16_t>(header + 8);
    const auto height = ReadInteger<std::uint16_t>(header + 10);
    if (bytes < kBlockHeaderBytes || bytes > source.size() - cursor ||
        width == 0 || height == 0 ||
        static_cast<std::size_t>(width) * height * 2 !=
            bytes - kBlockHeaderBytes) {
      return std::nullopt;
    }
    cursor += bytes;
    if (cursor > kTimCapacity) return std::nullopt;
  }
  return cursor;
}
}  // namespace

// ENDING/D4.c:999,583; G00802 aliases TIM8 at8007709C. G00788's imported
// trailing module span contains the complete TIMs, not just glyph widths.
extern "C" int Ending_Credits_PrepareEmbeddedArt(void) {
  prepared = false;
  Ending_Credits_illustrationTimWorkspace = nullptr;
  const std::span<const unsigned char> source(Ending_Credits_glyphWidths);
  for (std::size_t index = 0; index < kSourceAddresses.size(); ++index) {
    const auto offset =
        static_cast<std::size_t>(kSourceAddresses[index] - kSourceBase);
    if (offset >= source.size()) {
      Core_Errorf("ending.art", "embedded TIM%u source is outside its module",
                  static_cast<unsigned>(index + 1));
      return 0;
    }
    const auto bytes = TimLength(source.subspan(offset));
    if (!bytes) {
      Core_Errorf("ending.art", "embedded TIM%u has invalid source blocks",
                  static_cast<unsigned>(index + 1));
      return 0;
    }
    std::memcpy(&embedded_art[index], source.data() + offset, *bytes);
  }
  Ending_Credits_illustrationTimWorkspace =
      &embedded_art[kIllustrationIndex].header;
  prepared = true;
  return 1;
}

extern "C" Ending_Credits_TimHeader* Ending_Credits_EmbeddedArt(
    unsigned index) {
  if (!prepared || index >= embedded_art.size()) {
    Core_Errorf("ending.art", "unprepared or invalid embedded TIM index=%u",
                index);
    std::abort();
  }
  return &embedded_art[index].header;
}
