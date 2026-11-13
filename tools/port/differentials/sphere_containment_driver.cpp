#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <span>

#include "Battle/SphereContainmentLive.hpp"
extern "C" {
#include "Battle/BattleTypes.h"
#include "Battle/TargetVolume.h"

// Storage only for the unused current-sphere adapter in this small driver.
// The production registry/reset tests link Sphere.cpp's actual owner instead.
Battle_TargetSphereSetupView* Battle_targetSphere = nullptr;
}

namespace {
namespace b = VS::Battle;
constexpr std::size_t kVolumeBytes = 16;
constexpr std::size_t kPointComponents = 3;
static_assert(sizeof(Battle_TargetVolume) == kVolumeBytes);

struct Input {
  int operation{};
  int mode{};
  int alias{};
  std::array<std::uint8_t, kVolumeBytes> shape{};
  std::array<std::uint16_t, kPointComponents> point{};
};

bool ReadInput(Input& input) {
  if (!(std::cin >> input.operation >> input.mode >> input.alias)) return false;
  unsigned value = 0;
  for (auto& byte : input.shape) {
    std::cin >> value;
    byte = static_cast<std::uint8_t>(value);
  }
  for (auto& component : input.point) {
    std::cin >> value;
    component = static_cast<std::uint16_t>(value);
  }
  return static_cast<bool>(std::cin);
}

std::span<const std::uint16_t> Point(const Input& input,
                                     const Battle_TargetVolume& volume) {
  if (input.alias >= 0) {
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(&volume);
    return {reinterpret_cast<const std::uint16_t*>(bytes + input.alias),
            kPointComponents};
  }
  if (input.alias == -2 && input.operation == 5) return {};
  return input.point;
}

int Evaluate(const Input& input, Battle_TargetVolume& volume,
             std::span<const std::uint16_t> point) {
  switch (input.operation) {
    case 0:
      return 0;
    case 1:
      return b::IsLivePointInTargetEllipsoid(&volume, point);
    case 2:
      return b::IsLivePointInTargetCylinder(&volume, point, input.mode);
    case 3:
      return b::IsLivePointInTargetCone(&volume, point, input.mode);
    case 4:
      return b::IsLivePointInTargetPrism(&volume, point, input.mode);
    case 5:
      return b::IsLivePointInTargetVolume(&volume, point);
    default:
      return -1;
  }
}

void PrintTransform(const b::SphereContainmentTransform& transform) {
  std::cout << ' ' << transform.origin.x << ' ' << transform.origin.y << ' '
            << transform.origin.z;
  for (const auto reciprocal : transform.reciprocal_extents) {
    std::cout << ' ' << reciprocal;
  }
  for (const auto& row : transform.rotation.m) {
    for (const auto component : row) std::cout << ' ' << component;
  }
}

void Run(const Input& input) {
  Battle_TargetVolume volume{};
  std::memcpy(&volume, input.shape.data(), sizeof volume);
  const auto point = Point(input, volume);
  const auto result = Evaluate(input, volume, point);
  std::cout << result;
  std::array<std::uint8_t, kVolumeBytes> bytes{};
  std::memcpy(bytes.data(), &volume, sizeof volume);
  for (const auto byte : bytes) std::cout << ' ' << static_cast<unsigned>(byte);
  if (input.operation == 0) {
    PrintTransform(b::BuildLiveTargetVolumeTestTransform(&volume));
  } else if (input.operation >= 1 && input.operation <= 4) {
    std::array<std::int16_t, kPointComponents> components{};
    std::memcpy(components.data(), point.data(), sizeof components);
    const auto transform = b::BuildLiveTargetVolumeTestTransform(&volume);
    const auto local =
        transform.LocalPoint({components[0], components[1], components[2]});
    std::cout << ' ' << local.x << ' ' << local.y << ' ' << local.z;
  }
  std::cout << '\n';
}
}  // namespace

int main() {
  Input input;
  while (ReadInput(input)) Run(input);
}
