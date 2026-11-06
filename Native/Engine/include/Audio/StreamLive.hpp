#pragma once
// Hand-maintained (tools/port/owned.csv).
// Live bindings for Stream.hpp over the disc drive record (vs_main_disk),
// the slot record's effect streams (vs_main_soundData), the high end of the
// game heap and the sound control requests. Generated and owned callers keep
// the original call shapes (copy_fixups.py OWNED_CALL_BINDINGS).
namespace VS::Audio {
int LiveBeginXaStream(int sector, int byte_count);
int LiveRequestXaStreamStop();
void LiveStopCutsceneStream();
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
int LivePlayCutsceneStreamSlot(int slot, int pan, int volume);
void LivePlayCutsceneStreamSlotCentered(int slot);
int LiveFreeCutsceneStreamSlot(int slot);
void LiveSetXaVolumeMaximum();
}  // namespace VS::Audio
