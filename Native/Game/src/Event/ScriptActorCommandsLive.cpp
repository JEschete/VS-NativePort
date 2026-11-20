// Hand-maintained (tools/port/owned.csv).
// Binds the typed event script actor commands (ScriptActorCommands.cpp) to the
// event VM's opcode slots, the actor table, the script flags, the shared
// sequences, the items and the battle state.
#include "Event/ScriptActorCommandsLive.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <optional>
#include <type_traits>

#include "Actors/ActorMotionLive.hpp"
#include "Actors/ActorSpawningLive.hpp"
#include "Actors/ActorStateLive.hpp"
#include "Actors/AnimationControlLive.hpp"
#include "Actors/AppearanceLive.hpp"
#include "Actors/Location.h"
#include "Actors/ModelInstance.h"
#include "Actors/SkeletonLive.hpp"
#include "Battle/Actor.h"
#include "Battle/BattleModeLive.hpp"
#include "Battle/RewardFlowLive.hpp"
#include "Battle/StatusEffects.h"
#include "Battle/StatusLifecycleLive.hpp"
#include "Content/ActorAssetsLive.hpp"
#include "Core/Diagnostics.hpp"
#include "Core/Gte.h"
#include "Event/Actors.hpp"
#include "Event/ScriptActorCommands.hpp"
#include "Event/ScriptActorOpcodes.hpp"
#include "Event/ScriptActors.hpp"
#include "Event/ScriptVmLive.hpp"
#include "Event/ScriptVmLiveState.hpp"
#include "Field/Doors.h"
#include "Field/MinimapSceneLive.hpp"
#include "Items/InventoryLive.hpp"
#include "Items/LootLive.hpp"
#include "Legacy/Psx.h"
#include "Render/Palette.hpp"
#include "State/ScriptFlagsLive.hpp"

namespace {
namespace event = VS::Event;
using VS::Actors::MotionVector;
using VS::Actors::PackedTile;

static_assert(std::size(event::scriptControlledActors) ==
              static_cast<std::size_t>(event::kScriptActorSlots));
static_assert(sizeof(VS::Render::ColorCode) == sizeof(VS::Render::ColorCode));

constexpr char kSkipActive = 2;
// The tagged bone the dialog anchor projects through.
constexpr int kDialogAnchorTag = 1;
constexpr unsigned kHalfBits = 16;
constexpr std::uint32_t kLowHalf = 0xFFFF;

void Require(bool valid, const char* message) {
  if (!valid) {
    VS::Core::Errorf("event", "%s", message);
    std::abort();
  }
}

const char* RefusalText(event::ScriptActorRefusal refusal) {
  switch (refusal) {
    case event::ScriptActorRefusal::kControlledSlot:
      return "a script-control mark past the actor slots";
    case event::ScriptActorRefusal::kWeaponDrawState:
      return "a weapon draw-state index past its table";
    case event::ScriptActorRefusal::kZeroDistanceScale:
      return "a range command with a zero distance scale";
    case event::ScriptActorRefusal::kSelectionCursor:
      return "a selection read past its ids";
    case event::ScriptActorRefusal::kMissingPlacement:
      return "the placement of an actor that has none";
    case event::ScriptActorRefusal::kShortCommand:
      return "an instruction shorter than its operands";
  }
  return "an unknown request";
}

Actors_TileTarget ToLiveTile(PackedTile tile) {
  Actors_TileTarget live{};
  live.tileX = tile.x;
  live.tileLayer = tile.layer;
  live.tileZ = tile.z;
  live.facing = tile.spare;
  return live;
}

VS::Render::ColorCode ToLiveColor(const VS::Render::ColorCode& color) {
  return {color.red, color.green, color.blue, color.code};
}

class LiveScriptActorHost final : public event::ScriptActorHost {
 public:
  void Refuse(event::ScriptActorRefusal refusal, int value) override {
    VS::Core::Warnf("event", "Script actor command refused %s (%d)",
                    RefusalText(refusal), value);
  }

