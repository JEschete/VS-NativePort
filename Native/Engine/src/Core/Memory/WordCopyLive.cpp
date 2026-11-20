// Hand-maintained (tools/port/owned.csv).
// Live binding for the bounded word copies (WordCopy.cpp).
#include "Core/Memory/WordCopyLive.hpp"

#include <cinttypes>

#include "Core/Diagnostics.hpp"

namespace VS::Core::Memory {
namespace {
std::uint64_t truncatedCopies = 0;
}  // namespace

std::uint64_t TruncatedLiveCopyCount() { return truncatedCopies; }

void live_detail::ReportTruncatedCopy(const char* operation,
                                      std::size_t requested,
                                      std::size_t written,
                                      const std::source_location& site) {
  ++truncatedCopies;
  // Report the first and then every power-of-two refusal so a per-frame
  // overrun cannot flood the log.
  if ((truncatedCopies & (truncatedCopies - 1)) == 0) {
    VS::Core::Errorf(
        "memory",
        "%s at %s:%u: %zu of %zu bytes fit the destination or source "
        "array (refusal %" PRIu64 ")",
        operation, site.file_name(), static_cast<unsigned>(site.line()),
        written, requested, truncatedCopies);
  }
}
}  // namespace VS::Core::Memory
