// Hand-maintained (tools/port/owned.csv).
#include <algorithm>
#include <array>
#include <cstddef>
#include <span>

#include "Effects/PackageExtents.hpp"
#include "Legacy/Psx.h"

extern "C" {
#include "Effects/Package.h"
#include "Effects/Runtime.h"
}

namespace VS::Effects {
namespace {
// The action and secondary packages, with room for test packages.
constexpr std::size_t kRecordCapacity = 4;

struct PackageExtentRecord {
  const Effects_EffectPackage* package = nullptr;
  const void* definitions = nullptr;
  std::size_t definition_count = 0;
  const void* curves = nullptr;
  std::size_t curve_count = 0;
};

std::array<PackageExtentRecord, kRecordCapacity> records{};
std::size_t next_replacement = 0;

PackageExtentRecord* Find() {
  if (Effects_package == nullptr) return nullptr;
  const auto found = std::ranges::find(records, Effects_package,
                                       &PackageExtentRecord::package);
  return found == records.end() ? nullptr : &*found;
}

PackageExtentRecord* FindOrAdd() {
  if (Effects_package == nullptr) return nullptr;
  if (auto* record = Find()) return record;
  auto& record = records.at(next_replacement);
  next_replacement = (next_replacement + 1) % records.size();
  record = {.package = Effects_package};
  return &record;
}
}  // namespace

void ForgetLivePackageExtents() {
  if (auto* record = Find()) *record = {.package = Effects_package};
}

void RecordLiveParticleDefinitions(std::span<const std::byte> payload) {
  if (auto* record = FindOrAdd()) {
    record->definitions = payload.data();
    record->definition_count = CountParticleDefinitions(payload.size());
  }
}

void RecordLiveEffectCurves(std::span<const std::byte> payload) {
  if (auto* record = FindOrAdd()) {
    record->curves = payload.data();
    record->curve_count = CountContainedEffectCurves(payload);
  }
}

std::size_t LiveParticleDefinitionCount() {
  const auto* record = Find();
  if (record == nullptr || record->definitions == nullptr ||
      record->definitions != Effects_package->block5Data) {
    return 0;
  }
  return record->definition_count;
}

std::size_t LiveEffectCurveCount() {
  const auto* record = Find();
  if (record == nullptr || record->curves == nullptr ||
      record->curves != Effects_package->curveBlock) {
    return 0;
  }
  return record->curve_count;
}
}  // namespace VS::Effects
