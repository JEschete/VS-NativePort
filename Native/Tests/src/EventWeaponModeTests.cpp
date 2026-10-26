#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <span>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Content/ModuleData.h"
#include "Event/WeaponMode.h"
#include "Legacy/Psx.h"

extern "C" {
#include "Actors/ActorRecord.h"
#include "Actors/ActorState.h"
#include "Actors/Animation.h"
#include "Actors/SequencePlayback.h"
#include "Actors/WeaponModel.h"
#include "Battle/Abilities.h"
#include "Battle/Actor.h"
#include "Battle/Damage.h"
#include "Battle/Hud.h"
#include "Event/Actors.h"
#include "Event/Script.h"
#include "Event/ScriptActors.h"
#include "Event/ScriptVm.h"
#include "State/Flags.h"
}

namespace {
using Handler = intptr_t (*)(u_char*, std::int16_t);

auto RecordBytes(const Actors_ActorRecord& record) {
  std::array<std::byte, sizeof(record)> bytes{};
  std::memcpy(bytes.data(), &record, bytes.size());
  return bytes;
}

struct WeaponModeFixture {
  Actors_ActorRecord* saved_record = Actors_actorRecords[0];
  Battle_Actor* saved_actor = Battle_actors[0];
  Battle_Actor* saved_character = Battle_characterState;
  std::array<Actors_WeaponModel*, 2> saved_weapons{Actors_weaponModels[0],
                                                   Actors_weaponModels[1]};
  decltype(std::to_array(Event_weaponDrawStates)) saved_modes =
      std::to_array(Event_weaponDrawStates);
  decltype(std::to_array(Event_kOpcodeHandlers)) saved_handlers =
      std::to_array(Event_kOpcodeHandlers);
  decltype(std::to_array(Event_kOpcodeSizes)) saved_sizes =
      std::to_array(Event_kOpcodeSizes);
  decltype(Event_legacySkipHandlerCopy) saved_skip_handlers =
      Event_legacySkipHandlerCopy;
  char& pending = Event_weaponTransitionPending;
  char saved_pending = pending;
  char saved_skip = Event_skipState;
  char saved_phase = Event_playbackPhase;
  char saved_gate = State_ScriptFlag(0xAC);
  char saved_sequence = Actors_lastResolvedSequenceIndex;
  decltype(Battle_hudBattleMode) saved_hud = Battle_hudBattleMode;
  decltype(Actors_stateFlagsScratch) saved_flags = Actors_stateFlagsScratch;
  Actors_ActorRecord record{};
  Battle_Actor actor{};
  std::array<Actors_WeaponModel, 2> weapons{};
  std::array<std::uint32_t, 512> common{};
  std::array<std::uint32_t, 512> battle{};

  WeaponModeFixture() {
    Actors_actorRecords[0] = &record;
    Battle_characterState = Battle_actors[0] = &actor;
    for (std::size_t index = 0; index < weapons.size(); ++index) {
      Actors_weaponModels[index] = &weapons[index];
      weapons[index].attachmentBone = 1;
    }
    Event_weaponDrawStates[0] = 0;
    Event_weaponDrawStates[1] = 1;
    State_SetScriptFlag(0xAC, 1);
    Event_skipState = 0;
    Event_playbackPhase = 4;
    pending = 0;
    record.commonSequence = MakeBank(common);
    record.battleSequence = MakeBank(battle);
    record.ownedSequence = record.commonSequence;
    record.sequenceFlags = 128;
    record.commonAnimationCount = 128;
    record.framesPerStep = 1;
    record.instance.attachParent = 0xFF;
    record.instance.isVisible = -1;
    record.instance.isFrozen = -1;
    record.instance.soundEnabled = 1;
    record.soundEffect.kind = 2;
    record.modelId = 1;
  }

