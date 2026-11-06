#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstddef>
#include <cstdint>

#include "Audio/Sfx.hpp"
#include "Audio/SoundBanks.hpp"
#include "Audio/SoundControl.hpp"
#include "Content/CdDrive.hpp"

// The XA stream requests (vs_main_streamXa and vs_main_checkStreamXaEnd,
// SLUS_010.40 main.c:9358-9385) over the disc drive record, and the sound
// effect stream slots (func_800464DC through func_8004659C and func_800468DC,
// main.c:10310-10335 and 10450): three slots, numbered from 1, each holding
// a stream loaded into the high end of the game heap, played through the
// cutscene stream voices.
namespace VS::Audio {
// func_800468DC's disc (XA) volume.
inline constexpr std::int32_t kMaximumXaVolume = 0x7FFF;
// The reverb flag func_800464FC starts a stream with.
inline constexpr std::int32_t kEffectStreamReverb = 1;

// The drive record (vs_main_disk) fields vs_main_streamXa sets.
struct XaStreamDrive {
  Content::DiscDriveState state = Content::DiscDriveState::kIdle;
  std::int32_t start_sector = 0;
  std::int32_t byte_count = 0;
  std::uint8_t failure_count = 0;
  std::uint32_t sectors_received = 0;
  std::uint32_t destination_sector = 0;
  std::int32_t buffered_sectors = 0;
  std::uint32_t ring_sectors_written = 0;
  std::uint32_t ring_write_index = 0;
  std::uint32_t ring_read_index = 0;

  friend bool operator==(const XaStreamDrive&, const XaStreamDrive&) = default;
};

// Starts streaming `byte_count` bytes of XA audio from `sector` when the
// drive is idle: resets the read and ring counters and sets the XA read
// state. False (and nothing changes) when the drive is busy.
bool BeginXaStream(XaStreamDrive& drive, std::int32_t sector,
                   std::int32_t byte_count);
// Asks a streaming drive to end its XA stream. False when it is not
// streaming.
bool RequestXaStreamStop(XaStreamDrive& drive);

class EffectStreamHost {
 public:
  EffectStreamHost() = default;
  EffectStreamHost(const EffectStreamHost&) = delete;
  EffectStreamHost& operator=(const EffectStreamHost&) = delete;
  EffectStreamHost(EffectStreamHost&&) = delete;
  EffectStreamHost& operator=(EffectStreamHost&&) = delete;
  virtual ~EffectStreamHost() = default;

  // Effect stream slot `index`'s buffer (below kEffectStreamSlotCount).
  virtual BankBuffer EffectStream(std::size_t index) = 0;
  virtual void StoreEffectStream(std::size_t index, BankBuffer stream) = 0;
  virtual void FreeBuffer(BankBuffer buffer) = 0;
  // The sound control requests (SoundControl.hpp).
  virtual void FadeCutsceneVolume(Control::Fade fade) = 0;
  virtual void StartCutsceneStream(BankBuffer stream, std::int32_t pan,
                                   std::int32_t reverb) = 0;
  virtual void StopCutsceneStream() = 0;
  virtual void SetDiscVolume(std::int32_t volume) = 0;
};

// func_800464DC.
void StopCutsceneStream(EffectStreamHost& host);
// Plays slot `slot`'s stream at once at `mix`'s volume and pan, with reverb.
// False when the slot (1 to 3) holds no stream.
bool PlayCutsceneStreamSlot(EffectStreamHost& host, std::uint32_t slot,
                            SfxMix mix);
// Plays it centered at full volume.
void PlayCutsceneStreamSlotCentered(EffectStreamHost& host, std::uint32_t slot);
// Stops the cutscene stream and frees slot `slot`'s stream. False when the
// slot holds none.
bool FreeCutsceneStreamSlot(EffectStreamHost& host, std::uint32_t slot);
// func_800468DC: the disc volume at its maximum.
void SetXaVolumeMaximum(EffectStreamHost& host);
}  // namespace VS::Audio