  [[nodiscard]] int ResolveActor(std::uint16_t reference) override {
    return event::ResolveLiveScriptActorReference(reference);
  }
  [[nodiscard]] bool SkipActive() const override {
    return event::skipState == kSkipActive;
  }
  void MarkScriptControlled(int actor) override {
    Require(actor >= 0 && actor < event::kScriptActorSlots,
            "Script-controlled actor is outside its slots");
    event::scriptControlledActors[actor] = 1;
  }
  [[nodiscard]] std::optional<int> PresentActorKind(int actor) const override {
    const Battle_Actor* present = VS::Actors::LiveActorAtSlot(actor);
    if (present == nullptr) return std::nullopt;
    return present->kind;
  }
  [[nodiscard]] int FindActorByModelId(int model_id) const override {
    return VS::Actors::FindLiveActorByModelId(model_id);
  }
  [[nodiscard]] std::optional<VS::Actors::ActorPlacementView> ActorPlacement(
      int actor) const override {
    return VS::Actors::LiveActorPlacementView(actor);
  }
  void SetFlag(int flag, std::uint8_t value) override {
    VS::State::SetLiveScriptFlag(flag, value);
  }

  void InterruptAction(int actor, int animate) override {
    VS::Actors::InterruptLiveActorAction(actor, animate);
  }
  void BeginScriptedPath(int actor, const MotionVector& target) override {
    SVECTOR live = VS::Actors::LiveSvector(target);
    VS::Actors::BeginLiveScriptedActorPath(actor, &live);
  }
  [[nodiscard]] bool ScriptedPathFinished(int actor) override {
    return VS::Actors::IsLiveScriptedActorPathFinished(actor) != 0;
  }
  void ClearMovementModes(int actor) override {
    VS::Actors::ClearLiveActorMovementModes(actor);
  }
  void MoveToTile(int actor, PackedTile tile, int move_rate,
                  int turn_rate) override {
    Actors_TileTarget live = ToLiveTile(tile);
    VS::Actors::MoveLiveActorToTileAtRate(actor, &live, move_rate, turn_rate);
  }
  void MoveToPosition(int actor, const MotionVector& target, int move_rate,
                      int turn_rate) override {
    SVECTOR live = VS::Actors::LiveSvector(target);
    VS::Actors::MoveLiveActorToPositionAtRate(actor, &live, move_rate,
                                              turn_rate);
  }
  void TranslateToTile(int actor, PackedTile tile, int move_rate) override {
    Actors_TileTarget live = ToLiveTile(tile);
    VS::Actors::TranslateLiveActorToTileAtRate(actor, &live, move_rate);
  }
  void TranslateTo(int actor, const MotionVector& target,
                   int move_rate) override {
    SVECTOR live = VS::Actors::LiveSvector(target);
    VS::Actors::TranslateLiveActorToAtRate(actor, &live, move_rate);
  }
  void TurnBy(int actor, std::int16_t yaw_delta, int turn_rate) override {
    VS::Actors::TurnLiveActorByAtRate(actor, yaw_delta, turn_rate);
  }
  void TurnTo(int actor, std::int16_t yaw, int turn_rate) override {
    VS::Actors::TurnLiveActorToAtRate(actor, yaw, turn_rate);
  }
  void TurnTowardTile(int actor, PackedTile tile, int turn_rate) override {
    Actors_TileTarget live = ToLiveTile(tile);
    VS::Actors::TurnLiveActorTowardTileAtRate(actor, &live, turn_rate);
  }
  void RotateBy(int actor, const MotionVector& delta, int turn_rate) override {
    SVECTOR live = VS::Actors::LiveSvector(delta);
    VS::Actors::RotateLiveActorByAtRate(actor, &live, turn_rate);
  }
  void RotateTo(int actor, const MotionVector& rotation,
                int turn_rate) override {
    SVECTOR live = VS::Actors::LiveSvector(rotation);
    VS::Actors::RotateLiveActorToAtRate(actor, &live, turn_rate);
  }
  void ScaleTo(int actor, const MotionVector& scale, int steps) override {
    SVECTOR live = VS::Actors::LiveSvector(scale);
    VS::Actors::ScaleLiveActorToOverTime(actor, &live, steps);
  }
  void SetHeadTracking(int actor, int target, std::optional<MotionVector> aim,
                       int rate) override {
    if (!aim.has_value()) {
      VS::Actors::SetLiveActorHeadTracking(actor, target, nullptr, rate);
      return;
    }
    const SVECTOR live = VS::Actors::LiveSvector(*aim);
    VS::Actors::SetLiveActorHeadTracking(actor, target, &live, rate);
  }

