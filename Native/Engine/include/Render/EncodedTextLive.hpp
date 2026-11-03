#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstddef>
#include <cstdint>
#include <optional>
#include <source_location>
#include <span>
#include <type_traits>

#include "Render/EncodedText.hpp"

// Live binding for EncodedText.hpp: the arguments are the battle string
// context (Battle_stringContext) and the widths are the variable-width font's
// (Render_variableFontGlyphWidths). An array argument is bounded by its
// declared extent; a pointer destination is trusted for the bytes the request
// writes, and a pointer source or argument string is read up to
// kTrustedEncodedTextBytes looking for its terminator. A refused request
// leaves an empty text and is reported (EncodedTextLive.cpp).
namespace VS::Render {
inline constexpr std::size_t kTrustedEncodedTextBytes = 0x1000;

namespace encoded_text_live {
template <typename T>
inline constexpr bool kBounded = std::is_array_v<T> && std::extent_v<T> > 1;

// A text from a pointer, trusted for kTrustedEncodedTextBytes; null is empty.
[[nodiscard]] inline std::span<const std::uint8_t> TrustedText(
    const void* text) noexcept {
  if (text == nullptr) {
    return {};
  }
  return {static_cast<const std::uint8_t*>(text), kTrustedEncodedTextBytes};
}

template <typename T>
[[nodiscard]] std::span<const std::uint8_t> Text(T& text) {
  if constexpr (kBounded<T>) {
    return {reinterpret_cast<const std::uint8_t*>(&text), sizeof text};
  } else {
    return TrustedText(text);
  }
}

template <typename T>
[[nodiscard]] std::uint8_t* Start(T& target) {
  if constexpr (kBounded<T>) {
    return reinterpret_cast<std::uint8_t*>(&target);
  } else {
    return reinterpret_cast<std::uint8_t*>(target);
  }
}

template <typename T>
[[nodiscard]] std::size_t Capacity(T& target, std::size_t requested) {
  if constexpr (kBounded<T>) {
    return sizeof target;
  } else {
    return requested;
  }
}

// The formatted text's terminator, or, for a refused request (reported), a
// terminator written at the destination's start.
char* Finish(std::uint8_t* destination, std::optional<std::size_t> end,
             const std::source_location& site);

[[nodiscard]] int MeasureLine(std::span<const std::uint8_t> text);
}  // namespace encoded_text_live

// The battle string context's integers and strings; an unset string is empty.
[[nodiscard]] EncodedTextArguments LiveEncodedTextArguments();

// The variable-width font's glyph widths (module data).
[[nodiscard]] std::span<const std::uint8_t>
LiveVariableFontGlyphWidths() noexcept;

// Formatting requests refused since start-up.
[[nodiscard]] std::uint64_t RefusedLiveEncodedTextCount() noexcept;

// BATTLE.PRG vs_battle_printf over the battle string context. Returns the
// written terminator's address, so formatting calls chain.
template <typename Destination, typename Source>
char* FormatLiveEncodedText(
    Destination&& destination, Source&& source,
    const std::source_location site = std::source_location::current()) {
  const EncodedTextArguments arguments = LiveEncodedTextArguments();
  const std::span<const std::uint8_t> text = encoded_text_live::Text(source);
  std::uint8_t* const start = encoded_text_live::Start(destination);
  std::optional<std::size_t> end;
  if (start != nullptr && text.data() != nullptr) {
    if (const auto bytes = EncodedTextBytes(text, arguments)) {
      end = FormatEncodedText(
          {start, encoded_text_live::Capacity(destination, *bytes)}, text,
          arguments);
    }
  }
  return encoded_text_live::Finish(start, end, site);
}

// BATTLE.PRG vs_battle_getTextLineLength with the variable-width font.
template <typename Text>
[[nodiscard]] int MeasureLiveEncodedTextLine(Text&& text) {
  return encoded_text_live::MeasureLine(encoded_text_live::Text(text));
}
}  // namespace VS::Render
