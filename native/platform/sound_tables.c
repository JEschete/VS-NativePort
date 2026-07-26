/* The sound driver's three dispatch tables, rebuilt as real C.
 *
 * On PSX these live in .data as arrays of absolute code addresses
 * (build/src/SLUS_010.40/1FD44.data.s). The native build cannot use them: the
 * addresses are MIPS text addresses that mean nothing here, and the stub
 * generator -- which knows only that they are data symbols nobody defines --
 * emits each as a blob of zeroes. Calling through one jumps to address 0.
 *
 * Every entry below is a decompiled C function that is already linked into the
 * native binary, so the tables are transcribed verbatim from the .data listing
 * and just resolve to the real thing at link time. The one exception is
 * func_8001733C (command 0x22), which is still assembly and so resolves to the
 * generated stub.
 *
 *   D_800301E0  driver commands, indexed by the byte func_80018C30 receives
 *   D_800305E0  sound-VM opcodes 0xA0..0xFF, indexed by opcode - 0xA0
 *   D_80030760  extended opcodes, the operand byte after a 0xFE opcode
 *
 * The handlers are declared here with one uniform shape. A few are written
 * (void) or (signed char*) in 25AC.c, but this is a separate translation unit
 * and they are all cdecl, so the call is well-formed either way.
 */

typedef void (*SoundCmdHandler)(void* params);
typedef void (*SoundVMHandler)(void* channel, int arg);

/* g_Sound_LfoTable has the same problem for a different reason: it is not code
 * addresses but sixteen pointers to the vibrato/tremolo/auto-pan waveforms in
 * .data. The waveforms themselves are harvested (SOUND_HARVEST in
 * Makefile.native); only the pointer array has to be rebuilt here. Order is
 * from the retail table at 0x800301A0. */
extern short D_8002F99C[], D_8002F9B4[], D_8002F9C0[], D_8002F9D4[];
extern short D_8002F9E0[], D_8002F9F4[], D_8002FA00[], D_8002FB40[];
extern short D_8002FC6C[], D_8002FCF4[], D_8002FB94[], D_8002FC3C[];
extern short D_8002FD1C[], D_8002FF24[], D_8003012C[], D_80030184[];

short* g_Sound_LfoTable[16] = {
    D_8002F99C, D_8002F9B4, D_8002F9C0, D_8002F9D4,
    D_8002F9E0, D_8002F9F4, D_8002FA00, D_8002FB40,
    D_8002FC6C, D_8002FCF4, D_8002FB94, D_8002FC3C,
    D_8002FD1C, D_8002FF24, D_8003012C, D_80030184,
};

#define C(name) extern void name(void*);
C(Sound_Cmd_10_StartFieldMusic) C(Sound_Cmd_11_Nop) C(Sound_Cmd_11_StopAllMusic)
C(Sound_Cmd_12_unk) C(Sound_Cmd_19_Unk) C(Sound_Cmd_1A_Unk) C(Sound_Cmd_20_unk)
C(Sound_Cmd_21_unk) C(func_8001733C) C(Sound_Cmd_30_unk) C(Sound_Cmd_34_unk)
C(Sound_Cmd_40_PushMusicState) C(Sound_Cmd_80_unk) C(Sound_Cmd_81_unk)
C(Sound_Cmd_90_unk) C(Sound_Cmd_92_unk) C(Sound_Cmd_9A_unk) C(Sound_Cmd_9B_unk)
C(Sound_Cmd_9C_unk) C(Sound_Cmd_9D_unk) C(Sound_Cmd_9E_unk) C(Sound_Cmd_9F_unk)
C(Sound_Cmd_A0_unk) C(Sound_Cmd_A1_unk) C(Sound_Cmd_A2_unk) C(Sound_Cmd_A3_unk)
C(Sound_Cmd_A4_unk) C(Sound_Cmd_A5_unk) C(Sound_Cmd_A8_unk) C(Sound_Cmd_A9_unk)
C(Sound_Cmd_AA_unk) C(Sound_Cmd_AB_unk) C(Sound_Cmd_AC_unk) C(Sound_Cmd_AD_unk)
C(Sound_Cmd_C0_unk) C(Sound_Cmd_C1_unk) C(Sound_Cmd_C2_unk) C(Sound_Cmd_C8_unk)
C(Sound_Cmd_C9_unk) C(Sound_Cmd_CA_unk) C(func_8001833C) C(func_80018358)
C(func_800183C0) C(func_80018434) C(func_80018450) C(func_800184B8)
C(func_8001D644) C(func_8001D698) C(Sound_Cmd_E4_SetCutsceneVolume)
C(Sound_Cmd_E5_FadeOutCutscene) C(func_8001D7A8) C(func_8001852C)
C(func_80018608)
#undef C

