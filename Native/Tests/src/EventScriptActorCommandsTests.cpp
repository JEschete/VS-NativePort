#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <initializer_list>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Actors/ActorStateLive.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Event/ScriptActorCommands.hpp"
#include "Legacy/Psx.h"

extern "C" {
#include "Event/ScriptVm.h"
}

namespace {
namespace actors = VS::Actors;
namespace event = VS::Event;
using event::ScriptActorRefusal;
using event::ScriptStep;
using Refusal = std::pair<ScriptActorRefusal, int>;
using Calls = std::vector<std::string>;

std::string Text(const actors::MotionVector& value) {
  return std::format("({},{},{},{})", value.x, value.y, value.z, value.w);
}
std::string Text(const actors::PackedTile& value) {
  return std::format("[{},{},{},{}]", value.x, value.layer, value.z,
                     value.spare);
}
std::string Text(const event::AnimationRequest& value) {
  return std::format("a{}/b{}/f{}", value.animation, value.blend_frames,
                     value.start_frame);
}
std::string Text(const VS::Render::ColorCode& value) {
  return std::format("#{},{},{}", value.red, value.green, value.blue);
}
std::string Text(const event::ObjectModelRequest& value) {
  return std::format("s{}/m{}/t{}", value.slot, value.model,
                     value.texture_slot);
}
template <typename Value>
std::string Text(const std::optional<Value>& value) {
  return value.has_value() ? Text(*value) : std::string("none");
}

class RecordingHost final : public event::ScriptActorHost {
 public:
  Calls calls;
  std::vector<Refusal> refusals;
  std::map<std::uint16_t, int> references;
  bool skip = false;
  std::array<std::optional<int>, 32> kinds{};
  std::map<int, actors::ActorPlacementView> placements;
  std::map<int, int> models;
  std::map<int, std::uint8_t> flags;
  std::map<int, std::uint32_t> state_flags;
  std::array<std::uint8_t, 4> bank_bases{};
  std::vector<int> draw_states;
  bool path_finished = true;
  int free_request = 0;
  int unload_result = 0;
  bool menu_active = false;
  int reward_result = 1;
  event::DialogAnchorProjection projection;
  actors::MotionVector projected_from;

  template <typename... Values>
  void Log(std::string_view name, const Values&... values) {
    std::string line(name);
    ((line += std::format(" {}", values)), ...);
    calls.push_back(std::move(line));
  }

  void Refuse(ScriptActorRefusal refusal, int value) override {
    refusals.emplace_back(refusal, value);
  }
  [[nodiscard]] int ResolveActor(std::uint16_t reference) override {
    const auto found = references.find(reference);
    return found != references.end() ? found->second : reference;
  }
  [[nodiscard]] bool SkipActive() const override { return skip; }
  void MarkScriptControlled(int actor) override { Log("Mark", actor); }
  [[nodiscard]] std::optional<int> PresentActorKind(int actor) const override {
    if (actor < 0 || static_cast<std::size_t>(actor) >= kinds.size()) {
      return std::nullopt;
    }
    return kinds.at(static_cast<std::size_t>(actor));
  }
  [[nodiscard]] int FindActorByModelId(int model_id) const override {
    const auto found = models.find(model_id);
    return found != models.end() ? found->second : 1;
  }
  [[nodiscard]] std::optional<actors::ActorPlacementView> ActorPlacement(
      int actor) const override {
    const auto found = placements.find(actor);
    if (found == placements.end()) return std::nullopt;
    return found->second;
  }
  void SetFlag(int flag, std::uint8_t value) override { flags[flag] = value; }

  void InterruptAction(int actor, int animate) override {
    Log("Interrupt", actor, animate);
  }
  void BeginScriptedPath(int actor,
                         const actors::MotionVector& target) override {
    Log("Path", actor, Text(target));
  }
  [[nodiscard]] bool ScriptedPathFinished(int /*actor*/) override {
    return path_finished;
  }
  void ClearMovementModes(int actor) override { Log("Clear", actor); }
  void MoveToTile(int actor, actors::PackedTile tile, int move_rate,
                  int turn_rate) override {
    Log("MoveToTile", actor, Text(tile), move_rate, turn_rate);
  }
  void MoveToPosition(int actor, const actors::MotionVector& target,
                      int move_rate, int turn_rate) override {
    Log("MoveTo", actor, Text(target), move_rate, turn_rate);
  }
  void TranslateToTile(int actor, actors::PackedTile tile,
                       int move_rate) override {
    Log("SlideToTile", actor, Text(tile), move_rate);
  }
  void TranslateTo(int actor, const actors::MotionVector& target,
                   int move_rate) override {
    Log("SlideTo", actor, Text(target), move_rate);
  }
  void TurnBy(int actor, std::int16_t yaw_delta, int turn_rate) override {
    Log("TurnBy", actor, yaw_delta, turn_rate);
  }
  void TurnTo(int actor, std::int16_t yaw, int turn_rate) override {
    Log("TurnTo", actor, yaw, turn_rate);
  }
  void TurnTowardTile(int actor, actors::PackedTile tile,
                      int turn_rate) override {
    Log("TurnToward", actor, Text(tile), turn_rate);
  }
  void RotateBy(int actor, const actors::MotionVector& delta,
                int turn_rate) override {
    Log("RotateBy", actor, Text(delta), turn_rate);
  }
  void RotateTo(int actor, const actors::MotionVector& rotation,
                int turn_rate) override {
    Log("RotateTo", actor, Text(rotation), turn_rate);
  }
  void ScaleTo(int actor, const actors::MotionVector& scale,
               int steps) override {
    Log("Scale", actor, Text(scale), steps);
  }
  void SetHeadTracking(int actor, int target,
                       std::optional<actors::MotionVector> aim,
                       int rate) override {
    Log("Track", actor, target, Text(aim), rate);
  }

