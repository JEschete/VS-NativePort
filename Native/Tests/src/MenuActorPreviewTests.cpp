// Hand-maintained tests for the typed menu actor preview (#276).
#include <array>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Actors/ActorStateRecord.hpp"
#include "Content/ActorAssets.hpp"
#include "Content/ModuleData.h"
#include "Legacy/Psx.h"
#include "Menu/Framework/ActorPreview.hpp"
#include "Menu/Framework/ActorPreviewLive.hpp"
#include "Render/DepthCue.hpp"
#include "TestSupport/ChannelWarnings.hpp"

extern "C" {
#include "Actors/ActorRecord.h"
#include "Actors/Movement.h"
}

namespace {
namespace fw = VS::Menu::Framework;
using VS::Content::ActorAssetRequest;
using VS::Content::AssetAddress;
using VS::Content::SharedAssetAddress;

constexpr std::uintptr_t kFirstBlock = 0x1000;

// "name(a,b)" in decimal, the form the fake logs its calls in.
std::string Call(std::string_view name,
                 std::initializer_list<std::int64_t> args) {
  std::string text(name);
  text += '(';
  bool first = true;
  for (const std::int64_t arg : args) {
    if (!first) {
      text += ',';
    }
    text += std::to_string(arg);
    first = false;
  }
  return text + ')';
}

// AssetAddress is an opaque address handle; any address is valid.
constexpr AssetAddress Block(std::uintptr_t value) {
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  return static_cast<AssetAddress>(value);
}

std::int64_t Value(AssetAddress address) {
  return static_cast<std::int64_t>(address);
}

std::string Offset(std::string_view name, int slot,
                   const VS::Actors::CompanionOffset& offset, int last) {
  return Call(name,
              {slot, offset.x, offset.y, offset.z, offset.depth_bias, last});
}

// The actor and weapon records the preview reads; every host call lands in
// the fake's log, every request and refusal in its records.
class FakePreview final : public fw::ActorPreviewHost {
 public:
  std::array<std::optional<fw::PreviewActor>, fw::kActorSlots> actors{};
  std::array<std::optional<fw::PreviewWeapon>, fw::kWeaponModelSlots> weapons{};
  std::array<AssetAddress, fw::kActorSlots> actor_blocks{};
  std::array<AssetAddress, fw::kWeaponModelSlots> weapon_blocks{};
  std::vector<ActorAssetRequest> requests;
  std::vector<std::string> calls;
  std::vector<std::string> refusals;
  std::vector<VS::Actors::CompanionStyle> companions;
  std::uintptr_t next_block = kFirstBlock;
  int pending_steps = 2;
  int tick_speed = 5;
  // Runs inside DrawModel with its companion argument: the record edits the
  // model pass makes.
  std::function<void(FakePreview&, int)> on_draw;
  // Run inside UpdateAssetQueue and UpdateMotionAndLookAt: the records the
  // queue installs and the edits of the motion update.
  std::function<void(FakePreview&)> on_pump;
  std::function<void(FakePreview&)> on_motion;

  // The record a test edits in place; the slot must already hold one.
  fw::PreviewActor& ActorAt(std::size_t slot) {
    std::optional<fw::PreviewActor>& actor = actors.at(slot);
    if (!actor) {
      throw std::logic_error("no actor in the preview slot");
    }
    return *actor;
  }

