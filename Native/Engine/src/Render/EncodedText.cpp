// Hand-maintained (tools/port/owned.csv).
// Game-encoded text formatting and measurement (BATTLE.PRG 5BF94.c:1443-1676)
// over spans: the integers and strings come from an EncodedTextArguments, and
// the end of a span reads as its terminator.
#include "Render/EncodedText.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace VS::Render {
namespace {
constexpr std::uint8_t kSmallParameterLimit = 10;
constexpr std::uint8_t kDigitCountUnit = 10;
constexpr std::int32_t kFirstHexDivisor = 0x10000000;
constexpr std::int32_t kFirstDecimalDivisor = 1000000000;
constexpr std::uint32_t kHexRadix = 16;
constexpr std::uint32_t kDecimalRadix = 10;
constexpr int kHexDigitBits = 4;
constexpr std::uint32_t kHexDigitMask = 0xF;
constexpr int kLineUnitWidth = 6;

[[nodiscard]] std::int32_t WrapNegate(std::int32_t value) noexcept {
  return static_cast<std::int32_t>(0U - static_cast<std::uint32_t>(value));
}

// The R3000 build checked INT_MIN / -1 with a break; x86 traps on it. Every
// other quotient matches the original's signed divide.
[[nodiscard]] std::int32_t Quotient(std::int32_t value,
                                    std::int32_t divisor) noexcept {
  return divisor == -1 ? WrapNegate(value) : value / divisor;
}

[[nodiscard]] std::int32_t NextDivisor(std::int32_t divisor,
                                       bool hex) noexcept {
  return hex ? divisor >> kHexDigitBits
             : divisor / static_cast<std::int32_t>(kDecimalRadix);
}

[[nodiscard]] std::uint8_t Digit(std::int32_t value, std::int32_t divisor,
                                 bool hex) noexcept {
  const std::int32_t quotient = Quotient(value, divisor);
  if (hex) {
    return static_cast<std::uint8_t>(static_cast<std::uint32_t>(quotient) &
                                     kHexDigitMask);
  }
  // A wrapped negative divisor gives a negative digit; the byte keeps it.
  return static_cast<std::uint8_t>(quotient %
                                   static_cast<std::int32_t>(kDecimalRadix));
}

// The radix raised to `count`, wrapped to 32 bits as the original's multiply.
[[nodiscard]] std::int32_t FixedCountDivisor(std::uint32_t radix,
                                             int count) noexcept {
  std::uint32_t divisor = 1;
  for (int step = 0; step < count; ++step) {
    divisor *= radix;
  }
  return static_cast<std::int32_t>(divisor);
}

// Builds formatted text, or only counts it when the output span is empty.
class EncodedTextWriter {
 public:
  EncodedTextWriter(std::span<std::uint8_t> output,
                    const EncodedTextArguments& arguments) noexcept
      : output_(output), arguments_(arguments) {}

  // Appends the source up to its terminator; false when it is refused.
  bool Append(std::span<const std::uint8_t> source, int depth) noexcept {
    std::size_t at = 0;
    while (at < source.size()) {
      const std::uint8_t code = source[at++];
      if (code == kTextTerminator) {
        return true;
      }
      if (code < kTextFirstParameterized) {
        Put(code);
        continue;
      }
      if (at >= source.size()) {
        return false;
      }
      if (!AppendCode(code, source[at++], depth)) {
        return false;
      }
    }
    return false;
  }

  void Put(std::uint8_t value) noexcept {
    if (size_ < output_.size()) {
      output_[size_] = value;
    }
    ++size_;
  }

  [[nodiscard]] std::size_t Size() const noexcept { return size_; }