  [[nodiscard]] std::uint8_t AnimationBankBase(int bank) const override {
    if (bank < 0 || static_cast<std::size_t>(bank) >=
                        std::size(event::animationBankBases)) {
      VS::Core::Warnf("event", "Script animation bank %d is past its table",
                      bank);
      return 0;
    }
    return event::animationBankBases[bank];
  }
  void PlayAnimation(int actor, event::AnimationRequest request) override {
    VS::Actors::PlayLiveActorAnimation(actor, request.animation, 0,
                                       request.blend_frames);
  }
  void PlayAnimationAtFrame(int actor,
                            event::AnimationRequest request) override {
    VS::Actors::PlayLiveActorAnimationAtFrame(
        actor, request.animation, request.blend_frames, request.start_frame);
  }
  void QueueAnimation(int actor, event::AnimationRequest request) override {
    VS::Actors::QueueLiveActorAnimation(
        actor, request.animation, request.blend_frames, request.start_frame);
  }
  void SetTravelAnimation(int actor, int animation) override {
    VS::Actors::SetLiveActorTravelAnimation(actor, animation);
  }
  void SetSoundOverrides(int actor, VS::Actors::SoundEffectSlot slot) override {
    Actors_ModelEffectSlot live{.kind = slot.kind,
                                .level = slot.level,
                                .flags = slot.flags,
                                .parameter = slot.parameter};
    VS::Actors::SetLiveActorSoundOverrides(actor, &live);
  }
  void PlayWeaponDrawSound(int actor, int sound) override {
    VS::Actors::PlayLiveActorWeaponDrawSound(actor, sound);
  }

  void ReleaseAttachmentLock(int actor) override {
    VS::Actors::ReleaseLiveActorAttachmentLock(actor, 0);
  }
  void AttachToParentBone(int actor,
                          event::BoneAttachment attachment) override {
    VS::Actors::AttachLiveActorToParentBone(actor, attachment.parent,
                                            attachment.bone, 0);
  }
  [[nodiscard]] std::uint32_t StateFlags(int actor) const override {
    return VS::Actors::LiveActorStateFlags(actor);
  }
  void SetDisplayFlags(int actor, std::uint32_t flags) override {
    VS::Actors::SetLiveActorDisplayFlags(actor, static_cast<int>(flags));
  }
  void SetDetached(int actor, bool detached) override {
    VS::Actors::SetLiveActorDetached(actor, detached ? 1 : 0);
  }
  void ResetWeaponVisibilityBlend(int actor) override {
    VS::Actors::ResetLiveActorWeaponVisibilityBlend(actor);
  }
  void SetLogicEnabled(int actor, int enabled) override {
    VS::Actors::SetLiveActorLogicEnabled(actor, enabled);
  }
  void SetOtherLogicEnabled(int enabled) override {
    VS::Actors::SetOtherLiveActorLogicEnabled(enabled);
  }

  void SetTint(int actor, std::optional<VS::Render::ColorCode> tint) override {
    if (!tint.has_value()) {
      VS::Actors::SetLiveActorTint(actor, nullptr);
      return;
    }
    const VS::Render::ColorCode live = ToLiveColor(*tint);
    VS::Actors::SetLiveActorTint(actor, &live);
  }
  void SetCompanion(int actor,
                    std::optional<VS::Actors::CompanionStyle> style) override {
    if (!style.has_value()) {
      VS::Actors::SetLiveActorCompanion(actor, 0, nullptr, nullptr);
      return;
    }
    // The companion state travels as a colour code: x, y, z and depth bias.
    const VS::Render::ColorCode state{static_cast<u_char>(style->offset.x),
                                      static_cast<u_char>(style->offset.y),
                                      static_cast<u_char>(style->offset.z),
                                      style->offset.depth_bias};
    const VS::Render::ColorCode color = ToLiveColor(style->color);
    VS::Actors::SetLiveActorCompanion(actor, 1, &state, &color);
  }
  void SetRootSpin(int actor, int rate) override {
    VS::Actors::SetLiveActorPaletteFade(actor, rate);
  }
  void StartAppearanceTransition(int actor, int mode) override {
    VS::Actors::StartLiveActorAppearanceTransition(actor, mode);
  }

