#include <crtdbg.h>

#include <array>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <span>
#include <string_view>

#include "Battle/SphereContainmentLive.hpp"
#include "Core/Log.hpp"
#include "TestSupport/SphereContainmentLiveFixture.hpp"

namespace {
namespace b = VS::Battle;
namespace f = VS::test_support::sphere_containment;

void AbortObserved(int) {
  std::fputs("ABORT_OBSERVED\n", stderr);
  std::_Exit(85);
}

bool AliasedFlags(bool reverse_pitch) {
  auto volume = reverse_pitch ? f::Volume(5, {255, 8, 255, 128}, {0, 127, 0})
                              : f::Volume(2, {255, 128, 255, 0}, {0, 255, 0});
  const auto saved = f::Bytes(volume);
  return b::IsLivePointInTargetVolume(&volume, f::FlagsPoint(volume)) == 1 &&
         f::Bytes(volume) == saved;
}

bool Positive(std::string_view selected) {
  if (selected == "alias-height") return AliasedFlags(false);
  if (selected == "alias-pitch") return AliasedFlags(true);
  if (selected == "lazy-unknown") {
    auto volume = f::Volume(129);
    Battle_targetSphere = nullptr;
    return b::IsLivePointInTargetVolume(&volume, {}) == 0;
  }
  if (selected == "valid-zero-extents") {
    auto volume = f::Volume(1, {0, 0, 0, 0});
    const auto transform = b::BuildLiveTargetVolumeTestTransform(&volume);
    return transform.reciprocal_extents ==
               std::array<std::int16_t, 3>{-1, -1, -1} &&
           b::IsLivePointInTargetVolume(&volume, f::Point{32767, 0, 32768}) ==
               1;
  }
  if (selected == "current-owner-reentry") {
    f::State state;
    state.spheres[0].volumes[1] = f::Volume(1);
    state.spheres[1].volumes[1] = f::Volume(0);
    if (b::IsLivePointInCurrentSphereVolume(1, f::Point{}) != 1) return false;
    state.Select(1);
    if (b::IsLivePointInCurrentSphereVolume(1, {}) != 0) return false;
    state.Select(0);
    return b::IsLivePointInCurrentSphereVolume(1, f::Point{}) == 1;
  }
  return false;
}

void RequiredOwner(std::string_view selected, Battle_TargetVolume* volume) {
  if (selected == "transform-null") {
    static_cast<void>(b::BuildLiveTargetVolumeTestTransform(nullptr));
  } else if (selected == "ellipsoid-null") {
    static_cast<void>(b::IsLivePointInTargetEllipsoid(nullptr, {}));
  } else if (selected == "cylinder-null") {
    static_cast<void>(b::IsLivePointInTargetCylinder(nullptr, {}, 0));
  } else if (selected == "cone-null") {
    static_cast<void>(b::IsLivePointInTargetCone(nullptr, {}, 0));
  } else if (selected == "prism-null") {
    static_cast<void>(b::IsLivePointInTargetPrism(nullptr, {}, 0));
  } else if (selected == "dispatch-null") {
    static_cast<void>(b::IsLivePointInTargetVolume(nullptr, {}));
  } else if (selected == "current-null") {
    Battle_targetSphere = nullptr;
    static_cast<void>(b::IsLivePointInCurrentSphereVolume(0, {}));
  } else if (selected == "current-negative") {
    static_cast<void>(b::IsLivePointInCurrentSphereVolume(-1, {}));
  } else if (selected == "current-overcount") {
    static_cast<void>(b::IsLivePointInCurrentSphereVolume(2, {}));
  } else if (selected == "dispatch-used-empty") {
    static_cast<void>(b::IsLivePointInTargetVolume(volume, {}));
  }
}

void RequiredPoint(std::string_view selected, Battle_TargetVolume* volume) {
  const std::array<std::uint16_t, 2> short_point{};
  if (selected == "ellipsoid-short") {
    static_cast<void>(b::IsLivePointInTargetEllipsoid(volume, short_point));
  } else if (selected == "cylinder-short") {
    static_cast<void>(b::IsLivePointInTargetCylinder(volume, short_point, -1));
  } else if (selected == "cone-short") {
    static_cast<void>(b::IsLivePointInTargetCone(volume, short_point, 0));
  } else if (selected == "prism-short") {
    static_cast<void>(b::IsLivePointInTargetPrism(volume, short_point, 0));
  }
}
}  // namespace

int main(int argc, char** argv) {
  if (argc != 3) return 2;
  if (!VS::Core::SetLogFile(argv[2])) return 3;
  _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
  _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
  _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
  std::signal(SIGABRT, AbortObserved);
  const std::string_view selected(argv[1]);
  if (selected == "alias-height" || selected == "alias-pitch" ||
      selected == "lazy-unknown" || selected == "valid-zero-extents" ||
      selected == "current-owner-reentry") {
    if (!Positive(selected)) return 1;
    std::puts("CHECKED_LIVE_PASS");
    return 0;
  }
  f::State state;
  auto volume = f::Volume(1);
  RequiredOwner(selected, &volume);
  RequiredPoint(selected, &volume);
  std::fputs("REFUSAL_NOT_OBSERVED\n", stderr);
  return 1;
}