  [[nodiscard]] std::uint8_t AnimationBankBase(int bank) const override {
    return bank_bases.at(static_cast<std::size_t>(bank));
  }
  void PlayAnimation(int actor, event::AnimationRequest request) override {
    Log("Play", actor, Text(request));
  }
  void PlayAnimationAtFrame(int actor,
                            event::AnimationRequest request) override {
    Log("PlayAt", actor, Text(request));
  }
  void QueueAnimation(int actor, event::AnimationRequest request) override {
    Log("Queue", actor, Text(request));
  }
  void SetTravelAnimation(int actor, int animation) override {
    Log("Travel", actor, animation);
  }
  void SetSoundOverrides(int actor, actors::SoundEffectSlot slot) override {
    Log("Sound", actor, slot.kind, slot.level, slot.flags, slot.parameter);
  }
  void PlayWeaponDrawSound(int actor, int sound) override {
    Log("DrawSound", actor, sound);
  }

  void ReleaseAttachmentLock(int actor) override { Log("Release", actor); }
  void AttachToParentBone(int actor,
                          event::BoneAttachment attachment) override {
    Log("Attach", actor, attachment.parent, attachment.bone);
  }
  [[nodiscard]] std::uint32_t StateFlags(int actor) const override {
    const auto found = state_flags.find(actor);
    return found != state_flags.end() ? found->second : 0;
  }
  void SetDisplayFlags(int actor, std::uint32_t value) override {
    Log("Display", actor, value);
  }
  void SetDetached(int actor, bool detached) override {
    Log("Detached", actor, detached);
  }
  void ResetWeaponVisibilityBlend(int actor) override { Log("Blend", actor); }
  void SetLogicEnabled(int actor, int enabled) override {
    Log("Logic", actor, enabled);
  }
  void SetOtherLogicEnabled(int enabled) override {
    Log("OtherLogic", enabled);
  }

  void SetTint(int actor, std::optional<VS::Render::ColorCode> tint) override {
    Log("Tint", actor, Text(tint));
  }
  void SetCompanion(int actor,
                    std::optional<actors::CompanionStyle> style) override {
    if (!style.has_value()) {
      Log("Companion", actor, "none");
      return;
    }
    Log("Companion", actor, style->offset.x, style->offset.y, style->offset.z,
        style->offset.depth_bias, Text(style->color));
  }
  void SetRootSpin(int actor, int rate) override { Log("Spin", actor, rate); }
  void StartAppearanceTransition(int actor, int mode) override {
    Log("Appearance", actor, mode);
  }

  void QueueSharedSequence(int slot, int model) override {
    Log("Shared", slot, model);
  }
  void QueueSharedSequenceWithTextures(
      event::ObjectModelRequest request) override {
    Log("SharedTextures", Text(request));
  }
  void QueueSequenceEffectTextures(event::ObjectModelRequest request) override {
    Log("EffectTextures", Text(request));
  }
  void ReleaseSequenceEffectTextures(int bank) override {
    Log("ReleaseTextures", bank);
  }
  [[nodiscard]] int UnloadSharedSequence(int bank) override {
    Log("Unload", bank);
    return unload_result;
  }
  void SetSharedSequenceBank(int bank) override { Log("Bank", bank); }
  [[nodiscard]] int FindFreeAssetRequest() const override {
    return free_request;
  }

  void QueueScriptedSpawn(int actor, int model,
                          actors::SpawnPlacement placement) override {
    Log("Spawn", actor, model, placement.tile_x, placement.direction,
        placement.tile_z, placement.height);
  }
  void RemoveScriptedActor(int actor) override { Log("Remove", actor); }
  void RefreshEquipmentModels(int actor) override { Log("Refresh", actor); }
  void LoadEquipmentModels(int actor, event::EquipmentModels models) override {
    Log("Equip", actor, models.blade, models.shield);
  }
  void RestoreVitals(int actor) override { Log("Restore", actor); }
  [[nodiscard]] std::optional<int> WeaponDrawState(int index) const override {
    if (index < 0 || static_cast<std::size_t>(index) >= draw_states.size()) {
      return std::nullopt;
    }
    return draw_states.at(static_cast<std::size_t>(index));
  }
  void SetBattleMode(int actor, int mode) override {
    Log("BattleMode", actor, mode);
  }
  void PreserveAshleyStatuses() override { Log("Statuses"); }
  void MarkEnteredDoor(int door) override { Log("Door", door); }
  [[nodiscard]] bool MenuTransitionActive() const override {
    return menu_active;
  }
  void SetMenuTransitionActive(bool active) override { menu_active = active; }
  void PresentScriptedLoot(event::ScriptLoot loot) override {
    Log("Loot", loot.first_item, loot.first_count, loot.second_item,
        loot.second_count);
  }
  [[nodiscard]] int UpdateRewardSequence() override { return reward_result; }
  void ConsumeMiscItem(int item) override { Log("Consume", item); }

