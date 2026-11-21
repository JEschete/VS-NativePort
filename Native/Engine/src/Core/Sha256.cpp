// SHA-256 (FIPS 180-4, §6.2).
#include "Core/Sha256.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace VS::Core {
namespace {
constexpr std::array<std::uint32_t, 64> kRoundConstants = {
    0x428A2F98, 0x71374491, 0xB5C0FBCF, 0xE9B5DBA5, 0x3956C25B, 0x59F111F1,
    0x923F82A4, 0xAB1C5ED5, 0xD807AA98, 0x12835B01, 0x243185BE, 0x550C7DC3,
    0x72BE5D74, 0x80DEB1FE, 0x9BDC06A7, 0xC19BF174, 0xE49B69C1, 0xEFBE4786,
    0x0FC19DC6, 0x240CA1CC, 0x2DE92C6F, 0x4A7484AA, 0x5CB0A9DC, 0x76F988DA,
    0x983E5152, 0xA831C66D, 0xB00327C8, 0xBF597FC7, 0xC6E00BF3, 0xD5A79147,
    0x06CA6351, 0x14292967, 0x27B70A85, 0x2E1B2138, 0x4D2C6DFC, 0x53380D13,
    0x650A7354, 0x766A0ABB, 0x81C2C92E, 0x92722C85, 0xA2BFE8A1, 0xA81A664B,
    0xC24B8B70, 0xC76C51A3, 0xD192E819, 0xD6990624, 0xF40E3585, 0x106AA070,
    0x19A4C116, 0x1E376C08, 0x2748774C, 0x34B0BCB5, 0x391C0CB3, 0x4ED8AA4A,
    0x5B9CCA4F, 0x682E6FF3, 0x748F82EE, 0x78A5636F, 0x84C87814, 0x8CC70208,
    0x90BEFFFA, 0xA4506CEB, 0xBEF9A3F7, 0xC67178F2,
};
constexpr std::array<std::uint32_t, 8> kInitialState = {
    0x6A09E667, 0xBB67AE85, 0x3C6EF372, 0xA54FF53A,
    0x510E527F, 0x9B05688C, 0x1F83D9AB, 0x5BE0CD19,
};
constexpr std::size_t kScheduleWords = 64;
constexpr std::size_t kWordBytes = 4;
constexpr std::size_t kLengthBytes = 8;
constexpr std::uint8_t kPadMarker = 0x80;
constexpr int kByteBits = 8;
constexpr std::uint32_t kByteMask = 0xFF;

// How far back the message schedule reads (?6.2.2 step 1).
constexpr std::size_t kLagSigma1 = 2;
constexpr std::size_t kLagMiddle = 7;
constexpr std::size_t kLagSigma0 = 15;
constexpr std::size_t kLagOldest = 16;

// The working variables a to h (?6.2.2 step 2).
enum Working : std::size_t { kA, kB, kC, kD, kE, kF, kG, kH };

// Each upper-case sigma rotates three times; each lower-case sigma rotates
// twice and then shifts by `third` (?4.1.2).
struct Rotations {
  int first;
  int second;
  int third;
};
constexpr Rotations kBigSigma0{2, 13, 22};
constexpr Rotations kBigSigma1{6, 11, 25};
constexpr Rotations kSmallSigma0{7, 18, 3};
constexpr Rotations kSmallSigma1{17, 19, 10};

constexpr std::uint32_t BigSigma(std::uint32_t x, Rotations r) noexcept {
  return std::rotr(x, r.first) ^ std::rotr(x, r.second) ^ std::rotr(x, r.third);
}

constexpr std::uint32_t SmallSigma(std::uint32_t x, Rotations r) noexcept {
  return std::rotr(x, r.first) ^ std::rotr(x, r.second) ^ (x >> r.third);
}

constexpr std::uint32_t BigEndianWord(
    std::span<const std::uint8_t, kWordBytes> bytes) noexcept {
  std::uint32_t word = 0;
  for (const std::uint8_t byte : bytes) word = (word << kByteBits) | byte;
  return word;
}
}  // namespace

Sha256::Sha256() noexcept : state_(kInitialState) {}

