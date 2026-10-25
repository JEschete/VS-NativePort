"""Audio globals from code read directly. Citations are resolved by exact text search, never typed."""
import csv
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
SRC = ROOT / "resources" / "rood-reverseWin" / "src"
OUT = ROOT / "tools" / "naming" / "sym_out" / "Audio"
FILES = {"25AC.c": "SLUS_010.40/25AC.c", "main.c": "SLUS_010.40/main.c", "main.h": "SLUS_010.40/main.h",
         "146C.c": "BATTLE/BATTLE.PRG/146C.c", "44F14.c": "BATTLE/BATTLE.PRG/44F14.c", "sound.h": "include/sound.h", "25AC.h": "SLUS_010.40/25AC.h"}
_cache = {}


def cite(f, text):
    lines = _cache.setdefault(f, (SRC / FILES[f]).read_text(encoding="utf-8", errors="replace").splitlines())
    for i, line in enumerate(lines):
        if text in line:
            return f"source:{f}:{i + 1}"
    sys.exit(f"citation text not found in {f}: {text!r}")


def c(*pairs):
    return ";".join(cite(f, t) for f, t in pairs)


A = "Native\\Engine\\src\\Audio\\"
DRV, SEQ, MIX = A + "Driver\\SoundDriver.c", A + "Seq\\Sequencer.c", A + "Mixer\\VoiceMixer.c"
CH, SFX, SMP = A + "Seq\\Channel.c", A + "SoundEffects\\SoundEffects.c", A + "Samples\\SampleBanks.c"
CUT, PLY, CTL = A + "Cutscene\\Stream.c", A + "Playback\\SoundPlayback.c", A + "Control\\SoundControl.c"
BANKS = A + "SoundBanks.c"
rows = []


def G(gid, disp, name, loc, purpose, conf, notes):
    rows.append([gid, disp, name, loc, purpose, conf, notes])


S = "25AC.c"
G("G00091", "catalog", "Audio::objectSurfaceSoundIds", A + "ActorSound.c",
  "Gives the contact sound played when an actor stands on a room object of each kind.", "med",
  c(("44F14.c", "id = D_800E91A4[D_800F45E0[func_800A91DC(arg1->unk0.currentTileX,")) + ";indexed by the object's kind;inventory size is a gap, not a capacity")
G("G00250", "state", "Audio::zoneMusicSwitchEnabled", A + "Music.c",
  "Stops and frees the current music on zone load when the new zone uses a different track.", "high",
  c(("146C.c", "if (D_800F18B0 != 0) {"), ("146C.c", "int func_8007BE10(int arg0) { return D_800F18B0 = arg0; }")))
G("G01826", "drop", "Audio::Driver::legacySpuEvent", DRV, "Holds the root-counter event that runs the driver tick.", "high",
  "replaced-by:native audio thread;" + c((S, "_soundEvent = event;"), (S, "while (EnableEvent(_soundEvent) == 0)")))
G("G01827", "drop", "Audio::Driver::recentTickCostTotal", DRV, "Sums the root-counter cost of the last four driver ticks.", "high",
  "replaced-by:native profiler;" + c((S, "D_8002F548 = D_8002F550[0] + D_8002F550[1] + D_8002F550[2] + D_8002F550[3];")))
G("G01828", "state", "Audio::Seq::tickCounter", SEQ, "Counts driver ticks; every fourth tick advances the secondary music fade.", "high",
  c((S, "++D_8002F54C;"), (S, "if (!(D_8002F54C & 3)) {")))
G("G01829", "drop", "Audio::Driver::recentTickCosts", DRV, "Keeps the root-counter cost of the last four driver ticks.", "high",
  "replaced-by:native profiler;" + c((S, "D_8002F550[3] = var_s5;")))
G("G01830", "drop", "Audio::Driver::legacySpuFlushBytes", DRV, "Supplies the bytes written to SPU memory when the driver starts or stops.", "high",
  "replaced-by:native mixer reset;" + c((S, "_writeSpu(_soundFlush, 64);")))
G("G01831", "constexpr", "Audio::Seq::kOpcodeLengths", SEQ,
  "Gives the byte length of AKAO opcodes 0xA0 to 0xFF; zero marks opcodes that need special handling.", "high",
  c((S, "length = D_8002F5A0[*pc - 0xA0];")) + ";96 entries by address gap")
G("G01832", "constexpr", "Audio::Seq::kExtendedOpcodeLengths", SEQ,
  "Gives the byte length of 0xFE-prefixed AKAO opcodes; zero marks opcodes that need special handling.", "high",
  c((S, "length = D_8002F600[*pc];")) + ";32 entries by address gap")
G("G01833", "constexpr", "Audio::Seq::kNoteLengths", SEQ,
  "Gives the note length selected by a note opcode's remainder modulo 11 when no fixed length is set.", "high",
  c((S, "int tmp = D_8002F620[var_s0 % 11];")))
G("G01834", "constexpr", "Audio::Seq::kIdleProgram", SEQ,
  "Is the program word a released or idle channel points at instead of real bytecode.", "med",
  c((S, "pChannel->ProgramCounter = (char*)&g_Sound_ProgramCounter;")) + ";value comes from the binary data at 0x8002F638")
G("G01835", "constexpr", "Audio::Seq::kSemitonePitches", SEQ,
  "Gives the pitch of each of the twelve semitones before octave shifting and fine tuning.", "high",
  c((S, "var_t0 = D_8002F63C[var_a0] << 8;")) + ";12 entries by address gap;inventory size 352 overlaps later tables")
G("G01836", "constexpr", "Audio::Seq::kNoteRemapTables", SEQ,
  "Holds key remap tables that opcode 0xFE1C selects for the active music by byte offset.", "med",
  c((S, "g_pActiveMusicConfig->unk44 = (FSoundKeymapEntry8*)&D_8002F66C[temp_v0];")) + ";48 bytes by address gap")
G("G01837", "constexpr", "Audio::Seq::kRandomPitchOffsets", SEQ,
  "Gives signed random pitch offsets indexed by the global phase; bit 7 selects a downward shift.", "high",
  c((S, "sp10 *= D_8002F69C[g_Sound_LfoPhase];"), (S, "if (D_8002F69C[g_Sound_LfoPhase] & 0x80) {")))
G("G01838", "constexpr", "Audio::Mixer::kPanGains", MIX, "Gives the Q15 left or right gain for each of 256 pan positions.", "high",
  c((S, "(Volume * g_Sound_StereoPanGainTableQ15[temp]) >> 15;"), (S, "(Volume * g_Sound_StereoPanGainTableQ15[temp ^ 0xFF]) >> 15;")))
G("G01839", "constexpr", "Audio::Mixer::kCenterPanGain", MIX, "Is the pan table's center entry used for mono output.", "high",
  "view-of:G01838+0x100;" + c((S, "in_pChannel->VoiceParams.Volume.right = (Volume * D_8002F89C) >> 15;")))
G("G01840", "constexpr", "Audio::Seq::kLfoWaveforms", SEQ, "Points to the waveform used by each vibrato, tremolo or auto-pan shape.", "high",
  c((S, "in_pChannel->VibratoWave = g_Sound_LfoTable[in_pChannel->VibratoType];")) + ";16 pointers by address gap")
G("G01841", "constexpr", "Audio::Mixer::kCommandHandlers", MIX, "Maps each sound command id to its handler.", "high",
  c((S, "D_800301E0[0xD0](pParams);")) + ";256 entries by address gap")
G("G01842", "constexpr", "Audio::Seq::kOpcodeHandlers", SEQ, "Maps AKAO opcodes 0xA0 to 0xFF to their handlers.", "high",
  c((S, "D_800305E0[var_s0 - 0xA0](arg0, arg1);")))
G("G01843", "constexpr", "Audio::Seq::kExtendedOpcodeHandlers", SEQ, "Maps 0xFE-prefixed AKAO opcodes to their handlers.", "high",
  cite(S, "extern void (*D_80030760[])(FSoundChannel*, int);"))
G("G01844", "state", "Audio::Seq::musicChannels", CH, "Holds the 32 channels of the music being played.", "high",
  c((S, "pChannel = g_ActiveMusicChannels;"), (S, "for (i = 0, c = g_ActiveMusicChannels; i < 32; ++i, ++c) {")))
