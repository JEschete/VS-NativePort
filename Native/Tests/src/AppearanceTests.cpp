#include <algorithm>
#include <array>
#include <cstdint>
#include <iterator>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Actors/Appearance.hpp"
#include "Actors/AppearanceLive.hpp"
#include "Actors/AppearanceLiveState.h"
#include "Content/ModuleData.h"
#include "Core/Log.hpp"

extern "C" {
#include "Actors/ActorRecord.h"
#include "Actors/Movement.h"
#include "Actors/Skeleton.h"
#include "Actors/WeaponModel.h"
#include "Field/Gimmicks.h"
}

namespace actors = VS::Actors;
using actors::AppearanceStatus;

namespace {
// Collects the "actors" channel warnings while it lives.
struct ActorsWarnings {
  std::vector<std::string> messages;
  VS::Core::LogObserver saved =
      VS::Core::SetLogObserver([this](const VS::Core::LogRecord& record) {
        if (record.channel == "actors") {
          CHECK(record.level == VS::Core::LogLevel::Warn);
          messages.emplace_back(record.message);
        }
      });

  ActorsWarnings() = default;
  ActorsWarnings(const ActorsWarnings&) = delete;
  ActorsWarnings& operator=(const ActorsWarnings&) = delete;
  ActorsWarnings(ActorsWarnings&&) = delete;
  ActorsWarnings& operator=(ActorsWarnings&&) = delete;
  ~ActorsWarnings() { VS::Core::SetLogObserver(std::move(saved)); }
};

// The actor, weapon and gimmick tables and the room scale, emptied for one
// test and then restored.
struct LiveAppearanceState {
  decltype(std::to_array(Actors_actorRecords)) saved_actors =
      std::to_array(Actors_actorRecords);
  decltype(std::to_array(Actors_weaponModels)) saved_weapons =
      std::to_array(Actors_weaponModels);
  decltype(std::to_array(Field_gimmickInstances)) saved_gimmicks =
      std::to_array(Field_gimmickInstances);
  VECTOR saved_scale = Actors_roomActorScale;
  std::vector<std::unique_ptr<Actors_ActorRecord>> records;

  LiveAppearanceState() {
    std::fill(std::begin(Actors_actorRecords), std::end(Actors_actorRecords),
              nullptr);
    std::fill(std::begin(Actors_weaponModels), std::end(Actors_weaponModels),
              nullptr);
    std::fill(std::begin(Field_gimmickInstances),
              std::end(Field_gimmickInstances), nullptr);
  }
  LiveAppearanceState(const LiveAppearanceState&) = delete;
  LiveAppearanceState& operator=(const LiveAppearanceState&) = delete;
  LiveAppearanceState(LiveAppearanceState&&) = delete;
  LiveAppearanceState& operator=(LiveAppearanceState&&) = delete;
  ~LiveAppearanceState() {
    std::ranges::copy(saved_actors, std::begin(Actors_actorRecords));
    std::ranges::copy(saved_weapons, std::begin(Actors_weaponModels));
    std::ranges::copy(saved_gimmicks, std::begin(Field_gimmickInstances));
    Actors_roomActorScale = saved_scale;
  }

  Actors_ActorRecord& AddActor(int actor_id) {
    records.push_back(std::make_unique<Actors_ActorRecord>());
    Actors_ActorRecord& actor = *records.back();
    actor.instance.slot = static_cast<u_char>(actor_id);
    Actors_actorRecords[actor_id] = &actor;
    return actor;
  }
};

// The transition chain as plain arrays.
class TestTransitionActors final : public actors::AppearanceTransitionActors {
 public:
  std::array<bool, actors::kActorTableSize> exists{};
  std::array<int, actors::kActorTableSize> companion{};
  std::array<int, actors::kActorTableSize> state{};
  std::vector<int> visited;

