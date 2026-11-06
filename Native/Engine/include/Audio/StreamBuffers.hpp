#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Audio/StreamControl.hpp"

// The cutscene stream's SPU double buffering (Sound_Cutscene_FindFreeVoicePair
// through func_8001D614, SLUS_010.40/25AC.c:6254-6500): the stream plays on a
// voice pair looping over two buffers in SPU memory, each buffer's interrupt
// refilling the other from the stream's next chunk until the stream (and its
// repeat section) runs out, when the voices fall back to a silent loop. Chunk
// cursors are offsets into the stream's bytes, and every refill is checked
// against them before it is written.
namespace VS::Audio::Cutscene {
// The SPU memory addresses of the pair's buffers.
inline constexpr std::uint32_t kFirstBufferAddress = 0x1100;
inline constexpr std::uint32_t kSecondBufferAddress = 0x2100;
inline constexpr std::uint32_t kFirstRightBufferAddress = 0x1900;
inline constexpr std::uint32_t kSecondRightBufferAddress = 0x2900;
// The silent loop the voices repeat once the stream has run out.
inline constexpr std::uint32_t kSilentLoopAddress = 0x1030;
// A buffer's interrupt fires this far into it.
inline constexpr std::uint32_t kBufferInterruptOffset = 8;
inline constexpr std::uint32_t kMonoBufferBytes = 0x800;
inline constexpr std::uint32_t kStereoBufferBytes = 0x1000;
inline constexpr std::int32_t kStreamVoiceCount = 24;

// How InitializeStreamVoice spreads the stream's volume over a voice.
enum class StreamPanMode : std::uint8_t {
  kPanned = 0,
  kLeft = 1,
  kRight = 2,
  kCentered = 3,
};

// The refill a buffer's interrupt runs next.
enum class StreamRefill : std::uint8_t {
  kMonoFirst,
  kMonoSecond,
  kStereoFirst,
  kStereoSecond,
};

// What runs when an SPU transfer completes.
enum class StreamTransferCompletion : std::uint8_t {
  kNone,
  kBeginMonoPlayback,
  // The sound driver's sample transfer completion.
  kSampleTransfer,
};

enum class StreamBufferStatus : std::uint8_t {
  kApplied,
  // No stream plays (or none is left to stream): nothing changed.
  kIdle,
  kInvalidVoice,
  // The chunk to read or advance to lies outside the stream's bytes.
  kChunkOutOfRange,
};

// The stream state (g_Sound_Cutscene_StreamState) the buffers advance.
struct StreamBufferState {
  // The next chunk to upload; nullopt when the legacy cursor lies outside
  // the stream's bytes.
  std::optional<std::size_t> chunk;
  // The repeat section the stream continues from; nullopt when it has none.
  std::optional<std::size_t> repeat_chunk;
  // The bytes left to stream (TotalPages), and the repeat section's.
  std::uint32_t pages = 0;
  std::uint32_t repeat_pages = 0;
  // The pair's first voice, and the pair's mask (zero while no stream
  // plays).
  std::int32_t voice = 0;
  std::uint32_t voice_mask = 0;
  // _isSpuTransfer.
  bool transfer_busy = false;

  friend bool operator==(const StreamBufferState&,
                         const StreamBufferState&) = default;
};

// The volume, pan byte and pitch the stream's voices start with.
struct StreamVoiceMix {
  std::int32_t volume = 0;
  std::uint8_t pan = 0;
  std::int32_t sample_rate = 0;
  bool mono_output = false;
};

struct StreamVoiceGains {
  std::int16_t left = 0;
  std::int16_t right = 0;

  friend bool operator==(const StreamVoiceGains&,
                         const StreamVoiceGains&) = default;
};

// Sound_Cutscene_InitVoice's envelope arguments.
inline constexpr std::int32_t kStreamDecayRate = 0xF;
inline constexpr std::int32_t kStreamSustainLevel = 0xF;
inline constexpr std::int32_t kStreamSustainRate = 0x7F;
inline constexpr std::uint32_t kStreamSustainDirection = 3;
inline constexpr std::int32_t kStreamReleaseRate = 6;
inline constexpr std::uint32_t kStreamReleaseMode = 3;

// Sound_Cutscene_InitVoice's envelope.
struct StreamVoiceEnvelope {
  std::int32_t attack_step = 0;
  std::uint32_t attack_mode = 1;
  std::int32_t decay_rate = kStreamDecayRate;
  std::int32_t sustain_level = kStreamSustainLevel;
  std::int32_t sustain_rate = kStreamSustainRate;
  std::uint32_t sustain_direction = kStreamSustainDirection;
  std::int32_t release_rate = kStreamReleaseRate;
  std::uint32_t release_mode = kStreamReleaseMode;