G("G01845", "state", "Audio::Seq::savedMusicConfig", CH,
  "Holds the configuration of music saved by the push command, also played as the secondary layer.", "high",
  c((S, "Sound_memcpy32(&D_800366F0, &g_PushedMusicConfig, 0x80);"), (S, "g_pSavedMousicConfig = &g_PushedMusicConfig;")) + ";only 0x80 bytes are copied")
G("G01846", "state", "Audio::Seq::savedMusicId", CH, "Is the id of the saved music; cleared to forget it.", "high",
  "view-of:G01845+0x6E;" + c((S, "D_800358FE = 0;")) + ";MusicId read at +0x6E in Sound_Cmd_10_StartFieldMusic")
G("G01847", "state", "Audio::SoundEffects::channels", SFX, "Holds the twelve sound-effect channels.", "high",
  c((S, "for (i = 0, var_a1 = D_80035910; i < 12; ++i, ++var_a1, var_a3 *= 2) {")) + ";declared [10] but loops reach 12 channels ending at _cdlAtv")
G("G01848", "state", "Audio::SoundEffects::programPairChannels", SFX, "Aliases sound-effect channels 10 and 11 used by two-voice programs.", "med",
  "view-of:G01847+0xAA0;" + c((S, "channel = SfxSoundChannels;"), (S, "FSoundChannel* pChannel = SfxSoundChannels - 10;")) + ";declared [12] overstates the 2 channels before _cdlAtv")
G("G01849", "drop", "Audio::Control::legacyDiscMixAttenuation", CTL, "Holds the CD audio mix attenuation sent to the drive.", "high",
  "replaced-by:native stream mixer;" + c((S, "_cdlAtv.val2 = arg0;")))
G("G01850", "state", "Audio::Seq::secondaryMusicChannels", CH, "Points to the channels of the secondary music layer.", "high",
  c((S, "g_pSecondaryMusicChannels = D_800378E8;")))
G("G01851", "state", "Audio::Seq::secondaryMusicConfig", CH, "Points to the secondary layer's configuration or is null when none plays.", "high",
  c((S, "if (g_pSavedMousicConfig != NULL) {"), (S, "g_pSavedMousicConfig = &g_PushedMusicConfig;")))
G("G01852", "state", "Audio::Mixer::voiceEnvelopes", MIX, "Tracks each voice's envelope level for allocation and stealing.", "high",
  c((S, "pVoiceInfo = &g_SpuVoiceInfo[i];"), (S, "g_SpuVoiceInfo[var_a0].pEnvx = 0x7FFF;")))
G("G01853", "state", "Audio::Seq::primaryMusicConfig", CH, "Holds the configuration of the primary music.", "high",
  c((S, "g_pActiveMusicConfig = &D_800366F0;"), (S, "Sound_memcpy32(&D_800366F0, &g_PushedMusicConfig, 0x80);")) + ";declared 160 bytes but only 0x80 are used")
G("G01854", "state", "Audio::Samples::musicSampleBankIds", SMP, "Records the sample bank ids loaded for the current music.", "med",
  c((S, "return (u_short)D_80036770.unk0 | ((u_short)D_80036770.unk4 << 0x10);"), (S, "pSequence->unk14 == D_80036770.unk4 ? 0x40 : 0x20;")))
G("G01855", "state", "Audio::Mixer::musicPitchScaleStep", MIX, "Is the per-tick step of the music pitch scale slide.", "high",
  c((S, "temp_s1 = g_Sound_MasterPitchScaleQ16_16 + D_80036778;")))
G("G01856", "state", "Audio::Mixer::tempoScaleStep", MIX, "Is the per-tick step of the tempo scale slide.", "high",
  c((S, "g_Sound_TempoMultiplier += D_8003677C;")))
G("G01857", "state", "Audio::Mixer::musicPitchScaleSlideLength", MIX, "Counts the ticks left in the music pitch scale slide.", "high",
  c((S, "D_80036780 = (short)var_a1;"), (S, "if ((arg0 & 4) && (var_a1 < D_80036780)) {")))
G("G01858", "state", "Audio::Mixer::tempoScaleSlideLength", MIX, "Counts the ticks left in the tempo scale slide.", "high",
  c((S, "D_80036782 = var_a1;"), (S, "if (var_a1 < D_80036782) {")))
G("G01859", "state", "Audio::Samples::instruments", SMP, "Holds the 256 instrument definitions used to play notes.", "high",
  c((S, "temp_a2_2 = func_8001A4BC(&g_InstrumentInfo[arg0->InstrumentIndex],")))
G("G01860", "state", "Audio::Seq::pendingMusicData", CH, "Holds the next music's data while a transition waits; nonzero means pending.", "med",
  c((S, "D_80037788.Param1 = (u_int)pData;"), (S, "if (D_80037788.Param1 != 0) {")) + ";only Param1 is used, so the declared 24-byte record does not overlap G01861 in practice")
G("G01861", "state", "Audio::Mixer::commandParams", MIX, "Is the parameter record the command dispatcher passes to handlers.", "med",
  c((S, "pParams = &D_80037790;")) + ";starts 8 bytes after G01860")
G("G01862", "drop", "Audio::Driver::legacySpuAllocatorTable", DRV, "Gives SpuInitMalloc its allocation table.", "high",
  "replaced-by:native sample bank storage;" + c((S, "SpuInitMalloc(4, &_spuMemInfo);")))
G("G01863", "state", "Audio::Seq::musicLoopsRemaining", CH, "Counts the loops left before the music may end.", "med",
  c((S, "g_Music_LoopCounter = LoopCounter;"), (S, "--g_Music_LoopCounter;")))
G("G01864", "state", "Audio::Samples::soundEffectBankIds", SMP, "Records the id resident in each of the three sound-effect bank slots.", "med",
  c((S, "if (D_800377E0[i] == arg0[1]) {"), (S, "if (flags == D_800377E0[1]) {")))
G("G01865", "state", "Audio::Playback::soundEffectProgramOffsets", PLY, "Points to the per-effect program offset pairs of the bound effect file.", "high",
  c((S, "g_Sound_SfxProgramOffsets = in_Blob;"), (S, "block0 = (u_short*)g_Sound_SfxProgramOffsets;")))
G("G01866", "drop", "Audio::Cutscene::legacySpuTransferBusy", CUT, "Is set while a stream page is being transferred to SPU memory.", "high",
  "replaced-by:native stream buffer;" + c((S, "_isSpuTransfer = 1;"), (S, "_isSpuTransfer = 0;")))
G("G01867", "state", "Audio::Playback::soundEffectBankSelectors", PLY, "Points to the per-effect value that selects its sample bank slot.", "med",
  c((S, "g_Sound_SfxMetadataTable = in_Blob;"), (S, "arg0->ExtParam1 = func_80016DA8(g_Sound_SfxMetadataTable[arg0->Param1]);")))
G("G01868", "state", "Audio::Playback::soundEffectProgramData", PLY, "Points to the bytecode of the bound effect file.", "high",
  c((S, "g_Sound_SfxProgramData = in_Blob;"), (S, "block2 = g_Sound_SfxProgramData;")))
G("G01869", "state", "Audio::Seq::embeddedSoundEffectParams", SEQ, "Is the command record used to start sound effects embedded in music.", "med",
  c((S, "D_80037800.Param1 = (u_int)p;"), (S, "D_80037800.Param1 = 0;")))
G("G01870", "state", "Audio::Seq::activeMusicConfig", CH, "Points to the configuration of the music the sequencer is advancing.", "high",
  c((S, "g_pActiveMusicConfig->ActiveChannelMask &= ~in_VoiceFlags;")))
G("G01871", "state", "Audio::Seq::masterFade", SEQ, "Fades the music master volume over a number of ticks.", "high",
  c((S, "g_Sound_MasterFadeTimer.Value = 0x7F0000;"), (S, "Volume *= g_Sound_MasterFadeTimer.Value >> 8;")))
G("G01872", "state", "Audio::Seq::discVolumeFadeStep", CTL, "Is the per-tick step of the disc audio volume fade.", "high",
  c((S, "g_Sound_CdVolumeFadeStep = (((u_short)arg0->Param2 << 0x10) - g_CdVolume) / var_a1;")))
G("G01873", "state", "Audio::Seq::discVolumeFadeLength", CTL, "Counts the ticks left in the disc audio volume fade.", "high",
  c((S, "g_Sound_CdVolumeFadeLength = var_a1;")))
