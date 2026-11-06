// Hand-maintained (tools/port/owned.csv).
// Live binding for Stream.hpp: the disc drive record (vs_main_disk), the
// slot record's effect streams (vs_main_soundData), the high end of the
// game heap and the sound control requests.
#include "Audio/StreamLive.hpp"

#include <cstddef>
#include <cstdint>
#include <iterator>

#include "Audio/Sfx.hpp"
#include "Audio/SoundBanks.hpp"
#include "Audio/SoundControl.hpp"
#include "Audio/SoundControlLive.hpp"
#include "Audio/SoundSlotsLive.hpp"
#include "Audio/Stream.hpp"
#include "Content/CdDrive.hpp"

extern "C" {
#include "Audio/SoundBanks.h"
#include "DiscQueue.h"
#include "Legacy/Psx.h"
}

namespace VS::Audio {
namespace {
static_assert(std::size(Audio_soundState.sfxStreamData) ==
              kEffectStreamSlotCount);
static_assert(sizeof(Content_DiscDrive::state) == sizeof(std::uint8_t));
static_assert(sizeof(Content_DiscDrive::failureCount) == sizeof(std::uint8_t));
static_assert(sizeof(Content_DiscDrive::startSector) == sizeof(std::int32_t));
static_assert(sizeof(Content_DiscDrive::byteCount) == sizeof(std::int32_t));
static_assert(sizeof(Content_DiscDrive::bufferedSectors) ==
              sizeof(std::int32_t));

XaStreamDrive LoadDrive() {
  const Content_DiscDrive& drive = Content_discDriveState;
  return {
      // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
      .state = static_cast<Content::DiscDriveState>(
          static_cast<std::uint8_t>(drive.state)),
      .start_sector = drive.startSector,
      .byte_count = drive.byteCount,
      .failure_count = static_cast<std::uint8_t>(drive.failureCount),
      .sectors_received = drive.sectorsReceived,
      .destination_sector = drive.destinationSector,
      .buffered_sectors = drive.bufferedSectors,
      .ring_sectors_written = drive.ringSectorsWritten,
      .ring_write_index = drive.ringWriteIndex,
      .ring_read_index = drive.ringReadIndex,
  };
}

void CommitDrive(const XaStreamDrive& typed) {
  Content_DiscDrive& drive = Content_discDriveState;
  drive.state = static_cast<char>(typed.state);
  drive.startSector = typed.start_sector;
  drive.byteCount = typed.byte_count;
  drive.failureCount = static_cast<char>(typed.failure_count);
  drive.sectorsReceived = typed.sectors_received;
  drive.destinationSector = typed.destination_sector;
  drive.bufferedSectors = typed.buffered_sectors;
  drive.ringSectorsWritten = typed.ring_sectors_written;
  drive.ringWriteIndex = typed.ring_write_index;
  drive.ringReadIndex = typed.ring_read_index;
}

class LiveEffectStreams final : public EffectStreamHost {
 public:
  void StopCutsceneStream() override { Control::LiveStopCutsceneStream(); }

  BankBuffer EffectStream(std::size_t index) override {
    return LiveBankBuffer(Audio_soundState.sfxStreamData[index]);
  }

  void FadeCutsceneVolume(Control::Fade fade) override {
    Control::LiveFadeCutsceneVolume(fade.ticks, fade.target);
  }

  void StoreEffectStream(std::size_t index, BankBuffer stream) override {
    Audio_soundState.sfxStreamData[index] = LiveBufferPointer(stream);
  }

  void SetDiscVolume(std::int32_t volume) override {
    Control::LiveSetDiscVolume(volume);
  }

  void StartCutsceneStream(BankBuffer stream, std::int32_t pan,
                           std::int32_t reverb) override {
    Control::LiveStartCutsceneStream(LiveBufferPointer(stream), pan, reverb);
  }

  void FreeBuffer(BankBuffer buffer) override { FreeLiveSlotBuffer(buffer); }
};

// The original's unsigned slot comparison (`slot - 1u < 3`).
std::uint32_t SlotNumber(int slot) { return static_cast<std::uint32_t>(slot); }
}  // namespace

// decomp: SLUS_010.40 vs_main_streamXa (src/SLUS_010.40/main.c:9358)
int LiveBeginXaStream(int sector, int byte_count) {
  XaStreamDrive drive = LoadDrive();
  if (!BeginXaStream(drive, sector, byte_count)) return 0;
  CommitDrive(drive);
  return 1;
}

// decomp: SLUS_010.40 vs_main_checkStreamXaEnd (src/SLUS_010.40/main.c:9376)
int LiveRequestXaStreamStop() {
  XaStreamDrive drive = LoadDrive();
  if (!RequestXaStreamStop(drive)) return 0;
  CommitDrive(drive);
  return 1;
}

// decomp: SLUS_010.40 func_800464DC (src/SLUS_010.40/main.c:10310)
void LiveStopCutsceneStream() {
  LiveEffectStreams host;
  StopCutsceneStream(host);
}

// decomp: SLUS_010.40 func_800464FC (src/SLUS_010.40/main.c:10312)
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
int LivePlayCutsceneStreamSlot(int slot, int pan, int volume) {
  LiveEffectStreams host;
  return PlayCutsceneStreamSlot(host, SlotNumber(slot),
                                {.pan = pan, .volume = volume})
             ? 1
             : 0;
}

// decomp: SLUS_010.40 func_80046578 (src/SLUS_010.40/main.c:10324)
void LivePlayCutsceneStreamSlotCentered(int slot) {
  LiveEffectStreams host;
  PlayCutsceneStreamSlotCentered(host, SlotNumber(slot));
}

// decomp: SLUS_010.40 func_8004659C (src/SLUS_010.40/main.c:10326)
int LiveFreeCutsceneStreamSlot(int slot) {
  LiveEffectStreams host;
  return FreeCutsceneStreamSlot(host, SlotNumber(slot)) ? 1 : 0;
}

// decomp: SLUS_010.40 func_800468DC (src/SLUS_010.40/main.c:10450)
void LiveSetXaVolumeMaximum() {
  LiveEffectStreams host;
  SetXaVolumeMaximum(host);
}
}  // namespace VS::Audio
