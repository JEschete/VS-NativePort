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
#include "Actors/Location.hpp"
#include "Actors/ModelInstance.hpp"
#include "Actors/SkeletonLive.hpp"
#include "Battle/Actor.hpp"
#include "Battle/BattleModeLive.hpp"
#include "Battle/RewardFlowLive.hpp"
#include "Battle/StatusEffects.hpp"
#include "Battle/StatusLifecycleLive.hpp"
#include "Content/ActorAssetsLive.hpp"
#include "Core/Diagnostics.hpp"
#include "Core/Gte.hpp"
#include "Event/Actors.hpp"
#include "Event/ScriptActorCommands.hpp"
#include "Event/ScriptActorOpcodes.hpp"
#include "Event/ScriptActors.hpp"
#include "Event/ScriptVmLive.hpp"
#include "Event/ScriptVmLiveState.hpp"
#include "Field/MinimapSceneLive.hpp"
#include "Items/InventoryLive.hpp"
#include "Items/LootLive.hpp"
#include "Legacy/Psx.hpp"
#include "Render/Palette.hpp"
#include "State/ScriptFlagsLive.hpp"

using VS::Core::Gte::MATRIX;
using VS::Core::Gte::RotTransPers;
using VS::Core::Gte::SetRotMatrix;
using VS::Core::Gte::SetTransMatrix;
using VS::Core::Gte::SVECTOR;

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

VS::Actors::TileTarget ToLiveTile(PackedTile tile) {
  VS::Actors::TileTarget live{};
  live.tile_x = tile.x;
  live.tile_layer = tile.layer;
  live.tile_z = tile.z;
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
    const VS::Battle::Actor* present = VS::Actors::LiveActorAtSlot(actor);
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
    VS::Actors::TileTarget live = ToLiveTile(tile);
    VS::Actors::MoveLiveActorToTileAtRate(actor, &live, move_rate, turn_rate);
  }
  void MoveToPosition(int actor, const MotionVector& target, int move_rate,
                      int turn_rate) override {
    SVECTOR live = VS::Actors::LiveSvector(target);
    VS::Actors::MoveLiveActorToPositionAtRate(actor, &live, move_rate,
                                              turn_rate);
  }
  void TranslateToTile(int actor, PackedTile tile, int move_rate) override {
    VS::Actors::TileTarget live = ToLiveTile(tile);
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
    VS::Actors::TileTarget live = ToLiveTile(tile);
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
    VS::Actors::ModelEffectSlot live{.kind = slot.kind,
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
    const VS::Render::ColorCode state{
        static_cast<std::uint8_t>(style->offset.x),
        static_cast<std::uint8_t>(style->offset.y),
        static_cast<std::uint8_t>(style->offset.z), style->offset.depth_bias};
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
    std::uint32_t bone_length = 0;
    if (VS::Actors::LiveActorTaggedBoneMatrix(actor, kDialogAnchorTag, &bone,
                                              &bone_length) < 0) {
      VS::Core::Warnf("event", "Actor %d has no dialog anchor bone", actor);
      return {.anchor = anchor};
    }
    SetRotMatrix(&bone);
    SetTransMatrix(&bone);
    SVECTOR point = VS::Actors::LiveSvector(anchor);
    VS::Core::Gte::Word screen = 0;
    VS::Core::Gte::Word depth = 0;
    VS::Core::Gte::Word flags = 0;
    RotTransPers(&point, &screen, &depth, &flags);
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

intptr_t RunCommand(Handler handler, const std::uint8_t* command,
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

// decomp: vs_battle_script_1E_actorDeath (src/BATTLE/BATTLE.PRG/4DEA8.c:893;
// was func_800B7428), opcode 0x1E
std::intptr_t VS::Event::ScriptResetActorGroupStateOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::ScriptResetActorGroupState, command,
                    event::kResetActorGroupStateLength);
}

// decomp: vs_battle_script_1F_getActorTile (src/BATTLE/BATTLE.PRG/4DEA8.c:919;
// was func_800B7490), opcode 0x1F
std::intptr_t VS::Event::ScriptStoreActorTileOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::ScriptStoreActorTile, command,
                    event::kStoreActorTileLength);
}