G("G01874", "state", "Audio::Samples::bankChunkHeader", SMP, "Keeps the 64-byte header of the instrument bank being streamed.", "med",
  c((S, "Sound_memcpy32(in_Data, D_80037850, 0x40);"), (S, "D_80037850[4] = spuAddr;")))
G("G01875", "state", "Audio::Mixer::voiceScheduler", MIX, "Holds voice key-on, key-off and mode masks and the noise clock.", "high",
  c((S, "g_Sound_VoiceSchedulerState.ReverbVoiceFlags &= ~temp_a2;")))
G("G01876", "state", "Audio::Mixer::pendingKeyOnVoices", MIX, "Aliases the scheduler's key-on mask.", "med",
  "view-of:G01875+4;" + c((S, "D_80037894 = 0;"), ("sound.h", "u_int KeyOnFlags;")))
G("G01877", "state", "Audio::Mixer::keyedVoices", MIX, "Aliases the scheduler's keyed voice mask.", "med",
  "view-of:G01875+8;" + c((S, "if (((D_80039B00 | g_pActiveMusicConfig->ActiveNoteMask | D_80037898) != 0)"), ("sound.h", "u_int KeyedFlags;")))
G("G01878", "state", "Audio::Mixer::pendingKeyOffVoices", MIX, "Aliases the scheduler's key-off mask.", "med",
  "view-of:G01875+0xC;" + c((S, "if ((g_pActiveMusicConfig->PendingKeyOffMask != 0) || (D_8003789C != 0)"), ("sound.h", "u_int KeyOffFlags;")))
G("G01879", "state", "Audio::Mixer::pausedSoundEffectVoices", MIX, "Aliases the scheduler word that keeps the voices of paused world effects.", "med",
  "view-of:G01875+0x10;" + c((S, "if (D_800378A0 != 0) {"), (S, "int var_a2 = D_800378A0;")))
G("G01880", "state", "Audio::Driver::commandArguments", DRV, "Holds the arguments the game writes before dispatching a sound command.", "high",
  c((S, "D_800378C0.unk0 = arg0;"), (S, "D_800378C0.unk4 = arg1;")))
G("G01881", "state", "Audio::Seq::musicTransitionTimer", SEQ, "Holds the combined timer value checked while a music transition waits.", "med",
  "view-of:G01880+0x14;" + c((S, "D_800378D4 = temp_v1;"), (S, "if (D_800378D4 == 0) {")))
G("G01882", "state", "Audio::Seq::discVolume", CTL, "Holds the disc audio volume in 16.16 fixed point.", "high",
  c((S, "g_CdVolume = (u_short)arg0->Param1 << 0x10;")))
G("G01883", "state", "Audio::Seq::discVolumeLevel", CTL, "Aliases the integer part of the disc volume written to the CD volume registers.", "high",
  "view-of:G01882+2;" + c((S, "int v = D_800378E2;")))
G("G01884", "state", "Audio::Mixer::mutedMusicChannels", MIX, "Masks the music channels that are muted.", "high",
  c((S, "D_800378E4 = arg0->Param1;"), (S, "if (D_800378E4 & var_s2) {")))
G("G01885", "state", "Audio::Seq::savedMusicChannels", CH, "Holds the channels of saved music, also played as the secondary layer.", "high",
  c((S, "Sound_memcpy32(D_800378E8, g_ActiveMusicChannels, 0x2200);"), (S, "g_pSecondaryMusicChannels = D_800378E8;")))
G("G01886", "state", "Audio::Seq::randomPitchPhase", SEQ, "Advances each tick to index the random pitch table.", "high",
  c((S, "g_Sound_LfoPhase = (g_Sound_LfoPhase + 1) & 0xFF;"), (S, "sp10 *= D_8002F69C[g_Sound_LfoPhase];")))
G("G01887", "state", "Audio::Mixer::musicPitchScale", MIX, "Scales music pitch in 16.16 fixed point.", "high",
  c((S, "g_Sound_MasterPitchScaleQ16_16 = *arg0 << 0x10;")))
G("G01888", "state", "Audio::Mixer::tempoScale", MIX, "Scales music tempo in 16.16 fixed point.", "high", c((S, "g_Sound_TempoMultiplier = *arg0 << 0x10;")))
G("G01889", "state", "Audio::Mixer::tempoScaleLevel", MIX, "Aliases the integer byte of the tempo scale applied each tick.", "med",
  "view-of:G01888+2;" + c((S, "u_int tempoScale = D_80039AF2;")))
G("G01890", "state", "Audio::Driver::controlLatches", DRV, "Holds one-shot driver control flags such as pending music transitions.", "high",
  c((S, "if (g_Sound_GlobalFlags.ControlLatches & 0x6600) {")) + ";binary label is 4 bytes;the C struct's next two words are G01891 and G01892")
G("G01891", "state", "Audio::Driver::mixBehavior", DRV, "Holds the stereo or mono output mode and music stack flags.", "med",
  "view-of:G01890+4;" + c((S, "D_80039AFC = 1;"), (S, "D_80039AFC = 2;"), (S, "if (g_Sound_GlobalFlags.MixBehavior & 0x100) {")) + ";stereo and mono commands overwrite the whole word")
G("G01892", "state", "Audio::Driver::pendingUpdates", DRV, "Holds deferred voice and mode update flags.", "med",
  "view-of:G01890+8;" + c((S, "if (((D_80039B00 | g_pActiveMusicConfig->ActiveNoteMask | D_80037898) != 0)"),
                          (S, "g_Sound_GlobalFlags.UpdateFlags |= 0x100;")))
G("G01893", "state", "Audio::Cutscene::streamState", CUT, "Holds the cutscene audio stream's voices, pages, volume and flags.", "high",
  c((S, "if (g_Sound_Cutscene_StreamState.VoicesInUseFlags != 0) {")))
G("G01894", "state", "Audio::Cutscene::streamVoices", CUT, "Aliases the stream's voice mask.", "high",
  "view-of:G01893+0xC;" + c((S, "~mask & 0xFFFFFF & ~(g_Sound_VoiceSchedulerState.ActiveChannelMask | D_80039B14);")))
G("G01895", "state", "Audio::Cutscene::streamVolume", CUT, "Aliases the stream volume.", "med",
  "view-of:G01893+0x40;" + c((S, "D_80039B48 = temp_s1 & 0xFFFF;")))
G("G01896", "state", "Audio::Cutscene::streamControlFlags", CUT, "Aliases the stream control flags; bit 0 pauses it with the music.", "med",
  "view-of:G01893+0x5C;" + c((S, "D_80039B64 |= 1;"), (S, "D_80039B64 &= ~1;")))
G("G01897", "state", "Audio::Mixer::voiceOwners", MIX, "Records the music configuration that owns each voice.", "high",
  c((S, "ppThisChannelConfig = g_Sound_VoiceChannelConfigs;")))
G("G01898", "state", "Audio::Samples::bankStream", SMP, "Tracks an instrument bank streamed to SPU memory in chunks.", "high",
  c((S, "if (D_80039BC8.SpuAddr == 0) {"), (S, "D_80039BC8.SpuAddr = spuAddr;")))
G("G01899", "state", "Audio::Samples::bankStreamDestination", SMP, "Aliases the bank stream's next destination; zero starts a new stream.", "high",
  "view-of:G01898+4;" + c((S, "D_80039BCC = 0;")))
G("G01900", "state", "Audio::Mixer::voiceModes", MIX, "Holds the reverb, noise and FM voice masks.", "high",
  c((S, "func_80015970(&g_Sound_VoiceModeFlags.Noise,")))
G("G01937", "asset", "Audio::menuSoundEffects", BANKS, "Holds EFFECT00.DAT, the always-loaded sound effect programs.", "high",
  "asset:audio/sfx/effect00.json;" + c(("main.c", "vs_main_diskLoadFile(VS_EFFECT00_DAT_LBA, VS_EFFECT00_DAT_SIZE, _sfxData);")) + ";converted path is a proposed destination")
G("G01954", "state", "Audio::soundState", BANKS, "Tracks loaded music slots, sound-effect banks and the wave bank with their disc requests.", "high",
  c(("146C.c", "func_8004552C(vs_main_soundData.currentMusicSlot, 0, 0x78);"), ("main.h", "} soundData_t;")))