  ~WeaponModeFixture() {
    Actors_actorRecords[0] = saved_record;
    Battle_actors[0] = saved_actor;
    Battle_characterState = saved_character;
    std::copy(saved_weapons.begin(), saved_weapons.end(), Actors_weaponModels);
    std::copy(saved_modes.begin(), saved_modes.end(), Event_weaponDrawStates);
    std::copy(saved_handlers.begin(), saved_handlers.end(),
              Event_kOpcodeHandlers);
    std::copy(saved_sizes.begin(), saved_sizes.end(), Event_kOpcodeSizes);
    Event_legacySkipHandlerCopy = saved_skip_handlers;
    pending = saved_pending;
    Event_skipState = saved_skip;
    Event_playbackPhase = saved_phase;
    State_SetScriptFlag(0xAC, saved_gate);
    Actors_lastResolvedSequenceIndex = saved_sequence;
    Battle_hudBattleMode = saved_hud;
    Actors_stateFlagsScratch = saved_flags;
  }

  static Actors_SeqAnimationBank* MakeBank(
      std::array<std::uint32_t, 512>& storage) {
    auto* bank = reinterpret_cast<Actors_SeqAnimationBank*>(storage.data());
    bank->animationMap = 1536;
    bank->animationData = 1792;
    auto* map = reinterpret_cast<u_char*>(bank) + bank->animationMap;
    for (std::size_t index = 0; index < 128; ++index) {
      map[index] = static_cast<u_char>(index);
      bank->headers[index * 5] = 4;
      bank->headers[index * 5 + 1] = 0xFF;
    }
    return bank;
  }

  void Prepare(int mode) {
    actor.weaponDrawn = static_cast<u_char>(0xA4 | (mode ^ 1));
    record.instance.weaponDrawn = mode ^ 1;
    for (auto& weapon : weapons) {
      weapon.reserved08Bit4 = mode ^ 1;
      weapon.reserved11 = 17;
      weapon.reserved12 = 23;
      weapon.reserved13 = 9;
    }
  }

  void CheckImmediate(int mode) const {
    CHECK(pending == 0);
    CHECK((actor.weaponDrawn & 1) == mode);
    CHECK((actor.weaponDrawn & 0xFE) == 0xA4);
    CHECK(record.instance.weaponDrawn == mode);
    CHECK(Battle_hudBattleMode == mode);
    CheckVisibility(mode);
  }

  void CheckVisibility(int mode) const {
    for (const auto& weapon : weapons) {
      CHECK(weapon.reserved08Bit4 == mode);
      CHECK(weapon.reserved11 == (mode ? 64 : 0));
      CHECK(weapon.reserved12 == (mode ? 64 : 0));
      CHECK(weapon.reserved13 == 0);
    }
  }

  void AdvanceFrames(int count) {
    for (int frame = 0; frame < count; ++frame) {
      Actors_AdvanceSkeletalAnimation(&record, &record.pose, 0);
    }
  }

  void FinishAnimation(int count = 5) {
    REQUIRE(record.animationLength == 4);
    for (int frame = 0; frame < count; ++frame) {
      CHECK(record.pose.holdingLastFrame == 0);
      Actors_AdvanceSkeletalAnimation(&record, &record.pose, 0);
    }
    CAPTURE(record.animationId, record.queuedAnimation, record.pose.frame);
    REQUIRE(record.pose.holdingLastFrame != 0);
  }

  void CheckAnimatedStart(int mode, int variant, int phase) const {
    CHECK(pending == 1);
    CHECK(record.animationId == (variant - 1) * 2 + (mode ^ 1) + 109);
    CHECK(record.animationLength == 4);
    CHECK(record.pose.holdingLastFrame == 0);
    CHECK(record.queuedAnimation == (phase == 0 && variant == 2 ? 1 : 0));
  }

  void CheckAnimatedFinish(int mode, bool retained_idle = false) const {
    CHECK(pending == 0);
    CHECK(record.animationId == (mode ? 101 : 6));
    CHECK(record.pose.frame == (retained_idle ? 4 : 0));
    CHECK((record.pose.holdingLastFrame != 0) == retained_idle);
  }

