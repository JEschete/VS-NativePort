#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>

struct Effects_EffectInstance;

// Live binding for the effect script loop (EffectScript.hpp) over an effect
// instance: the cursor is the instance's program pointer, bounded by its
// recorded block-8 extent (PackageExtents.hpp), and its script offset. The
// entries keep the original signatures for the opcode handler table and the
// copied handlers that read operands. A null instance, a read past the
// recorded program and an opcode past the handler table are refused with a
// diagnostic; a refused operand read returns 0.
namespace VS::Effects {
// func_800D46DC (5BF94.c:7506).
void LiveBranchEffectScriptIf(int condition, Effects_EffectInstance* instance);
// func_800D4720 (5BF94.c:7515).
int LiveYieldEffectScriptAtOffset(Effects_EffectInstance* instance);
// func_800D474C (5BF94.c:7521).
int LiveJumpEffectScript(Effects_EffectInstance* instance);
// func_800D47C0 (5BF94.c:7538).
int LiveEndEffectScript(Effects_EffectInstance* instance);
// func_800D4DA8 (5BF94.c:7711).
int LiveContinueEffectScript(Effects_EffectInstance* instance);
// func_800D5170 (5BF94.c:7804).
std::uint8_t LiveReadScriptByte(Effects_EffectInstance* instance);
// func_800D5198 (5BF94.c:7806).
std::uint16_t LiveReadScriptHalfword(Effects_EffectInstance* instance);
// func_800D51D8 (5BF94.c:7813): 0 when the instance ended, 1 when it yielded.
int LiveRunEffectScript(Effects_EffectInstance* instance);

// The register and branch opcodes. A refused operand, and a register operand
// past the instance's four registers, end the instance (2) with a diagnostic.
// func_800D47C8 (5BF94.c:7540).
int LiveSetEffectScriptSelector(Effects_EffectInstance* instance);
// func_800D487C (5BF94.c:7563).
int LiveBranchIfEffectRenderersFinished(Effects_EffectInstance* instance);
// func_800D48E4 (5BF94.c:7576).
int LiveBranchIfActionVariantCleared(Effects_EffectInstance* instance);
// func_800D4AB4 (5BF94.c:7625).
int LiveLoadActionVariantRegister(Effects_EffectInstance* instance);
// func_800D4AEC (5BF94.c:7633).
int LiveSetEffectScriptRegister(Effects_EffectInstance* instance);
// func_800D4B34 (5BF94.c:7639).
int LiveBranchIfEffectRegisterEqual(Effects_EffectInstance* instance);
// func_800D4B90 (5BF94.c:7645).
int LiveBranchIfParticleCountEqual(Effects_EffectInstance* instance);
// func_800D4D98 (5BF94.c:7705): only clears the action variant.
int LiveClearScriptActionVariant(Effects_EffectInstance* instance);

// The transform, signal and particle colour opcodes. Heights apply to the
// package's transforms up to the instance's 16 (a larger count is clamped
// with a diagnostic); a definition past the package's and a colour preset
// past the seven end the instance (2) with a diagnostic.
// func_800D49A4 (5BF94.c:7602).
int LiveSetActionEffectVerticalOffset(Effects_EffectInstance* instance);
// func_800D4D64 (5BF94.c:7697).
int LiveSignalSourceActorFromScript(Effects_EffectInstance* instance);
// func_800D4F00 (5BF94.c:7749).
int LiveSetParticleColorCurvesFromScript(Effects_EffectInstance* instance);
// func_800D4FB4 (5BF94.c:7762).
int LiveApplyPairedParticleColorPreset(Effects_EffectInstance* instance);
// func_800D5048 (5BF94.c:7776): a parent whose running-child count was
// already 0 wraps to 31 as the original did, with a diagnostic.
int LiveCompleteChildEffectScript(Effects_EffectInstance* instance);
// func_800D5088 (5BF94.c:7785).
int LiveApplyParticleColorPresetTable(Effects_EffectInstance* instance);
// func_800D5150 (5BF94.c:7799): reports the invalid instruction and yields.
int LiveReportInvalidEffectInstruction(Effects_EffectInstance* instance);

// func_800CE644 (5BF94.c:5045): the original's empty diagnostic hook, now a
// rate-limited warning naming the code.
void LiveReportEffectDiagnostic(int code);
}  // namespace VS::Effects
