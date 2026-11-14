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

// The action and secondary packages' program blocks, with room for blocks
// rebound at other addresses and for test programs.
constexpr std::size_t kProgramCapacity = 8;

struct ProgramExtent {
  const std::byte* data = nullptr;
  std::size_t size = 0;
};

std::array<ProgramExtent, kProgramCapacity> programs{};
std::size_t next_program = 0;

ProgramExtent* FindProgram(const void* data) {
  if (data == nullptr) return nullptr;
  const auto found = std::ranges::find(programs, data, &ProgramExtent::data);
  return found == programs.end() ? nullptr : &*found;
}

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

void RecordLiveEffectPrograms(std::span<const std::byte> payload) {
  if (payload.data() == nullptr) return;
  auto* extent = FindProgram(payload.data());
  if (extent == nullptr) {
    extent = &programs.at(next_program);
    next_program = (next_program + 1) % programs.size();
  }
  *extent = {.data = payload.data(), .size = payload.size()};
}

void ForgetLiveEffectPrograms(const void* data) {
  if (auto* extent = FindProgram(data)) *extent = {};
}

std::span<const std::byte> LiveEffectPrograms(const void* data) {
  const auto* extent = FindProgram(data);
  if (extent == nullptr) return {};
  return {extent->data, extent->size};
}
}  // namespace VS::Effects
