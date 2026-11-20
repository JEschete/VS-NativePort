#pragma once
// Action resolution's shared state: a hit's target word, the limb injury
// statuses, the action effect mediator table and the hit results the
// prerequisite and previous-hit checks keep. ActionResolution.cpp defines the
// globals; the module image fills the tables at start-up.

#include <array>
#include <cstddef>
#include <cstdint>

struct Battle_ActionDefinition;
struct Battle_HitResult;

namespace VS::Battle {

// decomp: func_8008631C_t2 (src/BATTLE/BATTLE.PRG/hitEntity.h:5-12)
struct HitTargetWord {
  std::uint8_t target_actor;
  std::int8_t target_limb;
  std::uint8_t hit_threshold;
  std::uint8_t impact_kind;
  std::int16_t hp;
  std::int16_t mp;
};
inline constexpr std::size_t kHitTargetWordBytes = 0x8;
static_assert(sizeof(HitTargetWord) == kHitTargetWordBytes,
              "HitTargetWord must keep its PS1 layout");

// The table's entries return void, as upstream declares it, even where the
// decomp's handlers declare int; a promoted mediator must match this type.
using ActionEffectMediator = void (*)(Battle_ActionDefinition*,
                                      Battle_HitResult*, Battle_HitResult*, int,
                                      int);

inline constexpr std::size_t kLimbInjuryStatusBytes = 20;
inline constexpr std::size_t kActionEffectMediatorCount = 70;

// clang-format wraps these aliases inside the namespace, which cpplint
// reads as indentation.
// NOLINTBEGIN
using ActionEffectMediatorTable =
    std::array<ActionEffectMediator, kActionEffectMediatorCount>;
// NOLINTEND

// decomp: D_800E8200 (src/BATTLE/BATTLE.PRG/146C.c:297)
extern std::array<std::uint8_t, kLimbInjuryStatusBytes> limbInjuryStatusIds;
// decomp: _actionEffectMediators, upstream actionEffectMediators
// (src/BATTLE/BATTLE.PRG/146C.c:302)
extern ActionEffectMediatorTable actionEffectMediators;
// decomp: D_800F1A00 (src/BATTLE/BATTLE.PRG/146C.c:338)
extern std::uint32_t hitPrerequisiteResult;
// decomp: D_800F1A04 (src/BATTLE/BATTLE.PRG/146C.c:339)
extern std::uint32_t previousHitResult;

}  // namespace VS::Battle