  [[nodiscard]] bool Exists(int actor_id) const override {
    return actor_id >= 0 && actor_id < actors::kActorTableSize &&
           exists.at(static_cast<std::size_t>(actor_id));
  }
  void SetTransitionState(int actor_id, int new_state) override {
    visited.push_back(actor_id);
    state.at(static_cast<std::size_t>(actor_id)) = new_state;
  }
  [[nodiscard]] int CompanionSlot(int actor_id) const override {
    return companion.at(static_cast<std::size_t>(actor_id));
  }
};
}  // namespace

TEST_CASE("Actor root spin starts at twice the parameter and 0 stops it",
          "[actors-appearance-rendering][actors]") {
  actors::PaletteFade fade{
      .active = true, .reverse = true, .phase = 77, .rate = 9};
  actors::SetActorPaletteFade(fade, 0);
  CHECK_FALSE(fade.active);
  CHECK(fade.reverse);
  CHECK(fade.phase == 77);
  CHECK(fade.rate == 9);

  actors::SetActorPaletteFade(fade, 200);
  CHECK(fade.active);
  CHECK_FALSE(fade.reverse);
  CHECK(fade.phase == 0);
  // The doubled parameter is stored as a byte.
  CHECK(fade.rate == 144);
}

TEST_CASE("Weapon visibility blend settles attached weapons at their level",
          "[actors-appearance-rendering][actors]") {
  const actors::WeaponAppearance blending{.attached = true,
                                          .shown = true,
                                          .blend_level = 10,
                                          .blend_target = 64,
                                          .blend_frames = 5};
  std::array<std::optional<actors::WeaponAppearance>, 2> weapons{blending,
                                                                 std::nullopt};
  actors::ResetActorWeaponVisibilityBlend(weapons);
  actors::WeaponAppearance settled = blending;
  settled.blend_level = actors::kWeaponBlendShown;
  settled.blend_target = actors::kWeaponBlendShown;
  settled.blend_frames = 0;
  CHECK(weapons[0] == settled);
  CHECK_FALSE(weapons[1].has_value());

  actors::WeaponAppearance hidden = blending;
  hidden.shown = false;
  actors::WeaponAppearance loose = blending;
  loose.attached = false;
  weapons = {hidden, loose};
  actors::ResetActorWeaponVisibilityBlend(weapons);
  hidden.blend_level = 0;
  hidden.blend_target = 0;
  hidden.blend_frames = 0;
  CHECK(weapons[0] == hidden);
  // A weapon off its bone keeps blending.
  CHECK(weapons[1] == loose);

  actors::WeaponAppearance weapon;
  actors::SetWeaponBrightnessOverride(weapon, true, 200);
  CHECK(weapon == actors::WeaponAppearance{.brightness_override = true,
                                           .brightness = 200});
}

TEST_CASE("Room scale gives every actor its x as a signed halfword",
          "[actors-appearance-rendering][actors]") {
  VS::Core::Math::Vec3 stored;
  CHECK(actors::SetRoomActorScale(stored, {4096, 2048, 1024}) == 4096);
  CHECK(stored == VS::Core::Math::Vec3{4096, 2048, 1024});
  CHECK(actors::SetRoomActorScale(stored, {0x12345, 1, 2}) == 0x2345);
  CHECK(actors::SetRoomActorScale(stored, {0x8000, 0, 0}) == -32768);
  CHECK(stored.x == 0x8000);
}

TEST_CASE("Palette pulse slots arm one bit each and restart an idle pulse",
          "[actors-appearance-rendering][actors]") {
  actors::PalettePulseRequest request{.kind = 0,
                                      .pulse = {.level = 9, .flags = 0xF2}};
  CHECK(actors::EnableModelPalettePulse(request, 3, 0) ==
        actors::PalettePulseChange{.restarted = true});
  // The restart keeps the high flag bits and takes the direction bit.
  CHECK(request == actors::PalettePulseRequest{
                       .kind = 0x80, .pulse = {.level = 0, .flags = 0xF1}});

  request.pulse.level = 6;
  CHECK(actors::EnableModelPalettePulse(request, 0, 7) ==
        actors::PalettePulseChange{});
  CHECK(request == actors::PalettePulseRequest{
                       .kind = 0x81, .pulse = {.level = 6, .flags = 0xF1}});

  CHECK(actors::EnableModelPalettePulse(request, 0, 8) ==
        actors::PalettePulseChange{.status = AppearanceStatus::kBadPulseSlot});
  CHECK(request.kind == 0x81);

  CHECK(actors::EnableModelPalettePulse(request, 1,
                                        actors::kEveryPalettePulseSlot) ==
        actors::PalettePulseChange{});
  CHECK(request.kind == actors::kEveryPalettePulseKind);
}