for gid, var, mask, _function, init in [
        ("G01974", "D_8005FE70", "0x1", "PlaySfxOnPrimaryChannel", "D_8005FE70 = 1;"),
        ("G01975", "D_8005FE74", "0x7E", "PlaySfx", "D_8005FE74 = 2;"),
        ("G01976", "D_8005FE78", "0x180", "PlayBankSfx", "D_8005FE78 = 0x80;"),
        ("G01977", "D_8005FE7C", "0x200", "PlayBankSfx", "D_8005FE7C = 0x200;"),
        ("G01978", "D_8005FE80", "0xFF000", "PlaySfx", "D_8005FE80 = 0x1000;"),
        ("G01979", "D_8005FE84", "0xF00000", "PlayBankSfx", "D_8005FE84 = 0x100000;")]:
    G(gid, "state", f"Audio::nextSfxChannelGroup{mask[2:]}", A + "Sfx.c",
      f"Is the next channel bit used within sound-effect channel group {mask}.", "high",
      c(("main.c", init), ("main.c", f"{var} *= 2")))
for gid, disp, name, purpose, text, repl in [
        ("G02003", "drop", "Audio::legacyMusicFileSizes", "Gives each MUSIC file's size.", "cdFile.size = _musicFileSizes[id];", "Content music AssetId manifest"),
        ("G02004", "drop", "Audio::legacyMusicDiscOffsets", "Gives each MUSIC file's disc sector.", "cdFile.lba = _musicLBAs[id];", "Content music AssetId manifest"),
        ("G02008", "drop", "Audio::legacySfxBankFileSizes", "Gives each SEP sound-effect bank file's size.", "cdFile.size = _seFileSizes[id];", "Content sound AssetId manifest"),
        ("G02009", "drop", "Audio::legacySfxBankDiscOffsets", "Gives each SEP sound-effect bank file's disc sector.", "cdFile.lba = _seLBAs[id];", "Content sound AssetId manifest"),
        ("G02011", "drop", "Audio::legacyWaveBankFileSizes", "Gives each WAVE bank file's size.", "cdFile.size = _soundFileSizes[file];", "Content sound AssetId manifest"),
        ("G02012", "drop", "Audio::legacyWaveBankDiscOffsets", "Gives each WAVE bank file's disc sector.", "cdFile.lba = _soundLBAs[file];", "Content sound AssetId manifest")]:
    G(gid, disp, name, A + "LegacyFiles.c", purpose, "high", f"replaced-by:{repl};" + c(("main.c", text)))
G("G02010", "catalog", "Audio::waveBankFileMap", BANKS, "Maps a sound-set id to the WAVE bank file it loads.", "med",
  c(("main.c", "void vs_main_loadSoundFile(int id) { _loadSoundFile(_soundFileMap[id]); }")))


# ---------------------------------------------------------------- types and fields
TYPES, FIELDS = [], []
members = {}
with (ROOT / "Docs" / "ledgers" / "FIELD_LEDGER.csv").open(encoding="utf-8") as stream:
    for r in csv.DictReader(stream):
        members.setdefault(r["type_id"], []).append(r)
DECL_END = {}


def lines_of(f):
    return _cache.setdefault(f, (SRC / FILES[f]).read_text(encoding="utf-8", errors="replace").splitlines())


def use(member, recv, files=("25AC.c", "main.c")):
    rx = re.compile(r"(?:" + recv + r")(?:->|\.)" + re.escape(member) + r"(?!\w)")
    for f in files:
        for i, line in enumerate(lines_of(f)):
            if rx.search(line):
                return f"source:{f}:{i + 1}"
    return None


def decl(tid, member):
    f, end = DECL_END[tid]
    lines = lines_of(f)
    rx = re.compile(r"(?<!\w)" + re.escape(member) + r"(?!\w)")
    for i in range(end - 1, max(end - 140, 0), -1):
        if rx.search(lines[i]):
            return f"source:{f}:{i + 1}"
    sys.exit(f"{tid}: declaration of {member} not found above {f}:{end}")


def T(tid, role, name, loc, purpose, conf, notes, end, specs, recv=None):
    """specs: (orig, final, meaning, conf[, cite]) ; cite None -> first use on recv, else declaration-only."""
    DECL_END[tid] = end
    TYPES.append([tid, role, name, loc, purpose, conf, notes])
    order = members[tid]
    if [o["name"] for o in order] != [sp[0] for sp in specs]:
        sys.exit(f"{tid}: member order {[o['name'] for o in order]} != {[sp[0] for sp in specs]}")
    for o, sp in zip(order, specs, strict=True):
        orig, final, meaning, fconf = sp[:4]
        cite_ = sp[4] if len(sp) > 4 else None
        if cite_ is None and recv:
            cite_ = use(orig, recv)
        if cite_ is None:
            cite_ = "declaration-only;" + decl(tid, orig)
            if fconf == "high":
                fconf = "med"
        FIELDS.append([o["id"], final, meaning, fconf, cite_])


SH = ("sound.h",)
CHN = r"in_pChannel|pChannel|in_Channel"
T("T00194", "OnDisc", "Audio::AkaoFileHeader", A + "Seq\\AkaoFormat.h",
  "Starts every AKAO music or effect file with its signature, id, length, reverb mode and timestamp.", "high",
  c((S, "if (*(int*)pSequence->magic == AKAO_MAGIC) {")), ("25AC.c", 41), [
      ("magic", "magic", "File signature AKAO", "high", c((S, "if (*(int*)pSequence->magic == AKAO_MAGIC) {"))),
      ("id", "sequenceId", "Music id compared with the playing music", "high", c((S, "if ((g_pActiveMusicConfig->MusicId != pSequence->id)"))),
      ("length", "length", "File length", "med"),
      ("reverb_type", "reverbMode", "Reverb mode applied when the music starts", "high", c((S, "Sound_SetReverbMode(pSequence->reverb_type);"))),
      ("timestamp", "timestamp", "Authoring timestamp", "med"),
  ])
T("T00201", "State", "Audio::MusicSampleBankIds", A + "Samples\\SampleBanks.h",
  "Records the sample bank ids loaded for music.", "med", c((S, "return (u_short)D_80036770.unk0 | ((u_short)D_80036770.unk4 << 0x10);")), ("25AC.c", 22), [
      ("unk0", "bankId", "Sample bank loaded for music", "med", c((S, "return (u_short)D_80036770.unk0 | ((u_short)D_80036770.unk4 << 0x10);"))),
      ("unk4", "alternateBankId", "Alternate sample bank; a matching sequence selects it", "med", c((S, "pSequence->unk14 == D_80036770.unk4 ? 0x40 : 0x20;"))),
      ("unk8", "reserved8", "No access found", "low"),
      ("unkC", "reservedC", "No access found", "low"),
  ])
T("T00202", "State", "Audio::CommandArguments", A + "Driver\\SoundDriver.h",
  "Holds the six arguments the game writes before dispatching a sound command.", "high",
  c((S, "pParams->ExtParam1 = D_800378C0.unk10;")), ("25AC.c", 65), [
      ("unk0", "param1", "First argument, often a data pointer", "high", c((S, "D_800378C0.unk0 = arg0;"))),
      ("unk4", "param2", "Second argument", "med", c((S, "D_800378C0.unk4 = arg1;"))),
      ("unk8", "param3", "Third argument", "med", c((S, "D_800378C0.unk8 = arg2 & 0x7F;"))),
      ("unkC", "param4", "Fourth argument", "med", c((S, "D_800378C0.unkC = arg3 & 0x7F;"))),
      ("unk10", "extParam1", "Copied to the command's first extended parameter", "high", c((S, "pParams->ExtParam1 = D_800378C0.unk10;"))),
      ("unk14", "extParam2", "Copied to the command's second extended parameter", "high", c((S, "pParams->ExtParam2 = D_800378C0.unk14;"))),
  ])
T("T00203", "State", "Audio::BankStreamState", A + "Samples\\SampleBanks.h",
  "Tracks an instrument bank streamed to SPU memory in chunks.", "high", c((S, "if (D_80039BC8.SpuAddr == 0) {")), ("25AC.c", 73), [
      ("pInstrumentDst", "instrumentDestination", "Next instrument word to fill", "high"),
      ("SpuAddr", "spuAddress", "Next SPU transfer address; zero starts a new stream", "high", c((S, "if (D_80039BC8.SpuAddr == 0) {"))),
      ("SpuBytesRemaining", "spuBytesRemaining", "Sample bytes still to transfer", "high"),
      ("InstrumentBytesRemaining", "instrumentBytesRemaining", "Instrument bytes still to copy", "high"),
  ], recv=r"D_80039BC8")
