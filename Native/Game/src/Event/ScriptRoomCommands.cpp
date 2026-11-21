// Hand-maintained (tools/port/owned.csv).
#include "Event/ScriptRoomCommands.hpp"

#include <cstddef>
#include <cstdint>

#include "Event/ScriptCommand.hpp"
#include "Event/ScriptVm.hpp"

namespace VS::Event {
namespace {

// Operand offsets after the opcode byte.
constexpr std::size_t kFirstOperand = 1;
constexpr std::size_t kSecondOperand = 2;
constexpr std::size_t kThirdOperand = 3;
constexpr std::size_t kFourthOperand = 4;

[[nodiscard]] constexpr ScriptFlow Advance() {
  return ScriptFlow::Of(ScriptFlowKind::kAdvance);
}

[[nodiscard]] constexpr ScriptFlow Hold() {
  return ScriptFlow::Of(ScriptFlowKind::kHold);
}

[[nodiscard]] constexpr ScriptFlow Yield() {
  return ScriptFlow::Of(ScriptFlowKind::kAdvanceAndYield);
}

// A signed parallax operand in the background's 1/4096 steps.
[[nodiscard]] int ParallaxScale(ScriptCommand command, std::size_t offset) {
  const auto steps = static_cast<std::int8_t>(OperandByte(command, offset));
  return steps * kBackgroundParallaxOperandScale;
}

// A value kept in a halfword, wrapped as the original's stores did.
[[nodiscard]] constexpr std::int16_t Halfword(int value) {
  return static_cast<std::int16_t>(value);
}

[[nodiscard]] ScriptCommand Bytes(const ScriptInstruction& instruction) {
  return instruction.bytes;
}

}  // namespace

// decomp: BATTLE.PRG vs_battle_script_60_setCloudstoneMoving
// (src/BATTLE/BATTLE.PRG/4DEA8.c:2508; was func_800B9770).
ScriptFlow ScriptSetGimmickVisibility(const ScriptInstruction& instruction,
                                      ScriptVmHost& host) {
  if (!InstructionFits(instruction, kSetGimmickVisibilityLength, host)) {
    return Advance();
  }
  const ScriptCommand command = Bytes(instruction);
  const std::uint8_t platform = OperandByte(command, kFirstOperand);
  host.SetMovingPlatformVisibility(platform,
                                   OperandByte(command, kSecondOperand));
  return Advance();
}

// decomp: BATTLE.PRG vs_battle_script_61_overrideFogDepth
// (src/BATTLE/BATTLE.PRG/4DEA8.c:2520; was func_800B979C). The original spreads
// the operand's sign bit over the preserve flag (0 or 0xFF) and sets the depth
// only while that flag is set.
ScriptFlow ScriptOverrideDepthCueDistance(const ScriptInstruction& instruction,
                                          ScriptVmHost& host) {
  if (!InstructionFits(instruction, kOverrideDepthCueDistanceLength, host)) {
    return Advance();
  }
  const auto depth_cue = static_cast<std::uint16_t>(
      OperandHalf(Bytes(instruction), kFirstOperand));
  const bool keeps_depth = (depth_cue & kDepthCueOverrideBit) != 0;
  host.SetScriptBackdropEnabled(keeps_depth);
  if (keeps_depth) {
    host.SetRoomTextureLodDepth(
        static_cast<int>(depth_cue & kDepthCueDepthMask));
  }
  return Advance();
}

// decomp: BATTLE.PRG vs_battle_script_62_setFirstPersonRendering
// (src/BATTLE/BATTLE.PRG/4DEA8.c:2539; was func_800B97E4). The mode byte is
// stored zero-extended into the draw mode word.
ScriptFlow ScriptSetRoomLightingMode(const ScriptInstruction& instruction,
                                     ScriptVmHost& host) {
  if (!InstructionFits(instruction, kSetRoomLightingModeLength, host)) {
    return Advance();
  }
  const std::uint8_t draw_mode = OperandByte(Bytes(instruction), kFirstOperand);
  host.SetRoomGeometryDrawMode(draw_mode);
  return Advance();
}

// decomp: BATTLE.PRG vs_battle_script_64_showGeometry
// (src/BATTLE/BATTLE.PRG/4DEA8.c:2559; was func_800B9818).
ScriptFlow ScriptShowGeometryGroup(const ScriptInstruction& instruction,
                                   ScriptVmHost& host) {
  if (!InstructionFits(instruction, kShowGeometryGroupLength, host)) {
    return Advance();
  }
  const std::uint8_t shown_group =
      OperandByte(Bytes(instruction), kFirstOperand);
  host.ShowRoomGeometryGroup(shown_group);
  return Advance();
}

// decomp: BATTLE.PRG vs_battle_script_65_hideGeometry
// (src/BATTLE/BATTLE.PRG/4DEA8.c:2571; was func_800B983C).
ScriptFlow ScriptHideGeometryGroup(const ScriptInstruction& instruction,
                                   ScriptVmHost& host) {
  if (!InstructionFits(instruction, kHideGeometryGroupLength, host)) {
    return Advance();
  }
  const std::uint8_t hidden_group =
      OperandByte(Bytes(instruction), kFirstOperand);
  host.HideRoomGeometryGroup(hidden_group);
  return Advance();
}

// decomp: BATTLE.PRG vs_battle_script_66_swapGeometryFlags
// (src/BATTLE/BATTLE.PRG/4DEA8.c:2583; was func_800B9860).
ScriptFlow ScriptSwapGeometryGroupFlags(const ScriptInstruction& instruction,
                                        ScriptVmHost& host) {
  if (!InstructionFits(instruction, kSwapGeometryGroupFlagsLength, host)) {
    return Advance();
  }
  const ScriptCommand command = Bytes(instruction);
  const std::uint8_t first_group = OperandByte(command, kFirstOperand);
  const std::uint8_t second_group = OperandByte(command, kSecondOperand);
  host.SwapRoomGeometryGroupFlags(first_group, second_group);
  return Advance();
}

// decomp: BATTLE.PRG vs_battle_script_67_setTileBlocked
// (src/BATTLE/BATTLE.PRG/4DEA8.c:2598; was func_800B988C). The script orders
// the operands x, layer, z, blocked; func_8008B808 takes x, z, layer, blocked.
ScriptFlow ScriptSetCollisionTileMode(const ScriptInstruction& instruction,
                                      ScriptVmHost& host) {
  if (!InstructionFits(instruction, kSetCollisionTileModeLength, host)) {
    return Advance();
  }
  const ScriptCommand command = Bytes(instruction);
  host.SetCollisionCellBlocked(
      {.tile_x = OperandByte(command, kFirstOperand),
       .tile_z = OperandByte(command, kThirdOperand),
       .layer = OperandByte(command, kSecondOperand),
       .blocked = OperandByte(command, kFourthOperand)});
  return Advance();
}

// decomp: BATTLE.PRG vs_battle_script_7D (src/BATTLE/BATTLE.PRG/4DEA8.c:2916;
// was func_800BA288). The bias byte is signed.
ScriptFlow ScriptAdjustGeometryDepthBias(const ScriptInstruction& instruction,
                                         ScriptVmHost& host) {
  if (!InstructionFits(instruction, kAdjustGeometryDepthBiasLength, host)) {
    return Advance();
  }
  const ScriptCommand command = Bytes(instruction);
  const auto depth_bias =
      static_cast<std::int8_t>(OperandByte(command, kSecondOperand));
  host.AddRoomGeometryDepthBias(OperandByte(command, kFirstOperand),
                                depth_bias);
  return Advance();
}

// decomp: BATTLE.PRG vs_battle_script_73 (src/BATTLE/BATTLE.PRG/4DEA8.c:2805;
// was func_800B9F28). The byte is stored as the background's enabled flag
// unchanged.
ScriptFlow ScriptSetRoomBackgroundVisible(const ScriptInstruction& instruction,
                                          ScriptVmHost& host) {
  if (!InstructionFits(instruction, kSetRoomBackgroundVisibleLength, host)) {
    return Advance();
  }
  const std::uint8_t enabled = OperandByte(Bytes(instruction), kFirstOperand);
  host.SetTiledBackgroundEnabled(enabled);
  return Advance();
}

// decomp: BATTLE.PRG vs_battle_script_74 (src/BATTLE/BATTLE.PRG/4DEA8.c:2811;
// was func_800B9F4C). Whether the reads were queued is ignored; the wait opcode
// follows them.
ScriptFlow ScriptLoadRoomBackground(const ScriptInstruction& instruction,
                                    ScriptVmHost& host) {
  if (!InstructionFits(instruction, kLoadRoomBackgroundLength, host)) {
    return Advance();
  }
  const std::uint8_t texture_page =
      OperandByte(Bytes(instruction), kFirstOperand);
  host.QueueTiledBackgroundLoad(texture_page);
  return Advance();
}

// decomp: BATTLE.PRG vs_battle_script_75 (src/BATTLE/BATTLE.PRG/4DEA8.c:2817;
// was func_800B9F70): returns func_8008F0FC() == 0, holding on every step
// before the last.
ScriptFlow ScriptWaitForRoomBackground(const ScriptInstruction& /*instruction*/,
                                       ScriptVmHost& host) {
  const bool loaded = host.FinishTiledBackgroundLoad();
  return loaded ? Advance() : Hold();
}

// decomp: BATTLE.PRG vs_battle_script_76 (src/BATTLE/BATTLE.PRG/4DEA8.c:2822;
// was func_800B9F90): returns (D_800F4C2C == 2) * 4, read after the release.
ScriptFlow ScriptFreeRoomBackground(const ScriptInstruction& /*instruction*/,
                                    ScriptVmHost& host) {
  host.ReleaseTiledBackground();
  if (host.SkipState() == CutsceneSkip::kSkipping) {
    return Yield();
  }
  return Advance();
}

// decomp: BATTLE.PRG vs_battle_script_7B (src/BATTLE/BATTLE.PRG/4DEA8.c:2904;
// was func_800BA218). Each offset is a little-endian halfword sign-extended by
// vs_battle_getShort.
ScriptFlow ScriptSetBackgroundOffset(const ScriptInstruction& instruction,
                                     ScriptVmHost& host) {
  if (!InstructionFits(instruction, kSetBackgroundOffsetLength, host)) {
    return Advance();
  }
  const ScriptCommand command = Bytes(instruction);
  const std::int16_t offset_x = OperandHalf(command, kFirstOperand);
  const std::int16_t offset_y = OperandHalf(command, kThirdOperand);
  host.SetRoomBackgroundOffset(offset_x, offset_y);
  return Advance();
}

// decomp: BATTLE.PRG vs_battle_script_7C (src/BATTLE/BATTLE.PRG/4DEA8.c:2910;
// was func_800BA264). The operand is reduced to 0 or 1 before func_8007BDD8
// takes it as the disabled flag.
ScriptFlow ScriptSetFirstPersonView(const ScriptInstruction& instruction,
                                    ScriptVmHost& host) {
  if (!InstructionFits(instruction, kSetFirstPersonViewLength, host)) {
    return Advance();
  }
  const bool disabled = OperandByte(Bytes(instruction), kFirstOperand) != 0;
  host.SetFirstPersonViewDisabled(disabled);
  return Advance();
}

// decomp: BATTLE.PRG vs_battle_script_7E (src/BATTLE/BATTLE.PRG/4DEA8.c:2922;
// was func_800BA2B4). Both bytes are signed.
ScriptFlow ScriptSetBackgroundParallax(const ScriptInstruction& instruction,
                                       ScriptVmHost& host) {
  if (!InstructionFits(instruction, kSetBackgroundParallaxLength, host)) {
    return Advance();
  }
  const ScriptCommand command = Bytes(instruction);
  host.SetBackgroundParallaxScale(ParallaxScale(command, kFirstOperand),
                                  ParallaxScale(command, kSecondOperand));
  return Advance();
}

// decomp: BATTLE.PRG vs_battle_script_77 (src/BATTLE/BATTLE.PRG/4DEA8.c:2828;
// was func_800B9FC0). The weather is selected first. An immediate change leaves
// a running tween's frames, start and deltas alone. The tween's deltas are kept
// in halfwords, as the original's were; the wind direction's is first moved
// towards the short way round by one full turn at most.
ScriptFlow ScriptSetEnvironmentParticles(const ScriptInstruction& instruction,
                                         ScriptVmHost& host) {
  if (!InstructionFits(instruction, kSetEnvironmentParticlesLength, host)) {
    return Advance();
  }
  const ScriptCommand command = Bytes(instruction);
  const std::uint8_t weather = OperandByte(command, kFirstOperand);
  const auto intensity = static_cast<int>(weather & kWeatherIntensityMask);
  const int direction =
      OperandByte(command, kSecondOperand) * kWeatherDirectionOperandScale;
  const int strength =
      OperandByte(command, kThirdOperand) * kWeatherStrengthOperandScale;
  const std::uint8_t frames = OperandByte(command, kFourthOperand);

  host.SetRoomWeather(weather >> kWeatherKindShift);
  WeatherTween tween = host.EventWeatherTween();
  if (frames == 0) {
    tween.intensity = Halfword(intensity);
    tween.wind_direction = Halfword(direction);
    tween.wind_strength = Halfword(strength + kWeatherStrengthBase);
    host.SetEventWeatherTween(tween);
    host.SetWeatherIntensity(tween.intensity);
    host.SetWeatherWindAngles(tween.wind_direction, tween.wind_strength);
    return Advance();
  }
  tween.start_intensity = tween.intensity;
  tween.delta_intensity = Halfword(intensity - tween.intensity);
  tween.start_wind_strength = tween.wind_strength;
  tween.delta_wind_strength =
      Halfword(strength - (tween.wind_strength - kWeatherStrengthBase));
  tween.start_wind_direction = tween.wind_direction;
  int turn = direction - tween.wind_direction;
  if (turn < -kWeatherHalfTurn) {
    turn += kWeatherFullTurn;
  } else if (turn > kWeatherHalfTurn) {
    turn -= kWeatherFullTurn;
  }
  tween.delta_wind_direction = Halfword(turn);
  tween.duration = frames;
  tween.elapsed = 0;
  host.SetEventWeatherTween(tween);
  return Advance();
}

// decomp: BATTLE.PRG vs_battle_script_78 (src/BATTLE/BATTLE.PRG/4DEA8.c:2859;
// was func_800BA0E4). Only the operand's low bit is inverted; func_80091314
// stores the result as the enabled flag.
ScriptFlow ScriptSetParticleVariationEnabled(
    const ScriptInstruction& instruction, ScriptVmHost& host) {
  if (!InstructionFits(instruction, kSetParticleVariationEnabledLength, host)) {
    return Advance();
  }
  const std::uint8_t disabled = OperandByte(Bytes(instruction), kFirstOperand);
  host.SetWeatherVariationEnabled(static_cast<int>(disabled ^ 1U));
  return Advance();
}

// decomp: BATTLE.PRG vs_battle_script_7A (src/BATTLE/BATTLE.PRG/4DEA8.c:2865;
// was func_800BA108). Muting while already muted keeps the first saved sound;
// the saved sound is a halfword.
ScriptFlow ScriptMuteEnvironmentSound(const ScriptInstruction& instruction,
                                      ScriptVmHost& host) {
  if (!InstructionFits(instruction, kMuteEnvironmentSoundLength, host)) {
    return Advance();
  }
  const bool mute = OperandByte(Bytes(instruction), kFirstOperand) != 0;
  if (mute) {
    if (host.AmbientSoundSuspended()) {
      return Advance();
    }
    const bool suspended = host.RoomAmbientSequenceAbsent();
    host.SetAmbientSoundSuspended(suspended);
    if (suspended) {
      host.SetSuspendedAmbientSound(Halfword(
          host.ExchangeWeatherAmbientSound(kMutedWeatherAmbientSound)));
    }
  } else if (host.AmbientSoundSuspended()) {
    static_cast<void>(
        host.ExchangeWeatherAmbientSound(host.SuspendedAmbientSound()));
    host.SetAmbientSoundSuspended(false);
  }
  return Advance();
}

// decomp: BATTLE.PRG vs_battle_script_79 (src/BATTLE/BATTLE.PRG/4DEA8.c:2886;
// was func_800BA194). The effect byte is read whatever the mode.
ScriptFlow ScriptControlRoomAnimation(const ScriptInstruction& instruction,
                                      ScriptVmHost& host) {
  if (!InstructionFits(instruction, kControlRoomAnimationLength, host)) {
    return Advance();
  }
  const ScriptCommand command = Bytes(instruction);
  const std::uint8_t mode = OperandByte(command, kFirstOperand);
  const int effect = OperandByte(command, kSecondOperand);
  if (mode == kStartRoomAnimation) {
    if (host.SkipState() == CutsceneSkip::kSkipping &&
        host.RoomEffectActive(effect)) {
      host.FinishRoomDoorRotation(effect);
    }
    host.StartRoomEffect(effect);
  } else if (mode == kToggleRoomAnimation) {
    host.ToggleRoomEffectUpdates(effect);
  }
  return Advance();
}

}  // namespace VS::Event