  [[nodiscard]] event::DialogAnchorProjection ProjectDialogAnchor(
      int actor, const actors::MotionVector& anchor) override {
    Log("Project", actor);
    projected_from = anchor;
    return projection;
  }
};

std::vector<std::uint8_t> Bytes(std::initializer_list<int> values) {
  std::vector<std::uint8_t> bytes;
  for (const int value : values) {
    bytes.push_back(static_cast<std::uint8_t>(value));
  }
  return bytes;
}

actors::ActorPlacementView Placement(actors::PackedTile tile,
                                     actors::MotionVector position) {
  return {.tile = tile, .position = position};
}

constexpr ScriptActorRefusal kSlot = ScriptActorRefusal::kControlledSlot;
}  // namespace

TEST_CASE("Script actor selections resolve one referenced actor",
          "[event][event-script-actors]") {
  RecordingHost host;
  host.references[0x1005] = 5;
  const event::ActorSelection single =
      event::BuildScriptActorSelection(host, 0x1005);
  CHECK(single.remaining == 1);
  CHECK(single.ids[0] == 5);
  CHECK(host.refusals.empty());
}

TEST_CASE("Script actor selections scan every present slot",
          "[event][event-script-actors]") {
  RecordingHost host;
  host.kinds[2] = 4;
  host.kinds[3] = 0;
  host.kinds[16] = 4;
  host.kinds[17] = 4;
  event::ActorSelection all = event::BuildScriptActorSelection(host, 0x2000);
  CHECK(all.remaining == 3);
  CHECK(event::NextScriptSelectedActor(host, all) == 2);
  CHECK(event::NextScriptSelectedActor(host, all) == 3);
  CHECK(event::NextScriptSelectedActor(host, all) == 16);
  CHECK(event::NextScriptSelectedActor(host, all) == event::kSelectionEnd);
  CHECK(host.refusals.empty());
}

TEST_CASE("Script actor selections scan enemy slots including actor 0",
          "[event][event-script-actors]") {
  RecordingHost host;
  host.kinds[0] = 4;
  host.kinds[2] = 4;
  host.kinds[3] = 0;
  host.kinds[16] = 4;
  host.kinds[17] = 4;
  const event::ActorSelection enemies =
      event::BuildScriptActorSelection(host, 0x2003);
  CHECK(enemies.remaining == 3);
  CHECK(enemies.ids[0] == 0);
  CHECK(enemies.ids[1] == 2);
  CHECK(enemies.ids[2] == 16);
  CHECK(host.refusals.empty());
}

TEST_CASE("Script actor group commands stop at actor 1 as the original did",
          "[event][event-script-actors]") {
  RecordingHost host;
  host.kinds[0] = 1;
  host.kinds[1] = 1;
  host.kinds[3] = 1;
  const auto command = Bytes({0x2F, 0x00, 0x20});
  CHECK(event::ScriptRemoveActorGroup(command, host) == ScriptStep::kAdvance);
  CHECK(host.calls == Calls{"Remove 0"});
}

TEST_CASE("Script actor selections refuse a cursor past their capacity",
          "[event][event-script-actors]") {
  RecordingHost host;
  event::ActorSelection selection;
  selection.remaining = 1;
  selection.cursor = static_cast<std::uint8_t>(event::kSelectionCapacity);
  CHECK(event::NextScriptSelectedActor(host, selection) ==
        event::kSelectionEnd);
  CHECK(selection.remaining == 0);
  CHECK(host.refusals ==
        std::vector<Refusal>{{ScriptActorRefusal::kSelectionCursor,
                              static_cast<int>(event::kSelectionCapacity)}});
}

TEST_CASE("Script actor activity toggles every present actor but the player",
          "[event][event-script-actors]") {
  const int enabled = GENERATE(0, 1);
  RecordingHost host;
  host.kinds[0] = 1;
  host.kinds[4] = 1;
  host.state_flags[4] = 0x80;
  event::SetEventActorActivity(host, enabled);
  CHECK(host.calls ==
        Calls{std::format("Display 4 {}", enabled != 0 ? 0x81 : 0x80),
              std::format("OtherLogic {}", enabled)});
}

TEST_CASE("Script actor dialog anchors project above the tagged bone",
          "[event][event-script-actors]") {
  RecordingHost host;
  host.projection.anchor = {.x = 10, .y = 20, .z = 0, .w = 7};
  host.projection.screen_x = 13;
  host.projection.screen_y = 24;
  actors::MotionVector anchor{.x = 9, .y = 9, .z = 9, .w = 3};
  const int heading = event::ActorDialogAnchor(host, 6, anchor);
  CHECK(host.projected_from ==
        actors::MotionVector{.x = 0, .y = -1000, .z = 0, .w = 3});
  CHECK(heading == VS::Core::Math::Arctangent2(4, 3));
  CHECK(anchor ==
        actors::MotionVector{
            .x = 10, .y = 20, .z = static_cast<std::int16_t>(heading), .w = 7});
}