T("T00276", "OnDisc", "Audio::AkaoSequenceFile", A + "Seq\\AkaoFormat.h",
  "Lays out an AKAO music or instrument file: masks, region offsets and the bytecode payload.", "high",
  c((S, "channelMask = in_Sequence->ChannelEnableMask;")), ("sound.h", 444), [
      ("unk00", "reserved00", "No access found", "low"), ("unk04", "reserved04", "No access found", "low"),
      ("unk08", "reserved08", "No access found", "low"), ("unk0C", "reserved0C", "No access found", "low"),
      ("unk10", "reserved10", "No access found", "low"),
      ("unk14", "sampleBankId", "Sample bank the music expects; selects the alternate bank when it matches", "med",
       c((S, "pSequence->unk14 == D_80036770.unk4 ? 0x40 : 0x20;"))),
      ("unk18", "reserved18", "No access found", "low"),
      ("unk1C", "instrumentDataOffset", "Offset of the instrument data in an instrument file", "med", c((S, "in_Data = instruments + header->unk1C;"))),
      ("ChannelEnableMask", "channelEnableMask", "Channels the music uses", "high"),
      ("KeyedMask", "keyedMask", "Initial keyed voice mask", "high"),
      ("AllocatedVoiceMask", "allocatedVoiceMask", "Initial allocated voice mask", "high"),
      ("unk2C", "reserved2C", "No access found", "low"),
      ("PatchRegionOffset", "patchRegionOffset", "Offset of the patch table from field 0x30", "high"),
      ("KeymapRegionOffset", "keymapRegionOffset", "Offset of the keymap table from field 0x34", "high"),
      ("unk38", "resumeKeyOnMask", "Copied to the configuration's mask of channels that may key on when resumed", "med",
       c((S, "g_pActiveMusicConfig->unk2C = pSequence->unk38;"))),
      ("unk3C", "pushReleaseMask", "Copied to the configuration's mask of channels released when pushed", "med",
       c((S, "g_pActiveMusicConfig->unk30 = pSequence->unk3C;"))),
      ("Payload", "payload", "Bytecode or instrument data", "high", c((S, "in_Data = header->Payload;"))),
  ], recv=r"in_Sequence|pSequence|header")

TREM = "tremolo"
ch = [
    ("ProgramCounter", "programCounter", "Next bytecode byte", "high"),
    ("LoopStartPc", "loopStarts", "Loop start addresses of the four-deep loop stack", "high"),
    ("ReturnProgramCounter", "returnAddress", "Address resumed after a relative call", "high"),
    ("Keymap", "keymap", "Keymap of the current patch", "high"),
    ("VibratoWave", "vibratoWave", "Vibrato waveform", "high"),
    ("TremeloWave", "tremoloWave", "Tremolo waveform", "high"),
    ("AutoPanWave", "autoPanWave", "Auto-pan waveform", "high"),
    ("unk28", "effectTags", "Sound-effect tag flags matched by stop and scene commands; bit 25 exempts scene volume", "high",
     c((S, "in_Channel->unk28 = in_CommandParams->Param2;"), (S, "if ((activeVoices & voiceBit) && (pChannel->unk28 & in_StopFlags)) {"))),
    ("PitchBase", "pitchBase", "Pitch before modulation", "high"),
    ("PitchSlide", "pitchSlide", "Current pitch slide offset", "high"),
    ("UpdateFlags", "updateFlags", "Voice parameters that need writing", "high"),
    ("unk38", "effectVoiceMask", "Voices assigned to the sound-effect program", "high", c((S, "in_Channel->unk38 = voiceMask;"))),
    ("unk3C", "effectId", "Sound-effect id the channel plays", "high", c((S, "in_Channel->unk3C = in_CommandParams->Param1;"), (S, "} else if (pChannel->unk3C == in_SfxId) {"))),
    ("unk40", "effectPitchScale", "Per-effect pitch scale set by command 0xA4", "high", c((S, "var_a1->unk40 = ((char)arg0->Param3) << 8;"))),
    ("unk44", "reserved44", "No channel access found", "low"),
    ("Volume", "volume", "Channel volume", "high"),
    ("VolumeSlideStep", "volumeSlideStep", "Per-tick volume slide step", "high"),
    ("PitchSlideStep", "pitchSlideStep", "Per-tick pitch slide step", "high"),
    ("FinePitchDelta", "finePitchDelta", "Pitch offset from fine tuning", "high"),
    ("unk58", "priority", "Eviction priority of a sound effect", "high", c((S, "if ((activeVoices & voiceBit) && (maxPriority < pChannel->unk58)) {"))),
    ("unk5C", "reserved5C", "No channel access found", "low"),
    ("unk60", "keyOnVolumeSlideStep", "Per-tick key-on volume slide step", "high", c((S, "in_pChannel->unk60 = Delta / in_pChannel->KeyOnVolumeSlideLength;"))),
    ("Type", "type", "Music or sound-effect channel", "high"),
    ("Length1", "noteLength", "Ticks until the next note", "high"),
    ("Length2", "gateLength", "Ticks until the note is released", "high"),
    ("InstrumentIndex", "instrumentIndex", "Current instrument", "high"),
    ("unk6C", "effectPan", "Program pan set by command 0xA2", "high", c((S, "var_a1->unk6C = ((char)arg0->Param3) << 8;"))),
    ("unk6E", "effectPanSlideLength", "Ticks left in the program pan slide", "med", c((S, "var_a1->unk6E = 0;"))),
    ("OpcodeStepCounter", "opcodeStepCounter", "Steps executed, recorded by loop points", "high"),
    ("LoopIterationCount", "loopIterations", "Iterations done at each loop level", "high"),
    ("LoopStepCounterSnapshot", "loopStepSnapshots", "Step counter saved at each loop point", "high"),
    ("VolumeBalance", "volumeBalance", "Master volume balance", "high"),
    ("VolumeBalanceSlideLength", "volumeBalanceSlideLength", "Ticks left in the balance slide", "high"),
    ("unk86", "effectPitchScaleSlideLength", "Ticks left in the effect pitch scale slide", "med", c((S, "var_a1->unk86 = 0;"))),
    ("ChannelVolumeSlideLength", "volumeSlideLength", "Ticks left in the volume slide", "high"),
    ("KeyOnVolumeSlideLength", "keyOnVolumeSlideLength", "Ticks left in the key-on volume slide", "high"),
    ("unk8C", "effectVolumeSlideLength", "Ticks left in the program volume slide", "high", c((S, "if (var_a1_2->unk8C != 0) {"), (S, "var_a2->unk8C = v;"))),
    ("ChannelPan", "pan", "Channel pan", "high"),
    ("ChannelPanSlideLength", "panSlideLength", "Ticks left in the pan slide", "high"),
    ("PitchSlideStepsCurrent", "pitchSlideTicksLeft", "Ticks left in the pitch slide", "high"),
    ("Octave", "octave", "Current octave", "high"),
    ("PitchBendSlideLength", "pitchBendSlideLength", "Length of a pitch bend", "high"),
    ("KeyStored", "portamentoKey", "Key saved for portamento", "high"),
    ("PortamentoSteps", "portamentoSteps", "Portamento length", "high"),
    ("SfxMask", "playModes", "Legato and full-length flags", "high"),
    ("VibratoDelay", "vibratoDelay", "Ticks before vibrato starts", "high"),
    ("VibratoDelayCurrent", "vibratoDelayLeft", "Ticks left before vibrato starts", "high"),
    ("VibratoRatePhase", "vibratoRate", "Vibrato rate", "high"),
    ("unkA4", "vibratoRateCounter", "Countdown to the next vibrato step", "med", c((S, "in_pChannel->unkA4 = 1;"))),
    ("VibratoType", "vibratoShape", "Vibrato waveform index", "high"),
    ("VibratoBase", "vibratoBase", "Vibrato amplitude base", "high"),
    ("VibratoDepth", "vibratoDepth", "Vibrato depth", "high"),
    ("VibratoDepthSlideLength", "vibratoDepthSlideLength", "Ticks left in the vibrato depth slide", "high"),
    ("VibratoDepthSlideStep", "vibratoDepthSlideStep", "Per-tick vibrato depth step", "high"),
    ("TremeloDelay", "tremoloDelay", "Ticks before tremolo starts", "high"),
    ("TremeloDelayCurrent", "tremoloDelayLeft", "Ticks left before tremolo starts", "high"),
    ("TremeloRatePhase", "tremoloRate", "Tremolo rate", "high"),
    ("unkB6", "tremoloRateCounter", "Countdown to the next tremolo step", "med", c((S, "in_pChannel->unkB6 = 1;"))),
    ("TremeloType", "tremoloShape", "Tremolo waveform index", "high"),
    ("TremeloDepth", "tremoloDepth", "Tremolo depth", "high"),
    ("TremeloDepthSlideLength", "tremoloDepthSlideLength", "Ticks left in the tremolo depth slide", "high"),
    ("TremeloDepthSlideStep", "tremoloDepthSlideStep", "Per-tick tremolo depth step", "high"),
    ("AutoPanRatePhase", "autoPanRate", "Auto-pan rate", "high"),
    ("AutoPanRateCurrent", "autoPanRateCounter", "Countdown to the next auto-pan step", "high"),
    ("AutoPanType", "autoPanShape", "Auto-pan waveform index", "high"),
    ("AutoPanDepth", "autoPanDepth", "Auto-pan depth", "high"),
    ("AutoPanDepthSlideLength", "autoPanDepthSlideLength", "Ticks left in the auto-pan depth slide", "high"),
    ("AutoPanDepthSlideStep", "autoPanDepthSlideStep", "Per-tick auto-pan depth step", "high"),
    ("NoiseTimer", "noiseTimer", "Ticks until noise mode toggles", "high"),
    ("FmTimer", "fmTimer", "Ticks until FM mode toggles", "high"),
    ("LoopStackTop", "loopDepth", "Current loop stack level", "high"),
    ("RandomPitchDepth", "randomPitchDepth", "Depth of random pitch variation", "high"),
    ("LengthStored", "storedNoteLength", "Note length restored after an override", "high"),
    ("LengthFixed", "fixedNoteLength", "Fixed note length; zero uses the opcode's length", "high"),
    ("VolumeBalanceSlideStep", "volumeBalanceSlideStep", "Per-tick balance slide step", "high"),
    ("unkDA", "effectPanSlideStep", "Per-tick program pan slide step", "high", c((S, "var_a2->unkDA = (short)((((char)arg0->Param4) << 8) - var_a2->unk6C) / v;"))),
    ("unkDC", "effectVolume", "Program volume scale applied to the channel", "high", c((S, "Volume = (Volume * (in_pChannel->unkDC >> 8)) >> 7;"))),
    ("unkDE", "effectVolumeSlideStep", "Per-tick program volume slide step", "high", c((S, "temp_s1 = var_a1_2->unkDC + var_a1_2->unkDE;"))),
    ("PanSlideStep", "panSlideStep", "Per-tick pan slide step", "high"),
    ("Transpose", "transpose", "Semitone transpose", "high"),
    ("FineTune", "fineTune", "Fine tuning", "high"),
    ("Key", "key", "Last key played", "high"),
    ("PitchBendSlideTranspose", "pitchBendTarget", "Semitones a pitch bend moves", "high"),
    ("TransposeStored", "portamentoTranspose", "Transpose saved for portamento", "high"),
    ("VibratoPitch", "vibratoPitch", "Current vibrato pitch offset", "high"),
    ("TremeloVolume", "tremoloVolume", "Current tremolo volume offset", "high"),
    ("AutoPanVolume", "autoPanOffset", "Current auto-pan offset", "high"),
    ("VoiceParams", "voice", "Hardware voice parameters", "high"),
]
T("T00278", "State", "Audio::SeqChannel", A + "Seq\\Channel.h",
  "Holds one sequencer channel: program position, loops, note timing, slides, modulation and voice.", "high",
  c((S, "int pc = *in_pChannel->ProgramCounter++;")), ("sound.h", 410), ch, recv=CHN)