  void QueueSharedSequence(int slot, int model) override {
    VS::Actors::QueueLiveSharedSequenceLoad(slot, model);
  }
  void QueueSharedSequenceWithTextures(
      event::ObjectModelRequest request) override {
    VS::Actors::QueueLiveSharedSequenceWithTextures(request.slot, request.model,
                                                    request.texture_slot);
  }
  void QueueSequenceEffectTextures(event::ObjectModelRequest request) override {
    VS::Actors::QueueLiveSequenceEffectTextures(request.slot, request.model,
                                                request.texture_slot);
  }
  void ReleaseSequenceEffectTextures(int bank) override {
    VS::Actors::ReleaseLiveSequenceEffectTextures(bank);
  }
  [[nodiscard]] int UnloadSharedSequence(int bank) override {
    return VS::Actors::UnloadLiveSharedSequence(bank);
  }
  void SetSharedSequenceBank(int bank) override {
    VS::Actors::SetLiveSharedSequenceBank(bank);
  }
  [[nodiscard]] int FindFreeAssetRequest() const override {
    return VS::Content::FindLiveFreeActorAssetRequest();
  }

  void QueueScriptedSpawn(int actor, int model,
                          VS::Actors::SpawnPlacement placement) override {
    VS::Actors::QueueLiveScriptedActorSpawn(static_cast<unsigned>(actor), model,
                                            &placement);
  }
  void RemoveScriptedActor(int actor) override {
    VS::Actors::RemoveLiveScriptedActor(actor);
  }
  void RefreshEquipmentModels(int actor) override {
    VS::Actors::RefreshLiveActorEquipmentModels(actor);
  }
  void LoadEquipmentModels(int actor, event::EquipmentModels models) override {
    VS::Actors::LoadLiveActorEquipmentModels(actor, models.blade,
                                             models.shield);
  }
  void RestoreVitals(int actor) override {
    VS::Actors::RestoreLiveActorVitals(actor);
  }
  [[nodiscard]] std::optional<int> WeaponDrawState(int index) const override {
    if (index < 0 ||
        static_cast<std::size_t>(index) >= std::size(event::weaponDrawStates)) {
      return std::nullopt;
    }
    return event::weaponDrawStates[index];
  }
  void SetBattleMode(int actor, int mode) override {
    VS::Battle::LiveSetActorBattleMode(actor, mode);
  }
  void PreserveAshleyStatuses() override {
    VS::Battle::PreserveLiveAshleyStatuses();
  }
  void MarkEnteredDoor(int door) override {
    VS::Field::MarkEntranceDoorOnSceneMapEntry(door);
  }
  [[nodiscard]] bool MenuTransitionActive() const override {
    return event::isMenuTransitionActive != 0;
  }
  void SetMenuTransitionActive(bool active) override {
    event::isMenuTransitionActive = active ? 1 : 0;
  }
  void PresentScriptedLoot(event::ScriptLoot loot) override {
    VS::Items::PresentLiveScriptedLoot(loot.first_item, loot.first_count,
                                       loot.second_item, loot.second_count);
  }
  [[nodiscard]] int UpdateRewardSequence() override {
    return VS::Battle::UpdateLiveRewardSequence();
  }
  void ConsumeMiscItem(int item) override {
    VS::Items::ConsumeLiveMiscItem(item);
  }

