#pragma once

#include <array>
#include <bit>
#include <cstdint>
#include <functional>
#include <vector>

#include "Battle/ActionPresentation.hpp"

namespace VS::test_support::presentation {
namespace b = Battle;
struct Action final : b::PresentationAction {
  b::ActionRequest data{};
  std::uint16_t Id() const override { return data.action; }
  void ClearId() override { data.action = 0; }
  b::ActionHit Primary() const override { return data.primary; }
  b::ActionHit Target(std::size_t i) const override {
    return data.targets.at(i);
  }
  std::uint16_t Count() const override { return data.count; }
  std::uint16_t Animation() const override { return data.animation; }
  std::uint8_t Variant() const override { return data.affinity; }
  b::ActionPoint Focus() const override { return data.focus; }
};
struct State {
  Action selection;
  std::array<Action, 4> queue;
  std::uint32_t index{}, step{};
  std::uint8_t preview{};
  std::uint16_t zoom{};
  std::array<int, 8> fields{};
};
struct World final : b::ActionPresentationWorld {
  std::array<State, 2> states{};
  std::size_t current{};
  std::array<bool, 256> presentation{};
  std::array<std::uint8_t, 256> effects{}, zooms{};
  std::array<std::uint8_t, 3> mappings{22, 23, 24};
  std::array<std::uint8_t, 16> defaults{}, spawns{}, materials{}, models{};
  std::array<bool, 16> eligible{};
  std::array<std::uint16_t, 10> attacks{};
  std::uint16_t heals{}, item{};
  std::uint8_t category{}, spell_commit = 1;
  std::int16_t selection_kind{};
  std::uint32_t statuses{}, frame{}, phase{}, speed{};
  int distance{};
  bool information{};
  std::vector<std::uint32_t> events;
  std::function<void(std::uint32_t)> callback;
  b::ActionPresentationRequest dispatched{};
  std::vector<b::PresentationAsset> assets;
  State& Current() { return states.at(current); }
  template <typename... T>
  void Event(std::uint32_t address, T... args) {
    events.push_back(address);
    (events.push_back(static_cast<std::uint32_t>(args)), ...);
    if (callback) callback(address);
  }
  b::PresentationAction& Selection() override { return Current().selection; }
  b::PresentationAction& Queued() override {
    return Current().queue.at(Current().index & 3U);
  }
  std::uint32_t QueueIndex() override { return Current().index; }
  std::uint8_t SelectedPreview() override { return Current().preview; }
  void BeginCommit() override {
    Current().step = 1;
    Current().index = 0;
  }
  void SetZoom(std::uint8_t zoom) override { Current().zoom = zoom; }
  void SetField(b::AbilityField field, int value) override {
    Current().fields.at(static_cast<std::size_t>(field)) = value;
  }
  std::uint8_t ChainSlot() override {
    return static_cast<std::uint8_t>(Current().fields.at(
        static_cast<std::size_t>(b::AbilityField::kChainAbilitySlot)));
  }
  std::uint8_t MappedChain(std::size_t slot) override {
    return mappings.at(slot);
  }
  bool HasPresentation(std::uint16_t action) override {
    return presentation.at(action);
  }
  std::uint8_t Effect(std::uint16_t action) override {
    return effects.at(action);
  }
  std::uint8_t Zoom(std::uint16_t action) override { return zooms.at(action); }
  std::uint32_t PlayerStatuses() override { return statuses; }
  void SetFlag(int flag) override { Event(0x800BEC14, flag, 1); }
  void Animate(b::ActionPose pose, int actor, int parameter,
               std::uint32_t step) override {
    constexpr std::array<std::uint32_t, 6> kAddresses{
        0x8009DD00, 0x8009E2E0, 0x8009F298, 0x8009EFEC, 0x8009EC9C, 0x8009EE9C};
    const auto focus = Current().selection.Focus();
    Event(kAddresses.at(static_cast<std::size_t>(pose)), actor, focus.x,
          focus.y, focus.z, focus.tag, parameter, step);
  }
  std::uint32_t WindowFrame(int actor) override {
    Event(0x8009F8DC, actor);
    return frame;
  }
  void SetTickSpeed(int value) override {
    speed = static_cast<std::uint32_t>(value);
  }
  void SetPhase(int value) override {
    phase = static_cast<std::uint32_t>(value);
  }
  void StopSound(int sound) override { Event(0x80045D64, 0x7E, sound); }
  void ReleaseNotification() override { Event(0x800CB50C); }
  void ClearEffectMode() override { Event(0x800CF920, phase); }
  void PrepareCamera() override { Event(0x8006EF10); }
  void ClearSphereTargets() override { Event(0x800C16DC); }
  void ClearPrimaryVolume() override { Event(0x800C06E0); }
  void ClearSecondaryVolume() override { Event(0x800C0738); }
  void FreeSphere() override { Event(0x800C05B4); }
  void ReleaseMenu() override { Event(0x800CB114); }
  void ResetReaction(int actor) override { Event(0x800E4C64, actor); }
  void Recover() override { Event(0x8006FBCC, 0); }
  void ReturnToMenu() override { Event(0x800704D8); }
  std::int16_t SelectionKind() override { return selection_kind; }
  std::uint16_t SelectedItem() override { return item; }
  void ConsumeItem(std::uint16_t value) override { Event(0x800CCB9C, value); }
  void ClearSelectionKind() override { selection_kind = 0; }
  void Disengage(int actor) override { Event(0x800D82A8, actor); }
  void CollectEnvironment() override { Event(0x80070CAC); }
  void CollectAll() override { Event(0x80070B04); }
  void CollectSingle() override { Event(0x800708EC); }
  void ResolveSelection() override {
    Event(0x80085B10, Current().selection.Id(), 1);
  }
  std::uint8_t Spawn(int actor) override { return spawns.at(actor); }
  std::uint8_t WeaponCategory() override { return category; }
  std::uint16_t WeaponAttacks(std::size_t cat) override {
    return attacks.at(cat);
  }
  void SetWeaponAttacks(std::size_t cat, std::uint16_t count) override {
    attacks.at(cat) = count;
  }
  int TargetDistance() override {
    Event(0x80077F70);
    return distance;
  }
  void ClearSpellCommit() override { spell_commit = 0; }
  std::uint16_t HealCount() override { return heals; }
  void SetHealCount(std::uint16_t count) override { heals = count; }
  void RestoreIdle(int actor) override { Event(0x8009E5C4, actor, phase); }
  void EnableClut() override { Event(0x80048FEC, 1); }
  void TransformColors() override { Event(0x8007B1B8, 8, 4, 0, 0, 0); }
  void FreezeAnimations() override { Event(0x8006C250); }
  void HideMarkers() override { Event(0x80095B70, 1); }
  void HideMinimap() override { Event(0x8008B4C8, 0); }
  void ResumeRoomAnimations() override { Event(0x8008B4BC, 0); }
  b::ActionPoint Placement(int actor) override {
    Event(0x800A1108, actor);
    return {11, -22, 33, 0};
  }
  void PlaySound(int sound, b::ActionPoint point) override {
    Event(0x80045AA8, 0x7E, sound, point.x, point.y, point.z);
  }
  int BoneTag(int actor, int limb) const override {
    return limb == 255 ? 0 : actor * 8 + limb;
  }
  std::uint8_t DefaultLimb(int actor) const override {
    return defaults.at(actor);
  }
  bool EligibleBystander(int actor) const override {
    return eligible.at(actor);
  }
  void Dispatch(const b::ActionPresentationRequest& request, int mode,
                std::uint8_t variant) override {
    dispatched = request;
    Event(0x800CF0E8, mode, variant);
  }
  std::uint8_t SourceMaterial(int actor) override {
    return materials.at(actor);
  }
  std::uint8_t SourceModel(int actor) override { return models.at(actor); }
  void QueueAsset(b::PresentationAsset asset) override {
    assets.push_back(asset);
    Event(0x800995E8, 7, asset.slot, asset.kind, asset.model, asset.material);
  }
  void HideHud() override { Event(0x800CB654, 1); }
  bool InformationEnabled() override { return information; }
  void ShowAction(std::uint16_t action) override {
    Event(0x800CB2B8, 0xB, action, 1);
  }
};
}  // namespace VS::test_support::presentation
