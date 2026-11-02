// Hand-maintained (tools/port/owned.csv).
#include "Effects/PackageExtents.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>

namespace VS::Effects {
namespace {
constexpr std::size_t kCurveHeaderBytes = 4;
constexpr int kByteBits = 8;

std::size_t ReadU16(std::span<const std::byte> bytes, std::size_t offset) {
  return std::to_integer<std::size_t>(bytes[offset]) |
         (std::to_integer<std::size_t>(bytes[offset + 1]) << kByteBits);
}
}  // namespace

std::size_t CountParticleDefinitions(std::size_t payload_bytes) {
  if (payload_bytes < kParticleDefinitionHeaderBytes) return 0;
  return (payload_bytes - kParticleDefinitionHeaderBytes) /
         kParticleDefinitionBytes;
}

std::size_t CountContainedEffectCurves(std::span<const std::byte> payload) {
  if (payload.size() < kCurveHeaderBytes) return 0;
  const std::size_t declared =
      std::min(ReadU16(payload, 0), kEffectCurveCapacity);
  std::size_t cursor = ReadU16(payload, 2);
  std::size_t contained = 0;
  for (; contained < declared; ++contained) {
    const std::size_t size_offset = kCurveHeaderBytes + contained;
    if (size_offset >= payload.size()) break;
    cursor += std::to_integer<std::size_t>(payload[size_offset]);
    if (cursor > payload.size()) break;
  }
  return contained;
}
}  // namespace VS::Effects