TEST_CASE("Script actor tile stores write the tile or the no-tile marker",
          "[event][event-script-actors]") {
  RecordingHost host;
  host.kinds[3] = 1;
  host.placements[3] = Placement({.x = 4, .layer = 1, .z = 9}, {});
  host.models[0x55] = 3;
  CHECK(event::ScriptStoreActorTile(Bytes({0x1F, 0x03, 0x10, 0x20, 0x00}),
                                    host) == ScriptStep::kAdvance);
  CHECK(event::ScriptStoreActorTile(Bytes({0x1F, 0x55, 0x00, 0x30, 0x00}),
                                    host) == ScriptStep::kAdvance);
  CHECK(event::ScriptStoreActorTile(Bytes({0x1F, 0x04, 0x10, 0x40, 0x00}),
                                    host) == ScriptStep::kAdvance);
  CHECK(event::ScriptStoreActorTile(Bytes({0x1F, 0x66, 0x00, 0x50, 0x00}),
                                    host) == ScriptStep::kAdvance);
  CHECK(host.flags == std::map<int, std::uint8_t>{{0x20, 4},
                                                  {0x21, 1},
                                                  {0x22, 9},
                                                  {0x30, 4},
                                                  {0x31, 1},
                                                  {0x32, 9},
                                                  {0x40, 0xFF},
                                                  {0x50, 0xFF}});
  CHECK(host.refusals.empty());
}

TEST_CASE("Script actor commands refuse an absent actor's placement",
          "[event][event-script-actors]") {
  RecordingHost host;
  host.kinds[3] = 1;
  CHECK(event::ScriptStoreActorTile(Bytes({0x1F, 0x03, 0x10, 0x20, 0x00}),
                                    host) == ScriptStep::kAdvance);
  CHECK(host.flags ==
        std::map<int, std::uint8_t>{{0x20, 0}, {0x21, 0}, {0x22, 0}});
  CHECK(host.refusals ==
        std::vector<Refusal>{{ScriptActorRefusal::kMissingPlacement, 3}});
}

TEST_CASE("Script actor commands refuse operands past a short instruction",
          "[event][event-script-actors]") {
  RecordingHost host;
  CHECK(event::ScriptSpawnActor(Bytes({0x20, 0x07, 0x00, 0x22}), host) ==
        ScriptStep::kAdvance);
  CHECK(host.calls == Calls{"Spawn 7 34 0 0 0 0"});
  CHECK(host.refusals ==
        std::vector<Refusal>{{ScriptActorRefusal::kShortCommand, 4}});
}

TEST_CASE("Script actor spawns pass the model and placement",
          "[event][event-script-actors]") {
  RecordingHost host;
  host.references[0x1004] = 4;
  CHECK(event::ScriptSpawnActor(
            Bytes({0x20, 0x04, 0x10, 0x34, 0x12, 5, 2, 7, 1}), host) ==
        ScriptStep::kAdvance);
  CHECK(host.calls == Calls{"Spawn 4 4660 5 2 7 1"});
}

TEST_CASE(
    "Script actor navigation paths to a tile centre or jumps when skipped",
    "[event][event-script-actors]") {
  const bool skip = GENERATE(false, true);
  RecordingHost host;
  host.skip = skip;
  CHECK(event::ScriptNavigateActorToTile(
            Bytes({0x21, 0x05, 0x00, 0x02, 0x63, 0x03}), host) ==
        ScriptStep::kAdvance);
  const std::string target = "(320,0,448,-32768)";
  CHECK(host.calls ==
        Calls{skip ? "SlideTo 5 " + target + " 0" : "Path 5 " + target,
              "Clear 5", "Mark 5"});
}

TEST_CASE("Script actor moves refuse controlled marks past the marked slots",
          "[event][event-script-actors]") {
  const int actor = GENERATE(-1, 17, 0x2000);
  RecordingHost host;
  host.references[0x1009] = actor;
  CHECK(event::ScriptNavigateActorToWorldPos(
            Bytes({0x2A, 0x09, 0x10, 0x10, 0x00, 0x20, 0x00}), host) ==
        ScriptStep::kAdvance);
  CHECK(host.calls == Calls{std::format("Path {} (16,0,32,-32768)", actor),
                            std::format("Clear {}", actor)});
  CHECK(host.refusals == std::vector<Refusal>{{kSlot, actor}});
}

TEST_CASE("Script actor walks keep own rates and go instantly when skipped",
          "[event][event-script-actors]") {
  const bool skip = GENERATE(false, true);
  RecordingHost host;
  host.skip = skip;
  CHECK(event::ScriptWalkActorToTile(
            Bytes({0x25, 0x02, 0x00, 4, 1, 6, 9, 0xFF, 12}), host) ==
        ScriptStep::kAdvance);
  CHECK(host.calls == Calls{skip ? "MoveToTile 2 [4,1,6,9] 0 0"
                                 : "MoveToTile 2 [4,1,6,9] -1 12",
                            "Clear 2", "Mark 2"});
}

TEST_CASE("Script actor world walks add relative targets to the actor",
          "[event][event-script-actors]") {
  RecordingHost host;
  host.placements[2] = Placement({}, {.x = 100, .y = -5, .z = 300});
  // x = 21 with the relative bit, y = 10, z = -20, facing 0x10.
  CHECK(event::ScriptWalkActorToWorldPos(
            Bytes({0x26, 0x02, 0x00, 0x2B, 0x00, 0x0A, 0x00, 0xEC, 0xFF, 0x10,
                   3, 0xFF}),
            host) == ScriptStep::kAdvance);
  CHECK(host.calls ==
        Calls{"MoveTo 2 (121,5,280,256) 3 -1", "Clear 2", "Mark 2"});

  host.calls.clear();
  CHECK(event::ScriptWalkActorToWorldPos(
            Bytes({0x26, 0x02, 0x00, 0x2A, 0x00, 0x0A, 0x00, 0xEC, 0xFF, 0xFF,
                   3, 4}),
            host) == ScriptStep::kAdvance);
  CHECK(host.calls ==
        Calls{"MoveTo 2 (21,10,-20,-32768) 3 4", "Clear 2", "Mark 2"});
}

