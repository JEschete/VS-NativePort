"""Audio globals from code read directly. Citations are resolved by exact text search, never typed."""
import csv
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
SRC = ROOT / "resources" / "rood-reverseWin" / "src"
OUT = ROOT / "tools" / "naming" / "sym_out" / "Audio"
FILES = {"25AC.c": "SLUS_010.40/25AC.c", "main.c": "SLUS_010.40/main.c", "main.h": "SLUS_010.40/main.h",
         "146C.c": "BATTLE/BATTLE.PRG/146C.c", "44F14.c": "BATTLE/BATTLE.PRG/44F14.c", "sound.h": "include/sound.h"}
_cache = {}


def cite(f, text):
    lines = _cache.setdefault(f, (SRC / FILES[f]).read_text(encoding="utf-8", errors="replace").splitlines())
    for i, line in enumerate(lines):
        if text in line:
            return f"source:{f}:{i + 1}"
    sys.exit(f"citation text not found in {f}: {text!r}")


def c(*pairs):
    return ";".join(cite(f, t) for f, t in pairs)


A = "Native\\Audio\\src\\"
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
for gid, var, mask, fn, init in [
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

order = [r["id"] for r in csv.DictReader(open(ROOT / "tools" / "naming" / "sym_batches" / "Audio" / "globals.csv", encoding="utf-8"))]
byid = {r[0]: r for r in rows}
missing = [i for i in order if i not in byid]
OUT.mkdir(parents=True, exist_ok=True)
with open(OUT / "globals.csv", "w", newline="", encoding="utf-8") as f:
    w = csv.writer(f)
    w.writerow(["id", "disposition", "final_name", "final_location", "purpose", "confidence", "notes"])
    w.writerows(byid[i] for i in order if i in byid)
print(len(rows), "globals;", "missing:", missing)
