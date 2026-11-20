#pragma once
// The SHP character model header as it lies in memory: the section offsets and
// the skeleton's bone records that follow it. The typed skeleton view lives in
// Actors/Skeleton.hpp.

#include <cstddef>
#include <cstdint>

namespace VS::Actors {

// decomp: Armature (src/BATTLE/BATTLE.PRG/entity.h:62)
struct ShpBone {
  std::int32_t length;
  std::uint8_t parent_index;
  std::uint8_t reserved5;
  std::uint8_t tag;
  std::uint8_t limb_ids;
  std::uint8_t auxiliary_tag;
  std::uint8_t reserved9;
  std::uint16_t joint_init_value;
  std::uint8_t pose_lock_slot;
  std::uint8_t reserved0d;
  std::uint8_t reserved0e;
  std::uint8_t reserved0f;
};
inline constexpr std::size_t kShpBoneBytes = 0x10;
static_assert(sizeof(ShpBone) == kShpBoneBytes,
              "ShpBone must keep its PS1 layout");

// decomp: D_800F4538_unk68 (src/BATTLE/BATTLE.PRG/entity.h:77). The section
// fields are byte offsets from the start of the model (on-disc pointers).
inline constexpr std::size_t kShpHeaderPrefixBytes = 0x30;
struct ShpModel {
  std::uint8_t header[kShpHeaderPrefixBytes];
  std::uint32_t sound_section;
  std::uint32_t group_section;
  std::uint32_t vertex_section;
  std::uint32_t polygon_section;
  ShpBone bones[0];
};
inline constexpr std::size_t kShpModelHeaderBytes = 0x40;
static_assert(sizeof(ShpModel) == kShpModelHeaderBytes,
              "ShpModel must keep its PS1 layout");

}  // namespace VS::Actors
