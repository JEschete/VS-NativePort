// Hand-maintained (tools/port/owned.csv).
// Live binding for the bounded word copies (WordCopy.cpp).
#include "Core/Memory/WordCopyLive.hpp"

#include <cinttypes>

extern "C" {
#include "Core/Diagnostics.h"
}

namespace VS::Core::Memory {
namespace {
std::uint64_t truncated_copies = 0;
}  // namespace

std::uint64_t TruncatedLiveCopyCount() { return truncated_copies; }

void live_detail::ReportTruncatedCopy(const char* operation,
                                      std::size_t requested,
                                      std::size_t written,
                                      const std::source_location& site) {
  ++truncated_copies;
  // Report the first and then every power-of-two refusal so a per-frame
  // overrun cannot flood the log.
  if ((truncated_copies & (truncated_copies - 1)) == 0) {
    Core_Errorf("memory",
                "%s at %s:%u: %zu of %zu bytes fit the destination or source "
                "array (refusal %" PRIu64 ")",
                operation, site.file_name(), static_cast<unsigned>(site.line()),
                written, requested, truncated_copies);
  }
}
}  // namespace VS::Core::Memory
