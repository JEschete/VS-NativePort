#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstddef>
#include <cstdint>
#include <source_location>
#include <span>
#include <type_traits>

#include "Core/Memory/WordCopy.hpp"

// Copied callers pass raw addresses and sizes. When an argument is an array
// whose declared extent is known (more than one element, so not a
// trailing-storage idiom), the copy is bounded by that extent; a request past
// it writes only what fits and is reported instead of overrunning the
// neighbouring fields (#268). Pointer arguments carry no extent and are
// trusted for the requested size, as before.
namespace VS::Core::Memory {
namespace live_detail {
template <typename T>
inline constexpr bool kBounded = std::is_array_v<T> && std::extent_v<T> > 1;
using Bytes = std::span<std::byte>;
using ConstBytes = std::span<const std::byte>;
template <typename T>
using Bare = std::remove_cv_t<T>;
template <typename T>
inline constexpr bool kWritableSpan = std::is_same_v<Bare<T>, Bytes>;
template <typename T>
inline constexpr bool kReadableSpan = std::is_same_v<Bare<T>, ConstBytes>;

template <typename T>
std::span<std::byte> Destination(T& target, std::size_t requested) {
  if constexpr (std::is_null_pointer_v<T>) {
    return {};
  } else if constexpr (kWritableSpan<T>) {
    return target;
  } else if constexpr (kBounded<T>) {
    return {reinterpret_cast<std::byte*>(&target), sizeof target};
  } else {
    return {reinterpret_cast<std::byte*>(target), requested};
  }
}

template <typename T>
std::span<const std::byte> Source(T& target, std::size_t requested) {
  if constexpr (std::is_null_pointer_v<T>) {
    return {};
  } else if constexpr (kWritableSpan<T> || kReadableSpan<T>) {
    return target;
  } else if constexpr (kBounded<T>) {
    return {reinterpret_cast<const std::byte*>(&target), sizeof target};
  } else {
    return {reinterpret_cast<const std::byte*>(target), requested};
  }
}

void ReportTruncatedCopy(const char* operation, std::size_t requested,
                         std::size_t written, const std::source_location& site);

inline void CheckWritten(const char* operation, std::size_t requested,
                         std::size_t written,
                         const std::source_location& site) {
  if (written != requested) {
    ReportTruncatedCopy(operation, requested, written, site);
  }
}
}  // namespace live_detail

// The bytes of `record` from `member` to the end of the record, for copied
// callers that deliberately fill several consecutive fields in one copy.
template <typename Record, typename Member>
std::span<std::byte> RecordTail(Record& record, Member& member) {
  const auto begin = reinterpret_cast<std::uintptr_t>(&record);
  const auto first = reinterpret_cast<std::uintptr_t>(&member);
  return {reinterpret_cast<std::byte*>(&member),
          sizeof record - static_cast<std::size_t>(first - begin)};
}

// Copies refused by an array extent since start-up.
[[nodiscard]] std::uint64_t TruncatedLiveCopyCount();

template <typename Destination, typename Source>
void CopyLiveAlignedWords(
    Destination&& destination, Source&& source, std::uint32_t size,
    const std::source_location site = std::source_location::current()) {
  const std::size_t requested = AlignedWordBytes(size);
  live_detail::CheckWritten(
      "CopyAlignedWords", requested,
      CopyAlignedWords(live_detail::Destination(destination, requested),
                       live_detail::Source(source, requested), size),
      site);
}

template <typename Destination>
void ZeroLiveAlignedWords(
    Destination&& destination, std::int32_t size,
    const std::source_location site = std::source_location::current()) {
  const std::size_t requested = ZeroAlignedWordBytes(size);
  live_detail::CheckWritten(
      "ZeroAlignedWords", requested,
      ZeroAlignedWords(live_detail::Destination(destination, requested), size),
      site);
}

template <typename Destination>
void ClearLiveAlignedWords(
    Destination&& destination, std::int32_t size,
    const std::source_location site = std::source_location::current()) {
  const std::size_t requested = ClearAlignedWordBytes(size);
  live_detail::CheckWritten(
      "ClearAlignedWords", requested,
      ClearAlignedWords(live_detail::Destination(destination, requested), size),
      site);
}

template <typename Destination, typename Source>
void CopyLiveHalfwords(
    Destination&& destination, Source&& source, std::int32_t size,
    const std::source_location site = std::source_location::current()) {
  const std::size_t requested = HalfwordBytes(size);
  live_detail::CheckWritten(
      "CopyHalfwords", requested,
      CopyHalfwords(live_detail::Destination(destination, requested),
                    live_detail::Source(source, requested), size),
      site);
}

template <typename Destination, typename Source>
void CopyLiveHalfwordBlocks(
    Destination&& destination, Source&& source, std::int32_t size,
    const std::source_location site = std::source_location::current()) {
  const std::size_t requested = HalfwordBlockBytes(size);
  live_detail::CheckWritten(
      "CopyHalfwordBlocks", requested,
      CopyHalfwordBlocks(live_detail::Destination(destination, requested),
                         live_detail::Source(source, requested), size),
      site);
}
}  // namespace VS::Core::Memory