  void CheckVmWait(u_char* cursor, u_char* script) const {
    REQUIRE(cursor == script);
    CHECK(pending == 1);
  }

  void CheckVmFinish(u_char* cursor, int mode) const {
    CHECK(cursor == nullptr);
    CHECK(pending == 0);
    CHECK(record.animationId == (mode ? 101 : 6));
  }

  void CheckVmSkip(u_char* cursor, int mode) const {
    CHECK(cursor == nullptr);
    CheckImmediate(mode);
  }

  static Handler ResolveHandler() {
    const auto symbols =
        std::span(Content_moduleSymbols, Content_moduleSymbolCount);
    void* resolved = VS::Content::ResolveModuleAddress(
        symbols, "BATTLE/BATTLE.PRG", 0x800B9DB4u);
    REQUIRE(resolved != nullptr);
    const auto handler = reinterpret_cast<Handler>(resolved);
    CHECK(handler == Event_ScriptSetAshleyBattleMode);
    CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                            0x800B9DB5u) == nullptr);
    return handler;
  }

  static u_char Opcode(Handler handler) {
    auto* found = std::find(std::begin(Event_kSkipOpcodeHandlers),
                            std::end(Event_kSkipOpcodeHandlers), handler);
    REQUIRE(found != std::end(Event_kSkipOpcodeHandlers));
    return static_cast<u_char>(found - std::begin(Event_kSkipOpcodeHandlers));
  }
};
}  // namespace

TEST_CASE("Script weapon mode honors its story gate without changing actors",
          "[event][weapon-mode]") {
  WeaponModeFixture fixture;
  fixture.Prepare(1);
  State_SetScriptFlag(0xAC, 0);
  const auto before = RecordBytes(fixture.record);
  std::array<u_char, 3> command{0, 1, 1};
  CHECK(Event_ScriptSetAshleyBattleMode(command.data(), 0) == 0);
  CHECK(fixture.pending == 0);
  CHECK((fixture.actor.weaponDrawn & 1) == 0);
  CHECK(RecordBytes(fixture.record) == before);
}

TEST_CASE("Script weapon mode applies immediate and skipped final visibility",
          "[event][weapon-mode]") {
  const int mode = GENERATE(0, 1);
  const int skip = GENERATE(0, 2);
  WeaponModeFixture fixture;
  fixture.Prepare(mode);
  Event_skipState = static_cast<char>(skip);
  std::array<u_char, 3> command{0, static_cast<u_char>(mode),
                                static_cast<u_char>(skip ? 3 : 0)};
  REQUIRE(Event_ScriptSetAshleyBattleMode(command.data(), 0) == 0);
  fixture.CheckImmediate(mode);
}

TEST_CASE("Script weapon mode waits for real skeletal animation completion",
          "[event][weapon-mode]") {
  const int mode = GENERATE(0, 1);
  const int variant = GENERATE(1, 2, 3);
  const int phase = GENERATE(0, 4);
  CAPTURE(mode, variant, phase);
  WeaponModeFixture fixture;
  fixture.Prepare(mode);
  Event_playbackPhase = static_cast<char>(phase);
  std::array<u_char, 3> command{0, static_cast<u_char>(mode),
                                static_cast<u_char>(variant)};
  REQUIRE(Event_ScriptSetAshleyBattleMode(command.data(), 0) == 1);
  fixture.CheckAnimatedStart(mode, variant, phase);
  CHECK(Event_ScriptSetAshleyBattleMode(command.data(), 0) == 1);
  CHECK(fixture.record.pose.frame == 0);
  fixture.FinishAnimation(phase == 0 && variant == 2 ? 10 : 5);
  REQUIRE(Event_ScriptSetAshleyBattleMode(command.data(), 0) == 0);
  fixture.CheckAnimatedFinish(mode, phase == 0 && variant == 2 && mode == 1);
}