  // The original ignored a missing tagged bone and projected through an
  // unset matrix; here the anchor stays put and is refused.
  [[nodiscard]] event::DialogAnchorProjection ProjectDialogAnchor(
      int actor, const MotionVector& anchor) override {
    MATRIX bone{};
    u_long bone_length = 0;
    if (VS::Actors::LiveActorTaggedBoneMatrix(actor, kDialogAnchorTag, &bone,
                                              &bone_length) < 0) {
      VS::Core::Warnf("event", "Actor %d has no dialog anchor bone", actor);
      return {.anchor = anchor};
    }
    Core_Gte_SetRotMatrix(&bone);
    Core_Gte_SetTransMatrix(&bone);
    SVECTOR point = VS::Actors::LiveSvector(anchor);
    Core_GteWord screen = 0;
    Core_GteWord depth = 0;
    Core_GteWord flags = 0;
    Core_Gte_RotTransPers(&point, &screen, &depth, &flags);
    VS::Actors::ProjectLiveActorTaggedBone(actor, kDialogAnchorTag, &point,
                                           static_cast<int>(bone_length));
    const auto word = static_cast<std::uint32_t>(screen);
    return {.anchor = VS::Actors::LiveMotionVector(point),
            .screen_x = static_cast<std::int16_t>(word & kLowHalf),
            .screen_y = static_cast<std::int16_t>(word >> kHalfBits)};
  }
};

using Handler = event::ScriptStep (*)(event::ScriptCommand,
                                      event::ScriptActorHost&);

intptr_t RunCommand(Handler handler, const u_char* command,
                    std::size_t length) {
  Require(command != nullptr, "Script actor command is missing");
  LiveScriptActorHost host;
  return static_cast<intptr_t>(
      handler(event::ScriptCommand{command, length}, host));
}
}  // namespace

namespace VS::Event {
void SetLiveEventActorActivity(std::int16_t activity_enabled) {
  LiveScriptActorHost host;
  SetEventActorActivity(host, activity_enabled);
}

void BuildLiveScriptActorSelection(std::uint16_t actor_selector,
                                   ActorSelection& selection) {
  LiveScriptActorHost host;
  selection = BuildScriptActorSelection(host, actor_selector);
}

std::uint16_t NextLiveScriptSelectedActor(ActorSelection& selection) {
  LiveScriptActorHost host;
  return static_cast<std::uint16_t>(NextScriptSelectedActor(host, selection));
}

int LiveActorDialogAnchor(int actor_id, SVECTOR* dialog_anchor) {
  if (dialog_anchor == nullptr) {
    VS::Core::Warnf("event", "Actor %d dialog anchor output is missing",
                    actor_id);
    return 0;
  }
  LiveScriptActorHost host;
  Actors::MotionVector anchor = Actors::LiveMotionVector(*dialog_anchor);
  const int heading = ActorDialogAnchor(host, actor_id, anchor);
  *dialog_anchor = Actors::LiveSvector(anchor);
  return heading;
}
}  // namespace VS::Event

// decomp: func_800B7428 (src/BATTLE/BATTLE.PRG/4A0A8.c:967), opcode 0x1E
std::intptr_t VS::Event::ScriptResetActorGroupStateOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::ScriptResetActorGroupState, command,
                    event::kResetActorGroupStateLength);
}

// decomp: func_800B7490 (src/BATTLE/BATTLE.PRG/4A0A8.c:983), opcode 0x1F
std::intptr_t VS::Event::ScriptStoreActorTileOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::ScriptStoreActorTile, command,
                    event::kStoreActorTileLength);
}

// decomp: func_800B7574 (src/BATTLE/BATTLE.PRG/4A0A8.c:1009), opcode 0x20
std::intptr_t VS::Event::ScriptSpawnActorOpcode(std::uint8_t* command,
                                                std::int16_t /*thread_id*/) {
  return RunCommand(event::ScriptSpawnActor, command, event::kSpawnActorLength);
}

// decomp: func_800B75F4 (src/BATTLE/BATTLE.PRG/4A0A8.c:1022), opcode 0x21
std::intptr_t VS::Event::ScriptNavigateActorToTileOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::ScriptNavigateActorToTile, command,
                    event::kNavigateActorToTileLength);
}

// decomp: func_800B76A8 (src/BATTLE/BATTLE.PRG/4A0A8.c:1040), opcode 0x22
std::intptr_t VS::Event::ScriptPlayActorAnimationOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::ScriptPlayActorAnimation, command,
                    event::kPlayActorAnimationLength);
}

// decomp: func_800B770C (src/BATTLE/BATTLE.PRG/4A0A8.c:1047), opcode 0x23
std::intptr_t VS::Event::ScriptSetActorGroupFlagsOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::ScriptSetActorGroupFlags, command,
                    event::kSetActorGroupFlagsLength);
}