TEST_CASE("Script actor slides keep facing and take a spare tile byte",
          "[event][event-script-actors]") {
  RecordingHost host;
  CHECK(event::ScriptSlideActorToTile(Bytes({0x27, 0x03, 0x00, 4, 1, 6, 8}),
                                      host) == ScriptStep::kAdvance);
  CHECK(event::ScriptSlideActorToWorldPos(
            Bytes({0x28, 0x03, 0x00, 0x08, 0x00, 0x02, 0x00, 0x04, 0x00, 0xFF}),
            host) == ScriptStep::kAdvance);
  CHECK(host.calls == Calls{"SlideToTile 3 [4,1,6,255] 8", "Clear 3", "Mark 3",
                            "SlideTo 3 (4,2,4,-32768) -1", "Clear 3",
                            "Mark 3"});
}

TEST_CASE("Script actor turns pick the requested direction",
          "[event][event-script-actors]") {
  RecordingHost host;
  host.placements[2] = Placement({}, {.w = 3000});
  CHECK(event::ScriptTurnActorToFacing(Bytes({0x2B, 0x02, 0x00, 0x40, 0, 7}),
                                       host) == ScriptStep::kAdvance);
  CHECK(event::ScriptTurnActorToFacing(
            Bytes({0x2B, 0x02, 0x00, 0x00, 0x01, 0xFF}), host) ==
        ScriptStep::kAdvance);
  CHECK(event::ScriptTurnActorToFacing(
            Bytes({0x2B, 0x02, 0x00, 0xF0, 0xFF, 0xFF}), host) ==
        ScriptStep::kAdvance);
  // 0 - 3000 wraps to 1096; 3840 - 3000 is 840, then forced negative.
  CHECK(host.calls == Calls{"TurnTo 2 1024 7", "Mark 2", "TurnBy 2 1096 -1",
                            "Mark 2", "TurnBy 2 -3256 -1", "Mark 2"});
}

TEST_CASE("Script actor facing commands turn toward tiles and other actors",
          "[event][event-script-actors]") {
  RecordingHost host;
  host.references[0x1007] = 7;
  host.placements[7] = Placement({.x = 3, .layer = 2, .z = 5, .spare = 9}, {});
  CHECK(event::ScriptFaceOtherActor(Bytes({0x2C, 0x02, 0x00, 0x07, 0x10, 0x04}),
                                    host) == ScriptStep::kAdvance);
  CHECK(event::ScriptFaceTile(Bytes({0x2D, 0x02, 0x00, 8, 1, 6, 0xFF}), host) ==
        ScriptStep::kAdvance);
  CHECK(host.calls == Calls{"TurnToward 2 [3,2,5,9] 4", "Mark 2",
                            "TurnToward 2 [8,1,6,0] -1", "Mark 2"});
}

TEST_CASE("Script actor rotations are relative or absolute",
          "[event][event-script-actors]") {
  const bool relative = GENERATE(false, true);
  RecordingHost host;
  CHECK(event::ScriptRotateActorAxes(
            Bytes({0x33, 0x02, 0x00, relative ? 1 : 0, 0x01, 0x00, 0x02, 0x00,
                   0xFF, 0xFF, 5}),
            host) == ScriptStep::kAdvance);
  CHECK(host.calls == Calls{std::format("{} 2 (1,2,-1,0) 5",
                                        relative ? "RotateBy" : "RotateTo"),
                            "Mark 2"});
}

TEST_CASE("Script actor animations resolve banks and blend frames",
          "[event][event-script-actors]") {
  RecordingHost host;
  host.bank_bases = {0, 10, 20, 30};
  CHECK(event::ScriptPlayActorAnimation(Bytes({0x22, 2, 0, 3, 0x9, 0x45}),
                                        host) == ScriptStep::kAdvance);
  CHECK(event::ScriptQueueActorAnimation(Bytes({0x32, 2, 0, 4, 0, 0xC2}),
                                         host) == ScriptStep::kAdvance);
  CHECK(event::ScriptPlayActorAnimationAtFrame(
            Bytes({0x35, 2, 0, 1, 6, 0, 0x81}), host) == ScriptStep::kAdvance);
  CHECK(event::ScriptQueueActorAnimationAtFrame(
            Bytes({0x36, 2, 0, 1, 6, 0, 0x01}), host) == ScriptStep::kAdvance);
  CHECK(event::ScriptSetActorIdleAnimation(Bytes({0x37, 2, 0, 2, 5}), host) ==
        ScriptStep::kAdvance);
  CHECK(event::ScriptSetActorIdleAnimation(Bytes({0x37, 2, 0, 2, 0xFF}),
                                           host) == ScriptStep::kAdvance);
  CHECK(host.calls == Calls{"Play 2 a13/b5/f0", "Queue 2 a34/b2/f0",
                            "PlayAt 2 a21/b1/f6", "Queue 2 a1/b1/f6",
                            "Travel 2 25", "Travel 2 -1"});
}