/* 0x100 entries. Everything not named here is Sound_Cmd_11_Nop, which is what
 * the retail table holds in all 200-odd unused slots. */
SoundCmdHandler D_800301E0[0x100] = {
    [0x00 ... 0xFF] = Sound_Cmd_11_Nop,
    [0x10] = Sound_Cmd_10_StartFieldMusic,
    [0x11] = Sound_Cmd_11_StopAllMusic,
    [0x12] = Sound_Cmd_12_unk,
    [0x19] = Sound_Cmd_19_Unk,
    [0x1A] = Sound_Cmd_1A_Unk,
    [0x20] = Sound_Cmd_20_unk,
    [0x21] = Sound_Cmd_21_unk,
    [0x22] = func_8001733C,
    [0x30] = Sound_Cmd_30_unk,
    [0x34] = Sound_Cmd_34_unk,
    [0x40] = Sound_Cmd_40_PushMusicState,
    [0x80] = Sound_Cmd_80_unk,
    [0x81] = Sound_Cmd_81_unk,
    [0x90] = Sound_Cmd_90_unk,
    [0x92] = Sound_Cmd_92_unk,
    [0x9A] = Sound_Cmd_9A_unk,
    [0x9B] = Sound_Cmd_9B_unk,
    [0x9C] = Sound_Cmd_9C_unk,
    [0x9D] = Sound_Cmd_9D_unk,
    [0x9E] = Sound_Cmd_9E_unk,
    [0x9F] = Sound_Cmd_9F_unk,
    [0xA0] = Sound_Cmd_A0_unk,
    [0xA1] = Sound_Cmd_A1_unk,
    [0xA2] = Sound_Cmd_A2_unk,
    [0xA3] = Sound_Cmd_A3_unk,
    [0xA4] = Sound_Cmd_A4_unk,
    [0xA5] = Sound_Cmd_A5_unk,
    [0xA8] = Sound_Cmd_A8_unk,
    [0xA9] = Sound_Cmd_A9_unk,
    [0xAA] = Sound_Cmd_AA_unk,
    [0xAB] = Sound_Cmd_AB_unk,
    [0xAC] = Sound_Cmd_AC_unk,
    [0xAD] = Sound_Cmd_AD_unk,
    [0xC0] = Sound_Cmd_C0_unk,
    [0xC1] = Sound_Cmd_C1_unk,
    [0xC2] = Sound_Cmd_C2_unk,
    [0xC8] = Sound_Cmd_C8_unk,
    [0xC9] = Sound_Cmd_C9_unk,
    [0xCA] = Sound_Cmd_CA_unk,
    [0xD0] = func_8001833C,
    [0xD1] = func_80018358,
    [0xD2] = func_800183C0,
    [0xD4] = func_80018434,
    [0xD5] = func_80018450,
    [0xD6] = func_800184B8,
    [0xE0] = func_8001D644,
    [0xE2] = func_8001D698,
    [0xE4] = Sound_Cmd_E4_SetCutsceneVolume,
    [0xE5] = Sound_Cmd_E5_FadeOutCutscene,
    [0xE6] = func_8001D7A8,
    [0xF0] = func_8001852C,
    [0xF1] = func_80018608,
};

