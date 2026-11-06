// Hand-maintained (tools/port/owned.csv).
// Typed XA stream requests and effect stream slots (Stream.hpp).
#include <cstddef>
#include <cstdint>
#include <optional>

#include "Audio/Sfx.hpp"
#include "Audio/SoundBanks.hpp"
#include "Audio/SoundControl.hpp"
#include "Audio/Stream.hpp"
#include "Content/CdDrive.hpp"

namespace VS::Audio {
namespace {
// Slot `slot`'s index when it holds a stream (the original's unsigned
// `slot - 1 < 3`).
std::optional<std::size_t> HeldStream(EffectStreamHost& host,
                                      std::uint32_t slot) {
  if (slot - 1U >= kEffectStreamSlotCount) return std::nullopt;
  const std::size_t index = slot - 1U;
  if (host.EffectStream(index) == BankBuffer::kNone) return std::nullopt;
  return index;
}
}  // namespace

// decomp: SLUS_010.40 vs_main_streamXa (src/SLUS_010.40/main.c:9358)
bool BeginXaStream(XaStreamDrive& drive, std::int32_t sector,
                   std::int32_t byte_count) {
  if (drive.state != Content::DiscDriveState::kIdle) return false;
  drive.start_sector = sector;
  drive.byte_count = byte_count;
  drive.ring_read_index = 0;
  drive.ring_write_index = 0;
  drive.destination_sector = 0;
  drive.buffered_sectors = 0;
  drive.sectors_received = 0;
  drive.failure_count = 0;
  drive.ring_sectors_written = 0;
  drive.state = Content::DiscDriveState::kReadXaInit;
  return true;
}

// decomp: SLUS_010.40 vs_main_checkStreamXaEnd (src/SLUS_010.40/main.c:9376)
bool RequestXaStreamStop(XaStreamDrive& drive) {
  if (drive.state != Content::DiscDriveState::kStreamXa) return false;
  drive.state = Content::DiscDriveState::kStreamXaEnd;
  return true;
}

// decomp: SLUS_010.40 func_800464DC (src/SLUS_010.40/main.c:10310)
void StopCutsceneStream(EffectStreamHost& host) { host.StopCutsceneStream(); }

// decomp: SLUS_010.40 func_800464FC (src/SLUS_010.40/main.c:10312)
bool PlayCutsceneStreamSlot(EffectStreamHost& host, std::uint32_t slot,
                            SfxMix mix) {
  const std::optional<std::size_t> index = HeldStream(host, slot);
  if (!index) return false;
  host.FadeCutsceneVolume({.ticks = 0, .target = mix.volume});
  host.StartCutsceneStream(host.EffectStream(*index), mix.pan,
                           kEffectStreamReverb);
  return true;
}

// decomp: SLUS_010.40 func_80046578 (src/SLUS_010.40/main.c:10324)
void PlayCutsceneStreamSlotCentered(EffectStreamHost& host,
                                    std::uint32_t slot) {
  PlayCutsceneStreamSlot(host, slot, SfxMix{});
}

// decomp: SLUS_010.40 func_8004659C (src/SLUS_010.40/main.c:10326)
bool FreeCutsceneStreamSlot(EffectStreamHost& host, std::uint32_t slot) {
  const std::optional<std::size_t> index = HeldStream(host, slot);
  if (!index) return false;
  host.StopCutsceneStream();
  host.FreeBuffer(host.EffectStream(*index));
  host.StoreEffectStream(*index, BankBuffer::kNone);
  return true;
}

// decomp: SLUS_010.40 func_800468DC (src/SLUS_010.40/main.c:10450)
void SetXaVolumeMaximum(EffectStreamHost& host) {
  host.SetDiscVolume(kMaximumXaVolume);
}
}  // namespace VS::Audio
