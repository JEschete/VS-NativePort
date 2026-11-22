#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

// SHA-256 (FIPS 180-4) for content identity: the asset manifest records the
// hash of every source file a converter consumed (ASSET_PIPELINE.md §8).
namespace VS::Core {

inline constexpr std::size_t kSha256DigestBytes = 32;
using Sha256Digest = std::array<std::uint8_t, kSha256DigestBytes>;

class Sha256 {
 public:
  Sha256() noexcept;
  void Update(std::span<const std::byte> bytes) noexcept;
  // The digest of everything updated so far; the hasher is spent after.
  [[nodiscard]] Sha256Digest Finish() noexcept;

 private:
  static constexpr std::size_t kBlockBytes = 64;
  static constexpr std::size_t kStateWords = 8;

  void Compress(std::span<const std::uint8_t, kBlockBytes> block) noexcept;

  std::array<std::uint32_t, kStateWords> state_{};
  std::array<std::uint8_t, kBlockBytes> pending_{};
  std::size_t pending_bytes_ = 0;
  std::uint64_t total_bytes_ = 0;
};

[[nodiscard]] Sha256Digest Sha256Of(std::span<const std::byte> bytes) noexcept;
// Lowercase hexadecimal, as the manifest stores it.
[[nodiscard]] std::string Sha256Hex(std::span<const std::byte> bytes);
[[nodiscard]] std::string HexOf(const Sha256Digest& digest);

}  // namespace VS::Core