TEST_CASE("Script weapon mode also waits for the original queued idle",
          "[event][weapon-mode]") {
  const int mode = GENERATE(0, 1);
  WeaponModeFixture fixture;
  fixture.Prepare(mode);
  Event_playbackPhase = 0;
  std::array<u_char, 3> command{0, static_cast<u_char>(mode), 2};
  REQUIRE(Event_ScriptSetAshleyBattleMode(command.data(), 0) == 1);
  CHECK(fixture.record.queuedAnimation == 1);
  fixture.AdvanceFrames(5);
  CHECK(fixture.record.animationId == (mode ? 101 : 1));
  CHECK(fixture.record.queuedAnimation == 0);
  CHECK(fixture.record.pose.frame == 0);
  REQUIRE(Event_ScriptSetAshleyBattleMode(command.data(), 0) == 1);
  CHECK(fixture.pending == 1);
  fixture.FinishAnimation();
  REQUIRE(Event_ScriptSetAshleyBattleMode(command.data(), 0) == 0);
  fixture.CheckAnimatedFinish(mode, mode == 1);
}

TEST_CASE("Skipping an active weapon transition clears its wait without replay",
          "[event][weapon-mode]") {
  const int mode = GENERATE(0, 1);
  WeaponModeFixture fixture;
  fixture.Prepare(mode);
  std::array<u_char, 3> command{0, static_cast<u_char>(mode), 1};
  REQUIRE(Event_ScriptSetAshleyBattleMode(command.data(), 0) == 1);
  const auto animation = fixture.record.animationId;
  Event_skipState = 2;
  State_SetScriptFlag(0xAC, 0);
  REQUIRE(Event_ScriptSetAshleyBattleMode(command.data(), 0) == 0);
  fixture.CheckImmediate(mode);
  CHECK(fixture.record.animationId == animation);
}

TEST_CASE("Production event dispatch holds then advances the weapon opcode",
          "[event][weapon-mode]") {
  const int mode = GENERATE(0, 1);
  const int skip = GENERATE(0, 2);
  WeaponModeFixture fixture;
  fixture.Prepare(mode);
  Event_skipState = static_cast<char>(skip);
  const auto weapon =
      WeaponModeFixture::Opcode(Event_ScriptSetAshleyBattleMode);
  const auto stop = WeaponModeFixture::Opcode(Event_CommandStopScriptContext);
  Event_kOpcodeHandlers[weapon] = WeaponModeFixture::ResolveHandler();
  Event_kOpcodeHandlers[stop] = Event_CommandStopScriptContext;
  Event_kOpcodeSizes[weapon] = 3;
  Event_kOpcodeSizes[stop] = 1;
  Event_legacySkipHandlerCopy = Event_kSkipOpcodeHandlers;
  std::array<u_char, 4> script{weapon, static_cast<u_char>(mode), 1, stop};
  auto* cursor = script.data();
  REQUIRE(Event_ExecuteScriptThread(&cursor, 0) == 0);
  if (skip != 0) {
    fixture.CheckVmSkip(cursor, mode);
    return;
  }
  fixture.CheckVmWait(cursor, script.data());
  fixture.FinishAnimation();
  CHECK(Event_ExecuteScriptThread(&cursor, 0) == 0);
  fixture.CheckVmFinish(cursor, mode);
}

TEST_CASE(
    "Script weapon mode uses its native table without inventing a boolean "
    "bound",
    "[event][weapon-mode]") {
  WeaponModeFixture fixture;
  fixture.Prepare(0);
  constexpr auto kOperand = std::size(Event_weaponDrawStates) - 1;
  Event_weaponDrawStates[kOperand] = 0;
  Event_skipState = 2;
  std::array<u_char, 3> command{0, static_cast<u_char>(kOperand), 255};
  REQUIRE(Event_ScriptSetAshleyBattleMode(command.data(), 0) == 0);
  CHECK(fixture.pending == 0);
  CHECK((fixture.actor.weaponDrawn & 1) == 0);
  CHECK(fixture.record.instance.weaponDrawn == 0);
  CHECK(Battle_hudBattleMode == 0);
  fixture.CheckVisibility(1);
}