// decomp: vs_battle_script_20_spawnActor (src/BATTLE/BATTLE.PRG/4DEA8.c:958;
// was func_800B7574), opcode 0x20
std::intptr_t VS::Event::ScriptSpawnActorOpcode(std::uint8_t* command,
                                                std::int16_t /*thread_id*/) {
  return RunCommand(event::ScriptSpawnActor, command, event::kSpawnActorLength);
}

// decomp: vs_battle_script_21_walkActorToTile
// (src/BATTLE/BATTLE.PRG/4DEA8.c:980; was func_800B75F4), opcode 0x21
std::intptr_t VS::Event::ScriptNavigateActorToTileOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::ScriptNavigateActorToTile, command,
                    event::kNavigateActorToTileLength);
}

// decomp: vs_battle_script_22_playAnimation
// (src/BATTLE/BATTLE.PRG/4DEA8.c:1011; was func_800B76A8), opcode 0x22
std::intptr_t VS::Event::ScriptPlayActorAnimationOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::ScriptPlayActorAnimation, command,
                    event::kPlayActorAnimationLength);
}

// decomp: vs_battle_script_23_setActorFlags
// (src/BATTLE/BATTLE.PRG/4DEA8.c:1032; was func_800B770C), opcode 0x23
std::intptr_t VS::Event::ScriptSetActorGroupFlagsOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::ScriptSetActorGroupFlags, command,
                    event::kSetActorGroupFlagsLength);
}

// decomp: vs_battle_script_24_setActorSfxPanVolume
// (src/BATTLE/BATTLE.PRG/4DEA8.c:1082; was func_800B77DC), opcode 0x24
std::intptr_t VS::Event::ScriptSetActorSoundMixOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::ScriptSetActorSoundMix, command,
                    event::kSetActorSoundMixLength);
}

// decomp: vs_battle_script_25_moveActorToTile
// (src/BATTLE/BATTLE.PRG/4DEA8.c:1116; was func_800B784C), opcode 0x25
std::intptr_t VS::Event::ScriptWalkActorToTileOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::ScriptWalkActorToTile, command,
                    event::kWalkActorToTileLength);
}

// decomp: vs_battle_script_26_moveActorToCoordinate
// (src/BATTLE/BATTLE.PRG/4DEA8.c:1153; was func_800B792C), opcode 0x26
std::intptr_t VS::Event::ScriptWalkActorToWorldPosOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::ScriptWalkActorToWorldPos, command,
                    event::kWalkActorToWorldPosLength);
}

// decomp: vs_battle_script_27_slideActorToTile
// (src/BATTLE/BATTLE.PRG/4DEA8.c:1196; was func_800B7A98), opcode 0x27
std::intptr_t VS::Event::ScriptSlideActorToTileOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::ScriptSlideActorToTile, command,
                    event::kSlideActorToTileLength);
}

// decomp: vs_battle_script_28_slideActorToCoordinate
// (src/BATTLE/BATTLE.PRG/4DEA8.c:1229; was func_800B7B58), opcode 0x28
std::intptr_t VS::Event::ScriptSlideActorToWorldPosOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::ScriptSlideActorToWorldPos, command,
                    event::kSlideActorToWorldPosLength);
}

// decomp: vs_battle_script_29_attachActor (src/BATTLE/BATTLE.PRG/4DEA8.c:1269;
// was func_800B7C90), opcode 0x29
std::intptr_t VS::Event::ScriptSetActorAttachmentOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::ScriptSetActorAttachment, command,
                    event::kSetActorAttachmentLength);
}

// decomp: vs_battle_script_2A_walkActorToCoordinate
// (src/BATTLE/BATTLE.PRG/4DEA8.c:1290; was func_800B7D10), opcode 0x2A
std::intptr_t VS::Event::ScriptNavigateActorToWorldPosOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::ScriptNavigateActorToWorldPos, command,
                    event::kNavigateActorToWorldPosLength);
}