// decomp: func_800B77DC (src/BATTLE/BATTLE.PRG/4A0A8.c:1080), opcode 0x24
std::intptr_t VS::Event::ScriptSetActorSoundMixOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::ScriptSetActorSoundMix, command,
                    event::kSetActorSoundMixLength);
}

// decomp: func_800B784C (src/BATTLE/BATTLE.PRG/4A0A8.c:1097), opcode 0x25
std::intptr_t VS::Event::ScriptWalkActorToTileOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::ScriptWalkActorToTile, command,
                    event::kWalkActorToTileLength);
}

// decomp: func_800B792C (src/BATTLE/BATTLE.PRG/4A0A8.c:1116), opcode 0x26
std::intptr_t VS::Event::ScriptWalkActorToWorldPosOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::ScriptWalkActorToWorldPos, command,
                    event::kWalkActorToWorldPosLength);
}

// decomp: func_800B7A98 (src/BATTLE/BATTLE.PRG/4A0A8.c:1148), opcode 0x27
std::intptr_t VS::Event::ScriptSlideActorToTileOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::ScriptSlideActorToTile, command,
                    event::kSlideActorToTileLength);
}

// decomp: func_800B7B58 (src/BATTLE/BATTLE.PRG/4A0A8.c:1167), opcode 0x28
std::intptr_t VS::Event::ScriptSlideActorToWorldPosOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::ScriptSlideActorToWorldPos, command,
                    event::kSlideActorToWorldPosLength);
}

// decomp: func_800B7C90 (src/BATTLE/BATTLE.PRG/4A0A8.c:1200), opcode 0x29
std::intptr_t VS::Event::ScriptSetActorAttachmentOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::ScriptSetActorAttachment, command,
                    event::kSetActorAttachmentLength);
}

// decomp: func_800B7D10 (src/BATTLE/BATTLE.PRG/4A0A8.c:1212), opcode 0x2A
std::intptr_t VS::Event::ScriptNavigateActorToWorldPosOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::ScriptNavigateActorToWorldPos, command,
                    event::kNavigateActorToWorldPosLength);
}

// decomp: func_800B7DC4 (src/BATTLE/BATTLE.PRG/4A0A8.c:1230), opcode 0x2B
std::intptr_t VS::Event::ScriptTurnActorToFacingOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::ScriptTurnActorToFacing, command,
                    event::kTurnActorToFacingLength);
}

// decomp: func_800B7EF0 (src/BATTLE/BATTLE.PRG/4A0A8.c:1266), opcode 0x2C
std::intptr_t VS::Event::ScriptFaceOtherActorOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::ScriptFaceOtherActor, command,
                    event::kFaceOtherActorLength);
}

// decomp: func_800B7F84 (src/BATTLE/BATTLE.PRG/4A0A8.c:1285), opcode 0x2D
std::intptr_t VS::Event::ScriptFaceTileOpcode(std::uint8_t* command,
                                              std::int16_t /*thread_id*/) {
  return RunCommand(event::ScriptFaceTile, command, event::kFaceTileLength);
}

// decomp: func_800B804C (src/BATTLE/BATTLE.PRG/4A0A8.c:1299), opcode 0x2E
std::intptr_t VS::Event::ScriptScaleActorOpcode(std::uint8_t* command,
                                                std::int16_t /*thread_id*/) {
  return RunCommand(event::ScriptScaleActor, command, event::kScaleActorLength);
}

// decomp: func_800B80B4 (src/BATTLE/BATTLE.PRG/4A0A8.c:1309), opcode 0x2F
std::intptr_t VS::Event::ScriptRemoveActorGroupOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::ScriptRemoveActorGroup, command,
                    event::kRemoveActorGroupLength);
}

// decomp: func_800B810C (src/BATTLE/BATTLE.PRG/4A0A8.c:1324), opcode 0x30
std::intptr_t VS::Event::ScriptWaitForActorFlagsOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::ScriptWaitForActorFlags, command,
                    event::kWaitForActorFlagsLength);
}

// decomp: func_800B81A0 (src/BATTLE/BATTLE.PRG/4A0A8.c:1344), opcode 0x31
std::intptr_t VS::Event::ScriptSetActorColorOverrideOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::ScriptSetActorColorOverride, command,
                    event::kSetActorColorOverrideLength);
}