T("T00279", "State", "Audio::MusicConfig", A + "Seq\\Channel.h",
  "Holds one music's channel masks, tempo, tables, reverb, master volume and timers.", "high",
  c((S, "Sound_memcpy32(&D_800366F0, &g_PushedMusicConfig, 0x80);")) + ";real extent 0x80 bytes;members from 0x80 are not part of it", ("sound.h", 492), [
      ("StatusFlags", "statusFlags", "Load and voice allocation status", "high"),
      ("ActiveChannelMask", "activeChannels", "Channels still playing", "high"),
      ("KeyedMask", "keyedVoices", "Voices currently keyed on", "high"),
      ("AllocatedVoiceMask", "allocatedVoices", "Channels with voices allocated", "high"),
      ("PendingKeyOnMask", "pendingKeyOn", "Pending key-ons", "high"),
      ("ActiveNoteMask", "activeNotes", "Channels playing a note rather than a rest", "high"),
      ("PendingKeyOffMask", "pendingKeyOff", "Pending key-offs", "high"),
      ("unk1C", "pausedChannels", "Channels paused when the music was pushed", "med", c((S, "pConfig->unk1C |= channelMask;"), (S, "if (g_pActiveMusicConfig->unk1C != 0) {"))),
      ("Tempo", "tempo", "Tempo in 16.16", "high"),
      ("TempoSlideStep", "tempoSlideStep", "Per-tick tempo step", "high"),
      ("unk28", "tickAccumulator", "Accumulates tempo to decide when channels advance", "med", c((S, "g_pActiveMusicConfig->unk28 += var_a0;"))),
      ("unk2C", "resumeKeyOnMask", "Channels allowed to key on again when resumed", "med", c((S, "g_pSavedMousicConfig->PendingKeyOnMask &= g_pSavedMousicConfig->unk2C;"))),
      ("unk30", "pushReleaseMask", "Channels released when the music is pushed", "med", c((S, "g_PushedMusicConfig.ActiveChannelMask &= ~g_PushedMusicConfig.unk30;"))),
      ("unk34", "reserved34", "Written by opcode 0xFE0C; never read", "low", c((S, "g_pActiveMusicConfig->unk34 = *in_pChannel->ProgramCounter++;"))),
      ("unk38", "sequenceBase", "Start of the sequence file", "high", c((S, "g_pActiveMusicConfig->unk38 = (int)in_Sequence;"))),
      ("SequencePatchTable", "patchTable", "Patch table of the sequence", "high"),
      ("KeymapTable", "keymapTable", "Keymap table of the sequence", "high"),
      ("unk44", "noteRemapTable", "Key remap table selected by opcode 0xFE1C", "high", c((S, "g_pActiveMusicConfig->unk44 = (FSoundKeymapEntry8*)&D_8002F66C[temp_v0];"))),
      ("SomeIndexRelatedToSpuVoiceInfo", "voiceSearchStart", "Voice index where free-voice searches start", "med", c((S, "i = g_pActiveMusicConfig->SomeIndexRelatedToSpuVoiceInfo;"))),
      ("NoiseChannelFlags", "noiseChannels", "Channels in noise mode", "high"),
      ("ReverbChannelFlags", "reverbChannels", "Channels with reverb", "high"),
      ("FmChannelFlags", "fmChannels", "Channels in FM mode", "high"),
      ("RevDepth", "reverbDepth", "Reverb depth", "high"),
      ("unk5C", "reverbDepthSlideStep", "Per-tick reverb depth step", "high", c((S, "delta = pConfig->RevDepth + pConfig->unk5C * pConfig->ReverbDepthSlideLength;"))),
      ("A_Volume", "masterVolume", "Master volume in 16.16", "high", c((S, "Volume = in_Context->A_Volume;"))),
      ("unk64", "masterVolumeStep", "Per-tick master volume step", "high", c((S, "temp_s1 = g_pActiveMusicConfig->A_Volume + g_pActiveMusicConfig->unk64;"))),
      ("unk68", "masterVolumeSlideLength", "Ticks left in the master volume slide", "med", c((S, "var_a1 = g_pActiveMusicConfig->unk68;"))),
      ("ReverbDepthSlideLength", "reverbDepthSlideLength", "Ticks left in the reverb slide", "high"),
      ("TempoSlideLength", "tempoSlideLength", "Ticks left in the tempo slide", "high"),
      ("MusicId", "musicId", "Id of the music", "high"),
      ("JumpThresholdValue", "jumpThreshold", "Value compared by conditional jumps", "high"),
      ("NoiseClock", "noiseClock", "Noise clock", "high"),
      ("TimerUpper", "timerUpper", "Upper timer reload", "high"),
      ("TimerUpperCurrent", "timerUpperCount", "Upper timer count", "high"),
      ("TimerLower", "timerLower", "Lower timer reload", "high"),
      ("TimerLowerCurrent", "timerLowerCount", "Lower timer count", "high"),
      ("TimerTopCurrent", "timerTop", "Timer top value", "high"),
      ("unk80", "reserved80", "Past the real 0x80-byte extent; no access", "low"), ("unk84", "reserved84", "Past the real 0x80-byte extent; no access", "low"),
      ("unk88", "reserved88", "Past the real 0x80-byte extent; no access", "low"), ("unk8C", "reserved8C", "Past the real 0x80-byte extent; no access", "low"),
      ("unk90", "reserved90", "Past the real 0x80-byte extent; no access", "low"), ("unk94", "reserved94", "Past the real 0x80-byte extent; no access", "low"),
      ("unk98", "reserved98", "Past the real 0x80-byte extent; no access", "low"), ("unk9C", "reserved9C", "Past the real 0x80-byte extent; no access", "low"),
  ], recv=r"g_pActiveMusicConfig|in_Config|pConfig|g_pSavedMousicConfig|in_Context")