// decomp: vs_battle_script_2B_turnActorToFacing
// (src/BATTLE/BATTLE.PRG/4DEA8.c:1321; was func_800B7DC4), opcode 0x2B
std::intptr_t VS::Event::ScriptTurnActorToFacingOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::ScriptTurnActorToFacing, command,
                    event::kTurnActorToFacingLength);
}

// decomp: vs_battle_script_2C_turnActorToFaceActor
// (src/BATTLE/BATTLE.PRG/4DEA8.c:1373; was func_800B7EF0), opcode 0x2C
std::intptr_t VS::Event::ScriptFaceOtherActorOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::ScriptFaceOtherActor, command,
                    event::kFaceOtherActorLength);
}

// decomp: vs_battle_script_2D_turnActorToFaceTile
// (src/BATTLE/BATTLE.PRG/4DEA8.c:1394; was func_800B7F84), opcode 0x2D
std::intptr_t VS::Event::ScriptFaceTileOpcode(std::uint8_t* command,
                                              std::int16_t /*thread_id*/) {
  return RunCommand(event::ScriptFaceTile, command, event::kFaceTileLength);
}

// decomp: vs_battle_script_2E_setActorScale
// (src/BATTLE/BATTLE.PRG/4DEA8.c:1420; was func_800B804C), opcode 0x2E
std::intptr_t VS::Event::ScriptScaleActorOpcode(std::uint8_t* command,
                                                std::int16_t /*thread_id*/) {
  return RunCommand(event::ScriptScaleActor, command, event::kScaleActorLength);
}

// decomp: vs_battle_script_2F_despawnActor (src/BATTLE/BATTLE.PRG/4DEA8.c:1436;
// was func_800B80B4), opcode 0x2F
std::intptr_t VS::Event::ScriptRemoveActorGroupOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::ScriptRemoveActorGroup, command,
                    event::kRemoveActorGroupLength);
}

// decomp: vs_battle_script_30_waitActorActivity
// (src/BATTLE/BATTLE.PRG/4DEA8.c:1461; was func_800B810C), opcode 0x30
std::intptr_t VS::Event::ScriptWaitForActorFlagsOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::ScriptWaitForActorFlags, command,
                    event::kWaitForActorFlagsLength);
}

// decomp: vs_battle_script_31_setActorLightColor
// (src/BATTLE/BATTLE.PRG/4DEA8.c:1483; was func_800B81A0), opcode 0x31
std::intptr_t VS::Event::ScriptSetActorColorOverrideOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::ScriptSetActorColorOverride, command,
                    event::kSetActorColorOverrideLength);
}

// decomp: vs_battle_script_32_queueAnimation
// (src/BATTLE/BATTLE.PRG/4DEA8.c:1508; was func_800B822C), opcode 0x32
std::intptr_t VS::Event::ScriptQueueActorAnimationOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::ScriptQueueActorAnimation, command,
                    event::kQueueActorAnimationLength);
}

// decomp: vs_battle_script_33_rotateActor (src/BATTLE/BATTLE.PRG/4DEA8.c:1527;
// was func_800B8290), opcode 0x33
std::intptr_t VS::Event::ScriptRotateActorAxesOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::ScriptRotateActorAxes, command,
                    event::kRotateActorAxesLength);
}

// decomp: vs_battle_script_34_setActorLookAngles
// (src/BATTLE/BATTLE.PRG/4DEA8.c:1556; was func_800B8360), opcode 0x34
std::intptr_t VS::Event::ScriptSetActorHeadAnglesOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::ScriptSetActorHeadAngles, command,
                    event::kSetActorHeadAnglesLength);
}

// decomp: vs_battle_script_35_playAnimationFromFrame
// (src/BATTLE/BATTLE.PRG/4DEA8.c:1586; was func_800B83D4), opcode 0x35
std::intptr_t VS::Event::ScriptPlayActorAnimationAtFrameOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::ScriptPlayActorAnimationAtFrame, command,
                    event::kPlayActorAnimationAtFrameLength);
}

