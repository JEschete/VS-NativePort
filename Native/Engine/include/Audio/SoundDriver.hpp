#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstddef>
#include <cstdint>
#include <span>

// The sound driver's sample transfers, tick timer and SPU voice register
// writes (_spuWriteComplete through SetVoiceParamsByFlags,
// SLUS_010.40/25AC.c:989-1395). The registers, the transfer state, the SDK
// timer and event calls and the voice output go through SoundDriverHost.
namespace VS::Audio::Driver {
// The halfwords of one voice's SPU register block (src/include/hw.h:7-12).
enum class VoiceRegister : std::uint8_t {
  kVolumeLeft = 0,
  kVolumeRight = 1,
  kSampleRate = 2,
  kStartAddress = 3,
  kEnvelopeLower = 4,
  kEnvelopeUpper = 5,
  kEnvelopeLevel = 6,
  kLoopAddress = 7,
};

// The SPU transfer completion callback the driver installs.
enum class TransferCallback : std::uint8_t { kNone, kCompleteTransfer };

// The transfer state _waitTransferAvailable spins on.
inline constexpr std::int32_t kTransferIdle = 0;
inline constexpr std::int32_t kTransferBusy = 1;

// Root counter 2 counts to this target between driver ticks (StartSound).
inline constexpr std::uint16_t kTickTimerTarget = 0x44E8;
// OpenEvent's failure result.
inline constexpr std::int32_t kNoTickEvent = -1;
// The original retries each SDK call until it succeeds and spins on the
// transfer state until the transfer ends; the port gives up after these many
// attempts and refuses instead of hanging.
inline constexpr std::uint32_t kSdkRetryLimit = 1024;
inline constexpr std::uint32_t kTransferPollLimit = 1U << 20;

// Voice parameter dirty flags (src/include/sound.h:14-38).
inline constexpr std::uint32_t kVolumeParameters = 0x3;
inline constexpr std::uint32_t kSampleRateParameter = 0x10;
inline constexpr std::uint32_t kStartAddressParameter = 0x80;
inline constexpr std::uint32_t kEnvelopeLowerParameters = 0x9900;
inline constexpr std::uint32_t kEnvelopeUpperParameters = 0x6600;
inline constexpr std::uint32_t kLoopAddressParameter = 0x10000;

enum class DriverRefusal : std::uint8_t {
  kInvalidVoice,
  kTransferNeverEnded,
  kTickTimerUnavailable,
  kTickEventUnavailable,
};

// Audio_VoiceParams: one channel voice's SPU parameters at the original
// widths and signedness.
struct VoiceParameters {
  std::uint32_t dirty_flags = 0;
  std::uint32_t sample_start = 0;
  std::uint32_t loop_start = 0;
  std::uint16_t sample_rate = 0;
  std::uint16_t adsr_lower = 0;
  std::uint16_t adsr_upper = 0;
  std::uint16_t volume_scale = 0;
  std::int16_t volume_left = 0;
  std::int16_t volume_right = 0;

  friend bool operator==(const VoiceParameters&,
                         const VoiceParameters&) = default;
};

class SoundDriverHost {
 public:
  virtual ~SoundDriverHost() = default;

  // Voice registers; `voice` is below kVoiceCount.
  virtual std::uint16_t ReadVoiceRegister(std::uint32_t voice,
                                          VoiceRegister reg) = 0;
  virtual void WriteVoiceRegister(std::uint32_t voice, VoiceRegister reg,
                                  std::uint16_t value) = 0;
  // The voice output's SetVoiceSampleRate and SetVoiceVolume.
  virtual void SetVoiceSampleRate(std::uint32_t voice,
                                  std::int32_t sample_rate) = 0;
  virtual void SetVoiceVolume(std::uint32_t voice, std::uint32_t left,
                              std::uint32_t right, std::uint32_t scale) = 0;

  // Sample memory transfers.
  virtual std::int32_t TransferState() = 0;
  virtual void SetTransferState(std::int32_t state) = 0;
  virtual void SetTransferCallback(TransferCallback callback) = 0;
  virtual void WriteSampleMemory(std::span<const std::byte> data) = 0;
  virtual void ReadSampleMemory(std::span<std::byte> data) = 0;

  // The tick timer (root counter 2) and its event. Each call reports whether
  // the SDK accepted it; OpenTickEvent returns the event or kNoTickEvent.
  virtual void ResetPlayback() = 0;
  virtual bool SetTickTimer(std::uint16_t target) = 0;
  virtual bool StartTickTimer() = 0;
  virtual bool StopTickTimer() = 0;
  virtual std::int32_t OpenTickEvent() = 0;
  virtual std::int32_t TickEvent() = 0;
  virtual void SetTickEvent(std::int32_t event) = 0;
  virtual bool EnableTickEvent(std::int32_t event) = 0;
  virtual bool DisableTickEvent(std::int32_t event) = 0;
  virtual bool CloseTickEvent(std::int32_t event) = 0;
  virtual void ResetOutput() = 0;

  virtual void Refuse(DriverRefusal refusal) = 0;
};

void CompleteSampleTransfer(SoundDriverHost& host);
void ArmSampleTransferCallback(SoundDriverHost& host);
void WriteSampleMemory(SoundDriverHost& host, std::span<const std::byte> data);
void ReadSampleMemory(SoundDriverHost& host, std::span<std::byte> data);
// Returns whether the transfer ended within kTransferPollLimit polls.
bool WaitForSampleTransfer(SoundDriverHost& host);

// Returns whether every SDK call succeeded within kSdkRetryLimit attempts;
// a refused call ends the sequence.
bool Start(SoundDriverHost& host);
bool Stop(SoundDriverHost& host);

// Each voice register write refuses a voice outside the 24 SPU voices and
// writes nothing.
void SetVoiceStartAddress(SoundDriverHost& host, std::int32_t voice,
                          std::uint32_t address);
void SetVoiceLoopAddress(SoundDriverHost& host, std::int32_t voice,
                         std::uint32_t address);
void SetVoiceEnvelopeLower(SoundDriverHost& host, std::int32_t voice,
                           std::uint16_t value);
void SetVoiceEnvelopeUpper(SoundDriverHost& host, std::int32_t voice,
                           std::uint16_t value);
void SetVoiceAttackRateAndMode(SoundDriverHost& host, std::int32_t voice,
                               std::int32_t attack_step,
                               std::uint32_t attack_mode);
void SetVoiceDecayRate(SoundDriverHost& host, std::int32_t voice,
                       std::int32_t decay_rate);
void SetVoiceSustainLevel(SoundDriverHost& host, std::int32_t voice,
                          std::int32_t sustain_level);
void SetVoiceSustainRateAndMode(SoundDriverHost& host, std::int32_t voice,
                                std::int32_t sustain_rate,
                                std::uint32_t sustain_direction);
// Writes every register of `parameters` and clears its dirty flags; a
// nonzero `volume_scale` scales both volumes by volume_scale/128.
void WriteVoiceParameters(SoundDriverHost& host, std::int32_t voice,
                          VoiceParameters& parameters,
                          std::int32_t volume_scale);
// Writes the registers `parameters` marks dirty and clears its dirty flags.
void WriteDirtyVoiceParameters(SoundDriverHost& host, std::uint32_t voice,
                               VoiceParameters& parameters);
}  // namespace VS::Audio::Driver
