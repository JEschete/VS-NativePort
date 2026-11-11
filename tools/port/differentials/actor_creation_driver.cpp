// Native side of tools\port\differentials\actor_creation.py (F01818): one vector per stdin line, one
// line of integers per vector: the result (-2 when refused), the stored attribute mask (-1 when not
// stored), the special actor slot taken (-1 for none, -100 for a foreign actor), the call count and,
// per host call, its code, two arguments and the CreatedAiState fields it saw.
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <vector>

#include "Battle/Ai/ActorCreation.hpp"

extern "C" void Core_Debugf(const char*, const char*, ...) {}
extern "C" void Core_Errorf(const char*, const char*, ...) { std::abort(); }

namespace {
using namespace VS::Battle::Ai;

constexpr BattleActorHandle kActor{0x1000};
constexpr std::uint32_t kMotionLockBit = 0x200000;  // model word 0x08 bit 21
constexpr int kGridCells = kTileFlagGridSide * kTileFlagGridSide;
enum Code : int { kAllocate = 1, kAnchor, kBone, kSettings, kRelations, kCommand, kClear, kSaved, kRebuild };

struct Input {
  int id = 0;
  unsigned kind = 0, pose = 0, saved_settings_flags = 0;
  std::uint32_t settings = 0;
  std::int32_t allocation = 0;
  std::uint32_t object_word = 0, model_word8 = 0;
  int position_y = 0;
  std::uint32_t tile = 0;
  unsigned radius = 0, body_height = 0;
  std::array<unsigned, 4> stats{};
  int anchor_y = 0;
  std::int64_t bone = 0;
  std::uint32_t mask = 0;
  std::array<int, kRoomSpecialActorSlots> occupied{};
  unsigned tile_flag = 0;
  std::uint32_t far = 0;
  std::int64_t perturb_flags = -1, perturb_move = -1;
  int rebuilds = 0, saved = 0;
};

using Fields = std::array<std::int64_t, 30>;

Fields Snapshot(const CreatedAiState& s) {
  return {s.has_auxiliary_tag_bone, s.tile, s.previous_tile, s.saved_relation_options, s.point_height,
          s.actor_id, s.requested_movement_mode, s.current_movement_mode, s.radius, s.collision_radius,
          s.last_slide_tile, s.far_distance_squared, s.max_range, s.move_distance, s.body_height,
          s.ground_clearance, s.flight_height, s.patrol_direction, s.flee_tile, s.observed_command_opcode,
          s.locomotion_mode, s.behavior_mode, s.step_height, s.movement_flags, s.next_tile, s.move_y,
          s.detour_tile, s.locomotion_flags, s.walk_step, s.run_step};
}

struct Call {
  int code = 0;
  std::int64_t first = 0, second = 0;
  Fields fields{};
};

class Host final : public ActorCreationHost {
 public:
  explicit Host(const Input& input) : in_(input) {
    const std::uint32_t x = in_.tile & 0xFF, z = (in_.tile >> 16) & 0xFF;
    grid_.at(z * kTileFlagGridSide + x) = static_cast<std::uint16_t>(in_.tile_flag);
  }

  mutable std::vector<Call> calls;  // The const queries are recorded too.
  std::int64_t stored_mask = -1;
  int slot = -1;

