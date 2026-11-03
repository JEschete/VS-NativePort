// Hand-maintained (tools/port/owned.csv).
#include "Core/String/GameText.hpp"

#include <algorithm>
#include <array>
#include <charconv>

namespace VS::Core::String {
namespace {
// The vsstring literal {%, terminator, padding}.
// The vsstring struct is one word.
constexpr std::size_t kSuffixBytes = 4;
using Suffix = std::array<std::uint8_t, kSuffixBytes>;
constexpr Suffix kPercentSuffix{kGameTextPercent, kGameTextTerminator, 0, 0};
// "-2147483648".
constexpr std::size_t kMaxDigits = 11;

struct Digits {
  std::array<char, kMaxDigits> text{};
  std::size_t length = 0;
};

Digits Decimal(std::int32_t value) noexcept {
  Digits digits;
  const auto result = std::to_chars(
      digits.text.data(), digits.text.data() + digits.text.size(), value);
  digits.length = static_cast<std::size_t>(result.ptr - digits.text.data());
  return digits;
}

// Each character less '0', as the original's in-place subtraction leaves it.
void WriteDigitGlyphs(std::span<std::uint8_t> destination,
                      const Digits& digits) noexcept {
  for (std::size_t index = 0; index < digits.length; ++index) {
    destination[index] = static_cast<std::uint8_t>(digits.text[index] - '0');
  }
}

std::size_t TextLength(std::span<const std::uint8_t> text) noexcept {
  return static_cast<std::size_t>(
      std::find(text.begin(), text.end(), kGameTextTerminator) - text.begin());
}
}  // namespace

// MENU9.PRG _strcpy (238.c:2176).
std::size_t CopyGameText(std::span<std::uint8_t> destination,
                         std::span<const std::uint8_t> source) noexcept {
  const std::size_t length = TextLength(source);
  if (length == source.size() || length >= destination.size()) {
    return 0;
  }
  std::copy_n(source.begin(), length + 1, destination.begin());
  return length + 1;
}

// MENU9.PRG _strcat (238.c:2185).
std::size_t AppendGameText(std::span<std::uint8_t> destination,
                           std::span<const std::uint8_t> source) noexcept {
  const std::size_t end = TextLength(destination);
  if (end == destination.size()) {
    return 0;
  }
  return CopyGameText(destination.subspan(end), source);
}

std::size_t GameIntegerBytes(std::int32_t value) noexcept {
  return Decimal(value).length + 1;
}

// MENU9.PRG _toVsStringInt (238.c:2348).
std::size_t FormatGameInteger(std::span<std::uint8_t> destination,
                              std::int32_t value) noexcept {
  const Digits digits = Decimal(value);
  if (digits.length + 1 > destination.size()) {
    return 0;
  }
  WriteDigitGlyphs(destination, digits);
  destination[digits.length] = kGameTextTerminator;
  return digits.length + 1;
}

std::size_t GamePercentBytes(std::int32_t value) noexcept {
  return Decimal(value).length + kPercentSuffix.size();
}

// MENU9.PRG _toVsStringPercent (238.c:2370).
std::size_t FormatGamePercent(std::span<std::uint8_t> destination,
                              std::int32_t value) noexcept {
  const Digits digits = Decimal(value);
  if (digits.length + kPercentSuffix.size() > destination.size()) {
    return 0;
  }
  WriteDigitGlyphs(destination, digits);
  std::copy(kPercentSuffix.begin(), kPercentSuffix.end(),
            destination.begin() + static_cast<std::ptrdiff_t>(digits.length));
  return digits.length + kPercentSuffix.size();
}
}  // namespace VS::Core::String