TEST_CASE("Script actor group flags keep or set each equipment bit",
          "[event][event-script-actors]") {
  RecordingHost host;
  host.references[0x1003] = 3;
  host.state_flags[3] = 0x1800;
  // Keep the first, show neither, detach: 0x800 | 0x40.
  CHECK(event::ScriptSetActorGroupFlags(Bytes({0x23, 0x03, 0x10, 0x40, 0x22}),
                                        host) == ScriptStep::kAdvance);
  // Show the first, keep the second: 0x800 | 0x1000 | 0x01.
  CHECK(event::ScriptSetActorGroupFlags(Bytes({0x23, 0x03, 0x10, 0x01, 0x09}),
                                        host) == ScriptStep::kAdvance);
  CHECK(host.calls == Calls{"Detached 3 true",
                            std::format("Display 3 {}", 0x840), "Blend 3",
                            "Detached 3 false",
                            std::format("Display 3 {}", 0x1801), "Blend 3"});
}

TEST_CASE("Script actor flag waits hold until state and path settle",
          "[event][event-script-actors]") {
  RecordingHost host;
  host.state_flags[2] = 0x81;
  const auto wait = Bytes({0x30, 0x02, 0x00, 0x81, 0x00});
  CHECK(event::ScriptWaitForActorFlags(wait, host) == ScriptStep::kAdvance);
  host.path_finished = false;
  CHECK(event::ScriptWaitForActorFlags(wait, host) == ScriptStep::kHold);
  host.path_finished = true;
  host.state_flags[2] = 0x80;
  CHECK(event::ScriptWaitForActorFlags(wait, host) == ScriptStep::kHold);
  CHECK(event::ScriptWaitForActorFlags(Bytes({0x30, 0x02, 0x00, 0x80, 0x00}),
                                       host) == ScriptStep::kAdvance);
}

TEST_CASE("Script actor tints and overlays clear on black",
          "[event][event-script-actors]") {
  RecordingHost host;
  CHECK(event::ScriptSetActorColorOverride(Bytes({0x31, 2, 0, 0, 0, 0}),
                                           host) == ScriptStep::kAdvance);
  CHECK(event::ScriptSetActorColorOverride(Bytes({0x31, 2, 0, 1, 2, 3}),
                                           host) == ScriptStep::kAdvance);
  CHECK(event::ScriptSetActorOverlayPass(
            Bytes({0x3E, 2, 0, 0xFE, 2, 3, 4, 0, 0, 0}), host) ==
        ScriptStep::kAdvance);
  CHECK(event::ScriptSetActorOverlayPass(
            Bytes({0x3E, 2, 0, 0xFE, 2, 3, 4, 9, 8, 7}), host) ==
        ScriptStep::kAdvance);
  CHECK(host.calls == Calls{"Tint 2 none", "Tint 2 #1,2,3", "Companion 2 none",
                            "Companion 2 -2 2 3 4 #9,8,7"});
}

TEST_CASE("Script actor head tracking follows actors, points and angles",
          "[event][event-script-actors]") {
  RecordingHost host;
  host.references[0x2000] = 0x2000;
  CHECK(event::ScriptLookAtActor(Bytes({0x38, 2, 0, 5, 0, 0}), host) ==
        ScriptStep::kAdvance);
  CHECK(event::ScriptLookAtActor(Bytes({0x38, 2, 0, 0x00, 0x20, 9}), host) ==
        ScriptStep::kAdvance);
  CHECK(event::ScriptLookAtWorldPos(Bytes({0x39, 2, 0, 1, 0, 2, 0, 3, 0, 4}),
                                    host) == ScriptStep::kAdvance);
  CHECK(event::ScriptSetActorHeadAngles(Bytes({0x34, 2, 0, 1, 0, 2, 0, 0}),
                                        host) == ScriptStep::kAdvance);
  CHECK(host.calls == Calls{"Track 2 5 none 4096", "Track 2 -2 none 9",
                            "Track 2 -1 (1,2,3,0) 4",
                            "Track 2 -4 (1,2,0,0) 4096"});
}

TEST_CASE("Script actor attachments release on non-actor parents",
          "[event][event-script-actors]") {
  RecordingHost host;
  CHECK(event::ScriptSetActorAttachment(Bytes({0x29, 2, 0, 5, 0, 7}), host) ==
        ScriptStep::kAdvance);
  CHECK(event::ScriptSetActorAttachment(Bytes({0x29, 2, 0, 0, 0x20, 7}),
                                        host) == ScriptStep::kAdvance);
  CHECK(host.calls == Calls{"Attach 2 5 7", "Release 2"});
}

TEST_CASE("Script actor sound mixes address every actor on reference 0x2001",
          "[event][event-script-actors]") {
  RecordingHost host;
  CHECK(event::ScriptSetActorSoundMix(Bytes({0x24, 0x01, 0x20, 1, 2, 3, 4}),
                                      host) == ScriptStep::kAdvance);
  CHECK(event::ScriptSetActorSoundMix(Bytes({0x24, 0x02, 0x00, 5, 6, 7, 8}),
                                      host) == ScriptStep::kAdvance);
  CHECK(host.calls == Calls{"Sound -1 1 2 3 4", "Sound 2 5 6 7 8"});
}