// decomp: vs_battle_script_36_queueAnimationFromFrame
// (src/BATTLE/BATTLE.PRG/4DEA8.c:1598; was func_800B8438), opcode 0x36
std::intptr_t VS::Event::ScriptQueueActorAnimationAtFrameOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::ScriptQueueActorAnimationAtFrame, command,
                    event::kQueueActorAnimationAtFrameLength);
}

// decomp: vs_battle_script_37_setActorMoveAnimation
// (src/BATTLE/BATTLE.PRG/4DEA8.c:1614; was func_800B849C), opcode 0x37
std::intptr_t VS::Event::ScriptSetActorIdleAnimationOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::ScriptSetActorIdleAnimation, command,
                    event::kSetActorIdleAnimationLength);
}

// decomp: vs_battle_script_38_setActorLookTarget
// (src/BATTLE/BATTLE.PRG/4DEA8.c:1634; was func_800B8510), opcode 0x38
std::intptr_t VS::Event::ScriptLookAtActorOpcode(std::uint8_t* command,
                                                 std::int16_t /*thread_id*/) {
  return RunCommand(event::ScriptLookAtActor, command,
                    event::kLookAtActorLength);
}

// decomp: vs_battle_script_39_setActorLookPosition
// (src/BATTLE/BATTLE.PRG/4DEA8.c:1667; was func_800B8590), opcode 0x39
std::intptr_t VS::Event::ScriptLookAtWorldPosOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::ScriptLookAtWorldPos, command,
                    event::kLookAtWorldPosLength);
}

// decomp: vs_battle_script_3A_enqueueEventData
// (src/BATTLE/BATTLE.PRG/4DEA8.c:1697; was func_800B860C), opcode 0x3A
std::intptr_t VS::Event::ScriptLoadObjectModelOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::ScriptLoadObjectModel, command,
                    event::kLoadObjectModelLength);
}

// decomp: vs_battle_script_3B_waitDataQueueEmpty
// (src/BATTLE/BATTLE.PRG/4DEA8.c:1719; was func_800B86E8), opcode 0x3B
std::intptr_t VS::Event::ScriptWaitForObjectLoadOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::ScriptWaitForObjectLoad, command,
                    event::kWaitForObjectLoadLength);
}

// decomp: vs_battle_script_3C_unloadEventData
// (src/BATTLE/BATTLE.PRG/4DEA8.c:1730; was func_800B8708), opcode 0x3C
std::intptr_t VS::Event::ScriptReleaseObjectModelOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::ScriptReleaseObjectModel, command,
                    event::kReleaseObjectModelLength);
}

// decomp: vs_battle_script_3D_selectEventData
// (src/BATTLE/BATTLE.PRG/4DEA8.c:1746; was func_800B8764), opcode 0x3D
std::intptr_t VS::Event::ScriptSelectObjectTextureBankOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::ScriptSelectObjectTextureBank, command,
                    event::kSelectObjectTextureBankLength);
}

// decomp: vs_battle_script_3E_setActorEdgeHighlight
// (src/BATTLE/BATTLE.PRG/4DEA8.c:1762; was func_800B8788), opcode 0x3E
std::intptr_t VS::Event::ScriptSetActorOverlayPassOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::ScriptSetActorOverlayPass, command,
                    event::kSetActorOverlayPassLength);
}

// decomp: vs_battle_script_3F_setWeaponDrawn
// (src/BATTLE/BATTLE.PRG/4DEA8.c:1807; was func_800B884C), opcode 0x3F
std::intptr_t VS::Event::ScriptSetActorWeaponDrawnOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::ScriptSetActorWeaponDrawn, command,
                    event::kSetActorWeaponDrawnLength);
}

// decomp: vs_battle_script_54_playWeaponSfx
// (src/BATTLE/BATTLE.PRG/4DEA8.c:2210; was func_800B91F0), opcode 0x54
std::intptr_t VS::Event::ScriptPlayActorAnimationSoundOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::ScriptPlayActorAnimationSound, command,
                    event::kPlayActorAnimationSoundLength);
}