#define V(name) extern void name(void*, int);
V(SoundVM_A0_FinishChannel) V(SoundVM_A1_LoadInstrument)
V(SoundVM_A2_OverwriteNextNoteLength) V(SoundVM_A3_ChannelMasterVolume)
V(SoundVM_A4_PitchBendSlide) V(SoundVM_A5_SetOctave)
V(SoundVM_A6_IncreaseOctave) V(SoundVM_A7_DecreaseOctave)
V(SoundVM_A8_ChannelVolume) V(SoundVM_A9_ChannelVolumeSlide)
V(SoundVM_AA_ChannelPan) V(SoundVM_AB_ChannelPanSlide)
V(SoundVM_AC_NoiseClockFrequency) V(SoundVM_AD_AttackRate)
V(SoundVM_AE_DecayRate) V(SoundVM_AF_SustainLevel)
V(SoundVM_B0_DecayRateAndSustainLevel) V(SoundVM_B1_SustainRate)
V(SoundVM_B2_ReleaseRate) V(SoundVM_B3_ResetAdsr) V(SoundVM_B4_Vibrato)
V(SoundVM_B5_VibratoDepth) V(SoundVM_B6_DisableVibrato) V(SoundVM_B7_AttackMode)
V(SoundVM_B8_Tremelo) V(SoundVM_B9_TremeloDepth) V(SoundVM_BA_DisableTremelo)
V(SoundVM_BB_SustainMode) V(SoundVM_BC_AutoPan) V(SoundVM_BD_AutoPanDepth)
V(SoundVM_BE_DisableAutoPan) V(SoundVM_BF_ReleaseMode)
V(SoundVM_C0_ChannelTranspose_Absolute) V(SoundVM_C1_ChannelTranspose_Relative)
V(SoundVM_C2_EnableReverbVoices) V(SoundVM_C3_DisableReverbVoices)
V(SoundVM_C4_EnableNoiseVoices) V(SoundVM_C5_DisableNoiseVoices)
V(SoundVM_C6_EnableFmVoices) V(SoundVM_C7_DisableFmVoices)
V(SoundVM_C8_LoopPoint) V(SoundVM_C9_LoopN) V(SoundVM_CA_LoopInf)
V(SoundVM_CB_DisableVoiceModes) V(SoundVM_CC_EnableLegato) V(SoundVM_CD_DEBUG)
V(SoundVM_CE_EnableNoiseAndDelayToggle) V(SoundVM_CF_ToggleNoiseOnDelay)
V(SoundVM_D0_EnableSustainedNote) V(SoundVM_D1_DEBUG)
V(SoundVM_D2_EnableFmAndDelayToggle) V(SoundVM_D3_ToggleFmDelay)
V(SoundVM_D4_EnablePlaybackRateSidechain)
V(SoundVM_D5_DisablePlaybackRateSidechain)
V(SoundVM_D6_EnablePitchVolumeSidechain)
V(SoundVM_D7_DisablePitchVolumeSidechain)
V(SoundVM_D8_ChannelFineTune_Absolute) V(SoundVM_D9_ChannelFineTune_Relative)
V(SoundVM_DA_EnablePortamento) V(SoundVM_DB_DisablePortamento)
V(SoundVM_DC_FixNoteLength) V(SoundVM_DD_VibratoDepthSlide)
V(SoundVM_DE_TremeloDepthSlide) V(SoundVM_DF_AutoPanDepthSlide)
V(SoundVM_E0_unk) V(SoundVM_E1_SetRandomPitchDepth)
V(SoundVM_E2_ResetRandomPitchDepth) V(SoundVM_XX_Unimplemented)
V(SoundVM_FE00_SetTempo) V(SoundVM_FE01_SetTempoSlide)
V(SoundVM_FE02_SetMasterReverbDepth) V(SoundVM_FE03_SetMasterReverbSlide)
V(SoundVM_FE04_ClearKeymapTable) V(SoundVM_FE05_MuteVoice)
V(SoundVM_FE06_JumpRelativeOffset) V(SoundVM_FE07_JumpRelativeWithThreshold)
V(SoundVM_FE08_JumpOnNthLoopPass) V(SoundVM_FE09_JumpAndPopStackOnNthLoopPass)
V(SoundVM_FE0A_ClearInstrument) V(SoundVM_FE0B_unk) V(SoundVM_FE0C_unk)
V(SoundVM_FE0E_CallRelativeOffset) V(SoundVM_FE0F_Return) V(SoundVM_FE10_unk)
V(SoundVM_FE11_unk) V(SoundVM_FE12_VolumeBalanceSlide) V(SoundVM_FE14_ChangePatch)
V(SoundVM_FE15_unk) V(SoundVM_FE16_unk) V(SoundVM_FE19_unk) V(SoundVM_FE1A_unk)
V(SoundVM_FE1B_unk) V(SoundVM_FE1C_unk) V(SoundVM_FE1D_MarkVoicesKeyed)
V(SoundVM_FE1E_ClearVoicesKeyed) V(SoundVM_FE1F_unk)
#undef V

