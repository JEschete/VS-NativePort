#pragma once

#include <source_location>
#include <utility>

#include "Render/EncodedText.hpp"
#include "Render/EncodedTextLive.hpp"

// The battle string context (vs_battle_stringContext) as the encoded-text
// formatter's arguments (StringContextLive.cpp).
namespace VS::Battle {
// The battle string context's integers and strings; an unset string is empty
// and a set one is trusted for Render::kTrustedEncodedTextBytes.
[[nodiscard]] Render::EncodedTextArguments LiveEncodedTextArguments();

// decomp: F01328 vs_battle_printf (src/BATTLE/BATTLE.PRG/5BF94.c:991): the
// encoded text over the battle string context. Returns the written
// terminator's address, so formatting calls chain.
template <typename Destination, typename Source>
char* FormatLiveEncodedText(
    Destination&& destination, Source&& source,
    const std::source_location site = std::source_location::current()) {
  return Render::FormatLiveEncodedText(std::forward<Destination>(destination),
                                       std::forward<Source>(source),
                                       LiveEncodedTextArguments(), site);
}
}  // namespace VS::Battle