TEST_CASE("Palette pulse slots disarm one bit or every bit",
          "[actors-appearance-rendering][actors]") {
  actors::PalettePulseRequest request{.kind = 0x81,
                                      .pulse = {.level = 6, .flags = 0xF1}};
  CHECK(actors::DisableModelPalettePulse(request, 0) == AppearanceStatus::kOk);
  CHECK(request.kind == 0x01);
  CHECK(actors::DisableModelPalettePulse(request, -2) ==
        AppearanceStatus::kBadPulseSlot);
  CHECK(request.kind == 0x01);
  CHECK(actors::DisableModelPalettePulse(
            request, actors::kEveryPalettePulseSlot) == AppearanceStatus::kOk);
  CHECK(request.kind == 0);

  // An idle pulse restarts even when the slot is refused.
  request.pulse.level = 4;
  const actors::PalettePulseChange change =
      actors::EnableModelPalettePulse(request, 0, 9);
  CHECK(change ==
        actors::PalettePulseChange{.status = AppearanceStatus::kBadPulseSlot,
                                   .restarted = true});
  CHECK(request.pulse.level == 0);
  CHECK(request.kind == 0);
}

TEST_CASE("Shadow blend snaps or steps toward the actor's own rates",
          "[actors-appearance-rendering][actors]") {
  actors::ShadowBlend shadow{.levelFrames = 3,
                             .radiusFrames = 4,
                             .level = 40,
                             .radiusExpandRate = 100,
                             .radiusShrinkRate = 300};
  actors::BeginActorShadowBlend(shadow, {.expand = 160, .shrink = 200}, 8);
  CHECK(shadow == actors::ShadowBlend{.levelFrames = 8,
                                      .radiusFrames = 8,
                                      .level = 40,
                                      .levelDelta = -40,
                                      .radiusExpandRate = 100,
                                      .radiusShrinkRate = 300,
                                      .radiusExpandDelta = 60,
                                      .radiusShrinkDelta = -100});

  actors::BeginActorShadowBlend(shadow, {.expand = 160, .shrink = 200}, 0);
  CHECK(shadow.radiusExpandRate == 160);
  CHECK(shadow.radiusShrinkRate == 200);
  CHECK(shadow.level == 0);
  CHECK(shadow.levelFrames == 0);
  CHECK(shadow.radiusFrames == 0);
}

TEST_CASE("Actor fade color and tint keep the caller's code and last tint",
          "[actors-appearance-rendering][actors]") {
  actors::ActorColors colors;
  actors::SetActorFadeParameters(colors, {10, 20, 30, 99});
  VS::Render::ColorCode out{1, 2, 3, 44};
  actors::ActorFadeParameters(colors, out);
  CHECK(out == VS::Render::ColorCode{10, 20, 30, 44});

  actors::SetActorTint(colors, VS::Render::ColorCode{5, 6, 7, 8});
  CHECK(colors.custom_tint);
  actors::SetActorTint(colors, std::nullopt);
  CHECK(colors == actors::ActorColors{.fade = {10, 20, 30},
                                      .custom_tint = false,
                                      .tint = {5, 6, 7, 8}});
  CHECK(actors::ActorTint(colors) == VS::Render::ColorCode{5, 6, 7, 8});
}

TEST_CASE("Appearance transition kinds name black, delay and white",
          "[actors-appearance-rendering][actors]") {
  CHECK(actors::AppearanceTransitionState(0) == actors::kAppearanceBlack);
  CHECK(actors::AppearanceTransitionState(1) == actors::kAppearanceDelay);
  CHECK(actors::AppearanceTransitionState(2) == actors::kAppearanceWhite);
  CHECK_FALSE(actors::AppearanceTransitionState(3).has_value());
}