/* 0x60 entries, indexed by opcode - 0xA0. The tail past 0xE2 is unimplemented
 * in the retail table too. */
SoundVMHandler D_800305E0[0x60] = {
    [0x00 ... 0x5F] = SoundVM_XX_Unimplemented,
    [0x00] = SoundVM_A0_FinishChannel,
    [0x01] = SoundVM_A1_LoadInstrument,
    [0x02] = SoundVM_A2_OverwriteNextNoteLength,
    [0x03] = SoundVM_A3_ChannelMasterVolume,
    [0x04] = SoundVM_A4_PitchBendSlide,
    [0x05] = SoundVM_A5_SetOctave,
    [0x06] = SoundVM_A6_IncreaseOctave,
    [0x07] = SoundVM_A7_DecreaseOctave,
    [0x08] = SoundVM_A8_ChannelVolume,
    [0x09] = SoundVM_A9_ChannelVolumeSlide,
    [0x0A] = SoundVM_AA_ChannelPan,
    [0x0B] = SoundVM_AB_ChannelPanSlide,
    [0x0C] = SoundVM_AC_NoiseClockFrequency,
    [0x0D] = SoundVM_AD_AttackRate,
    [0x0E] = SoundVM_AE_DecayRate,
    [0x0F] = SoundVM_AF_SustainLevel,
    [0x10] = SoundVM_B0_DecayRateAndSustainLevel,
    [0x11] = SoundVM_B1_SustainRate,
    [0x12] = SoundVM_B2_ReleaseRate,
    [0x13] = SoundVM_B3_ResetAdsr,
    [0x14] = SoundVM_B4_Vibrato,
    [0x15] = SoundVM_B5_VibratoDepth,
    [0x16] = SoundVM_B6_DisableVibrato,
    [0x17] = SoundVM_B7_AttackMode,
    [0x18] = SoundVM_B8_Tremelo,
    [0x19] = SoundVM_B9_TremeloDepth,
    [0x1A] = SoundVM_BA_DisableTremelo,
    [0x1B] = SoundVM_BB_SustainMode,
    [0x1C] = SoundVM_BC_AutoPan,
    [0x1D] = SoundVM_BD_AutoPanDepth,
    [0x1E] = SoundVM_BE_DisableAutoPan,
    [0x1F] = SoundVM_BF_ReleaseMode,
    [0x20] = SoundVM_C0_ChannelTranspose_Absolute,
    [0x21] = SoundVM_C1_ChannelTranspose_Relative,
    [0x22] = SoundVM_C2_EnableReverbVoices,
    [0x23] = SoundVM_C3_DisableReverbVoices,
    [0x24] = SoundVM_C4_EnableNoiseVoices,
    [0x25] = SoundVM_C5_DisableNoiseVoices,
    [0x26] = SoundVM_C6_EnableFmVoices,
    [0x27] = SoundVM_C7_DisableFmVoices,
    [0x28] = SoundVM_C8_LoopPoint,
    [0x29] = SoundVM_C9_LoopN,
    [0x2A] = SoundVM_CA_LoopInf,
    [0x2B] = SoundVM_CB_DisableVoiceModes,
    [0x2C] = SoundVM_CC_EnableLegato,
    [0x2D] = SoundVM_CD_DEBUG,
    [0x2E] = SoundVM_CE_EnableNoiseAndDelayToggle,
    [0x2F] = SoundVM_CF_ToggleNoiseOnDelay,
    [0x30] = SoundVM_D0_EnableSustainedNote,
    [0x31] = SoundVM_D1_DEBUG,
    [0x32] = SoundVM_D2_EnableFmAndDelayToggle,
    [0x33] = SoundVM_D3_ToggleFmDelay,
    [0x34] = SoundVM_D4_EnablePlaybackRateSidechain,
    [0x35] = SoundVM_D5_DisablePlaybackRateSidechain,
    [0x36] = SoundVM_D6_EnablePitchVolumeSidechain,
    [0x37] = SoundVM_D7_DisablePitchVolumeSidechain,
    [0x38] = SoundVM_D8_ChannelFineTune_Absolute,
    [0x39] = SoundVM_D9_ChannelFineTune_Relative,
    [0x3A] = SoundVM_DA_EnablePortamento,
    [0x3B] = SoundVM_DB_DisablePortamento,
    [0x3C] = SoundVM_DC_FixNoteLength,
    [0x3D] = SoundVM_DD_VibratoDepthSlide,
    [0x3E] = SoundVM_DE_TremeloDepthSlide,
    [0x3F] = SoundVM_DF_AutoPanDepthSlide,
    [0x40] = SoundVM_E0_unk,
    [0x41] = SoundVM_E1_SetRandomPitchDepth,
    [0x42] = SoundVM_E2_ResetRandomPitchDepth,
};

