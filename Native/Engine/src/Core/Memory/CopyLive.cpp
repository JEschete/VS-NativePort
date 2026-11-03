// Hand-maintained (tools/port/owned.csv).
// Live binding for the bounded copies (Copy.cpp).
#include "Core/Memory/CopyLive.hpp"

#include <cinttypes>

extern "C" {
#include "Core/Diagnostics.h"
}

namespace VS::Core::Memory {
namespace {
std::uint64_t refused_sizes = 0;
}  // namespace

std::uint64_t RefusedLiveSizeCount() noexcept { return refused_sizes; }

void live_detail::ReportRefusedSize(const char* operation, std::int64_t size,
                                    const std::source_location& site) {
  ++refused_sizes;
  if ((refused_sizes & (refused_sizes - 1)) == 0) {
    Core_Errorf("memory",
                "%s at %s:%u: size %" PRId64
                " would not terminate in the original and was refused "
                "(refusal %" PRIu64 ")",
                operation, site.file_name(), static_cast<unsigned>(site.line()),
                size, refused_sizes);
  }
}

std::int16_t ReadLiveUnalignedShortAt(const std::uint8_t* source) noexcept {
  return ReadLiveUnalignedShort(source);
}

void ZeroLiveWordsBackwardAt(void* destination, std::int32_t size) {
  ZeroLiveWordsBackward(destination, size);
}

void CopyLiveBytesBackwardAt(void* destination, const void* source,
                             std::int32_t size) {
  CopyLiveBytesBackward(destination, source, size);
}

void CopyLiveWordsAt(const void* source, void* destination,
                     std::uint32_t size) {
  CopyLiveWords(source, destination, size);
}

void SwapLiveWordsAt(void* first, void* second, std::uint32_t size) {
  SwapLiveWords(first, second, size);
}
}  // namespace VS::Core::Memory
