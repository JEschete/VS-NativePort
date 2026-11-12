// Hand-maintained (tools/port/owned.csv).
#include "Battle/ActionPresentation.hpp"

#include <algorithm>
#include <bit>
#include <limits>

namespace VS::Battle {
namespace {
constexpr int kFirstChain = 22;
constexpr int kChainRange = 32;
constexpr int kFirstDefense = 40;
constexpr int kFirstBreakArt = 141;
constexpr int kFirstSpecial = 184;
constexpr int kFirstItem = 224;
constexpr int kFirstSpell = 55;
constexpr int kSpellSound = 0x34;
constexpr int kItemSound = 0x35;
constexpr int kBreakArtSound = 0x36;
constexpr int kChainFlag = 0xB8;
constexpr int kSpellFlag = 0xB7;
constexpr int kSpecialFlag = 0xB9;
constexpr std::uint32_t kNoAbilityInput = 0x8000;
constexpr std::uint16_t kActorTarget = 4;
constexpr std::uint16_t kWorldTarget = 5;
constexpr int kRootTag = 255;
constexpr int kFarDistance = 449;
constexpr int kTransientSpawn = 128;
constexpr std::uint16_t kHealCountLimit = 1000;
constexpr std::size_t kWeaponCategories = 10;
constexpr std::array kHeals{77, 96, 224, 225, 226, 227, 232, 233, 234, 235};
constexpr int kArtEffectMode = 3;
constexpr int kAnimationTickSpeed = 4;
constexpr int kAnimationPhase = 7;
constexpr int kEffectPhase = 6;
constexpr int kCommitPhase = 4;
constexpr int kPresentationPhase = 5;
constexpr int kMiscSelection = 8;
constexpr std::uint8_t kActorAsset = 5;
constexpr std::uint8_t kPlayerAsset = 4;
constexpr int kPlayerModels = 4;

ActionPose Pose(std::uint16_t action) {
  if (action >= kFirstBreakArt && action < kFirstSpecial) {
    return ActionPose::kBreakArt;
  }
  if (action >= kFirstSpecial && action < kFirstItem) {
    return ActionPose::kSpecial;
  }
  return ActionPose::kSpell;
}
void AnimateWindow(PresentationAction& action, ActionPresentationWorld& world) {
  const auto frame = world.WindowFrame(action.Primary().actor);
  world.SetField(AbilityField::kAbilityWindow,
                 static_cast<std::uint16_t>(frame * 2U));
}
bool CancelSelection(ActionPresentationWorld& world) {
  if (world.Selection().Primary().actor != 0) {
    world.ReleaseMenu();
    world.ResetReaction(world.Selection().Primary().actor);
  } else if (world.HasPresentation(world.Selection().Id())) {
    world.Selection().ClearId();
    world.ReturnToMenu();
    return true;
  } else {
    world.ReleaseMenu();
  }
  world.Selection().ClearId();
  world.Recover();
  return true;
}
bool RecordAttack(ActionPresentationWorld& world) {
  auto& selection = world.Selection();
  const auto action = selection.Id();
  if (selection.Primary().actor != 0 ||
      (action != 1 && !(action >= kFirstSpecial && action < kFirstItem))) {
    return true;
  }
  const auto target = selection.Target(0);
  if (target.position_kind != 0 ||
      world.Spawn(target.actor) == kTransientSpawn) {
    return true;
  }
  auto category = world.WeaponCategory();
  if (category == kWeaponCategories) category = 0;
  if (category >= kWeaponCategories) return false;
  const auto attacks = world.WeaponAttacks(category);
  if (attacks != std::numeric_limits<std::uint16_t>::max()) {
    world.SetWeaponAttacks(category, static_cast<std::uint16_t>(attacks + 1));
  }
  return true;
}
void ResetInputs(ActionPresentationWorld& world) {
  world.SetField(AbilityField::kPreviousChainButtons, 0);
  world.SetField(AbilityField::kPreviousChainInput, 0);
  world.SetField(AbilityField::kChainAbilitySlot, 0);
  world.SetField(AbilityField::kChainContinued, 0);
}
bool Targeted(const PresentationAction& action, int actor) {
  for (std::size_t i = 0; i < action.Count(); ++i) {
    const auto hit = action.Target(i);
    if (hit.position_kind == 0 && hit.actor == actor) return true;
  }
  return false;
}
bool AddTarget(const ActionHit& hit, const ActionPresentationWorld& world,
               ActionPresentationRequest& request) {
  if (hit.position_kind == 0) {
    for (std::size_t j = 0; j < request.count; ++j) {
      auto& target = request.targets.at(j);
      if (target.kind == kActorTarget && target.actor == hit.actor) {
        target.part = 0;
        return true;
      }
    }
  }
  if (request.count >= request.targets.size()) return false;
  auto& target = request.targets.at(request.count);
  if (hit.position_kind == 0) {
    const int limb = std::bit_cast<std::int8_t>(hit.limb) == -1
                         ? world.DefaultLimb(hit.actor)
                         : std::bit_cast<std::int8_t>(hit.limb);
    target.kind = kActorTarget;
    target.actor = hit.actor;
    target.part = static_cast<std::uint8_t>(world.BoneTag(hit.actor, limb));
  } else {
    target.kind = kWorldTarget;
    target.position = hit.Position();
  }
  ++request.count;
  return true;
}
int PreparePresentation(PresentationAction& action,
                        ActionPresentationWorld& world) {
  const auto sound_position = world.Placement(action.Primary().actor);
  const auto id = action.Id();
  world.Animate(Pose(id), action.Primary().actor, 0, 0);
  if (id < kFirstBreakArt) {
    world.PlaySound(kSpellSound, sound_position);
    if (action.Primary().actor == 0) world.SetFlag(kSpellFlag);
    return 2;
  }
  if (id < kFirstSpecial) {
    world.PlaySound(kBreakArtSound, sound_position);
    return kArtEffectMode;
  }
  if (id < kFirstItem) {
    world.PlaySound(kSpellSound, sound_position);
    if (action.Primary().actor == 0) world.SetFlag(kSpecialFlag);
    return kArtEffectMode;
  }
  world.PlaySound(kItemSound, sound_position);
  return 1;
}
}  // namespace
// decomp: func_80070278 (src/BATTLE/BATTLE.PRG/146C.c:3510).
bool StartActionAnimation(ActionPresentationWorld& world) {
  world.SetField(AbilityField::kAbilityWindow, 0);
  world.SetField(AbilityField::kAbilityTimer, 0);
  auto& action = world.Queued();
  if (static_cast<std::uint32_t>(action.Id() - kFirstChain) < kChainRange &&
      action.Primary().position_kind == 0 && action.Primary().actor == 0) {
    world.SetFlag(kChainFlag);
  }
  const auto step = world.QueueIndex();
  if (step == 0) {
    if (!world.HasPresentation(action.Id())) {
      world.Animate(ActionPose::kMelee, action.Primary().actor,
                    action.Animation(), 0);
      AnimateWindow(action, world);
    } else {
      world.Animate(Pose(action.Id()), action.Primary().actor, 1, 0);
      world.SetTickSpeed(kAnimationTickSpeed);
      world.SetField(AbilityField::kAbilityWindow, 0);
    }
  } else {
    if (action.Id() < kFirstDefense) {
      const auto slot = world.ChainSlot();
      if (slot == 0 || slot > kAbilityInputSlots) return false;
      world.Animate(ActionPose::kChain, action.Primary().actor,
                    world.MappedChain(slot - 1) - kFirstChain, step);
    } else {
      world.Animate(ActionPose::kDefense, action.Primary().actor, 0, 0);
    }
    AnimateWindow(action, world);
  }
  world.StopSound(kSpellSound);
  world.StopSound(kItemSound);
  world.StopSound(kBreakArtSound);
  world.ReleaseNotification();
  world.SetField(AbilityField::kChainAbilitySlot, 0);
  world.SetPhase(kAnimationPhase);
  return true;
}
// decomp: func_800704B0 (src/BATTLE/BATTLE.PRG/146C.c:3572).
void AdvanceActionEffectPhase(ActionPresentationWorld& world) {
  world.SetPhase(kEffectPhase);
  world.ClearEffectMode();
}
// decomp: func_80070F28 (src/BATTLE/BATTLE.PRG/146C.c:3775).
bool CommitSelectedAction(int target_selection_mode,
                          ActionPresentationWorld& world) {
  world.PrepareCamera();
  const auto preview = world.SelectedPreview();
  world.ClearSphereTargets();
  world.ClearPrimaryVolume();
  world.ClearSecondaryVolume();
  world.FreeSphere();
  if (target_selection_mode != 0 && preview == kRootTag) {
    return CancelSelection(world);
  }
  world.BeginCommit();
  if (world.Selection().Primary().actor == 0) {
    if (world.SelectionKind() == kMiscSelection) {
      world.ConsumeItem(world.SelectedItem());
    }
    world.ClearSelectionKind();
  } else {
    world.Disengage(world.Selection().Primary().actor);
  }
  world.ReleaseMenu();
  if (target_selection_mode == 0) {
    world.CollectEnvironment();
  } else if (target_selection_mode == 2) {
    world.CollectAll();
  } else {
    world.CollectSingle();
  }
  world.ResolveSelection();
  if (!RecordAttack(world)) return false;
  const auto distance = world.TargetDistance();
  const auto zoom = world.Zoom(world.Selection().Id());
  world.SetZoom(distance >= kFarDistance && zoom == 1 ? 0 : zoom);
  ResetInputs(world);
  if (world.Selection().Id() >= kFirstSpell) {
    world.ClearSpellCommit();
    if (std::ranges::find(kHeals, world.Selection().Id()) != kHeals.end() &&
        world.HealCount() < kHealCountLimit) {
      world.SetHealCount(static_cast<std::uint16_t>(world.HealCount() + 1));
    }
  }
  world.SetPhase(kCommitPhase);
  for (int actor = 0; actor < static_cast<int>(kPresentationTargets); ++actor) {
    if (actor != world.Selection().Primary().actor) world.RestoreIdle(actor);
  }
  world.EnableClut();
  world.TransformColors();
  world.FreezeAnimations();
  world.HideMarkers();
  world.HideMinimap();
  world.ResumeRoomAnimations();
  return true;
}
bool BuildActionPresentationRequest(PresentationAction& action,
                                    const ActionPresentationWorld& world,
                                    ActionPresentationRequest& request) {
  request.count = 0;
  request.other_count = 0;
  request.source.kind = kActorTarget;
  request.source.actor = action.Primary().actor;
  request.source.part = static_cast<std::uint8_t>(
      world.BoneTag(action.Primary().actor, kRootTag));
  for (std::size_t i = 0; i < action.Count(); ++i) {
    if (i >= kActionTargetCapacity ||
        !AddTarget(action.Target(i), world, request)) {
      return false;
    }
  }
  for (int actor = 0; actor < static_cast<int>(kPresentationTargets); ++actor) {
    if (world.EligibleBystander(actor) && !Targeted(action, actor) &&
        actor != action.Primary().actor) {
      request.others.at(request.other_count++) =
          static_cast<std::uint8_t>(actor);
    }
  }
  request.focus = action.Focus();
  request.focus.tag = 0;
  constexpr int kTerrainEffect = 39;
  constexpr int kGroundEffect = 36;
  if (request.effect == kTerrainEffect &&
      action.Target(0).position_kind == kActorTarget) {
    request.effect = kGroundEffect;
  }
  return true;
}
// decomp: func_8007138C (src/BATTLE/BATTLE.PRG/146C.c:3906).
bool StartActionPresentation(ActionPresentationWorld& world) {
  world.SetField(AbilityField::kAbilityInputSucceeded, 0);
  world.SetField(AbilityField::kAbilityInputAttempted,
                 (world.PlayerStatuses() & kNoAbilityInput) != 0 ? 1 : 0);
  auto& action = world.Queued();
  if (!world.HasPresentation(action.Id())) return StartActionAnimation(world);
  const auto mode = PreparePresentation(action, world);
  ActionPresentationRequest request{};
  request.effect = world.Effect(action.Id());
  if (!BuildActionPresentationRequest(action, world, request)) return false;
  world.Dispatch(request, mode, action.Variant());
  if (action.Id() >= kFirstBreakArt && action.Id() < kFirstItem) {
    const auto actor = action.Primary().actor;
    PresentationAsset asset{actor};
    if (action.Id() < kFirstSpecial || actor != 0) {
      asset.kind = kActorAsset;
      asset.model = world.SourceModel(actor);
    } else {
      asset.kind = kPlayerAsset;
      asset.model = static_cast<std::uint8_t>((action.Id() - kFirstSpecial) %
                                              kPlayerModels);
    }
    asset.material = world.SourceMaterial(actor);
    world.QueueAsset(asset);
  }
  world.HideHud();
  if (world.InformationEnabled()) world.ShowAction(action.Id());
  world.SetPhase(kPresentationPhase);
  return true;
}
}  // namespace VS::Battle
