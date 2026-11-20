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
#include "Audio/SoundState.hpp"
#include "Audio/Stream.hpp"
#include "Content/CdDrive.hpp"
#include "Content/DiscQueue.hpp"
#include "Legacy/Psx.hpp"

namespace VS::Audio {
namespace {
static_assert(std::size(VS::Audio::soundState.sfx_stream_data) ==
              kEffectStreamSlotCount);
static_assert(sizeof(VS::Content::DiscDrive::state) == sizeof(std::uint8_t));
static_assert(sizeof(VS::Content::DiscDrive::failure_count) ==
              sizeof(std::uint8_t));
static_assert(sizeof(VS::Content::DiscDrive::start_sector) ==
              sizeof(std::int32_t));
static_assert(sizeof(VS::Content::DiscDrive::byte_count) ==
              sizeof(std::int32_t));
static_assert(sizeof(VS::Content::DiscDrive::buffered_sectors) ==
              sizeof(std::int32_t));

XaStreamDrive LoadDrive() {
  const VS::Content::DiscDrive& drive = VS::Content::discDriveState;
  return {
      // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
      .state = drive.state,
      .start_sector = drive.start_sector,
      .byte_count = drive.byte_count,
      .failure_count = drive.failure_count,
      .sectors_received = drive.sectors_received,
      .destination_sector = drive.destination_sector,
      .buffered_sectors = drive.buffered_sectors,
      .ring_sectors_written = drive.ring_sectors_written,
      .ring_write_index = drive.ring_write_index,
      .ring_read_index = drive.ring_read_index,
  };
}

void CommitDrive(const XaStreamDrive& typed) {
  VS::Content::DiscDrive& drive = VS::Content::discDriveState;
  drive.state = typed.state;
  drive.start_sector = typed.start_sector;
  drive.byte_count = typed.byte_count;
  drive.failure_count = typed.failure_count;
  drive.sectors_received = typed.sectors_received;
  drive.destination_sector = typed.destination_sector;
  drive.buffered_sectors = typed.buffered_sectors;
  drive.ring_sectors_written = typed.ring_sectors_written;
  drive.ring_write_index = typed.ring_write_index;
  drive.ring_read_index = typed.ring_read_index;
}

class LiveEffectStreams final : public EffectStreamHost {
 public:
  void StopCutsceneStream() override { Control::LiveStopCutsceneStream(); }

  BankBuffer EffectStream(std::size_t index) override {
    return LiveBankBuffer(VS::Audio::soundState.sfx_stream_data[index]);
  }

  void FadeCutsceneVolume(Control::Fade fade) override {
    Control::LiveFadeCutsceneVolume(fade.ticks, fade.target);
  }

  void StoreEffectStream(std::size_t index, BankBuffer stream) override {
    VS::Audio::soundState.sfx_stream_data[index] = LiveBufferPointer(stream);
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