  std::uint32_t AttributeActorMask() const override { return in_.mask; }
  void StoreAttributeActorMask(std::uint32_t mask) override { stored_mask = mask; }
  std::optional<RoomSpecialActors> SpecialActors() const override {
    RoomSpecialActors slots{};
    for (std::size_t i = 0; i < slots.size(); ++i)
      slots[i] = in_.occupied[i] ? static_cast<BattleActorHandle>(0x2000 + i) : kNoBattleActor;
    return slots;
  }
  void StoreSpecialActor(std::size_t index, BattleActorHandle actor) override {
    slot = actor == kActor && slot == -1 ? static_cast<int>(index) : -100;
  }
  std::span<const std::uint16_t> TileFlags() const override { return grid_; }
  std::optional<std::uint32_t> ModelObjectWord(BattleActorHandle actor) const override {
    if (actor != kActor) std::abort();
    return in_.object_word;
  }
  std::optional<CreationModel> Model(BattleActorHandle actor) const override {
    if (actor != kActor) std::abort();
    return CreationModel{(in_.model_word8 & kMotionLockBit) != 0, static_cast<std::int16_t>(in_.position_y),
                         in_.tile, static_cast<std::uint16_t>(in_.radius),
                         static_cast<std::uint16_t>(in_.body_height)};
  }
  std::optional<CreationStats> Stats(BattleActorHandle actor) const override {
    if (actor != kActor) std::abort();
    return CreationStats{static_cast<std::uint8_t>(in_.stats[0]), static_cast<std::uint8_t>(in_.stats[1]),
                         static_cast<std::uint8_t>(in_.stats[2]), static_cast<std::uint8_t>(in_.stats[3])};
  }
  bool AllocateAiState(int actor_id, BattleActorHandle actor) override {
    calls.push_back({kAllocate, actor_id, actor == kActor});
    return true;
  }
  std::optional<std::int16_t> AnchorHeightY(int actor_id) const override {
    calls.push_back({kAnchor, actor_id});
    return static_cast<std::int16_t>(in_.anchor_y);
  }
  bool HasAuxiliaryBoneTag(int actor_id, int tag) const override {
    calls.push_back({kBone, actor_id, tag});
    return in_.bone >= 0;
  }
  void ClearActorCommand(BattleActorHandle actor) override { calls.push_back({kClear, actor == kActor}); }
  void ApplySettings(CreatedAiState& state, std::uint32_t settings) override {
    calls.push_back({kSettings, 0, settings, Snapshot(state)});
    state.far_distance_squared = in_.far;
    if (in_.perturb_flags >= 0) state.movement_flags = static_cast<std::uint16_t>(in_.perturb_flags);
  }
  void InitializeRelations(CreatedAiState& state) override {
    calls.push_back({kRelations, 0, 0, Snapshot(state)});
    if (in_.perturb_move >= 0) state.move_distance = static_cast<std::int32_t>(in_.perturb_move);
  }
  void IssueCommand(CreatedAiState& state, CommandIntent intent) override {
    calls.push_back({kCommand, 0, static_cast<int>(intent), Snapshot(state)});
  }
  void ApplySavedRelations(CreatedAiState& state) override { calls.push_back({kSaved, 0, 0, Snapshot(state)}); }
  bool RebuildActionPlans(CreatedAiState& state) override {
    calls.push_back({kRebuild, 0, 0, Snapshot(state)});
    return ++rebuild_calls_ <= in_.rebuilds;
  }

 private:
  const Input& in_;
  std::array<std::uint16_t, kGridCells> grid_{};
  int rebuild_calls_ = 0;
};
}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) return 2;
  std::string line;
  while (std::getline(std::cin, line)) {
    std::istringstream in(line);
    Input v;
    in >> v.id >> v.kind >> v.pose >> v.saved_settings_flags >> v.settings >> v.allocation >> v.object_word >>
        v.model_word8 >> v.position_y >> v.tile >> v.radius >> v.body_height;
    for (auto& stat : v.stats) in >> stat;
    in >> v.anchor_y >> v.bone >> v.mask;
    for (auto& taken : v.occupied) in >> taken;
    in >> v.tile_flag >> v.far >> v.perturb_flags >> v.perturb_move >> v.rebuilds >> v.saved;
    if (!in) std::abort();
    Host host(v);
    CreationActor actor{.handle = kActor,
                        .id = v.id,
                        .kind = static_cast<std::uint16_t>(v.kind),
                        .combat_pose = static_cast<std::uint8_t>(v.pose),
                        .saved_settings_flags = static_cast<std::uint16_t>(v.saved_settings_flags),
                        .settings = v.settings,
                        .allocation_kind = v.allocation};
    const std::optional<bool> result = CreateActorAiState(host, actor, static_cast<std::int16_t>(v.saved));
    std::printf("%d %lld %d %zu", result ? int{*result} : -2, static_cast<long long>(host.stored_mask), host.slot,
                host.calls.size());
    for (const Call& call : host.calls) {
      std::printf(" %d %lld %lld", call.code, static_cast<long long>(call.first),
                  static_cast<long long>(call.second));
      for (const std::int64_t field : call.fields) std::printf(" %lld", static_cast<long long>(field));
    }
    std::printf("\n");
  }
}