  friend bool operator==(const StreamVoiceEnvelope&,
                         const StreamVoiceEnvelope&) = default;
};

struct StreamVoiceSetup {
  std::int32_t voice = 0;
  StreamPanMode mode = StreamPanMode::kPanned;
  std::uint32_t start_address = 0;
  std::uint32_t loop_address = 0;
};

// Sound_Cutscene_BeginPlayback's arguments: how far the first upload
// advanced, where the interrupt waits and the refill it arms.
struct StreamPlaybackStart {
  std::uint32_t advance = 0;
  std::uint32_t interrupt_address = 0;
  StreamRefill next = StreamRefill::kMonoFirst;
};

// Sound_Cutscene_LoadNextBuffer's arguments: the buffers to fill (left,
// then the right voice's loop), the chunk size and the refill to arm next.
struct StreamRefillBuffers {
  std::uint32_t left_address = 0;
  std::uint32_t right_address = 0;
  std::uint32_t bytes = 0;
  StreamRefill next = StreamRefill::kMonoFirst;
};

class StreamBufferHost {
 public:
  StreamBufferHost() = default;
  StreamBufferHost(const StreamBufferHost&) = delete;
  StreamBufferHost& operator=(const StreamBufferHost&) = delete;
  StreamBufferHost(StreamBufferHost&&) = delete;
  StreamBufferHost& operator=(StreamBufferHost&&) = delete;
  virtual ~StreamBufferHost() = default;

  virtual StreamBufferState State() = 0;
  virtual void StoreState(const StreamBufferState& state) = 0;
  // The playing stream's bytes; empty when none is bound.
  virtual std::span<const std::uint8_t> StreamBytes() = 0;
  virtual StreamVoiceMix VoiceMix() = 0;
  virtual std::span<const std::int16_t, kPanGainCount> PanGains() = 0;

  // The voices the sequencer keeps (playing or paused effects).
  virtual std::uint32_t BusyVoices() = 0;
  // Releases the lowest priority effect voice (Sound_EvictSfxVoice).
  virtual void ReleaseLowestEffectVoice() = 0;

  virtual void SetVoiceGains(std::int32_t voice, StreamVoiceGains gains) = 0;
  virtual void SetVoiceSampleRate(std::int32_t voice,
                                  std::int32_t sample_rate) = 0;
  virtual void SetVoiceStartAddress(std::int32_t voice,
                                    std::uint32_t address) = 0;
  virtual void SetVoiceLoopAddress(std::int32_t voice,
                                   std::uint32_t address) = 0;
  virtual void SetVoiceEnvelope(std::int32_t voice,
                                const StreamVoiceEnvelope& envelope) = 0;
  virtual void KeyOnVoices(std::uint32_t mask) = 0;

  // The SPU transfer: where the next write lands, what runs once it
  // completes, and the write itself.
  virtual void SetTransferAddress(std::uint32_t address) = 0;
  virtual void SetTransferCompletion(StreamTransferCompletion completion) = 0;
  virtual void WriteSamples(std::span<const std::uint8_t> samples) = 0;

  // The buffer interrupt: disarmed, or armed at `address` to run `next`
  // (nullopt: the stream's end handler, IRQCallbackProc).
  virtual void DisarmBufferInterrupt() = 0;
  virtual void ArmBufferInterrupt(std::optional<StreamRefill> next,
                                  std::uint32_t address) = 0;
};

// The highest free voice pair from voices 12-13 up to 22-23, releasing
// effect voices until one frees or none is left to release; nullopt then.
std::optional<std::int32_t> FindFreeVoicePair(StreamBufferHost& host);
// Uploads the mono stream's second buffer; its completion begins playback.
StreamBufferStatus UploadSecondMonoBuffer(StreamBufferHost& host);
// Sets up one stream voice: gains, pitch, buffers and envelope.
StreamBufferStatus InitializeStreamVoice(StreamBufferHost& host,
                                         const StreamVoiceSetup& setup);
// Keys the pair on once both buffers hold the stream's first chunks.
StreamBufferStatus BeginStreamPlayback(StreamBufferHost& host,
                                       const StreamPlaybackStart& start);
StreamBufferStatus BeginMonoPlayback(StreamBufferHost& host);
StreamBufferStatus BeginStereoPlayback(StreamBufferHost& host);
// Uploads the stream's next chunk into the buffer the voices just left.
StreamBufferStatus RefillStreamBuffer(StreamBufferHost& host,
                                      const StreamRefillBuffers& refill);
StreamBufferStatus RefillMonoFirstBuffer(StreamBufferHost& host);
StreamBufferStatus RefillMonoSecondBuffer(StreamBufferHost& host);
StreamBufferStatus RefillStereoFirstBuffer(StreamBufferHost& host);
StreamBufferStatus RefillStereoSecondBuffer(StreamBufferHost& host);
}  // namespace VS::Audio::Cutscene