 private:
  bool AppendCode(std::uint8_t code, std::uint8_t parameter,
                  int depth) noexcept {
    switch (code) {
      case kTextPrintHex:
      case kTextPrintDecimal: {
        const EncodedInteger digits = EncodeTextInteger(
            code, parameter,
            arguments_.integers[parameter % kEncodedTextArgumentCount]);
        for (const std::uint8_t digit : digits.View()) {
          Put(digit);
        }
        return true;
      }
      case kTextPrintString: {
        // The nested string's terminator is not copied.
        if (parameter >= kEncodedTextArgumentCount ||
            depth >= kMaxEncodedTextNesting) {
          return false;
        }
        const std::span<const std::uint8_t> nested =
            arguments_.strings[parameter];
        return !nested.empty() && Append(nested, depth + 1);
      }
      default:
        Put(code);
        Put(parameter);
        return true;
    }
  }

  std::span<std::uint8_t> output_;
  const EncodedTextArguments& arguments_;
  std::size_t size_ = 0;
};
}  // namespace

EncodedInteger EncodeTextInteger(std::uint8_t code, std::uint8_t parameter,
                                 std::int32_t value) noexcept {
  EncodedInteger encoded;
  const auto put = [&encoded](std::uint8_t byte) {
    if (encoded.size < encoded.bytes.size()) {
      encoded.bytes[encoded.size++] = byte;
    }
  };
  const bool hex = code == kTextPrintHex;
  std::int32_t integer = value;
  if (integer < 0) {
    integer = WrapNegate(integer);
    put(kTextMinusGlyph);
  }
  std::int32_t divisor = 0;
  if (parameter < kSmallParameterLimit) {
    if (integer == 0) {
      put(0);
      return encoded;
    }
    // Every significant digit: a nonzero value has a nonzero quotient by 1.
    divisor = hex ? kFirstHexDivisor : kFirstDecimalDivisor;
    while (Quotient(integer, divisor) == 0) {
      divisor = NextDivisor(divisor, hex);
    }
  } else {
    divisor = FixedCountDivisor(hex ? kHexRadix : kDecimalRadix,
                                (parameter / kDigitCountUnit) - 1);
  }
  while (divisor != 0) {
    put(Digit(integer, divisor, hex));
    divisor = NextDivisor(divisor, hex);
  }
  return encoded;
}

std::optional<std::size_t> EncodedTextBytes(
    std::span<const std::uint8_t> source,
    const EncodedTextArguments& arguments) noexcept {
  EncodedTextWriter counter({}, arguments);
  if (!counter.Append(source, 0)) {
    return std::nullopt;
  }
  return counter.Size() + 1;
}

std::optional<std::size_t> FormatEncodedText(
    std::span<std::uint8_t> destination, std::span<const std::uint8_t> source,
    const EncodedTextArguments& arguments) noexcept {
  const std::optional<std::size_t> needed = EncodedTextBytes(source, arguments);
  if (!needed.has_value() || *needed > destination.size()) {
    return std::nullopt;
  }
  EncodedTextWriter writer(destination.first(*needed), arguments);
  (void)writer.Append(source, 0);
  writer.Put(kTextTerminator);
  return *needed - 1;
}

int MeasureEncodedTextLine(
    std::span<const std::uint8_t> text,
    std::span<const std::uint8_t> glyph_widths) noexcept {
  int length = 0;
  std::size_t at = 0;
  while (at < text.size()) {
    const std::uint8_t code = text[at++];
    if (code < kTextFirstControl) {
      length += code < glyph_widths.size() ? glyph_widths[code] : 0;
      continue;
    }
    if (code == kTextPageBreak || code == kTextTerminator ||
        code == kTextNewline) {
      break;
    }
    if (code < kTextChunkSize) {
      continue;
    }
    if (at >= text.size()) {
      break;
    }
    const std::uint8_t parameter = text[at++];
    if (code == kTextSpacing) {
      length += parameter;
    } else if (code == kTextPrintDecimal && parameter >= kSmallParameterLimit) {
      length += (parameter / kDigitCountUnit) * kLineUnitWidth;
    }
  }
  return length / kLineUnitWidth;
}
}  // namespace VS::Render