TEST_CASE("Script object models load by mode and release by bank",
          "[event][event-script-actors]") {
  RecordingHost host;
  CHECK(event::ScriptLoadObjectModel(Bytes({0x3A, 0x34, 0x12, 0x31}), host) ==
        ScriptStep::kAdvance);
  CHECK(event::ScriptLoadObjectModel(Bytes({0x3A, 0x05, 0x00, 0x26}), host) ==
        ScriptStep::kAdvance);
  CHECK(event::ScriptLoadObjectModel(Bytes({0x3A, 0x05, 0x00, 0x2A}), host) ==
        ScriptStep::kAdvance);
  CHECK(event::ScriptLoadObjectModel(Bytes({0x3A, 0x05, 0x00, 0x0C}), host) ==
        ScriptStep::kAdvance);
  CHECK(event::ScriptSelectObjectTextureBank(Bytes({0x3D, 3}), host) ==
        ScriptStep::kAdvance);
  CHECK(host.calls == Calls{"Shared 1 4660", "SharedTextures s2/m5/t28",
                            "EffectTextures s2/m5/t28", "Bank 3"});
}

TEST_CASE("Script object model waits and releases hold or yield",
          "[event][event-script-actors]") {
  RecordingHost host;
  host.free_request = 2;
  CHECK(event::ScriptWaitForObjectLoad(Bytes({0x3B}), host) ==
        ScriptStep::kHold);
  host.free_request = 0;
  CHECK(event::ScriptWaitForObjectLoad(Bytes({0x3B}), host) ==
        ScriptStep::kAdvance);
  host.free_request = -1;
  CHECK(event::ScriptWaitForObjectLoad(Bytes({0x3B}), host) ==
        ScriptStep::kAdvance);

  host.unload_result = -1;
  CHECK(event::ScriptReleaseObjectModel(Bytes({0x3C, 1}), host) ==
        ScriptStep::kHold);
  host.unload_result = 0;
  CHECK(event::ScriptReleaseObjectModel(Bytes({0x3C, 1}), host) ==
        ScriptStep::kAdvance);
  host.skip = true;
  CHECK(event::ScriptReleaseObjectModel(Bytes({0x3C, 6}), host) ==
        ScriptStep::kAdvanceAndYield);
  CHECK(host.calls == Calls{"Unload 1", "Unload 1", "ReleaseTextures 2"});
}

TEST_CASE("Script weapon draw states refuse indexes past their table",
          "[event][event-script-actors]") {
  RecordingHost host;
  host.references[0x1002] = 2;
  host.draw_states = {0, 1, 0};
  CHECK(event::ScriptSetActorWeaponDrawn(Bytes({0x3F, 0x02, 0x10, 1}), host) ==
        ScriptStep::kAdvance);
  CHECK(event::ScriptSetActorWeaponDrawn(Bytes({0x3F, 0x02, 0x10, 3}), host) ==
        ScriptStep::kAdvance);
  CHECK(event::ScriptSetActorWeaponDrawn(Bytes({0x3F, 0x02, 0x10, 0xFF}),
                                         host) == ScriptStep::kAdvance);
  CHECK(host.calls == Calls{"BattleMode 2 1"});
  CHECK(host.refusals ==
        std::vector<Refusal>{{ScriptActorRefusal::kWeaponDrawState, 3},
                             {ScriptActorRefusal::kWeaponDrawState, 0xFF}});
}

TEST_CASE("Script actor equipment, vitals and appearance commands",
          "[event][event-script-actors]") {
  RecordingHost host;
  CHECK(event::ScriptSetActorEquipmentModels(Bytes({0x5C, 2, 0, 4, 9, 5}),
                                             host) == ScriptStep::kAdvance);
  CHECK(event::ScriptSetActorEquipmentModels(Bytes({0x5C, 2, 0, 0xFE, 9, 5}),
                                             host) == ScriptStep::kAdvance);
  CHECK(event::ScriptRestoreActorVitals(Bytes({0x57, 2, 0}), host) ==
        ScriptStep::kAdvance);
  CHECK(event::ScriptSetActorYawOscillation(Bytes({0x5D, 2, 0, 6}), host) ==
        ScriptStep::kAdvance);
  CHECK(event::ScriptSetActorRenderMode(Bytes({0x71, 2, 0, 3}), host) ==
        ScriptStep::kAdvance);
  CHECK(event::ScriptScaleActor(
            Bytes({0x2E, 2, 0, 0, 0x10, 0, 0x10, 0, 0x08, 4}), host) ==
        ScriptStep::kAdvance);
  CHECK(event::ScriptPlayActorAnimationSound(Bytes({0x54, 2, 0, 1}), host) ==
        ScriptStep::kAdvance);
  CHECK(host.calls == Calls{"Equip 2 4 5", "Refresh 2", "Restore 2", "Spin 2 6",
                            "Appearance 2 3", "Scale 2 (4096,4096,2048,0) 4",
                            "DrawSound 2 1"});
}

TEST_CASE("Script actor state and logic commands",
          "[event][event-script-actors]") {
  RecordingHost host;
  host.references[0x1003] = 3;
  CHECK(event::ScriptResetActorGroupState(Bytes({0x1E, 0x03, 0x10, 1}), host) ==
        ScriptStep::kAdvance);
  CHECK(event::ScriptSetNpcUpdatesEnabled(Bytes({0x55, 0}), host) ==
        ScriptStep::kAdvance);
  CHECK(event::CommandSetActorActivity(Bytes({0xB3, 0x03, 0x10, 1}), host) ==
        ScriptStep::kAdvance);
  CHECK(event::ScriptClearAshleyStatusEffects(Bytes({0x63}), host) ==
        ScriptStep::kAdvance);
  CHECK(event::CommandSetEnteredDoor(Bytes({0xB1, 4}), host) ==
        ScriptStep::kAdvance);
  CHECK(event::CommandConsumeMiscItem(Bytes({0xB5, 0x2C, 0x01}), host) ==
        ScriptStep::kAdvance);
  CHECK(host.calls == Calls{"Interrupt 3 1", "OtherLogic 0", "Logic 3 1",
                            "Statuses", "Door 4", "Consume 300"});
}

