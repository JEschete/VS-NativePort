// Hand-maintained (tools/port/owned.csv).
// Live binding for SoundDriver.hpp: the legacy SPU register file, the SPU
// transfer state and callback, the SDK root counter 2 and its event, and the
// voice output.
#include "Audio/SoundDriverLive.hpp"

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <span>

#include "Audio/PlaybackResetLive.hpp"
#include "Audio/SeqRecordsLive.hpp"
#include "Audio/SeqSequencerLive.hpp"
#include "Audio/SequencerOutput.h"
#include "Audio/SoundDriver.hpp"
#include "Audio/VoiceOutput.hpp"
#include "Audio/VoiceOutputLive.hpp"

extern "C" {
#include "Audio/Cutscene/Stream.h"
#include "Audio/Driver/SoundDriver.h"
#include "Legacy/Psx.h"
}

namespace VS::Audio::Driver {
namespace {
constexpr std::size_t kRegistersPerVoice = 8;
static_assert(LEGACY_SPU_REGISTER_BYTES >=
              kVoiceCount * kRegistersPerVoice * sizeof(std::uint16_t));
static_assert(sizeof(Audio_Driver_legacySpuEvent) == sizeof(std::int32_t));
static_assert(sizeof(Audio_Cutscene_legacySpuTransferBusy) ==
              sizeof(std::int32_t));

std::uint16_t& Register(std::uint32_t voice, VoiceRegister reg) {
  auto* const registers = static_cast<std::uint16_t*>(LegacySpu_Registers());
  return registers[(voice * kRegistersPerVoice) +
                   static_cast<std::size_t>(reg)];
}

bool Accepted(std::integral auto result) { return result != 0; }

const char* RefusalReason(DriverRefusal refusal) {
  switch (refusal) {
    case DriverRefusal::kInvalidVoice:
      return "the voice lies outside the 24 SPU voices";
    case DriverRefusal::kTransferNeverEnded:
      return "the SPU transfer never ended";
    case DriverRefusal::kTickTimerUnavailable:
      return "root counter 2 refused every attempt";
    case DriverRefusal::kTickEventUnavailable:
      return "the root counter 2 event refused every attempt";
  }
  return "refused";
}

class LiveSoundDriver final : public SoundDriverHost {
 public:
  explicit LiveSoundDriver(const char* operation,
                           unsigned char* buffer = nullptr)
      : operation_(operation), buffer_(buffer) {}

  std::uint16_t ReadVoiceRegister(std::uint32_t voice,
                                  VoiceRegister reg) override {
    return Register(voice, reg);
  }

  void WriteVoiceRegister(std::uint32_t voice, VoiceRegister reg,
                          std::uint16_t value) override {
    Register(voice, reg) = value;
  }

  void SetVoiceSampleRate(std::uint32_t voice,
                          std::int32_t sample_rate) override {
    SetLiveVoiceSampleRate(static_cast<int>(voice), sample_rate);
  }

  void SetVoiceVolume(std::uint32_t voice, std::uint32_t left,
                      std::uint32_t right, std::uint32_t scale) override {
    SetLiveVoiceVolume(static_cast<int>(voice), left, right, scale);
  }

  std::int32_t TransferState() override {
    return Audio_Cutscene_legacySpuTransferBusy;
  }

  void SetTransferState(std::int32_t state) override {
    Audio_Cutscene_legacySpuTransferBusy = state;
  }

  void SetTransferCallback(TransferCallback callback) override {
    SpuSetTransferCallback(callback == TransferCallback::kCompleteTransfer
                               ? &LiveCompleteSampleTransfer
                               : nullptr);
  }

  // SpuWrite takes the caller's buffer; `data` is a view of it.
  void WriteSampleMemory(std::span<const std::byte> data) override {
    const auto* const start = reinterpret_cast<const std::byte*>(buffer_);
    SpuWrite(buffer_ + (data.data() - start), data.size());
  }

  void ReadSampleMemory(std::span<std::byte> data) override {
    SpuRead(reinterpret_cast<unsigned char*>(data.data()), data.size());
  }

  void ResetPlayback() override { ResetLivePlaybackState(); }

  bool SetTickTimer(std::uint16_t target) override {
    return Accepted(SetRCnt(RCntCNT2, target, RCntMdINTR));
  }

  bool StartTickTimer() override { return Accepted(StartRCnt(RCntCNT2)); }

  bool StopTickTimer() override { return Accepted(StopRCnt(RCntCNT2)); }

  std::int32_t OpenTickEvent() override {
    return static_cast<std::int32_t>(
        OpenEvent(RCntCNT2, EvSpINT, EvMdINTR, &Seq::LiveTickPlayback));
  }

  std::int32_t TickEvent() override { return Audio_Driver_legacySpuEvent; }

  void SetTickEvent(std::int32_t event) override {
    Audio_Driver_legacySpuEvent = event;
  }

  bool EnableTickEvent(std::int32_t event) override {
    return Accepted(EnableEvent(event));
  }