TEST_CASE("Appearance transitions follow companion slots past slot 1",
          "[actors-appearance-rendering][actors]") {
  TestTransitionActors chain;
  chain.exists[0] = chain.exists[5] = chain.exists[9] = true;
  chain.companion[0] = 5;
  chain.companion[5] = 9;
  chain.companion[9] = 1;
  CHECK(actors::StartActorAppearanceTransition(chain, 0, 2));
  CHECK(chain.visited == std::vector<int>{0, 5, 9});
  CHECK(chain.state[9] == actors::kAppearanceWhite);

  // An unknown kind changes no state but still walks the chain.
  chain.visited.clear();
  CHECK(actors::StartActorAppearanceTransition(chain, 0, 7));
  CHECK(chain.visited.empty());
}

TEST_CASE("Appearance transitions refuse a missing or looping companion",
          "[actors-appearance-rendering][actors]") {
  TestTransitionActors chain;
  chain.exists[0] = chain.exists[5] = chain.exists[9] = true;
  chain.companion[0] = 5;
  chain.companion[5] = 9;
  // The original recursed into a missing actor or around a loop forever.
  chain.companion[9] = 4;
  CHECK_FALSE(actors::StartActorAppearanceTransition(chain, 0, 0));
  CHECK(chain.state[0] == actors::kAppearanceBlack);
  chain.companion[9] = 5;
  chain.visited.clear();
  CHECK_FALSE(actors::StartActorAppearanceTransition(chain, 0, 1));
  CHECK(chain.visited.size() ==
        static_cast<std::size_t>(actors::kActorTableSize));
}

TEST_CASE("Live room scale sets each actor's scale from the stored x",
          "[actors-appearance-rendering][actors][redirect-binding]") {
  LiveAppearanceState state;
  Actors_ActorRecord& first = state.AddActor(0);
  Actors_ActorRecord& last = state.AddActor(16);
  first.scale = 1;
  last.scale = 2;
  const VECTOR room_scale{0x11000, 2048, 1024, 7};
  actors::SetLiveRoomActorScale(&room_scale);
  // #283: the actors took the halfword at the stored x, never a stale view.
  CHECK(first.scale == 0x1000);
  CHECK(last.scale == 0x1000);
  CHECK(Actors_roomActorScale.vx == 0x11000);
  CHECK(Actors_roomActorScale.vy == 2048);
  CHECK(Actors_roomActorScale.vz == 1024);
  CHECK(Actors_roomActorScale.pad == 7);

  ActorsWarnings warnings;
  actors::SetLiveRoomActorScale(nullptr);
  CHECK(warnings.messages ==
        std::vector<std::string>{"SetRoomActorScale refused: null scale"});
  CHECK(first.scale == 0x1000);
}

TEST_CASE("Live weapon requests bind the actor's weapon records",
          "[actors-appearance-rendering][actors][redirect-binding]") {
  LiveAppearanceState state;
  state.AddActor(3);
  Actors_WeaponModel weapon{};
  Actors_WeaponModel shield{};
  weapon.attachmentBone = 1;
  weapon.reserved08Bit4 = 1;
  weapon.reserved11 = 9;
  weapon.reserved13 = 4;
  Actors_weaponModels[6] = &weapon;
  Actors_weaponModels[7] = &shield;
  shield.reserved11 = 33;

  actors::ResetLiveActorWeaponVisibilityBlend(3);
  CHECK(weapon.reserved11 == actors::kWeaponBlendShown);
  CHECK(weapon.reserved12 == actors::kWeaponBlendShown);
  CHECK(weapon.reserved13 == 0);
  CHECK(shield.reserved11 == 33);
}