  std::optional<fw::PreviewActor> Actor(int slot) const override {
    if (slot < 0 || slot >= fw::kActorSlots) {
      FAIL_CHECK("actor slot " << slot << " past the actor table");
      return std::nullopt;
    }
    return actors.at(static_cast<std::size_t>(slot));
  }
  std::optional<fw::PreviewWeapon> Weapon(int slot) const override {
    if (slot < 0 || slot >= fw::kWeaponModelSlots) {
      FAIL_CHECK("weapon slot " << slot << " past the weapon models");
      return std::nullopt;
    }
    return weapons.at(static_cast<std::size_t>(slot));
  }
  AssetAddress Allocate(fw::PreviewAllocation kind) override {
    calls.push_back(Call("Allocate", {static_cast<std::int64_t>(kind)}));
    return Block(next_block++);
  }
  void Free(AssetAddress block) override {
    calls.push_back(Call("Free", {Value(block)}));
  }
  AssetAddress ActorBlock(int slot) const override {
    return actor_blocks.at(static_cast<std::size_t>(slot));
  }
  AssetAddress WeaponBlock(int slot) const override {
    return weapon_blocks.at(static_cast<std::size_t>(slot));
  }
  void QueueAsset(const ActorAssetRequest& request) override {
    requests.push_back(request);
  }
  bool UpdateAssetQueue() override {
    calls.emplace_back("Pump");
    if (on_pump) {
      on_pump(*this);
    }
    return pending_steps-- > 0;
  }
  void ReleaseModel(int slot) override {
    calls.push_back(Call("ReleaseModel", {slot}));
  }
  void ReleaseSequences(int slot) override {
    calls.push_back(Call("ReleaseSequences", {slot}));
  }
  void CopySequences(const fw::PreviewSequenceCopy& copy) override {
    calls.push_back(
        Call("CopySequences",
             {Value(copy.clone), Value(copy.source), Value(copy.battle_copy)}));
  }
  void SetWeaponDrawn(int slot) override {
    calls.push_back(Call("SetWeaponDrawn", {slot}));
  }
  void SetWeaponAttached(int weapon_slot) override {
    calls.push_back(Call("SetWeaponAttached", {weapon_slot}));
  }
  void SetWeaponPose(int weapon_slot, std::uint8_t pose) override {
    calls.push_back(Call("SetWeaponPose", {weapon_slot, pose}));
  }
  void ResetPlacement(int slot) override {
    calls.push_back(Call("ResetPlacement", {slot}));
  }
  void ClearSkipped(int slot) override {
    calls.push_back(Call("ClearSkipped", {slot}));
  }
  void ClearPreviewFlag(int slot) override {
    calls.push_back(Call("ClearPreviewFlag", {slot}));
  }
  void PlayAnimation(int slot, int animation) override {
    calls.push_back(Call("PlayAnimation", {slot, animation}));
  }
  void ResetMotion(int slot) override {
    calls.push_back(Call("ResetMotion", {slot}));
  }
  void SetCompanion(int slot, int companion_slot,
                    const VS::Actors::CompanionStyle& style) override {
    calls.push_back(Call("SetCompanion", {slot, companion_slot}));
    companions.push_back(style);
  }
  void SetFade(int slot, const VS::Render::ColorCode& fade) override {
    calls.push_back(
        Call("SetFade", {slot, fade.red, fade.green, fade.blue, fade.code}));
  }
  void ClearTint(int slot) override {
    calls.push_back(Call("ClearTint", {slot}));
  }
  void RefreshInteractionTile(int slot) override {
    calls.push_back(Call("RefreshInteractionTile", {slot}));
  }
  void SetAnimation(int slot, int animation) override {
    calls.push_back(Call("SetAnimation", {slot, animation}));
  }
  void InitializePose(int slot) override {
    calls.push_back(Call("InitializePose", {slot}));
  }
  void AdvanceAnimation(int slot) override {
    calls.push_back(Call("AdvanceAnimation", {slot}));
  }
  void SnapshotBlendPose(int slot) override {
    calls.push_back(Call("SnapshotBlendPose", {slot}));
  }
  void ClearPoseCounters(int slot) override {
    calls.push_back(Call("ClearPoseCounters", {slot}));
  }
  void PrepareCamera() override { calls.emplace_back("PrepareCamera"); }
  int TickSpeed() const override { return tick_speed; }
  void SetFramesPerStep(int slot, std::uint8_t frames) override {
    calls.push_back(Call("SetFramesPerStep", {slot, frames}));
  }
  void UpdateTextureScrolling(int slot) override {
    calls.push_back(Call("UpdateTextureScrolling", {slot}));
  }
  void UpdateAnimationBlend(int slot) override {
    calls.push_back(Call("UpdateAnimationBlend", {slot}));
  }
  void PrepareRootTransform(int slot) override {
    calls.push_back(Call("PrepareRootTransform", {slot}));
  }
  void DrawModel(int slot, int companion_slot) override {
    calls.push_back(Call("DrawModel", {slot, companion_slot}));
    if (on_draw) {
      on_draw(*this, companion_slot);
    }
  }
  void OffsetOrderingTable(int entries) override {
    calls.push_back(Call("OffsetOrderingTable", {entries}));
  }
  void MoveActor(int slot, const VS::Actors::CompanionOffset& offset,
                 int sign) override {
    calls.push_back(Offset("MoveActor", slot, offset, sign));
  }
  void UpdateMotionAndLookAt(int slot, int companion_slot) override {
    calls.push_back(Call("UpdateMotionAndLookAt", {slot, companion_slot}));
    if (on_motion) {
      on_motion(*this);
    }
  }
  void DrawWeapon(int weapon_slot, int slot, std::uint8_t bone,
                  const VS::Actors::CompanionOffset& offset) override {
    calls.push_back(Offset("DrawWeapon", weapon_slot, offset, bone) +
                    Call("", {slot}));
  }
  void Refuse(std::string_view what) override { refusals.emplace_back(what); }
};

// Actor 3: a model in block 0x3000 with a companion record in slot 5 and the
// weapon pair 4 (weapon models 8 and 9).
void SetCloneSource(FakePreview& host) {
  host.actors[3] = fw::PreviewActor{
      .model_id = 77, .variant = 2, .weapon_pair = 4, .companion_slot = 5};
  host.actor_blocks[3] = Block(0x3000);
  host.actors[5] = fw::PreviewActor{.model_id = 88};
  host.weapons[8] =
      fw::PreviewWeapon{.model_id = 9, .attachment_slot = 6, .material = 3};
  host.weapons[9] =
      fw::PreviewWeapon{.model_id = 10, .attachment_slot = 7, .material = 4};
}

ActorAssetRequest Request(std::uint8_t kind, std::int8_t target,
                          std::int16_t model, AssetAddress destination,
                          AssetAddress data) {
  return {.kind = kind,
          .target_slot = target,
          .model_id = model,
          .destination = destination,
          .data = data};
}

fw::PreviewActor DrawnActor() { return {.model_id = 77, .bone_count = 10}; }

TEST_CASE("Actor preview clones a record with its companion and weapons",
          "[menu][menu-rows-details][menu-actor-preview]") {
  FakePreview host;
  SetCloneSource(host);
  fw::CloneActorPreview(host, 3);

  ActorAssetRequest record =
      Request(2, 1, 77, Block(0x1000), SharedAssetAddress(3));
  record.sub_kind = 0xFF;
  record.variant = 2;
  ActorAssetRequest companion =
      Request(2, 16, 88, Block(0x1001), SharedAssetAddress(5));
  companion.sub_kind = 1;
  companion.texture_slot = 0xFC;
  companion.variant = 2;
  ActorAssetRequest first_weapon =
      Request(4, 2, 9, Block(0x1002), SharedAssetAddress(6));
  first_weapon.sub_kind = 1;
  first_weapon.texture_slot = 6;
  first_weapon.material = 3;
  first_weapon.variant = 2;
  ActorAssetRequest second_weapon =
      Request(4, 3, 10, Block(0x1003), SharedAssetAddress(7));
  second_weapon.sub_kind = 1;
  second_weapon.texture_slot = 7;
  second_weapon.material = 4;
  second_weapon.variant = 2;
  CHECK(host.requests == std::vector<ActorAssetRequest>{
                             record, companion, first_weapon, second_weapon});
  CHECK(host.calls == std::vector<std::string>{"Allocate(0)", "Allocate(0)",
                                               "Allocate(1)", "Allocate(1)",
                                               "Pump", "Pump", "Pump",
                                               "CopySequences(4096,12288,0)"});
  CHECK(host.refusals.empty());
}

TEST_CASE("Actor preview copies Ashley's battle sequence",
          "[menu][menu-rows-details][menu-actor-preview]") {
  FakePreview host;
  host.pending_steps = 0;
  host.actors[0] = fw::PreviewActor{.model_id = 1, .companion_slot = 1};
  host.actor_blocks[0] = Block(0x2000);
  fw::CloneActorPreview(host, 0);

  ActorAssetRequest record =
      Request(2, 1, 1, Block(0x1000), SharedAssetAddress(0));
  record.sub_kind = 0xFF;
  // The outline marker is no companion record; Ashley's weapon models are
  // empty.
  CHECK(host.requests == std::vector<ActorAssetRequest>{record});
  CHECK(host.calls ==
        std::vector<std::string>{"Allocate(0)", "Pump", "Allocate(2)",
                                 "CopySequences(4096,8192,4097)"});
  CHECK(host.refusals.empty());
}

TEST_CASE("Actor preview refuses a clone of a missing record",
          "[menu][menu-rows-details][menu-actor-preview]") {
  for (const int slot : {-1, fw::kActorSlots}) {
    FakePreview host;
    SetCloneSource(host);
    fw::CloneActorPreview(host, slot);
    CHECK(host.refusals ==
          std::vector<std::string>{"preview clone of an actor past the actor "
                                   "table"});
    CHECK(host.requests.empty());
    CHECK(host.calls.empty());
  }

  FakePreview missing;
  SetCloneSource(missing);
  fw::CloneActorPreview(missing, 4);
  CHECK(missing.refusals ==
        std::vector<std::string>{"preview clone of a missing actor"});
  CHECK(missing.requests.empty());
  CHECK(missing.calls.empty());
}

TEST_CASE("Actor preview clones without a companion or weapon past the tables",
          "[menu][menu-rows-details][menu-actor-preview]") {
  FakePreview past;
  SetCloneSource(past);
  past.ActorAt(3).companion_slot = fw::kActorSlots;
  past.ActorAt(3).weapon_pair = 10;
  fw::CloneActorPreview(past, 3);
  CHECK(past.refusals ==
        std::vector<std::string>{
            "preview clone of a companion past the actor table",
            "preview clone of a weapon past the weapon models",
            "preview clone of a weapon past the weapon models"});
  REQUIRE(past.requests.size() == 1);
  CHECK(past.requests[0].target_slot == 1);
  CHECK(past.calls == std::vector<std::string>{"Allocate(0)", "Pump", "Pump",
                                               "Pump",
                                               "CopySequences(4096,12288,0)"});
}

TEST_CASE("Actor preview clones without a missing companion",
          "[menu][menu-rows-details][menu-actor-preview]") {
  FakePreview missing;
  SetCloneSource(missing);
  missing.ActorAt(3).companion_slot = 6;
  missing.weapons[8].reset();
  fw::CloneActorPreview(missing, 3);
  CHECK(missing.refusals ==
        std::vector<std::string>{"preview clone of a missing companion"});
  REQUIRE(missing.requests.size() == 2);
  CHECK(missing.requests[1].target_slot == 3);
  CHECK(missing.requests[1].model_id == 10);
  CHECK(missing.requests[1].destination == Block(0x1001));
  CHECK(missing.requests[1].data == SharedAssetAddress(7));
}

TEST_CASE("Actor preview copies the sequences of the record it cloned",
          "[menu][menu-rows-details][menu-actor-preview]") {
  // Cloning the preview itself: the queue installs the clone in slot 1, and
  // the copy still reads the record the clone was made from (224.c:29,
  // 78-90).
  FakePreview host;
  host.pending_steps = 1;
  host.actors[1] = fw::PreviewActor{.model_id = 5, .weapon_pair = 1};
  host.actor_blocks[1] = Block(0x5000);
  host.on_pump = [](FakePreview& preview) {
    preview.actors[1] = fw::PreviewActor{};
    preview.actor_blocks[1] = Block(kFirstBlock);
  };
  fw::CloneActorPreview(host, 1);
  CHECK(host.calls == std::vector<std::string>{"Allocate(0)", "Pump", "Pump",
                                               "CopySequences(4096,20480,0)"});
  REQUIRE(host.requests.size() == 1);
  CHECK(host.requests[0].target_slot == 1);
  CHECK(host.requests[0].model_id == 5);
  CHECK(host.requests[0].data == SharedAssetAddress(1));
  CHECK(host.refusals.empty());
}

TEST_CASE("Actor preview pose reset outlines a preview without a companion",
          "[menu][menu-rows-details][menu-actor-preview]") {
  FakePreview host;
  host.actors[1] = DrawnActor();
  host.weapons[2] = fw::PreviewWeapon{};
  fw::ResetActorPreviewPose(host);
  CHECK(host.calls ==
        std::vector<std::string>{"SetWeaponAttached(2)", "ResetMotion(1)",
                                 "ResetPlacement(1)", "SetCompanion(1,1)",
                                 "SetFade(1,0,0,0,0)", "ClearTint(1)",
                                 "ClearPreviewFlag(1)"});
  REQUIRE(host.companions.size() == 1);
  CHECK(host.companions[0] == fw::kPreviewOutline);
  CHECK(fw::kPreviewOutline.offset ==
        VS::Actors::CompanionOffset{.x = 2, .y = -2, .z = 2, .depth_bias = 16});
  CHECK(fw::kPreviewOutline.color ==
        VS::Render::ColorCode{.red = 0xFF, .green = 0xFF, .blue = 0xFF});
  CHECK(host.refusals.empty());
}

TEST_CASE("Actor preview pose reset shows a companion record",
          "[menu][menu-rows-details][menu-actor-preview]") {
  FakePreview companion;
  companion.actors[1] = DrawnActor();
  companion.actors[16] = fw::PreviewActor{};
  companion.weapons[3] = fw::PreviewWeapon{};
  fw::ResetActorPreviewPose(companion);
  CHECK(companion.calls ==
        std::vector<std::string>{"SetWeaponAttached(3)", "ResetMotion(1)",
                                 "ResetPlacement(1)", "ClearSkipped(16)",
                                 "SetFade(1,0,0,0,0)", "ClearTint(1)",
                                 "ClearPreviewFlag(1)"});
  CHECK(companion.companions.empty());
}

TEST_CASE("Actor preview pose reset refuses a missing preview record",
          "[menu][menu-rows-details][menu-actor-preview]") {
  FakePreview empty;
  fw::ResetActorPreviewPose(empty);
  CHECK(empty.calls == std::vector<std::string>{"ResetMotion(1)"});
  CHECK(empty.refusals ==
        std::vector<std::string>{"preview pose without a preview record"});
}

TEST_CASE("Actor preview load clones, poses and animates the preview",
          "[menu][menu-rows-details][menu-actor-preview]") {
  FakePreview host;
  SetCloneSource(host);
  host.pending_steps = 0;
  host.actors[1] = DrawnActor();
  host.actors[16] = fw::PreviewActor{};
  fw::LoadActorPreview(host, 3);
  CHECK(host.requests.size() == 4);
  CHECK(host.calls == std::vector<std::string>{
                          "Allocate(0)", "Allocate(0)", "Allocate(1)",
                          "Allocate(1)", "Pump", "CopySequences(4096,12288,0)",
                          "ResetMotion(1)", "ResetPlacement(1)",
                          "ClearSkipped(16)", "SetFade(1,0,0,0,0)",
                          "ClearTint(1)", "ClearPreviewFlag(1)",
                          "SetWeaponDrawn(1)", "PlayAnimation(1,1)"});
  CHECK(host.refusals.empty());

  FakePreview missing;
  SetCloneSource(missing);
  missing.pending_steps = 0;
  fw::LoadActorPreview(missing, 3);
  CHECK(missing.refusals ==
        std::vector<std::string>{"preview pose without a preview record",
                                 "preview load without a preview record"});
  CHECK(missing.calls.back() == "ResetMotion(1)");
}

TEST_CASE("Actor preview release frees the preview's blocks",
          "[menu][menu-rows-details][menu-actor-preview]") {
  FakePreview host;
  host.actor_blocks[1] = Block(0x2000);
  host.actor_blocks[16] = Block(0x2200);
  host.weapon_blocks[2] = Block(0x2100);
  host.weapon_blocks[3] = Block(0x2300);
  fw::ReleaseActorPreview(host);
  CHECK(host.calls == std::vector<std::string>{"ReleaseModel(1)", "Free(8192)",
                                               "ReleaseModel(16)", "Free(8704)",
                                               "Free(8448)", "Free(8960)"});

  FakePreview single;
  single.actor_blocks[1] = Block(0x2000);
  single.weapon_blocks[3] = Block(0x2300);
  fw::ReleaseActorPreview(single);
  CHECK(single.calls == std::vector<std::string>{"ReleaseModel(1)",
                                                 "Free(8192)", "Free(8960)"});

  // The original freed the record block unconditionally (224.c:159).
  FakePreview empty;
  fw::ReleaseActorPreview(empty);
  CHECK(empty.calls == std::vector<std::string>{"ReleaseModel(1)", "Free(0)"});
}

TEST_CASE("Actor preview draw skips an absent, boneless or skipped model",
          "[menu][menu-rows-details][menu-actor-preview]") {
  FakePreview absent;
  CHECK(fw::DrawActorPreview(absent) == fw::PreviewDrawResult::kNotDrawn);
  CHECK(absent.calls.empty());

  FakePreview boneless;
  boneless.actors[1] = fw::PreviewActor{};
  CHECK(fw::DrawActorPreview(boneless) == fw::PreviewDrawResult::kNotDrawn);
  CHECK(boneless.calls.empty());

  FakePreview skipped;
  skipped.actors[1] = DrawnActor();
  skipped.ActorAt(1).skipped = true;
  CHECK(fw::DrawActorPreview(skipped) == fw::PreviewDrawResult::kNotDrawn);
  CHECK(skipped.calls.empty());

  FakePreview released;
  released.actors[1] = DrawnActor();
  released.on_draw = [](FakePreview& host, int /*companion*/) {
    host.actors[1].reset();
  };
  CHECK(fw::DrawActorPreview(released) == fw::PreviewDrawResult::kNotDrawn);
  CHECK(released.calls.back() == "DrawModel(1,0)");
}

TEST_CASE("Actor preview draw poses the model and its attached weapons",
          "[menu][menu-rows-details][menu-actor-preview]") {
  FakePreview host;
  host.actors[1] = DrawnActor();
  host.weapons[2] = fw::PreviewWeapon{.attachment_bone = 7, .attached = true};
  host.weapons[3] = fw::PreviewWeapon{.attachment_bone = 8};
  CHECK(fw::DrawActorPreview(host) == fw::PreviewDrawResult::kDrawn);
  CHECK(host.calls ==
        std::vector<std::string>{"PrepareCamera", "SetFramesPerStep(1,2)",
                                 "PrepareRootTransform(1)", "DrawModel(1,0)",
                                 "DrawWeapon(2,0,0,0,0,7)(1)"});

  FakePreview frozen;
  frozen.tick_speed = 4;
  frozen.actors[1] = DrawnActor();
  frozen.ActorAt(1).frozen = true;
  CHECK(fw::DrawActorPreview(frozen) == fw::PreviewDrawResult::kDrawn);
  CHECK(frozen.calls ==
        std::vector<std::string>{"PrepareCamera", "SetFramesPerStep(1,2)",
                                 "UpdateTextureScrolling(1)",
                                 "UpdateAnimationBlend(1)", "DrawModel(1,0)"});

  FakePreview culled;
  culled.actors[1] = DrawnActor();
  culled.weapons[2] = fw::PreviewWeapon{.attached = true};
  culled.on_draw = [](FakePreview& preview, int /*companion*/) {
    preview.ActorAt(1).culled = true;
  };
  CHECK(fw::DrawActorPreview(culled) == fw::PreviewDrawResult::kCulled);
  CHECK(culled.calls.back() == "DrawModel(1,0)");
}

TEST_CASE("Actor preview draw refuses a weapon past the bone matrices",
          "[menu][menu-rows-details][menu-actor-preview]") {
  FakePreview host;
  host.actors[1] = DrawnActor();
  host.weapons[2] = fw::PreviewWeapon{.attachment_bone = fw::kActorBoneMatrices,
                                      .attached = true};
  host.weapons[3] = fw::PreviewWeapon{
      .attachment_bone = fw::kActorBoneMatrices - 1, .attached = true};
  CHECK(fw::DrawActorPreview(host) == fw::PreviewDrawResult::kDrawn);
  CHECK(host.refusals ==
        std::vector<std::string>{"preview weapon on a bone past the bone "
                                 "matrices"});
  CHECK(host.calls.back() == "DrawWeapon(3,0,0,0,0,41)(1)");
}

TEST_CASE("Actor preview draw moves the model for its companion pass",
          "[menu][menu-rows-details][menu-actor-preview]") {
  constexpr VS::Actors::CompanionOffset kBefore{
      .x = 1, .y = -1, .z = 3, .depth_bias = 4};
  constexpr VS::Actors::CompanionOffset kAfter{
      .x = 1, .y = -1, .z = 3, .depth_bias = 8};
  FakePreview host;
  host.actors[1] = DrawnActor();
  host.ActorAt(1).companion_slot = 16;
  host.ActorAt(1).companion = kBefore;
  host.weapons[3] = fw::PreviewWeapon{.attachment_bone = 5, .attached = true};
  // The companion pass reads the offset back after it draws (224.c:228-233).
  host.on_draw = [&](FakePreview& preview, int companion) {
    if (companion == 16) {
      preview.ActorAt(1).companion = kAfter;
    }
  };
  CHECK(fw::DrawActorPreview(host) == fw::PreviewDrawResult::kDrawn);
  CHECK(host.calls ==
        std::vector<std::string>{
            "PrepareCamera", "SetFramesPerStep(1,2)", "PrepareRootTransform(1)",
            "DrawModel(1,0)", "UpdateTextureScrolling(16)",
            "OffsetOrderingTable(52)", Offset("MoveActor", 1, kBefore, 1),
            "UpdateMotionAndLookAt(1,16)", "DrawModel(1,16)",
            "OffsetOrderingTable(-56)", Offset("MoveActor", 1, kAfter, -1),
            Offset("DrawWeapon", 3, kAfter, 5) + "(1)"});

  // The outline marker has no record whose textures scroll.
  FakePreview outline;
  outline.actors[1] = DrawnActor();
  outline.ActorAt(1).companion_slot = 1;
  outline.ActorAt(1).companion = fw::kPreviewOutline.offset;
  CHECK(fw::DrawActorPreview(outline) == fw::PreviewDrawResult::kDrawn);
  CHECK(outline.calls ==
        std::vector<std::string>{
            "PrepareCamera", "SetFramesPerStep(1,2)", "PrepareRootTransform(1)",
            "DrawModel(1,0)", "OffsetOrderingTable(64)",
            Offset("MoveActor", 1, fw::kPreviewOutline.offset, 1),
            "UpdateMotionAndLookAt(1,1)", "DrawModel(1,1)",
            "OffsetOrderingTable(-64)",
            Offset("MoveActor", 1, fw::kPreviewOutline.offset, -1)});
}

TEST_CASE("Actor preview companion pass draws the slot the motion leaves",
          "[menu][menu-rows-details][menu-actor-preview]") {
  // The model pass reads the companion slot back after the motion update
  // (224.c:216-233).
  FakePreview host;
  host.actors[1] = DrawnActor();
  host.ActorAt(1).companion_slot = 1;
  host.on_motion = [](FakePreview& preview) {
    preview.ActorAt(1).companion_slot = 3;
  };
  CHECK(fw::DrawActorPreview(host) == fw::PreviewDrawResult::kDrawn);
  REQUIRE(host.calls.size() == 10);
  CHECK(host.calls[4] == "OffsetOrderingTable(48)");
  CHECK(host.calls[6] == "UpdateMotionAndLookAt(1,1)");
  CHECK(host.calls[7] == "DrawModel(1,3)");
  CHECK(host.calls[8] == "OffsetOrderingTable(-48)");
  CHECK(host.refusals.empty());
}

TEST_CASE("Actor preview frame and ordering arithmetic",
          "[menu][menu-rows-details][menu-actor-preview]") {
  STATIC_CHECK(fw::PreviewFramesPerStep(5) == 2);
  STATIC_CHECK(fw::PreviewFramesPerStep(4) == 2);
  STATIC_CHECK(fw::PreviewFramesPerStep(1) == 0);
  STATIC_CHECK(fw::CompanionOrderingEntries(0) == 48);
  STATIC_CHECK(fw::CompanionOrderingEntries(16) == 64);
  STATIC_CHECK(fw::kPreviewWeaponModelSlot == 2);
}

TEST_CASE("Ashley's equipment pose follows her drawn weapon",
          "[menu][menu-rows-details][menu-actor-preview]") {
  FakePreview drawn;
  drawn.actors[0] = fw::PreviewActor{.weapon_drawn = true};
  drawn.weapons[0] = fw::PreviewWeapon{};
  drawn.weapons[1] = fw::PreviewWeapon{};
  fw::ResetAshleyEquipmentPose(drawn);
  CHECK(drawn.calls == std::vector<std::string>{
                           "SetWeaponPose(0,64)", "SetWeaponAttached(0)",
                           "SetWeaponPose(1,64)", "SetWeaponAttached(1)",
                           "RefreshInteractionTile(0)", "SetAnimation(0,101)",
                           "InitializePose(0)", "AdvanceAnimation(0)",
                           "SnapshotBlendPose(0)", "ClearPoseCounters(0)"});

  FakePreview idle;
  idle.actors[0] = fw::PreviewActor{};
  idle.weapons[0] = fw::PreviewWeapon{};
  fw::ResetAshleyEquipmentPose(idle);
  CHECK(idle.calls == std::vector<std::string>{
                          "SetWeaponPose(0,0)", "RefreshInteractionTile(0)",
                          "SetAnimation(0,1)", "InitializePose(0)",
                          "AdvanceAnimation(0)", "SnapshotBlendPose(0)",
                          "ClearPoseCounters(0)"});
  CHECK(idle.refusals.empty());

  FakePreview missing;
  fw::ResetAshleyEquipmentPose(missing);
  CHECK(missing.calls.empty());
  CHECK(missing.refusals ==
        std::vector<std::string>{"Ashley's equipment pose without her record"});
}

TEST_CASE("Ashley's weapon animations replace her weapon sequences",
          "[menu][menu-rows-details][menu-actor-preview]") {
  FakePreview host;
  fw::QueueAshleyWeaponAnimations(host, 33);
  CHECK(host.calls == std::vector<std::string>{"ReleaseSequences(0)"});
  CHECK(host.requests ==
        std::vector<ActorAssetRequest>{{.kind = 7, .model_id = 33}});
}

TEST_CASE("Actor preview live adapter refuses a missing record or camera",
          "[menu][menu-rows-details][menu-actor-preview]") {
  std::array<Actors_ActorRecord*, 2> saved{Actors_actorRecords[0],
                                           Actors_actorRecords[5]};
  Actors_actorRecords[0] = nullptr;
  Actors_actorRecords[5] = nullptr;
  {
    VS::test_support::ChannelWarnings warnings{"menu.framework"};
    CHECK(fw::DrawLiveActorPreview(nullptr) ==
          static_cast<int>(fw::PreviewDrawResult::kNotDrawn));
    fw::CloneLiveActorPreview(-1);
    fw::CloneLiveActorPreview(fw::kActorSlots);
    fw::CloneLiveActorPreview(5);
    fw::ResetLiveAshleyEquipmentPose();
    CHECK(warnings.messages ==
          std::vector<std::string>{
              "actor preview: draw without a camera matrix",
              "actor preview: preview clone of an actor past the actor table",
              "actor preview: preview clone of an actor past the actor table",
              "actor preview: preview clone of a missing actor",
              "actor preview: Ashley's equipment pose without her record"});
  }
  Actors_actorRecords[0] = saved[0];
  Actors_actorRecords[5] = saved[1];
}

TEST_CASE("Menu rows, details and actor preview retire their module symbols",
          "[menu][menu-rows-details]") {
  // MAINMENU.PRG 224.c, C48.c and 2D10.c: func_800F9A24, func_800FA3FC,
  // vs_mainMenu_getEquipmentMaxStats, vs_mainMenu_resetStats,
  // vs_mainMenu_setWeaponRow, vs_mainMenu_loadItemText,
  // vs_mainMenu_ensureItemTextUnloaded and vs_mainMenu_initWeaponDetailsRow.
  // No module image stores them.
  constexpr auto kAddresses =
      std::to_array<unsigned>({0x800F9A24, 0x800FA3FC, 0x800FA598, 0x800FD220,
                               0x800FC85C, 0x800FE5CC, 0x800FE694, 0x800FC510});
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  for (const unsigned address : kAddresses) {
    CHECK(VS::Content::ResolveModuleAddress(symbols, "MENU/MAINMENU.PRG",
                                            address) == nullptr);
  }
}
}  // namespace