TEST_CASE("Script range bearings refuse a zero scale and saturate the range",
          "[event][event-script-actors]") {
  const auto delta =
      GENERATE(std::pair<int, int>{300, 400}, std::pair<int, int>{-300, -400},
               std::pair<int, int>{-400, 300});
  const int scale = GENERATE(0, 1, 2, 3);
  RecordingHost host;
  host.references[0x1001] = 1;
  host.references[0x1002] = 2;
  host.placements[1] =
      Placement({}, {.x = static_cast<std::int16_t>(delta.first + 10),
                     .z = static_cast<std::int16_t>(delta.second + 20)});
  host.placements[2] = Placement({}, {.x = 10, .z = 20});
  CHECK(event::CommandStoreActorRangeBearing(
            Bytes({0xB2, 0x01, 0x10, 0x02, 0x10, scale, 0x40, 0x00}), host) ==
        ScriptStep::kAdvance);
  // Every delta is the same length; one unit of scale passes 0x100.
  const int distance = VS::Core::Math::SquareRootInteger(
      (delta.first * delta.first) + (delta.second * delta.second));
  const int range =
      scale == 0 || distance / scale >= 0x100 ? 0xFF : distance / scale;
  const int bearing =
      (VS::Core::Math::Arctangent2(delta.first, delta.second) >> 4) & 0xFF;
  CHECK(static_cast<int>(host.flags.at(0x40)) == range);
  CHECK(static_cast<int>(host.flags.at(0x41)) == bearing);
  CHECK(host.flags.size() == 2);
  if (scale == 0) {
    CHECK(host.refusals ==
          std::vector<Refusal>{{ScriptActorRefusal::kZeroDistanceScale, 0x40}});
  } else {
    CHECK(host.refusals.empty());
  }
}

TEST_CASE("Script range bearings saturate at exactly 0x100",
          "[event][event-script-actors]") {
  const int length = GENERATE(400, 460, 506, 508, 512, 514, 600);
  RecordingHost host;
  host.references[0x1001] = 1;
  host.placements[0] = Placement({}, {});
  host.placements[1] = Placement({}, {.x = static_cast<std::int16_t>(length)});
  CHECK(event::CommandStoreActorRangeBearing(
            Bytes({0xB2, 0x01, 0x10, 0x00, 0x00, 2, 0x40, 0x00}), host) ==
        ScriptStep::kAdvance);
  const int scaled = VS::Core::Math::SquareRootInteger(length * length) / 2;
  CHECK(static_cast<int>(host.flags.at(0x40)) ==
        (scaled < 0x100 ? scaled : 0xFF));
  if (length <= 460) CHECK(host.flags.at(0x40) < 0xFF);
  if (length >= 600) CHECK(host.flags.at(0x40) == 0xFF);
  CHECK(host.refusals.empty());
}

TEST_CASE("Script actor distances store 128-unit steps",
          "[event][event-script-actors]") {
  RecordingHost host;
  host.references[0x1001] = 1;
  host.references[0x1002] = 2;
  host.placements[1] = Placement({}, {.x = 310, .z = 420});
  host.placements[2] = Placement({}, {.x = 10, .z = 20});
  CHECK(event::CommandStoreActorDistance(
            Bytes({0xB6, 0x01, 0x10, 0x02, 0x10, 0x50, 0x00}), host) ==
        ScriptStep::kAdvance);
  CHECK(host.flags == std::map<int, std::uint8_t>{{0x50, 3}});
}

TEST_CASE("Script loot presents once, holds, then yields when dismissed",
          "[event][event-script-actors]") {
  RecordingHost host;
  const auto command = Bytes({0xB4, 0x2C, 0x01, 2, 0x2D, 0x01, 1});
  CHECK(event::CommandAwardMiscLoot(command, host) == ScriptStep::kHold);
  CHECK(host.menu_active);
  CHECK(event::CommandAwardMiscLoot(command, host) == ScriptStep::kHold);
  host.reward_result = 0;
  CHECK(event::CommandAwardMiscLoot(command, host) ==
        ScriptStep::kAdvanceAndYield);
  CHECK_FALSE(host.menu_active);
  CHECK(host.calls == Calls{"Loot 300 2 301 1"});
}

TEST_CASE("Event script actor references resolve every selector nibble",
          "[event][event-script-actors][live]") {
  CHECK(Event_ResolveScriptActorReference(0x0123) ==
        VS::Actors::FindLiveActorByModelId(0x123));
  CHECK(Event_ResolveScriptActorReference(0x1025) == 5);
  CHECK(Event_ResolveScriptActorReference(0x2003) == 0x2003);
  // Selectors 3 and up fall through unhandled and return the reference (#303).
  CHECK(Event_ResolveScriptActorReference(0x3005) == 0x3005);
  CHECK(Event_ResolveScriptActorReference(0xF00F) == 0xF00F);
}