TEST_CASE("Live weapon brightness keeps the low flag bit and level byte",
          "[actors-appearance-rendering][actors][redirect-binding]") {
  LiveAppearanceState state;
  state.AddActor(3);
  Actors_WeaponModel weapon{};
  weapon.reserved08Bit4 = 1;
  Actors_weaponModels[6] = &weapon;
  actors::SetLiveWeaponBrightnessOverride(3, 3, 0x1FF);
  CHECK(weapon.reserved08Bit6 == 1);
  CHECK(weapon.reserved5C0 == 0xFF);
  CHECK(weapon.reserved08Bit4 == 1);
  actors::SetLiveWeaponBrightnessOverride(3, 2, 7);
  CHECK(weapon.reserved08Bit6 == 0);
  CHECK(weapon.reserved5C0 == 7);
}

TEST_CASE("Live palette fade and shadow blend bind the actor record",
          "[actors-appearance-rendering][actors][redirect-binding]") {
  LiveAppearanceState state;
  Actors_ActorRecord& actor = state.AddActor(3);
  actor.paletteFadeState[0] = 50;
  actors::SetLiveActorPaletteFade(3, 3);
  CHECK(actor.paletteFadeActive == 1);
  CHECK(actor.paletteFadeReverse == 0);
  CHECK(actor.paletteFadeState[0] == 0);
  CHECK(actor.paletteFadeState[1] == 6);

  actor.shadowRadiusExpandRate = 70;
  actor.shadowRadiusShrinkRate = 80;
  actor.shadowBlend.level = 12;
  actors::BeginLiveActorShadowBlend(&actor, 0);
  CHECK(actor.shadowBlend.radiusExpandRate == 70);
  CHECK(actor.shadowBlend.radiusShrinkRate == 80);
  CHECK(actor.shadowBlend.level == 0);
}

TEST_CASE("Live fade color requests bind the actor record",
          "[actors-appearance-rendering][actors][redirect-binding]") {
  LiveAppearanceState state;
  Actors_ActorRecord& actor = state.AddActor(3);
  const Render_ColorCode fade{11, 22, 33, 44};
  CHECK(actors::SetLiveActorFadeParameters(3, &fade) == 0);
  CHECK(actor.fadeRed == 11);
  CHECK(actor.fadeGreen == 22);
  CHECK(actor.fadeBlue == 33);
  Render_ColorCode out{0, 0, 0, 55};
  CHECK(actors::LiveActorFadeParameters(3, &out) == 0);
  CHECK(out.red == 11);
  CHECK(out.blue == 33);
  CHECK(out.code == 55);
}

TEST_CASE("Live tint requests bind the actor's model instance",
          "[actors-appearance-rendering][actors][redirect-binding]") {
  LiveAppearanceState state;
  Actors_ActorRecord& actor = state.AddActor(3);
  const Render_ColorCode tint{1, 2, 3, 4};
  CHECK(actors::SetLiveActorTint(3, &tint) == 0);
  CHECK(actor.customTint == 1);
  CHECK(actor.instance.tint.green == 2);
  CHECK(actors::SetLiveActorTint(3, nullptr) == 0);
  CHECK(actor.customTint == 0);
  Render_ColorCode tint_out{};
  CHECK(actors::LiveActorTint(3, &tint_out) == 0);
  CHECK(tint_out.code == 4);
}

TEST_CASE("Live palette pulse requests bind the actor's model instance",
          "[actors-appearance-rendering][actors][redirect-binding]") {
  LiveAppearanceState state;
  Actors_ActorRecord& actor = state.AddActor(3);
  actor.instance.palettePulse = {
      .kind = 0x80, .level = 5, .flags = 0, .parameter = 0};
  CHECK(actors::EnableLiveModelPalettePulse(3, 1, 1) == 0);
  CHECK(actor.instance.palettePulse.kind == 0xC0);
  CHECK(actor.instance.palettePulse.level == 5);
  CHECK(actors::EnableLiveModelPalettePulse(3, 1, 8) ==
        static_cast<int>(AppearanceStatus::kBadPulseSlot));
  CHECK(actors::DisableLiveModelPalettePulse(3, 0) == 0);
  CHECK(actor.instance.palettePulse.kind == 0x40);
}

