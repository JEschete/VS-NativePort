#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstddef>
#include <cstdint>
#include <source_location>
#include <span>
#include <type_traits>

#include "Core/String/GameText.hpp"

// Live binding for GameText.hpp. An array argument is bounded by its declared
// extent; a pointer destination is trusted for the bytes the request writes,
// and a pointer source is read up to kTrustedSourceBytes looking for its
// terminator. Refused requests are reported.
namespace VS::Core::String {
namespace live_detail {
inline constexpr std::size_t kTrustedSourceBytes = 0x1000;

template <typename T>
inline constexpr bool kBounded = std::is_array_v<T> && std::extent_v<T> > 1;

template <typename T>
std::span<std::uint8_t> Destination(T& target, std::size_t requested) {
  if constexpr (kBounded<T>) {
    return {reinterpret_cast<std::uint8_t*>(&target), sizeof target};
  } else {
    return {reinterpret_cast<std::uint8_t*>(target), requested};
  }
}

template <typename T>
std::span<const std::uint8_t> Source(T& target) {
  if constexpr (kBounded<T>) {
    return {reinterpret_cast<const std::uint8_t*>(&target), sizeof target};
  } else {
    return {reinterpret_cast<const std::uint8_t*>(target), kTrustedSourceBytes};
  }
}

// The length through the terminator, or one more than the span when it has
// none, so the copy is refused rather than guessed.
[[nodiscard]] std::size_t TerminatedLength(
    std::span<const std::uint8_t> text) noexcept;

void ReportRefusedText(const char* operation, std::size_t requested,
                       std::size_t written, const std::source_location& site);

inline void CheckWritten(const char* operation, std::size_t requested,
                         std::size_t written,
                         const std::source_location& site) {
  if (written != requested) {
    ReportRefusedText(operation, requested, written, site);
  }
}
}  // namespace live_detail

// Text requests refused since start-up.
[[nodiscard]] std::uint64_t RefusedLiveTextCount() noexcept;

template <typename Destination, typename Source>
void CopyLiveGameText(
    Destination&& destination, Source&& source,
    const std::source_location site = std::source_location::current()) {
  const auto text = live_detail::Source(source);
  const std::size_t requested = live_detail::TerminatedLength(text);
  live_detail::CheckWritten(
      "CopyGameText", requested,
      CopyGameText(live_detail::Destination(destination, requested), text),
      site);
}

template <typename Destination>
void FormatLiveGameInteger(
    Destination&& destination, std::int32_t value,
    const std::source_location site = std::source_location::current()) {
  const std::size_t requested = GameIntegerBytes(value);
  live_detail::CheckWritten(
      "FormatGameInteger", requested,
      FormatGameInteger(live_detail::Destination(destination, requested),
                        value),
      site);
}

template <typename Destination>
void FormatLiveGamePercent(
    Destination&& destination, std::int32_t value,
    const std::source_location site = std::source_location::current()) {
  const std::size_t requested = GamePercentBytes(value);
  live_detail::CheckWritten(
      "FormatGamePercent", requested,
      FormatGamePercent(live_detail::Destination(destination, requested),
                        value),
      site);
}
}  // namespace VS::Core::String
