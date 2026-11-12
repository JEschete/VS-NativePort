#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace VS::Battle {
inline constexpr std::size_t kActionStatChangeCount = 6;
inline constexpr std::size_t kActionLimbCount = 6;
inline constexpr std::size_t kActionEquipmentChangeCount = 4;
inline constexpr std::size_t kActionAxisCount = 3;
inline constexpr std::size_t kActionPointComponents = 4;
inline constexpr std::size_t kActionAffinityCount = 7;
struct ActionPoint {
  std::int16_t x{}, y{}, z{}, tag{};
  friend bool operator==(const ActionPoint&, const ActionPoint&) = default;
};
struct ActionHit {
  std::uint8_t actor{}, limb{}, threshold{}, impact{};
  std::int16_t hp{}, mp{};
  std::array<std::int16_t, kActionStatChangeCount> stat_changes{};
  std::uint32_t remove_statuses{}, inflict_statuses{}, effects{};
  std::array<std::uint32_t, kActionLimbCount> limbs{};
  std::array<std::int16_t, kActionEquipmentChangeCount> equipment_changes{};
  std::int32_t position_kind{};

  ActionPoint Position() const;
  void SetPosition(ActionPoint point);
  friend bool operator==(const ActionHit&, const ActionHit&) = default;
};
inline constexpr std::size_t kActionTargetCapacity = 255;
struct ActionRequest {
  std::uint16_t action{};
  std::uint8_t affinity{}, damage_type{};
  ActionHit primary{};
  std::uint16_t animation{}, count{};
  std::array<ActionHit, kActionTargetCapacity> targets{};
  ActionPoint focus{};
  friend bool operator==(const ActionRequest&, const ActionRequest&) = default;
};
struct PreviewVolume {
  std::uint8_t shape{}, kind{};
  std::int16_t yaw{};
  std::array<std::uint8_t, kActionAxisCount> ranges{};
  std::uint8_t pitch{};
  ActionPoint origin{};
  friend bool operator==(const PreviewVolume&, const PreviewVolume&) = default;
};
struct ResolutionDescriptor {
  std::uint8_t prerequisite{}, stat{}, damage_type{}, affinity{};
};
class ResolutionDefinition {
 public:
  virtual ~ResolutionDefinition() = default;
  virtual ResolutionDescriptor Hit(int hit) = 0;
  virtual bool Redirects() = 0;
  virtual std::uint8_t Category() = 0;
  virtual PreviewVolume Range() = 0;
  virtual PreviewVolume Area() = 0;
};
class ResolutionActor {
 public:
  virtual ~ResolutionActor() = default;
  virtual std::uint8_t Slot() = 0;
  virtual std::uint8_t DefaultLimb() = 0;
  virtual bool Cancels() = 0;
  virtual bool HasBlade() = 0;
  virtual std::uint8_t DamageType() = 0;
  virtual std::array<std::int16_t, kActionAffinityCount> Affinities() = 0;
};
class ActionPreviewWorld {
 public:
  virtual ~ActionPreviewWorld() = default;
  // Returned owners remain stable across callbacks, but their fields are fresh.
  virtual ResolutionDefinition* Definition(std::uint32_t action) = 0;
  // Missing record is refused immediately; a present record may carry null
  // stats until a selected policy actually needs those stats.
  virtual std::optional<ResolutionActor*> Actor(int slot) = 0;
  virtual std::optional<std::uint32_t> WeaponDrawn(int slot) = 0;
  virtual std::optional<std::uint8_t> SpawnId(int slot) = 0;
  virtual std::optional<ActionPoint> SourceOrigin(int slot) = 0;
  virtual std::optional<ActionPoint> TargetPosition(int slot, int limb) = 0;
  virtual std::optional<ActionPoint> LimbPosition(int slot, int limb) = 0;
  virtual std::optional<ActionPoint> PreviewPosition(bool target) = 0;
  virtual bool Contains(const PreviewVolume& volume, ActionPoint point) = 0;
  virtual std::optional<ActionPoint> Trace(ActionPoint origin,
                                           ActionPoint target, int source) = 0;
  virtual void ResetHitState() = 0;
  virtual void Prerequisite(std::uint32_t value) = 0;
  virtual void PreviousHit(std::uint32_t value) = 0;
  virtual std::optional<std::uint32_t> QueueIndex() = 0;
  virtual std::uint32_t Check(ResolutionDefinition& definition,
                              ActionHit& target, int hit) = 0;
  virtual std::uint32_t Roll(ResolutionDefinition& definition,
                             ActionHit& source, ActionHit& target, int hit,
                             int execute) = 0;
  virtual void Effect(ResolutionDefinition& definition, ActionHit& source,
                      ActionHit& target, int execute, int hit) = 0;
  virtual void Injuries(ActionHit& target) = 0;
  virtual bool Recovery(std::uint32_t action, ResolutionActor& actor) = 0;
  virtual std::optional<std::uint32_t> Cost(std::uint32_t action,
                                            ResolutionActor* actor,
                                            int update) = 0;
  virtual bool Resolve(std::uint32_t action, ActionRequest& result,
                       ActionRequest& source, int execute);
  // Publishes queue copy-back before the cost callbacks at the Live boundary.
  virtual void Publish() {}
};

void ClearActionRequest(ActionRequest& request);
void InitializeActionHit(ActionHit& hit);
void FinalizeActionHit(ActionHit& hit);
// False refuses only a used missing owner/index or unavailable redirect origin.
// World sources remain valid for nonphysical, nonredirecting trap actions.
bool ResolveActionTargets(std::uint32_t action, ActionRequest& result,
                          ActionRequest& source, int execute,
                          ActionPreviewWorld& world);
// Fresh-request contract (#353): zero source limb and focus, not stack residue.
std::optional<std::uint32_t> PreviewActionAgainstLimb(
    std::uint32_t action, int source, int target, int limb,
    ActionRequest& result, ActionPreviewWorld& world);
std::optional<std::uint32_t> PreviewActionAgainstActor(
    std::uint32_t action, int source, int target, int limb,
    ActionRequest& result, ActionPreviewWorld& world);
}  // namespace VS::Battle