TEST_CASE("Live appearance requests treat an empty actor slot as no model",
          "[actors-appearance-rendering][actors][redirect-binding]") {
  LiveAppearanceState state;
  ActorsWarnings warnings;
  const int no_model = static_cast<int>(AppearanceStatus::kNoModel);
  Render_ColorCode color{};

  // An empty slot is the original's no-model result, without a report.
  CHECK(actors::LiveActorTint(4, &color) == no_model);
  CHECK(actors::SetLiveActorTint(4, &color) == no_model);
  CHECK(actors::DisableLiveModelPalettePulse(4, 0) == no_model);
  actors::SetLiveActorPaletteFade(4, 1);
  actors::ResetLiveActorWeaponVisibilityBlend(4);
  actors::SetLiveWeaponBrightnessOverride(4, 1, 1);
  CHECK(warnings.messages.empty());
}

TEST_CASE("Live appearance requests refuse bad ids and null colors",
          "[actors-appearance-rendering][actors][redirect-binding]") {
  LiveAppearanceState state;
  state.AddActor(2);
  ActorsWarnings warnings;
  const int no_model = static_cast<int>(AppearanceStatus::kNoModel);
  Render_ColorCode color{};
  CHECK(actors::LiveActorTint(17, &color) == no_model);
  CHECK(actors::SetLiveActorFadeParameters(-1, &color) == no_model);
  CHECK(actors::LiveActorTint(2, nullptr) == no_model);
  CHECK(actors::LiveActorFadeParameters(2, nullptr) == no_model);
  CHECK(actors::SetLiveActorFadeParameters(2, nullptr) == no_model);
  actors::BeginLiveActorShadowBlend(nullptr, 3);
  actors::SetLiveWeaponBrightnessOverride(12, 1, 1);
  const std::string outside_actors = " is outside the actor table";
  CHECK(warnings.messages ==
        std::vector<std::string>{
            "ActorTint refused: actor id 17" + outside_actors,
            "SetActorFadeParameters refused: actor id -1" + outside_actors,
            "ActorTint refused: null color",
            "ActorFadeParameters refused: null color",
            "SetActorFadeParameters refused: null color",
            "BeginActorShadowBlend refused: null actor",
            std::string("SetWeaponBrightnessOverride refused: weapon slot 12") +
                " is outside the table",
        });
}

TEST_CASE("Live appearance transitions refuse a broken companion chain",
          "[actors-appearance-rendering][actors][redirect-binding]") {
  LiveAppearanceState state;
  Actors_ActorRecord& lead = state.AddActor(0);
  Actors_ActorRecord& companion = state.AddActor(4);
  lead.companionSlot = 4;
  companion.companionSlot = 1;
  ActorsWarnings warnings;
  actors::StartLiveActorAppearanceTransition(0, 0);
  CHECK(lead.instance.actionAnimationState == actors::kAppearanceBlack);
  CHECK(companion.instance.actionAnimationState == actors::kAppearanceBlack);
  CHECK(warnings.messages.empty());

  companion.companionSlot = 4;
  actors::StartLiveActorAppearanceTransition(0, 1);
  CHECK(lead.instance.actionAnimationState == actors::kAppearanceDelay);
  CHECK(warnings.messages ==
        std::vector<std::string>{
            "StartActorAppearanceTransition refused: the chain from actor 0 "
            "names a missing actor or repeats"});
}

TEST_CASE("Appearance bindings retire the module entries they replace",
          "[actors-appearance-rendering][actors][redirect-binding]") {
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  const auto resolve = [&](unsigned address) {
    return VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                             address);
  };
  // The appearance requests are typed C++ now; no BATTLE.PRG image or table
  // stores these addresses.
  constexpr std::array<unsigned, 12> kRetired{
      0x8009D7E8u, 0x8009D88Cu, 0x8009F898u, 0x8009FB64u,
      0x8009FD5Cu, 0x8009FE74u, 0x800A06B8u, 0x800A071Cu,
      0x800A0768u, 0x800A07B4u, 0x800A07FCu, 0x800AC37Cu};
  for (const auto address : kRetired) {
    INFO("address " << address);
    CHECK(resolve(address) == nullptr);
  }
  CHECK(resolve(0x800F4438u) == static_cast<void*>(&Actors_roomActorScale));
}
