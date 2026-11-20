// Hand-maintained (tools/port/owned.csv).
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "Effects/PackageExtents.hpp"
#include "Effects/Runtime.hpp"
#include "Legacy/Psx.h"

namespace VS::Effects {
namespace {
// The action and secondary packages, with room for test packages.
constexpr std::size_t kRecordCapacity = 4;

struct PackageExtentRecord {
  const LiveEffectPackage* package = nullptr;
  const void* definitions = nullptr;
  std::size_t definition_count = 0;
  const void* curves = nullptr;
  std::size_t curve_count = 0;
};

std::array<PackageExtentRecord, kRecordCapacity> records{};
std::size_t next_replacement = 0;

// Whether the action or secondary package record still binds `data` as its
// `block` pointer; the current and saved package pointers only ever select
// one of the two. The resident secondary package binds the empty package's
// blocks once, while every loaded package takes a new buffer, so a bound
// extent must outlive the loads after it (#442).
template <typename Block>
bool IsBound(const void* data, Block LiveEffectPackage::* block) {
  if (data == nullptr) return false;
  return static_cast<const void*>(actionPackage.*block) == data ||
         static_cast<const void*>(secondaryPackage.*block) == data;
}

// A free extent, else the oldest one no package binds, else the oldest; a
// free extent has the lowest order.
template <typename Extents, typename Block>
auto& ReplaceableExtent(Extents& extents, Block LiveEffectPackage::* block) {
  auto* oldest = &*std::ranges::min_element(
      extents, {}, [](const auto& extent) { return extent.recorded; });
  decltype(oldest) oldest_unbound = nullptr;
  for (auto& extent : extents) {
    if (IsBound(extent.data, block)) continue;
    if (oldest_unbound == nullptr ||
        extent.recorded < oldest_unbound->recorded) {
      oldest_unbound = &extent;
    }
  }
  return oldest_unbound != nullptr ? *oldest_unbound : *oldest;
}

// The action and secondary packages' program blocks, with room for blocks
// rebound at other addresses and for test programs.
constexpr std::size_t kProgramCapacity = 8;

struct ProgramExtent {
  const std::byte* data = nullptr;
  std::size_t size = 0;
  // Order of recording; the oldest unbound extent is replaced when none is
  // free.
  std::uint64_t recorded = 0;
};

std::array<ProgramExtent, kProgramCapacity> programs{};
std::uint64_t program_records = 0;

ProgramExtent* FindProgram(const void* data) {
  if (data == nullptr) return nullptr;
  const auto found = std::ranges::find(programs, data, &ProgramExtent::data);
  return found == programs.end() ? nullptr : &*found;
}

// The action and secondary packages' animation tables, with room for tables
// rebound at other addresses and for test tables.
constexpr std::size_t kAnimationTableCapacity = 8;

struct AnimationTableExtent {
  std::byte* data = nullptr;
  std::size_t size = 0;
  // Order of recording; the oldest unbound extent is replaced when none is
  // free.
  std::uint64_t recorded = 0;

  [[nodiscard]] std::uintptr_t Begin() const {
    return reinterpret_cast<std::uintptr_t>(data);
  }
  [[nodiscard]] std::uintptr_t End() const { return Begin() + size; }
};

std::array<AnimationTableExtent, kAnimationTableCapacity> animation_tables{};
std::uint64_t animation_records = 0;

AnimationTableExtent* FindAnimationTable(const void* data) {
  if (data == nullptr) return nullptr;
  const auto found =
      std::ranges::find(animation_tables, data, &AnimationTableExtent::data);
  return found == animation_tables.end() ? nullptr : &*found;
}

PackageExtentRecord* Find() {
  if (VS::Effects::package == nullptr) return nullptr;
  const auto found = std::ranges::find(records, VS::Effects::package,
                                       &PackageExtentRecord::package);
  return found == records.end() ? nullptr : &*found;
}

PackageExtentRecord* FindOrAdd() {
  if (VS::Effects::package == nullptr) return nullptr;
  if (auto* record = Find()) return record;
  auto& record = records.at(next_replacement);
  next_replacement = (next_replacement + 1) % records.size();
  record = {.package = VS::Effects::package};
  return &record;
}
}  // namespace

void ForgetLivePackageExtents() {
  if (auto* record = Find()) *record = {.package = VS::Effects::package};
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
      record->definitions != VS::Effects::package->particle_definitions) {
    return 0;
  }
  return record->definition_count;
}

std::size_t LiveEffectCurveCount() {
  const auto* record = Find();
  if (record == nullptr || record->curves == nullptr ||
      record->curves != VS::Effects::package->curve_block) {
    return 0;
  }
  return record->curve_count;
}

void RecordLiveEffectPrograms(std::span<const std::byte> payload) {
  if (payload.data() == nullptr) return;
  auto* extent = FindProgram(payload.data());
  if (extent == nullptr) {
    extent = &ReplaceableExtent(programs, &LiveEffectPackage::programs);
  }
  *extent = {.data = payload.data(),
             .size = payload.size(),
             .recorded = ++program_records};
}

void ForgetLiveEffectPrograms(const void* data) {
  if (auto* extent = FindProgram(data)) *extent = {};
}

std::span<const std::byte> LiveEffectPrograms(const void* data) {
  const auto* extent = FindProgram(data);
  if (extent == nullptr) return {};
  return {extent->data, extent->size};
}

void RecordLiveAnimationTable(std::span<std::byte> table) {
  if (table.data() == nullptr) return;
  const AnimationTableExtent recorded{.data = table.data(),
                                      .size = table.size(),
                                      .recorded = ++animation_records};
  for (auto& extent : animation_tables) {
    if (extent.data != nullptr && extent.Begin() < recorded.End() &&
        recorded.Begin() < extent.End()) {
      extent = {};
    }
  }
  ReplaceableExtent(animation_tables, &LiveEffectPackage::animation_table) =
      recorded;
}

void ForgetLiveAnimationTable(const void* data) {
  if (auto* extent = FindAnimationTable(data)) *extent = {};
}

std::span<std::byte> LiveAnimationTable(const void* data) {
  const auto* extent = FindAnimationTable(data);
  if (extent == nullptr) return {};
  return {extent->data, extent->size};
}

std::span<const std::byte> LiveAnimationProgram(const void* program) {
  const auto at = reinterpret_cast<std::uintptr_t>(program);
  const auto found =
      std::ranges::find_if(animation_tables, [at](const auto& extent) {
        return extent.data != nullptr && extent.Begin() <= at &&
               at < extent.End();
      });
  if (found == animation_tables.end()) return {};
  const std::span<const std::byte> table(found->data, found->size);
  return table.subspan(at - found->Begin());
}
}  // namespace VS::Effects