CMD = r"in_Params|arg0|pParams|in_CommandParams|D_80037800|D_80037790"
TYPES.append(["T00742", "Value", "Audio::CommandHandler", A + "Mixer\\MixControl.h",
              "Is the signature of a sound command handler taking the command parameters.", "high",
              c((S, "D_800301E0[0xD0](pParams);"))])
T("T00280", "Value", "Audio::CommandParams", A + "Mixer\\MixControl.h",
  "Carries a sound command's four parameters and two extended parameters.", "high", c((S, "pParams = &D_80037790;")), ("sound.h", 501), [
      ("Param1", "param1", "First parameter", "high"), ("Param2", "param2", "Second parameter", "high"),
      ("Param3", "param3", "Third parameter, usually flags", "high"), ("Param4", "param4", "Fourth parameter", "high"),
      ("ExtParam1", "extParam1", "First extended parameter", "high"), ("ExtParam2", "extParam2", "Second extended parameter", "high"),
  ], recv=CMD)
CUTR = r"g_Sound_Cutscene_StreamState"
T("T00282", "State", "Audio::CutsceneStreamState", A + "Cutscene\\Stream.h",
  "Holds the cutscene audio stream's voices, page ring, volume fade, pan and flags.", "high",
  c((S, "if (g_Sound_Cutscene_StreamState.VoicesInUseFlags != 0) {")), ("sound.h", 597), [
      ("pCurrentChunk", "currentChunk", "Stream chunk being consumed", "high"),
      ("unk_Mask_0x4", "auxiliaryMask", "Set when the stream starts and tested at page refills; meaning unresolved", "low"),
      ("field2_0x8", "formatFlags", "Stream format flags from the header; bit 1 tested at start", "med"),
      ("VoicesInUseFlags", "voiceMask", "Voices the stream plays on", "high"),
      ("VoiceIndex", "voiceIndex", "First stream voice", "high"),
      ("TotalPages", "totalPages", "Pages in the stream", "high"),
      ("CurrentPage", "currentPage", "Page being played", "high"),
      ("field7_0x1c", "reserved1C", "Written at stream start; meaning unresolved", "low"),
      ("field8_0x20", "reserved20", "No access found", "low"), ("field9_0x24", "reserved24", "No access found", "low"),
      ("PageIndex", "pageIndex", "Index of the page within the ring", "med"),
      ("pStreamBase", "streamBase", "Start of the stream data", "med"),
      ("field12_0x30", "reserved30", "No access found", "low"),
      ("StreamPageIndex", "streamPageIndex", "Next stream page to upload", "med"),
      ("field14_0x38", "reserved38", "No access found", "low"),
      ("PageRingBufferSize", "pageRingSize", "Pages in the upload ring", "med"),
      ("Volume", "volume", "Stream volume", "high"),
      ("VolFadeStepSize", "volumeFadeStep", "Per-tick volume step", "high"),
      ("VolFadeStepsRemaining", "volumeFadeTicksLeft", "Ticks left in the volume fade", "high"),
      ("field19_0x4c", "pan", "Stream pan; its second byte indexes the pan gain table", "med",
       c((S, "[g_Sound_Cutscene_StreamState.field19_0x4c.u8[1] ^ Mask])"))),
      ("field23_0x50", "reserved50", "No access found", "low"), ("field24_0x54", "reserved54", "No access found", "low"),
      ("VoiceSampleRate", "sampleRate", "Voice sample rate", "high"),
      ("ControlFlags", "controlFlags", "Stream control flags; bit 0 pauses with the music", "high"),
  ], recv=CUTR)
T("T00283", "State", "Audio::FadeTimer", A + "Seq\\Sequencer.h",
  "Steps a value toward a target over a number of ticks and keeps a saved value.", "high",
  c((S, "g_Sound_MasterFadeTimer.Value = 0x7F0000;")), ("sound.h", 514), [
      ("Value", "value", "Current value", "high"), ("Step", "step", "Per-tick step", "high"),
      ("TicksRemaining", "ticksRemaining", "Ticks left", "high"), ("SavedValue", "savedValue", "Value saved before the fade", "high"),
  ], recv=r"g_Sound_MasterFadeTimer")
T("T00284", "State", "Audio::DriverFlags", A + "Driver\\SoundDriver.h",
  "Groups the driver's control latches, mix behavior and pending update words.", "high",
  c((S, "if (g_Sound_GlobalFlags.ControlLatches & 0x6600) {")) + ";in the binary the second and third words are separate labels (G01891 G01892)", ("sound.h", 203), [
      ("ControlLatches", "controlLatches", "One-shot control flags", "high"),
      ("MixBehavior", "mixBehavior", "Output mode and music stack flags", "high"),
      ("UpdateFlags", "updateFlags", "Deferred voice update flags", "high"),
  ], recv=r"g_Sound_GlobalFlags")
T("T00285", "Def", "Audio::Instrument", A + "Samples\\SampleBanks.h",
  "Describes one instrument: sample addresses, tuning, root note and envelope.", "high",
  c((S, "temp_a2_2 = func_8001A4BC(&g_InstrumentInfo[arg0->InstrumentIndex],")), ("sound.h", 257), [
      ("StartAddr", "sampleStart", "Sample start address", "high"), ("LoopAddr", "loopStart", "Sample loop address", "high"),
      ("FineTune", "fineTune", "Fine tuning", "high"), ("SampleNote", "rootNote", "Note at which the sample plays unshifted", "high"),
      ("AdsrLower", "adsrLower", "Lower envelope word", "high"), ("AdsrUpper", "adsrUpper", "Upper envelope word", "high"),
  ], recv=r"InstrumentInfo|in_A|in_B|arg0|g_InstrumentInfo\[[^\]]*\]")
T("T00286", "Def", "Audio::KeymapEntry", A + "Seq\\AkaoFormat.h",
  "Maps a key to an instrument, note, envelope rates, volume and pan.", "med", "declaration-only;" + cite("sound.h", "} FSoundKeymapEntry8;"), ("sound.h", 421), [
      ("InstrumentIndex", "instrumentIndex", "Instrument played", "med"), ("Note", "note", "Note played", "med"),
      ("AdsrAttackRate", "attackRate", "Envelope attack rate", "med"), ("AdsrSustainRate", "sustainRate", "Envelope sustain rate", "med"),
      ("SustainModeCode", "sustainMode", "Envelope sustain mode", "med"), ("ReleaseRate", "releaseRate", "Envelope release rate", "med"),
      ("VolumeScale", "volumeScale", "Volume scale", "med"), ("PanAndReverb", "panAndReverb", "Pan and reverb flag", "med"),
  ])
T("T00287", "State", "Audio::VoiceModeMasks", A + "Mixer\\VoiceMixer.h", "Holds the reverb, noise and FM voice masks.", "high",
  c((S, "func_80015970(&g_Sound_VoiceModeFlags.Noise,")), ("sound.h", 520), [
      ("Reverb", "reverb", "Voices with reverb", "high"), ("Noise", "noise", "Voices in noise mode", "high"), ("Fm", "fm", "Voices in FM mode", "high"),
  ], recv=r"g_Sound_VoiceModeFlags")