  bool DisableTickEvent(std::int32_t event) override {
    return Accepted(DisableEvent(event));
  }

  bool CloseTickEvent(std::int32_t event) override {
    return Accepted(CloseEvent(event));
  }

  void ResetOutput() override { Audio_Native_Reset(); }

  void Refuse(DriverRefusal refusal) override {
    Seq::ReportRefusal(operation_, RefusalReason(refusal));
  }

 private:
  const char* operation_;
  unsigned char* buffer_;
};

std::span<std::byte> BufferOf(char* data, unsigned size) {
  if (data == nullptr) return {};
  return {reinterpret_cast<std::byte*>(data), size};
}
}  // namespace

// decomp: SLUS_010.40 StartSound (src/SLUS_010.40/25AC.c:1060)
void LiveStart() {
  LiveSoundDriver host("StartSound");
  Start(host);
}

// decomp: SLUS_010.40 StopSound (src/SLUS_010.40/25AC.c:1085)
void LiveStop() {
  LiveSoundDriver host("StopSound");
  Stop(host);
}

// decomp: SLUS_010.40 _spuWriteComplete (src/SLUS_010.40/25AC.c:989)
void LiveCompleteSampleTransfer() {
  LiveSoundDriver host("CompleteSampleTransfer");
  CompleteSampleTransfer(host);
}

// decomp: SLUS_010.40 spuSetTransferCallback (src/SLUS_010.40/25AC.c:995)
void LiveArmSampleTransferCallback() {
  LiveSoundDriver host("ArmSampleTransferCallback");
  ArmSampleTransferCallback(host);
}

// decomp: SLUS_010.40 _writeSpu (src/SLUS_010.40/25AC.c:1001)
void LiveWriteSampleMemory(char* data, unsigned size) {
  if (data == nullptr && size != 0) {
    Seq::ReportRefusal("WriteSampleMemory", "no source buffer");
    return;
  }
  LiveSoundDriver host("WriteSampleMemory",
                       reinterpret_cast<unsigned char*>(data));
  WriteSampleMemory(host, BufferOf(data, size));
}

// decomp: SLUS_010.40 _readSpu (src/SLUS_010.40/25AC.c:1008)
void LiveReadSampleMemory(char* data, unsigned size) {
  if (data == nullptr && size != 0) {
    Seq::ReportRefusal("ReadSampleMemory", "no destination buffer");
    return;
  }
  LiveSoundDriver host("ReadSampleMemory");
  ReadSampleMemory(host, BufferOf(data, size));
}

// decomp: SLUS_010.40 _waitTransferAvailable (src/SLUS_010.40/25AC.c:1014)
void LiveWaitForSampleTransfer() {
  LiveSoundDriver host("WaitForSampleTransfer");
  WaitForSampleTransfer(host);
}

// decomp: SLUS_010.40 SetVoiceStartAddr (src/SLUS_010.40/25AC.c:1153)
void LiveSetVoiceStartAddress(int voice, unsigned address) {
  LiveSoundDriver host("SetVoiceStartAddress");
  SetVoiceStartAddress(host, voice, address);
}

// decomp: SLUS_010.40 SetVoiceRepeatAddr (src/SLUS_010.40/25AC.c:1158)
void LiveSetVoiceLoopAddress(int voice, unsigned address) {
  LiveSoundDriver host("SetVoiceLoopAddress");
  SetVoiceLoopAddress(host, voice, address);
}

// decomp: SLUS_010.40 SetVoiceAdsrAttackRateAndMode
// (src/SLUS_010.40/25AC.c:1262)
void LiveSetVoiceAttackRateAndMode(int voice, int attack_step,
                                   unsigned attack_mode) {
  LiveSoundDriver host("SetVoiceAttackRateAndMode");
  SetVoiceAttackRateAndMode(host, voice, attack_step, attack_mode);
}

// decomp: SLUS_010.40 SetVoiceAdsrDecayRate (src/SLUS_010.40/25AC.c:1274)
void LiveSetVoiceDecayRate(int voice, int decay_rate) {
  LiveSoundDriver host("SetVoiceDecayRate");
  SetVoiceDecayRate(host, voice, decay_rate);
}

// decomp: SLUS_010.40 SetVoiceAdsrSustainLevel (src/SLUS_010.40/25AC.c:1283)
void LiveSetVoiceSustainLevel(int voice, int sustain_level) {
  LiveSoundDriver host("SetVoiceSustainLevel");
  SetVoiceSustainLevel(host, voice, sustain_level);
}

// decomp: SLUS_010.40 SetVoiceAdsrSustainRateAndDirection
// (src/SLUS_010.40/25AC.c:1290)
void LiveSetVoiceSustainRateAndMode(int voice, int sustain_rate,
                                    unsigned sustain_direction) {
  LiveSoundDriver host("SetVoiceSustainRateAndMode");
  SetVoiceSustainRateAndMode(host, voice, sustain_rate, sustain_direction);
}
}  // namespace VS::Audio::Driver