void Sha256::Compress(
    std::span<const std::uint8_t, kBlockBytes> block) noexcept {
  constexpr std::size_t kBlockWords = kBlockBytes / kWordBytes;
  std::array<std::uint32_t, kScheduleWords> w{};
  for (std::size_t i = 0; i < kBlockWords; ++i) {
    w.at(i) = BigEndianWord(block.subspan(i * kWordBytes).first<kWordBytes>());
  }
  for (std::size_t i = kBlockWords; i < kScheduleWords; ++i) {
    w.at(i) =
        w.at(i - kLagOldest) + SmallSigma(w.at(i - kLagSigma0), kSmallSigma0) +
        w.at(i - kLagMiddle) + SmallSigma(w.at(i - kLagSigma1), kSmallSigma1);
  }
  std::array<std::uint32_t, kStateWords> v = state_;
  for (std::size_t i = 0; i < kScheduleWords; ++i) {
    const std::uint32_t e = v[kE];
    const std::uint32_t a = v[kA];
    const std::uint32_t choice = (e & v[kF]) ^ (~e & v[kG]);
    const std::uint32_t t1 = v[kH] + BigSigma(e, kBigSigma1) + choice +
                             kRoundConstants.at(i) + w.at(i);
    const std::uint32_t majority = (a & v[kB]) ^ (a & v[kC]) ^ (v[kB] & v[kC]);
    const std::uint32_t t2 = BigSigma(a, kBigSigma0) + majority;
    std::shift_right(v.begin(), v.end(), 1);
    v[kE] += t1;
    v[kA] = t1 + t2;
  }
  for (std::size_t i = 0; i < kStateWords; ++i) state_.at(i) += v.at(i);
}

void Sha256::Update(std::span<const std::byte> bytes) noexcept {
  total_bytes_ += bytes.size();
  while (!bytes.empty()) {
    const std::size_t take =
        std::min(kBlockBytes - pending_bytes_, bytes.size());
    std::ranges::transform(
        bytes.first(take),
        pending_.begin() + static_cast<std::ptrdiff_t>(pending_bytes_),
        [](std::byte b) { return std::to_integer<std::uint8_t>(b); });
    pending_bytes_ += take;
    bytes = bytes.subspan(take);
    if (pending_bytes_ == kBlockBytes) {
      Compress(pending_);
      pending_bytes_ = 0;
    }
  }
}

Sha256Digest Sha256::Finish() noexcept {
  const std::uint64_t bits = total_bytes_ * kByteBits;
  pending_.at(pending_bytes_++) = kPadMarker;
  if (pending_bytes_ > kBlockBytes - kLengthBytes) {
    std::fill(pending_.begin() + static_cast<std::ptrdiff_t>(pending_bytes_),
              pending_.end(), std::uint8_t{0});
    Compress(pending_);
    pending_bytes_ = 0;
  }
  std::fill(pending_.begin() + static_cast<std::ptrdiff_t>(pending_bytes_),
            pending_.end() - kLengthBytes, std::uint8_t{0});
  for (std::size_t i = 0; i < kLengthBytes; ++i) {
    pending_.at(kBlockBytes - 1 - i) =
        static_cast<std::uint8_t>((bits >> (kByteBits * i)) & kByteMask);
  }
  Compress(pending_);
  Sha256Digest digest{};
  for (std::size_t i = 0; i < kStateWords; ++i) {
    for (std::size_t b = 0; b < kWordBytes; ++b) {
      digest.at((kWordBytes * i) + b) = static_cast<std::uint8_t>(
          (state_.at(i) >> (kByteBits * (kWordBytes - 1 - b))) & kByteMask);
    }
  }
  return digest;
}

Sha256Digest Sha256Of(std::span<const std::byte> bytes) noexcept {
  Sha256 hasher;
  hasher.Update(bytes);
  return hasher.Finish();
}

std::string HexOf(const Sha256Digest& digest) {
  constexpr std::string_view kHex = "0123456789abcdef";
  constexpr int kNibbleBits = 4;
  constexpr unsigned kNibbleMask = 0xF;
  std::string text;
  text.reserve(digest.size() * 2);
  for (const std::uint8_t value : digest) {
    text.push_back(kHex[value >> kNibbleBits]);
    text.push_back(kHex[value & kNibbleMask]);
  }
  return text;
}

std::string Sha256Hex(std::span<const std::byte> bytes) {
  return HexOf(Sha256Of(bytes));
}

}  // namespace VS::Core
