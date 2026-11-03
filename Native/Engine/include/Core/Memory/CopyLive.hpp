#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstddef>
#include <cstdint>
#include <source_location>
#include <span>

#include "Core/Memory/Copy.hpp"
#include "Core/Memory/WordCopyLive.hpp"

// Live binding for Copy.hpp. Copied callers pass raw addresses and sizes; an
// array argument is bounded by its declared extent and a pointer is trusted
// for the requested size, as for the word copies (WordCopyLive.hpp). A copy
// cut short by an extent or refused for its size is reported.
namespace VS::Core::Memory {
namespace live_detail {
// A size the original would have turned into a runaway loop; nothing written.
void ReportRefusedSize(const char* operation, std::int64_t size,
                       const std::source_location& site);
}  // namespace live_detail

// Sizes refused by the live backward copies since start-up.
[[nodiscard]] std::uint64_t RefusedLiveSizeCount() noexcept;

template <typename Source>
std::int16_t ReadLiveUnalignedShort(const Source* source) noexcept {
  return ReadUnalignedShort(std::span<const std::byte, 2>(
      reinterpret_cast<const std::byte*>(source), 2));
}

template <typename Destination>
void ZeroLiveWordsBackward(
    Destination&& destination, std::int32_t size,
    const std::source_location site = std::source_location::current()) {
  const std::size_t requested = ZeroWordsBackwardBytes(size);
  if (requested == 0) {
    live_detail::ReportRefusedSize("ZeroWordsBackward", size, site);
    return;
  }
  live_detail::CheckWritten(
      "ZeroWordsBackward", requested,
      ZeroWordsBackward(live_detail::Destination(destination, requested), size),
      site);
}

template <typename Destination, typename Source>
void CopyLiveBytesBackward(
    Destination&& destination, Source&& source, std::int32_t size,
    const std::source_location site = std::source_location::current()) {
  const std::size_t requested = CopyBytesBackwardBytes(size);
  if (requested == 0) {
    live_detail::ReportRefusedSize("CopyBytesBackward", size, site);
    return;
  }
  live_detail::CheckWritten(
      "CopyBytesBackward", requested,
      CopyBytesBackward(live_detail::Destination(destination, requested),
                        live_detail::Source(source, requested), size),
      site);
}

template <typename Source, typename Destination>
void CopyLiveWords(
    Source&& source, Destination&& destination, std::uint32_t size,
    const std::source_location site = std::source_location::current()) {
  const std::size_t requested = WordBytes(size);
  live_detail::CheckWritten(
      "CopyWords", requested,
      CopyWords(live_detail::Source(source, requested),
                live_detail::Destination(destination, requested), size),
      site);
}

template <typename First, typename Second>
void SwapLiveWords(
    First&& first, Second&& second, std::uint32_t size,
    const std::source_location site = std::source_location::current()) {
  const std::size_t requested = WordBytes(size);
  live_detail::CheckWritten(
      "SwapWords", requested,
      SwapWords(live_detail::Destination(first, requested),
                live_detail::Destination(second, requested), size),
      site);
}

// The module symbol map's entries for the original addresses, with the
// original pointer arguments trusted for the requested size.
std::int16_t ReadLiveUnalignedShortAt(const std::uint8_t* source) noexcept;
void ZeroLiveWordsBackwardAt(void* destination, std::int32_t size);
void CopyLiveBytesBackwardAt(void* destination, const void* source,
                             std::int32_t size);
void CopyLiveWordsAt(const void* source, void* destination, std::uint32_t size);
void SwapLiveWordsAt(void* first, void* second, std::uint32_t size);
}  // namespace VS::Core::Memory
