// Hand-maintained (tools/port/owned.csv).
// Live binding for the bounded game-text operations (GameText.cpp).
#include "Core/String/GameTextLive.hpp"

#include <algorithm>
#include <cinttypes>

extern "C" {
#include "Core/Diagnostics.h"
}

namespace VS::Core::String {
namespace {
std::uint64_t refused_text = 0;
}  // namespace

std::uint64_t RefusedLiveTextCount() noexcept { return refused_text; }

std::size_t live_detail::TerminatedLength(
    std::span<const std::uint8_t> text) noexcept {
  const auto end = std::find(text.begin(), text.end(), kGameTextTerminator);
  return static_cast<std::size_t>(end - text.begin()) + 1;
}

void live_detail::ReportRefusedText(const char* operation,
                                    std::size_t requested, std::size_t written,
                                    const std::source_location& site) {
  ++refused_text;
  // The first and then every power-of-two refusal, so a per-frame request
  // cannot flood the log.
  if ((refused_text & (refused_text - 1)) == 0) {
    Core_Errorf("text",
                "%s at %s:%u: wrote %zu of %zu bytes; the text has no "
                "terminator or does not fit the destination (refusal %" PRIu64
                ")",
                operation, site.file_name(), static_cast<unsigned>(site.line()),
                written, requested, refused_text);
  }
}
}  // namespace VS::Core::String