// decomp: vs_battle_script_55_setActorsAwake
// (src/BATTLE/BATTLE.PRG/4DEA8.c:2223; was func_800B9230), opcode 0x55
std::intptr_t VS::Event::ScriptSetNpcUpdatesEnabledOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::ScriptSetNpcUpdatesEnabled, command,
                    event::kSetNpcUpdatesEnabledLength);
}

// decomp: vs_battle_script_57_fullyRestoreActor
// (src/BATTLE/BATTLE.PRG/4DEA8.c:2254; was func_800B92A0), opcode 0x57
std::intptr_t VS::Event::ScriptRestoreActorVitalsOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::ScriptRestoreActorVitals, command,
                    event::kRestoreActorVitalsLength);
}

// decomp: vs_battle_script_5C_setActorWeaponModels
// (src/BATTLE/BATTLE.PRG/4DEA8.c:2425; was func_800B9608), opcode 0x5C
std::intptr_t VS::Event::ScriptSetActorEquipmentModelsOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::ScriptSetActorEquipmentModels, command,
                    event::kSetActorEquipmentModelsLength);
}

// decomp: vs_battle_script_5D_setActorSway (src/BATTLE/BATTLE.PRG/4DEA8.c:2444;
// was func_800B9670), opcode 0x5D
std::intptr_t VS::Event::ScriptSetActorYawOscillationOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::ScriptSetActorYawOscillation, command,
                    event::kSetActorYawOscillationLength);
}

// decomp: vs_battle_script_63_statusUnk (src/BATTLE/BATTLE.PRG/4DEA8.c:2547;
// was func_800B97F8), opcode 0x63
std::intptr_t VS::Event::ScriptClearAshleyStatusEffectsOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::ScriptClearAshleyStatusEffects, command,
                    event::kClearAshleyStatusEffectsLength);
}

// decomp: vs_battle_script_71 (src/BATTLE/BATTLE.PRG/4DEA8.c:2741; was
// func_800B9D74), opcode 0x71
std::intptr_t VS::Event::ScriptSetActorRenderModeOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::ScriptSetActorRenderMode, command,
                    event::kSetActorRenderModeLength);
}

// decomp: vs_battle_script_B1 (src/BATTLE/BATTLE.PRG/4DEA8.c:3199; was
// func_800BAC38), opcode 0xB1
std::intptr_t VS::Event::CommandSetEnteredDoorOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::CommandSetEnteredDoor, command,
                    event::kSetEnteredDoorLength);
}

// decomp: vs_battle_script_B2 (src/BATTLE/BATTLE.PRG/4DEA8.c:3205; was
// func_800BAC5C), opcode 0xB2
std::intptr_t VS::Event::CommandStoreActorRangeBearingOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::CommandStoreActorRangeBearing, command,
                    event::kStoreActorRangeBearingLength);
}

// decomp: vs_battle_script_B3 (src/BATTLE/BATTLE.PRG/4DEA8.c:3225; was
// func_800BAD78), opcode 0xB3
std::intptr_t VS::Event::CommandSetActorActivityOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::CommandSetActorActivity, command,
                    event::kSetActorActivityLength);
}

// decomp: vs_battle_script_B4 (src/BATTLE/BATTLE.PRG/4DEA8.c:3240; was
// func_800BADE0), opcode 0xB4
std::intptr_t VS::Event::CommandAwardMiscLootOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::CommandAwardMiscLoot, command,
                    event::kAwardMiscLootLength);
}

// decomp: vs_battle_script_B5 (src/BATTLE/BATTLE.PRG/4DEA8.c:3255; was
// func_800BAE74), opcode 0xB5
std::intptr_t VS::Event::CommandConsumeMiscItemOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::CommandConsumeMiscItem, command,
                    event::kConsumeMiscItemLength);
}

// decomp: vs_battle_script_B6 (src/BATTLE/BATTLE.PRG/4DEA8.c:3261; was
// func_800BAEA0), opcode 0xB6
std::intptr_t VS::Event::CommandStoreActorDistanceOpcode(
    std::uint8_t* command, std::int16_t /*thread_id*/) {
  return RunCommand(event::CommandStoreActorDistance, command,
                    event::kStoreActorDistanceLength);
}