/* 0x20 entries. In the retail .data the two words after this table are numeric
 * constants, not pointers, which is how the extent is known. */
SoundVMHandler D_80030760[0x20] = {
    [0x00] = SoundVM_FE00_SetTempo,
    [0x01] = SoundVM_FE01_SetTempoSlide,
    [0x02] = SoundVM_FE02_SetMasterReverbDepth,
    [0x03] = SoundVM_FE03_SetMasterReverbSlide,
    [0x04] = SoundVM_FE04_ClearKeymapTable,
    [0x05] = SoundVM_FE05_MuteVoice,
    [0x06] = SoundVM_FE06_JumpRelativeOffset,
    [0x07] = SoundVM_FE07_JumpRelativeWithThreshold,
    [0x08] = SoundVM_FE08_JumpOnNthLoopPass,
    [0x09] = SoundVM_FE09_JumpAndPopStackOnNthLoopPass,
    [0x0A] = SoundVM_FE0A_ClearInstrument,
    [0x0B] = SoundVM_FE0B_unk,
    [0x0C] = SoundVM_FE0C_unk,
    [0x0D] = SoundVM_XX_Unimplemented,
    [0x0E] = SoundVM_FE0E_CallRelativeOffset,
    [0x0F] = SoundVM_FE0F_Return,
    [0x10] = SoundVM_FE10_unk,
    [0x11] = SoundVM_FE11_unk,
    [0x12] = SoundVM_FE12_VolumeBalanceSlide,
    [0x13] = SoundVM_XX_Unimplemented,
    [0x14] = SoundVM_FE14_ChangePatch,
    [0x15] = SoundVM_FE15_unk,
    [0x16] = SoundVM_FE16_unk,
    [0x17] = SoundVM_XX_Unimplemented,
    [0x18] = SoundVM_XX_Unimplemented,
    [0x19] = SoundVM_FE19_unk,
    [0x1A] = SoundVM_FE1A_unk,
    [0x1B] = SoundVM_FE1B_unk,
    [0x1C] = SoundVM_FE1C_unk,
    [0x1D] = SoundVM_FE1D_MarkVoicesKeyed,
    [0x1E] = SoundVM_FE1E_ClearVoicesKeyed,
    [0x1F] = SoundVM_FE1F_unk,
};
