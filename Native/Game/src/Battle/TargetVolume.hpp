#pragma once
// The target sphere's state: its two volumes, wireframe animation, selected
// targets, floating labels and terrain cache, and the coarser view the sphere
// setup writes through. VS::Battle::targetSphere (Battle/Sphere.hpp) points at
// it.

#include <cstddef>
#include <cstdint>

#include "Battle/BattleTypes.hpp"
#include "Core/Gte.hpp"

namespace VS::Battle {

struct TargetPreview;

inline constexpr int kSphereWireVertexCount = 297;
inline constexpr int kSphereTargetLabelSlots = 24;
inline constexpr int kSphereTerrainCellCount = 81;
inline constexpr std::size_t kTargetSphereSpecBytes = 0x10;
inline constexpr std::size_t kSphereReservedB70Bytes = 0x3800;
inline constexpr std::size_t kSphereWireEdgeSlots = 0x302;
inline constexpr std::size_t kSetupTerrainRowFlagBytes = 0x64;

// decomp: D_800EB9B8 (the earlier decomp's src/BATTLE/BATTLE.PRG/58578.h:43;
// was D_800EB9B8_t2), upstream func_800C1564_t
// (src/BATTLE/BATTLE.PRG/57B1C.h:23): one sphere volume, set as a whole; only
// its kind byte is read.
struct TargetSphereSpec {
  std::uint8_t kind;
  std::uint8_t reserved1;
  std::uint8_t reserved2;
  std::uint8_t reserved3;
  std::int32_t reserved4;
  std::int32_t reserved8;
  std::int32_t reserved_c;
};
static_assert(sizeof(TargetSphereSpec) == kTargetSphereSpecBytes,
              "TargetSphereSpec must keep its PS1 layout");

// decomp: D_800EB9B8_unk990 (src/BATTLE/BATTLE.PRG/58578.h:17)
struct SphereTargetLabelRecord {
  char state;
  char depth;
  char gradient;
  char pulse_phase;
  std::int16_t screen_x;
  std::uint8_t offset_y;
  char fade;
  VS::Core::Gte::SVECTOR world_position;
  char* text;
};

// decomp: D_800EB9B8_t as the earlier decomp's src/BATTLE/BATTLE.PRG/573B8.c:55
// declared it, upstream D_800EB9B8_t (src/BATTLE/BATTLE.PRG/58578.h:41):
// TargetSphereRecord while its volumes are set up, with the two volumes as
// separate members.
struct TargetSphereSetupView {
  TargetSphereSpec primary_volume;
  TargetSphereSpec secondary_volume;
  std::int32_t depth_and_yaw;
  std::int16_t wire_phase_a;
  std::int16_t wire_phase_b;
  std::int16_t wire_phase_c;
  std::int16_t wire_shape;
  char target_count;
  char selected_target;
  std::uint8_t area_target_count;
  char highlighted_actor;
  // The highlighted pulse slot, target mode and wire vertex count.
  std::int32_t pulse_mode_and_vertex_count;
  std::int32_t highlight_tint;
  std::int32_t cursor_xy;
  std::int32_t cursor_z_and_locked;
  TargetPreview* target_list0;
  TargetPreview* target_list1;
  VS::Battle::ShapeVertex wire_vertices[kSphereWireVertexCount];
  SphereTargetLabelRecord labels[kSphereTargetLabelSlots];
  char reserved_b70[kSphereReservedB70Bytes +
                    (kSphereTerrainCellCount * sizeof(std::int32_t))];
  std::int16_t wire_edges[kSphereWireEdgeSlots];
  std::uint8_t terrain_row_flags[kSetupTerrainRowFlagBytes];
};

// decomp: D_800EB9B8_t (src/BATTLE/BATTLE.PRG/58578.h:41)
struct TargetSphereRecord {
  VS::Battle::TargetVolume volumes[2];
  std::int16_t depth;
  std::int16_t camera_yaw;
  std::int16_t wire_phase_a;
  std::int16_t wire_phase_b;
  std::int16_t wire_phase_c;
  std::int16_t wire_shape;
  char target_count;
  char selected_target;
  char area_target_count;
  char highlighted_actor;
  char highlighted_pulse_slot;
  char highlights_all_targets;
  std::int16_t vertex_count;
  std::int32_t highlight_tint;
  // The cursor's x and y halfwords.
  std::int32_t cursor_xy;
  std::int16_t cursor_z;
  std::int16_t cursor_locked;
  TargetPreview* target_lists[2];
  VS::Battle::ShapeVertex wire_vertices[kSphereWireVertexCount];
  SphereTargetLabelRecord labels[kSphereTargetLabelSlots];
  char reserved_b70[kSphereReservedB70Bytes];
  std::int32_t terrain_heights[kSphereTerrainCellCount];
  char wire_edges[kSphereWireEdgeSlots * sizeof(std::int16_t)];
  std::uint8_t terrain_row_flags[kSphereTerrainCellCount];
};
static_assert(offsetof(TargetSphereSetupView, labels) ==
                  offsetof(TargetSphereRecord, labels),
              "the setup view must alias the sphere's label slots");
static_assert(offsetof(TargetSphereSetupView, wire_edges) ==
                  offsetof(TargetSphereRecord, wire_edges),
              "the setup view must alias the sphere's wire edges");

}  // namespace VS::Battle
