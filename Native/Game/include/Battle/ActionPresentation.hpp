#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>

#include "Battle/Abilities.hpp"
#include "Battle/ActionPreview.hpp"

namespace VS::Battle {
inline constexpr std::size_t kPresentationTargets = 16;
enum class ActionPose { kMelee, kSpell, kBreakArt, kSpecial, kChain, kDefense };

// An owner, not a snapshot: reads after services observe that same request,
// even when a callback replaces the world's current action state.
class PresentationAction {
 public:
  virtual ~PresentationAction() = default;
  virtual std::uint16_t Id() const = 0;
  virtual void ClearId() = 0;
  virtual ActionHit Primary() const = 0;
  virtual ActionHit Target(std::size_t index) const = 0;
  virtual std::uint16_t Count() const = 0;
  virtual std::uint16_t Animation() const = 0;
  virtual std::uint8_t Variant() const = 0;
  virtual ActionPoint Focus() const = 0;
};
struct PresentationTarget {
  std::uint16_t kind{};
  std::uint8_t actor{}, part{};
  ActionPoint position{};
  bool operator==(const PresentationTarget&) const = default;
};
struct ActionPresentationRequest {
  std::uint16_t effect{};
  std::uint8_t count{}, other_count{};
  PresentationTarget source{};
  std::array<PresentationTarget, kPresentationTargets> targets{};
  std::array<std::uint8_t, kPresentationTargets> others{};
  ActionPoint focus{};
};
struct PresentationAsset {
  std::uint8_t slot{}, kind{}, model{}, material{};
  bool operator==(const PresentationAsset&) const = default;
};
class ActionPresentationWorld {
 public:
  virtual ~ActionPresentationWorld() = default;
  virtual PresentationAction& Selection() = 0;
  virtual PresentationAction& Queued() = 0;
  virtual std::uint32_t QueueIndex() = 0;
  virtual std::uint8_t SelectedPreview() = 0;
  virtual void BeginCommit() = 0;
  virtual void SetZoom(std::uint8_t zoom) = 0;
  virtual void SetField(AbilityField field, int value) = 0;
  virtual std::uint8_t ChainSlot() = 0;
  virtual std::uint8_t MappedChain(std::size_t slot) = 0;
  virtual bool HasPresentation(std::uint16_t action) = 0;
  virtual std::uint8_t Effect(std::uint16_t action) = 0;
  virtual std::uint8_t Zoom(std::uint16_t action) = 0;
  virtual std::uint32_t PlayerStatuses() = 0;
  virtual void SetFlag(int flag) = 0;
  // Uses the current selection's focus at call entry.
  virtual void Animate(ActionPose pose, int actor, int parameter,
                       std::uint32_t step) = 0;
  virtual std::uint32_t WindowFrame(int actor) = 0;
  virtual void SetTickSpeed(int speed) = 0;
  virtual void SetPhase(int phase) = 0;
  virtual void StopSound(int sound) = 0;
  virtual void ReleaseNotification() = 0;
  virtual void ClearEffectMode() = 0;
  virtual void PrepareCamera() = 0;
  virtual void ClearSphereTargets() = 0;
  virtual void ClearPrimaryVolume() = 0;
  virtual void ClearSecondaryVolume() = 0;
  virtual void FreeSphere() = 0;
  virtual void ReleaseMenu() = 0;
  virtual void ResetReaction(int actor) = 0;
  virtual void Recover() = 0;
  virtual void ReturnToMenu() = 0;
  virtual std::int16_t SelectionKind() = 0;
  virtual std::uint16_t SelectedItem() = 0;
  virtual void ConsumeItem(std::uint16_t item) = 0;
  virtual void ClearSelectionKind() = 0;
  virtual void Disengage(int actor) = 0;
  virtual void CollectEnvironment() = 0;
  virtual void CollectAll() = 0;
  virtual void CollectSingle() = 0;
  virtual void ResolveSelection() = 0;
  virtual std::uint8_t Spawn(int actor) = 0;
  virtual std::uint8_t WeaponCategory() = 0;
  virtual std::uint16_t WeaponAttacks(std::size_t category) = 0;
  virtual void SetWeaponAttacks(std::size_t category, std::uint16_t count) = 0;
  virtual int TargetDistance() = 0;
  virtual void ClearSpellCommit() = 0;
  virtual std::uint16_t HealCount() = 0;
  virtual void SetHealCount(std::uint16_t count) = 0;
  virtual void RestoreIdle(int actor) = 0;
  virtual void EnableClut() = 0;
  virtual void TransformColors() = 0;
  virtual void FreezeAnimations() = 0;
  virtual void HideMarkers() = 0;
  virtual void HideMinimap() = 0;
  virtual void ResumeRoomAnimations() = 0;
  virtual ActionPoint Placement(int actor) = 0;
  virtual void PlaySound(int sound, ActionPoint position) = 0;
  // Read-only skeleton query; the original's first, overwritten lookup is
  // deliberately not performed. A -1 input limb resolves its default first.
  virtual int BoneTag(int actor, int limb) const = 0;
  virtual std::uint8_t DefaultLimb(int actor) const = 0;
  virtual bool EligibleBystander(int actor) const = 0;
  virtual void Dispatch(const ActionPresentationRequest& request, int mode,
                        std::uint8_t variant) = 0;
  virtual std::uint8_t SourceMaterial(int actor) = 0;
  virtual std::uint8_t SourceModel(int actor) = 0;
  virtual void QueueAsset(PresentationAsset asset) = 0;
  virtual void HideHud() = 0;
  virtual bool InformationEnabled() = 0;
  virtual void ShowAction(std::uint16_t action) = 0;
};
bool StartActionAnimation(ActionPresentationWorld& world);
void AdvanceActionEffectPhase(ActionPresentationWorld& world);
bool CommitSelectedAction(int target_selection_mode,
                          ActionPresentationWorld& world);
// Leaves dispatch to the caller. False means unsupported capacity, never a
// successfully truncated effect. Duplicate input hits do not consume capacity.
bool BuildActionPresentationRequest(PresentationAction& action,
                                    const ActionPresentationWorld& world,
                                    ActionPresentationRequest& request);
bool StartActionPresentation(ActionPresentationWorld& world);
}  // namespace VS::Battle
