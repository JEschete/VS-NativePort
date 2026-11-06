#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>

struct SVECTOR;

// Live bindings for Sfx.hpp over the channel groups' tag cursors
// (D_8005FE70-D_8005FE84), the sound-effect bank slots (vs_main_soundData),
// the playback front end, the sound control requests, the sequence memory
// and the geometry transform. Generated and owned callers keep the original
// call shapes (copy_fixups.py OWNED_CALL_BINDINGS); a request with no caller
// has no binding. A bank is read only within the sequence memory
// (SeqRecordsLive.hpp LiveProgramRegion).
namespace VS::Audio {
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void LivePlaySfx(int group, int effect_id, int pan, int volume);
void LivePlaySfxCentered(int group, int effect_id);
// The pan in the high halfword and the volume in the low halfword.
int LiveSfxPanAndVolume(int screen_xy, int depth);
// A null `point` is refused.
void LivePlayWorldSfx(int group, int effect_id, SVECTOR* point);
// The program of a banked group's effect, 0 for a built-in group's effect or
// an unknown group, or -1 when refused.
std::intptr_t LiveResolveSfxProgram(int group, int effect_id);
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void LiveFadeSfxVolume(int group, int effect_id, int target_volume,
                       int fade_ticks);
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void LiveFadeSfxPitch(int group, int effect_id, int target_pitch_scale,
                      int fade_ticks);
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void LiveFadeSfxPan(int group, int effect_id, int target_pan, int fade_ticks);
void LiveStopSfx(int group, int effect_id);
void LiveStopAllSfxPlayback();
void LivePlayBankSfx(int group, const void* bank, unsigned index, int pan,
                     int volume);
void LivePlayBankSfxCentered(int group, const void* bank, unsigned index);
void LiveStopBankSfx(int group, const void* bank, int index);
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void LiveFadeBankSfxVolume(int group, const void* bank, int index,
                           int target_volume, int fade_ticks);
}  // namespace VS::Audio