T("T00288", "State", "Audio::VoiceParams", A + "Mixer\\VoiceMixer.h",
  "Holds the hardware voice parameters a channel writes when they change.", "high",
  c((S, "in_VoiceParams->VoiceParamFlags = 0;")), ("sound.h", 316), [
      ("AssignedVoiceNumber", "assignedVoice", "Voice assigned to the channel", "high"),
      ("VoiceParamFlags", "dirtyFlags", "Parameters that need writing", "high"),
      ("StartAddress", "sampleStart", "Sample start address", "high"), ("LoopAddress", "loopStart", "Sample loop address", "high"),
      ("SampleRate", "sampleRate", "Pitch as sample rate", "high"), ("AdsrLower", "adsrLower", "Lower envelope word", "high"),
      ("AdsrUpper", "adsrUpper", "Upper envelope word", "high"), ("VolumeScale", "volumeScale", "Volume scale", "high"),
      ("Volume", "volume", "Left and right volume", "high"),
  ], recv=r"in_VoiceParams|VoiceParams")
SCH = r"g_Sound_VoiceSchedulerState"
T("T00289", "State", "Audio::VoiceScheduler", A + "Mixer\\VoiceMixer.h",
  "Holds sound-effect voice masks, key-on and key-off requests, tempo accumulation and the noise clock.", "high",
  c((S, "g_Sound_VoiceSchedulerState.ReverbVoiceFlags &= ~temp_a2;")), ("sound.h", 274), [
      ("ActiveChannelMask", "activeVoices", "Voices playing sound effects", "high"),
      ("KeyOnFlags", "pendingKeyOn", "Voices to key on", "high"), ("KeyedFlags", "keyedVoices", "Voices keyed on", "high"),
      ("KeyOffFlags", "pendingKeyOff", "Voices to key off", "high"),
      ("unk_Flags_0x10", "pausedEffectVoices", "Voices of paused world effects, protected from reuse", "med"),
      ("unk14", "effectTempo", "Tempo whose upper half is added to the accumulator each tick", "med", c((S, "(g_Sound_VoiceSchedulerState.unk14 >> 0x10) & 0xFFFF;"))),
      ("TempoAccumumulator", "tempoAccumulator", "Accumulates tempo to decide when effect channels advance", "high"),
      ("NoiseVoiceFlags", "noiseVoices", "Voices in noise mode", "high"), ("ReverbVoiceFlags", "reverbVoices", "Voices with reverb", "high"),
      ("FmVoiceFlags", "fmVoices", "Voices in FM mode", "high"), ("NoiseClock", "noiseClock", "Noise clock", "high"),
      ("field11_0x2a", "pad2A", "No access found", "low"), ("field12_0x2b", "pad2B", "No access found", "low"),
      ("field13_0x2c", "reserved2C", "No access found", "low"),
  ], recv=SCH)
T("T00290", "State", "Audio::VoiceEnvelope", A + "Mixer\\VoiceMixer.h", "Caches one voice's envelope level for allocation.", "high",
  c((S, "g_SpuVoiceInfo[var_a0].pEnvx = 0x7FFF;")), ("sound.h", 507), [
      ("unk0", "reserved0", "Unused per the original comment; no access found", "low"),
      ("pEnvx", "envelopeLevel", "Envelope level; zero means free", "high", c((S, "if (var_v1->pEnvx != 0) {"))),
      ("unk6", "reserved6", "No access found", "low"),
  ])
SND = r"vs_main_soundData"
T("T00603", "State", "Audio::SoundState", A + "SoundBanks.h",
  "Tracks music slots, sound-effect banks, cutscene streams and the wave bank with their disc requests.", "high",
  c(("146C.c", "func_8004552C(vs_main_soundData.currentMusicSlot, 0, 0x78);")), ("main.h", 135), [
      ("currentMusicSlot", "currentMusicSlot", "Music slot playing", "high"),
      ("unk4", "playingMusicHandle", "Handle of the music playing; 0xFFFF when none", "med", c(("main.c", "vs_main_soundData.unk4 = 0xFFFF;"))),
      ("unk8", "currentMusicId", "Id of the music in the current slot", "med", c(("main.c", "vs_main_soundData.unk8 = vs_main_soundData.musicIds[id - 1];"))),
      ("currentMusicData", "currentMusicData", "Data of the music playing", "high"),
      ("musicIds", "musicIds", "Music id in each slot", "high"),
      ("unk14", "musicHandles", "Handle used to stop each slot's music; 0xFFFF when none", "med", c(("main.c", "func_80011FDC(vs_main_soundData.unk14[id - 1]);"))),
      ("musicData", "musicData", "Loaded music data of each slot", "high"),
      ("musicQueueSlot", "musicRequests", "Disc request of each music slot", "high"),
      ("currentSfxId", "currentSfxBank", "Sound-effect bank in use", "high"),
      ("sfxIds", "sfxBankIds", "Id in each sound-effect bank slot", "high"),
      ("sfxData", "sfxBankData", "Loaded data of each sound-effect bank", "high"),
      ("sfxQueueSlot", "sfxBankRequests", "Disc request of each sound-effect bank", "high"),
      ("unk64", "sfxStreamData", "Cutscene stream data started from a bank", "med", c(("main.c", "func_800132C4(vs_main_soundData.unk64[arg0 - 1], arg1, 1);"))),
      ("unk70", "reserved70", "Cleared at bank release; never read", "low", c(("main.c", "vs_main_soundData.unk70[i] = NULL;"))),
      ("soundFileId", "waveBankId", "Loaded WAVE bank", "high"), ("soundData", "waveBankData", "Loaded WAVE bank data", "high"),
      ("soundQueueSlot", "waveBankRequest", "Disc request of the WAVE bank", "high"),
  ], recv=SND)
T("T00708", "OnDisc", "Audio::SfxProgramHeader", A + "Playback\\SoundPlayback.h",
  "Starts a sound-effect program passed by address; bit 7 of byte 9 requests playback tags.", "med",
  c((S, "int func_800123C8(vs_main_sfxContext* arg0)")), ("25AC.h", 9), [
      ("unk0", "reserved0", "No access found", "low"), ("unk4", "reserved4", "No access found", "low"),
      ("unk8", "reserved8", "No access found", "low"),
      ("unk9", "flags", "Bit 7 requests playback tags", "high", c((S, "ret = arg0->unk9 >> 7;"))),
  ])

problems = []
byfid = {m["id"]: m for ms in members.values() for m in ms}
for fid, _final, _meaning, _confidence, notes in FIELDS:
    orig = byfid[fid]["name"]
    ok = False
    for m in re.finditer(r"source:([^:;]+):(\d+)", notes):
        text = lines_of(m.group(1))[int(m.group(2)) - 1]
        if re.search(r"(?<!\w)" + re.escape(orig) + r"(?!\w)", text):
            ok = True
    if not ok:
        problems.append(f"{fid} {orig}: no cited line names the member ({notes})")
if problems:
    sys.exit("\n".join(problems))
with (ROOT / "tools" / "naming" / "sym_batches" / "Audio" / "types.csv").open(encoding="utf-8") as stream:
    torder = [r["id"] for r in csv.DictReader(stream)]
tby = {t[0]: t for t in TYPES}
with open(OUT / "types.csv", "w", newline="", encoding="utf-8") as f:
    w = csv.writer(f)
    w.writerow(["id", "role", "final_name", "final_location", "purpose", "confidence", "notes"])
    w.writerows(tby[i] for i in torder if i in tby)
with open(OUT / "fields.csv", "w", newline="", encoding="utf-8") as f:
    w = csv.writer(f)
    w.writerow(["id", "final_name", "meaning", "confidence", "notes"])
    w.writerows(FIELDS)
print(len(TYPES), "types;", len(FIELDS), "fields")

with (ROOT / "tools" / "naming" / "sym_batches" / "Audio" / "globals.csv").open(encoding="utf-8") as stream:
    order = [r["id"] for r in csv.DictReader(stream)]
byid = {r[0]: r for r in rows}
missing = [i for i in order if i not in byid]
OUT.mkdir(parents=True, exist_ok=True)
with open(OUT / "globals.csv", "w", newline="", encoding="utf-8") as f:
    w = csv.writer(f)
    w.writerow(["id", "disposition", "final_name", "final_location", "purpose", "confidence", "notes"])
    w.writerows(byid[i] for i in order if i in byid)
print(len(rows), "globals;", "missing:", missing)