// decomp: func_800B822C (src/BATTLE/BATTLE.PRG/4A0A8.c:1360), opcode 0x32
std::intptr_t VS::Event::ScriptQueueActorAnimationOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::ScriptQueueActorAnimation, command,
                    event::kQueueActorAnimationLength);
}

// decomp: func_800B8290 (src/BATTLE/BATTLE.PRG/4A0A8.c:1367), opcode 0x33
std::intptr_t VS::Event::ScriptRotateActorAxesOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::ScriptRotateActorAxes, command,
                    event::kRotateActorAxesLength);
}

// decomp: func_800B8360 (src/BATTLE/BATTLE.PRG/4A0A8.c:1390), opcode 0x34
std::intptr_t VS::Event::ScriptSetActorHeadAnglesOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::ScriptSetActorHeadAngles, command,
                    event::kSetActorHeadAnglesLength);
}

// decomp: func_800B83D4 (src/BATTLE/BATTLE.PRG/4A0A8.c:1408), opcode 0x35
std::intptr_t VS::Event::ScriptPlayActorAnimationAtFrameOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::ScriptPlayActorAnimationAtFrame, command,
                    event::kPlayActorAnimationAtFrameLength);
}

// decomp: func_800B8438 (src/BATTLE/BATTLE.PRG/4A0A8.c:1415), opcode 0x36
std::intptr_t VS::Event::ScriptQueueActorAnimationAtFrameOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::ScriptQueueActorAnimationAtFrame, command,
                    event::kQueueActorAnimationAtFrameLength);
}

// decomp: func_800B849C (src/BATTLE/BATTLE.PRG/4A0A8.c:1422), opcode 0x37
std::intptr_t VS::Event::ScriptSetActorIdleAnimationOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::ScriptSetActorIdleAnimation, command,
                    event::kSetActorIdleAnimationLength);
}

// decomp: func_800B8510 (src/BATTLE/BATTLE.PRG/4A0A8.c:1434), opcode 0x38
std::intptr_t VS::Event::ScriptLookAtActorOpcode(std::uint8_t* command,
                                                 std::int16_t /*thread_id*/) {
  return RunCommand(event::ScriptLookAtActor, command,
                    event::kLookAtActorLength);
}

// decomp: func_800B8590 (src/BATTLE/BATTLE.PRG/4A0A8.c:1454), opcode 0x39
std::intptr_t VS::Event::ScriptLookAtWorldPosOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::ScriptLookAtWorldPos, command,
                    event::kLookAtWorldPosLength);
}

// decomp: func_800B860C (src/BATTLE/BATTLE.PRG/4A0A8.c:1471), opcode 0x3A
std::intptr_t VS::Event::ScriptLoadObjectModelOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::ScriptLoadObjectModel, command,
                    event::kLoadObjectModelLength);
}

// decomp: func_800B86E8 (src/BATTLE/BATTLE.PRG/4A0A8.c:1488), opcode 0x3B
std::intptr_t VS::Event::ScriptWaitForObjectLoadOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::ScriptWaitForObjectLoad, command,
                    event::kWaitForObjectLoadLength);
}

// decomp: func_800B8708 (src/BATTLE/BATTLE.PRG/4A0A8.c:1490), opcode 0x3C
std::intptr_t VS::Event::ScriptReleaseObjectModelOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::ScriptReleaseObjectModel, command,
                    event::kReleaseObjectModelLength);
}

// decomp: func_800B8764 (src/BATTLE/BATTLE.PRG/4A0A8.c:1500), opcode 0x3D
std::intptr_t VS::Event::ScriptSelectObjectTextureBankOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::ScriptSelectObjectTextureBank, command,
                    event::kSelectObjectTextureBankLength);
}

// decomp: func_800B8788 (src/BATTLE/BATTLE.PRG/4A0A8.c:1506), opcode 0x3E
std::intptr_t VS::Event::ScriptSetActorOverlayPassOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::ScriptSetActorOverlayPass, command,
                    event::kSetActorOverlayPassLength);
}

// decomp: func_800B884C (src/BATTLE/BATTLE.PRG/4A0A8.c:1526), opcode 0x3F
std::intptr_t VS::Event::ScriptSetActorWeaponDrawnOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::ScriptSetActorWeaponDrawn, command,
                    event::kSetActorWeaponDrawnLength);
}

// decomp: func_800B91F0 (src/BATTLE/BATTLE.PRG/4A0A8.c:1786), opcode 0x54
std::intptr_t VS::Event::ScriptPlayActorAnimationSoundOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::ScriptPlayActorAnimationSound, command,
                    event::kPlayActorAnimationSoundLength);
}

// decomp: func_800B9230 (src/BATTLE/BATTLE.PRG/4A0A8.c:1792), opcode 0x55
std::intptr_t VS::Event::ScriptSetNpcUpdatesEnabledOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::ScriptSetNpcUpdatesEnabled, command,
                    event::kSetNpcUpdatesEnabledLength);
}

// decomp: func_800B92A0 (src/BATTLE/BATTLE.PRG/4A0A8.c:1807), opcode 0x57
std::intptr_t VS::Event::ScriptRestoreActorVitalsOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::ScriptRestoreActorVitals, command,
                    event::kRestoreActorVitalsLength);
}

// decomp: func_800B9608 (src/BATTLE/BATTLE.PRG/4A0A8.c:1916), opcode 0x5C
std::intptr_t VS::Event::ScriptSetActorEquipmentModelsOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::ScriptSetActorEquipmentModels, command,
                    event::kSetActorEquipmentModelsLength);
}

// decomp: func_800B9670 (src/BATTLE/BATTLE.PRG/4A0A8.c:1928), opcode 0x5D
std::intptr_t VS::Event::ScriptSetActorYawOscillationOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::ScriptSetActorYawOscillation, command,
                    event::kSetActorYawOscillationLength);
}

// decomp: func_800B97F8 (src/BATTLE/BATTLE.PRG/4A0A8.c:1988), opcode 0x63
std::intptr_t VS::Event::ScriptClearAshleyStatusEffectsOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::ScriptClearAshleyStatusEffects, command,
                    event::kClearAshleyStatusEffectsLength);
}

// decomp: func_800B9D74 (src/BATTLE/BATTLE.PRG/4A0A8.c:2151), opcode 0x71
std::intptr_t VS::Event::ScriptSetActorRenderModeOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::ScriptSetActorRenderMode, command,
                    event::kSetActorRenderModeLength);
}

// decomp: func_800BAC38 (src/BATTLE/BATTLE.PRG/4A0A8.c:2549), opcode 0xB1
std::intptr_t VS::Event::CommandSetEnteredDoorOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::CommandSetEnteredDoor, command,
                    event::kSetEnteredDoorLength);
}

// decomp: func_800BAC5C (src/BATTLE/BATTLE.PRG/4A0A8.c:2555), opcode 0xB2
std::intptr_t VS::Event::CommandStoreActorRangeBearingOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::CommandStoreActorRangeBearing, command,
                    event::kStoreActorRangeBearingLength);
}

// decomp: func_800BAD78 (src/BATTLE/BATTLE.PRG/4A0A8.c:2575), opcode 0xB3
std::intptr_t VS::Event::CommandSetActorActivityOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::CommandSetActorActivity, command,
                    event::kSetActorActivityLength);
}

// decomp: func_800BADE0 (src/BATTLE/BATTLE.PRG/4A0A8.c:2590), opcode 0xB4
std::intptr_t VS::Event::CommandAwardMiscLootOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::CommandAwardMiscLoot, command,
                    event::kAwardMiscLootLength);
}

// decomp: func_800BAE74 (src/BATTLE/BATTLE.PRG/4A0A8.c:2606), opcode 0xB5
std::intptr_t VS::Event::CommandConsumeMiscItemOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::CommandConsumeMiscItem, command,
                    event::kConsumeMiscItemLength);
}

// decomp: func_800BAEA0 (src/BATTLE/BATTLE.PRG/4A0A8.c:2612), opcode 0xB6
std::intptr_t VS::Event::CommandStoreActorDistanceOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::CommandStoreActorDistance, command,
                    event::kStoreActorDistanceLength);
}
