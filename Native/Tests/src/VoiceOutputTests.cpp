#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Audio/VoiceEngine.hpp"
#include "Audio/VoiceOutput.hpp"
#include "Audio/VoiceOutputLive.hpp"
#include "Content/AudioBank.hpp"
extern "C" {
#include "Audio/Cutscene/Stream.h"
}  // extern "C"

namespace {

using VS::Audio::EnvelopeRate;
using VS::Audio::Driver::VoiceGains;
using VS::Audio::Driver::VoiceMaskResult;
using VS::Audio::Driver::VoiceMode;
using VS::Audio::Driver::VoiceOutputHost;
using VS::Audio::Driver::VoiceStatus;
using VS::Audio::Samples::BankUploadStatus;
using VS::Audio::Samples::InstrumentRecord;
using VS::Audio::Samples::SampleBankHost;
using VS::Content::AudioInstrument;

namespace Driver = VS::Audio::Driver;
namespace Samples = VS::Audio::Samples;

struct ModeCall {
  std::uint32_t voice = 0;
  VoiceMode mode = VoiceMode::kReverb;
  bool enabled = false;
};

class RecordingVoices final : public VoiceOutputHost {
 public:
  std::vector<std::string> calls;
  std::vector<std::uint32_t> keyed_on;
  std::vector<std::uint32_t> keyed_off;
  std::vector<ModeCall> modes;
  std::uint32_t gains_voice = 0;
  VoiceGains gains;
  std::uint32_t rate_voice = 0;
  float rate = -1;
  std::uint32_t release_voice = 0;
  EnvelopeRate release;

  void KeyOn(std::uint32_t voice) override {
    calls.emplace_back("KeyOn");
    keyed_on.push_back(voice);
  }
  void KeyOff(std::uint32_t voice) override {
    calls.emplace_back("KeyOff");
    keyed_off.push_back(voice);
  }
  void SetVoiceMode(std::uint32_t voice, VoiceMode mode,
                    bool enabled) override {
    calls.emplace_back("Mode");
    modes.push_back({voice, mode, enabled});
  }
  void SetVoiceGains(std::uint32_t voice, const VoiceGains& value) override {
    calls.emplace_back("Gains");
    gains_voice = voice;
    gains = value;
  }
  void SetVoiceRate(std::uint32_t voice, float value) override {
    calls.emplace_back("Rate");
    rate_voice = voice;
    rate = value;
  }
  void SetVoiceRelease(std::uint32_t voice,
                       const EnvelopeRate& value) override {
    calls.emplace_back("Release");
    release_voice = voice;
    release = value;
  }
};

std::uint32_t EnabledMask(const std::vector<ModeCall>& modes) {
  std::uint32_t mask = 0;
  for (const auto& call : modes) {
    if (call.enabled) mask |= 1U << call.voice;
  }
  return mask;
}

bool SameRate(const EnvelopeRate& left, const EnvelopeRate& right) {
  return left.shift == right.shift && left.step == right.step &&
         left.exponential == right.exponential &&
         left.decreasing == right.decreasing && left.hold == right.hold;
}

struct BoundInstrument {
  std::size_t slot = 0;
  InstrumentRecord record;
  std::uint32_t bank_id = 0;
  std::size_t index = 0;
  bool available = false;
};

class RecordingBanks final : public SampleBankHost {
 public:
  std::vector<AudioInstrument> instruments;
  bool loadable = true;
  std::vector<std::uint32_t> loads;
  std::vector<BoundInstrument> bound;

  std::optional<std::span<const AudioInstrument>> LoadBank(
      std::uint32_t bank_id) override {
    loads.push_back(bank_id);
    if (!loadable) return std::nullopt;
    return std::span<const AudioInstrument>(instruments);
  }
  void BindInstrument(std::size_t slot, const InstrumentRecord& record,
                      std::uint32_t bank_id, std::size_t index,
                      bool available) override {
    bound.push_back({slot, record, bank_id, index, available});
  }
};

using BankHeaderBytes = std::array<std::byte, Samples::kBankHeaderBytes>;

void PutWord(BankHeaderBytes& bytes, std::size_t word, std::uint32_t value) {
  for (std::size_t byte = 0; byte < 4; ++byte) {
    bytes[word * 4 + byte] = static_cast<std::byte>(value >> (byte * 8));
  }
}

BankHeaderBytes AkaoHeader(std::uint32_t bank_id, std::uint32_t count) {
  BankHeaderBytes bytes{};
  PutWord(bytes, 0, Samples::kAkaoMagic);
  PutWord(bytes, 1, bank_id);
  PutWord(bytes, 7, count);
  return bytes;
}

AudioInstrument Instrument(std::uint32_t sample, std::uint32_t loop,
                           bool available = true) {
  AudioInstrument instrument;
  instrument.sample_offset = sample;
  instrument.loop_offset = loop;
  instrument.fine_tune = -3;
  instrument.root_note = 60;
  instrument.adsr_lower = 0x00FF;
  instrument.adsr_upper = 0x5FC4;
  instrument.available = available;
  return instrument;
}

// Uploads a two-instrument bank 7 at slots 254-255 with a sample base that
// wraps past 0xFFFFFFFF.
RecordingBanks UploadTwoInstruments() {
  RecordingBanks host;
  host.instruments = {Instrument(0x10, 0x20), Instrument(0x30, 0x40, false)};
  const auto bytes = AkaoHeader(7, 2);
  const auto result = Samples::UploadBank(host, bytes, 254, 0xFFFFFFF0);
  CHECK(result.status == BankUploadStatus::kUploaded);
  CHECK(result.header.bank_id == 7);
  return host;
}

}  // namespace

TEST_CASE("Voice key masks visit selected voices in ascending order",
          "[audio][driver]") {
  RecordingVoices host;
  const VoiceMaskResult on = Driver::KeyOnVoices(host, 0x00800005);
  CHECK(host.keyed_on == std::vector<std::uint32_t>{0, 2, 23});
  CHECK(on.voices == 0x00800005);
  CHECK(on.ignored == 0);
  const VoiceMaskResult off = Driver::KeyOffVoices(host, 0x80000012);
  CHECK(host.keyed_off == std::vector<std::uint32_t>{1, 4});
  CHECK(off.voices == 0x12);
  CHECK(off.ignored == 0x80000000);
  CHECK(host.calls.size() == 5);
}

TEST_CASE("Voice key masks above voice 23 reach no voice", "[audio][driver]") {
  RecordingVoices host;
  const VoiceMaskResult result = Driver::KeyOnVoices(host, 0xFF000000);
  CHECK(host.calls.empty());
  CHECK(result.voices == 0);
  CHECK(result.ignored == 0xFF000000);
  CHECK(Driver::KeyOffVoices(host, 0).voices == 0);
  CHECK(host.calls.empty());
}

TEST_CASE("Voice mode masks set every voice from the mask", "[audio][driver]") {
  RecordingVoices host;
  const VoiceMaskResult reverb = Driver::SetVoiceReverbMask(host, 0x01000003);
  REQUIRE(host.modes.size() == Driver::kVoiceCount);
  for (std::uint32_t voice = 0; voice < Driver::kVoiceCount; ++voice) {
    CHECK(host.modes[voice].voice == voice);
    CHECK(host.modes[voice].mode == VoiceMode::kReverb);
  }
  CHECK(EnabledMask(host.modes) == 3);
  CHECK(reverb.ignored == 0x01000000);

  host.modes.clear();
  Driver::SetVoiceNoiseMask(host, 0x00400000);
  REQUIRE(host.modes.size() == Driver::kVoiceCount);
  CHECK(host.modes.front().mode == VoiceMode::kNoise);
  CHECK(EnabledMask(host.modes) == 0x00400000);
}

TEST_CASE("Frequency modulation drops voice 0, which has no modulator",
          "[audio][driver]") {
  RecordingVoices host;
  const VoiceMaskResult result =
      Driver::SetVoiceFrequencyModulationMask(host, 0x00FFFFFF);
  REQUIRE(host.modes.size() == Driver::kVoiceCount);
  CHECK(host.modes.front().mode == VoiceMode::kFrequencyModulation);
  CHECK_FALSE(host.modes.front().enabled);
  CHECK(EnabledMask(host.modes) == 0x00FFFFFE);
  CHECK(result.voices == 0x00FFFFFE);
}

TEST_CASE("Voice volume registers scale, wrap and mask like the SPU write",
          "[audio][driver]") {
  CHECK(Driver::VoiceVolumeRegister(0x3FFF, 0) == 0x3FFF);
  CHECK(Driver::VoiceVolumeRegister(0x1000, 64) == 0x0800);
  CHECK(Driver::VoiceVolumeRegister(0x1000, 128) == 0x1000);
  CHECK(Driver::VoiceVolumeRegister(0xFFFF, 0) == 0x7FFF);
  // The unsigned multiply wraps before the shift.
  CHECK(Driver::VoiceVolumeRegister(0x80000000, 2) == 0);
  CHECK(Driver::VoiceVolumeRegister(0xFFFFFFFF, 1) == 0x7FFF);
  CHECK(Driver::VoiceVolumeGain(0x2000) == 0.5F);
  CHECK(Driver::VoiceVolumeGain(0x4000) == -1.0F);
  CHECK(Driver::VoiceVolumeGain(0x7FFF) == -1.0F / 0x4000);
  CHECK(Driver::VoiceVolumeGain(0xFFFF) == Driver::VoiceVolumeGain(0x7FFF));
}

TEST_CASE("Voice volume writes signed gains to valid voices",
          "[audio][driver]") {
  RecordingVoices host;
  CHECK(Driver::SetVoiceVolume(host, 23, 0x2000, 0x1000, 0) ==
        VoiceStatus::kApplied);
  CHECK(host.gains_voice == 23);
  CHECK(host.gains.left == 0.5F);
  CHECK(host.gains.right == 0.25F);
  CHECK(Driver::SetVoiceVolume(host, 0, 0x4000, 0x2000, 256) ==
        VoiceStatus::kApplied);
  CHECK(host.gains_voice == 0);
  CHECK(host.gains.left == 0);
  CHECK(host.gains.right == -1.0F);
}

TEST_CASE("Voice writes refuse voices outside the 24 SPU voices",
          "[audio][driver]") {
  RecordingVoices host;
  for (const int voice : {-1, 24, 0x7FFFFFFF}) {
    CHECK(Driver::SetVoiceVolume(host, voice, 0x1000, 0x1000, 0) ==
          VoiceStatus::kInvalidVoice);
    CHECK(Driver::SetVoiceSampleRate(host, voice, 0x1000) ==
          VoiceStatus::kInvalidVoice);
    CHECK(Driver::SetVoiceReleaseRateAndMode(host, voice, 5, 3) ==
          VoiceStatus::kInvalidVoice);
  }
  CHECK(host.calls.empty());
}

TEST_CASE("Voice sample rates are pitch ratios capped at four times",
          "[audio][driver]") {
  CHECK(Driver::VoiceSampleRateRatio(0) == 0);
  CHECK(Driver::VoiceSampleRateRatio(0x0800) == 0.5F);
  CHECK(Driver::VoiceSampleRateRatio(0x1000) == 1.0F);
  CHECK(Driver::VoiceSampleRateRatio(0x4000) == 4.0F);
  CHECK(Driver::VoiceSampleRateRatio(0xFFFF) == 4.0F);
}

TEST_CASE("Voice sample rate writes reach the selected voice",
          "[audio][driver]") {
  RecordingVoices host;
  CHECK(Driver::SetVoiceSampleRate(host, 7, 0x2000) == VoiceStatus::kApplied);
  CHECK(host.rate_voice == 7);
  CHECK(host.rate == 2.0F);
  CHECK(Driver::SetVoiceSampleRate(host, 7, 0xFFFF) == VoiceStatus::kApplied);
  CHECK(host.rate == 4.0F);
}

TEST_CASE("Voice sample rates outside the 16-bit pitch register are refused",
          "[audio][driver]") {
  RecordingVoices host;
  CHECK(Driver::SetVoiceSampleRate(host, 0, -1) ==
        VoiceStatus::kInvalidSampleRate);
  CHECK(Driver::SetVoiceSampleRate(host, 0, 0x10000) ==
        VoiceStatus::kInvalidSampleRate);
  CHECK(host.calls.empty());
}

TEST_CASE("Voice release decodes the rate and bit 2 of the mode",
          "[audio][driver]") {
  RecordingVoices host;
  CHECK(Driver::SetVoiceReleaseRateAndMode(host, 3, 5, 3) ==
        VoiceStatus::kApplied);
  CHECK(host.release_voice == 3);
  CHECK(SameRate(host.release, VS::Audio::DecodeAkaoEnvelope(0, 5).release));
  CHECK_FALSE(host.release.exponential);
  CHECK(host.release.decreasing);

  CHECK(Driver::SetVoiceReleaseRateAndMode(host, 3, 6, 7) ==
        VoiceStatus::kApplied);
  CHECK(SameRate(host.release, VS::Audio::DecodeAkaoEnvelope(0, 0x26).release));
  CHECK(host.release.exponential);
}

TEST_CASE("Voice release at the slowest rate holds the envelope",
          "[audio][driver]") {
  const auto slowest = Driver::ReleaseEnvelope(31, 0);
  REQUIRE(slowest);
  CHECK(slowest.value_or(EnvelopeRate{}).hold);
}

TEST_CASE("Voice release refuses values that spill into other ADSR fields",
          "[audio][driver]") {
  RecordingVoices host;
  CHECK(Driver::SetVoiceReleaseRateAndMode(host, 0, 32, 0) ==
        VoiceStatus::kInvalidRelease);
  CHECK(Driver::SetVoiceReleaseRateAndMode(host, 0, -1, 0) ==
        VoiceStatus::kInvalidRelease);
  CHECK(Driver::SetVoiceReleaseRateAndMode(host, 0, 0, 8) ==
        VoiceStatus::kInvalidRelease);
  CHECK(host.calls.empty());
  CHECK_FALSE(Driver::ReleaseEnvelope(0, 0xFFFFFFFF));
}

TEST_CASE("Live voice writes ignore invalid voices instead of aborting",
          "[audio][driver]") {
  Driver::SetLiveVoiceVolume(24, 0x1000, 0x1000, 0);
  Driver::SetLiveVoiceVolume(-1, 0x1000, 0x1000, 0);
  Driver::SetLiveVoiceSampleRate(24, 0x1000);
  Driver::SetLiveVoiceSampleRate(0, -1);
  Driver::SetLiveVoiceReleaseRateAndMode(24, 5, 3);
  Driver::SetLiveVoiceReleaseRateAndMode(0, 32, 3);
  Driver::KeyOffLiveVoices(0xFF000000);
  SUCCEED("invalid voice writes returned");
}

TEST_CASE("Bank headers expose the AKAO bank id and instrument count",
          "[audio][samples]") {
  const auto bytes = AkaoHeader(0x1234, 9);
  const auto header = Samples::ReadBankHeader(bytes);
  REQUIRE(header);
  const auto fields = header.value_or(Samples::BankHeader{});
  CHECK(fields.bank_id == 0x1234);
  CHECK(fields.instrument_count == 9);
  CHECK_FALSE(Samples::ReadBankHeader(std::span(bytes).first(31)));
  auto other = bytes;
  other[0] = std::byte{0};
  CHECK_FALSE(Samples::ReadBankHeader(other));
}

TEST_CASE("Bank uploads relocate records into consecutive slots",
          "[audio][samples]") {
  const auto host = UploadTwoInstruments();
  CHECK(host.loads == std::vector<std::uint32_t>{7});
  REQUIRE(host.bound.size() == 2);
  CHECK(host.bound[0].slot == 254);
  CHECK(host.bound[0].available);
  CHECK(host.bound[1].slot == 255);
  CHECK(host.bound[1].index == 1);
  CHECK(host.bound[1].bank_id == 7);
  CHECK_FALSE(host.bound[1].available);
}

TEST_CASE("Bank upload records copy the instrument and wrap the sample base",
          "[audio][samples]") {
  const auto host = UploadTwoInstruments();
  REQUIRE(host.bound.size() == 2);
  const auto& first = host.bound[0].record;
  CHECK(first.sample_start == 0);
  CHECK(first.loop_start == 0x10);
  CHECK(first.fine_tune == -3);
  CHECK(first.root_note == 60);
  CHECK(first.adsr_lower == 0x00FF);
  CHECK(first.adsr_upper == 0x5FC4);
  CHECK(host.bound[1].record.sample_start == 0x20);
  CHECK(host.bound[1].record.loop_start == 0x30);
}

TEST_CASE("Bank uploads refuse malformed headers before loading",
          "[audio][samples]") {
  RecordingBanks host;
  host.instruments = {Instrument(0, 0)};
  auto bytes = AkaoHeader(1, 1);
  CHECK(Samples::UploadBank(host, std::span(bytes).first(16), 0, 0).status ==
        BankUploadStatus::kTruncatedHeader);
  bytes[3] = std::byte{0};
  CHECK(Samples::UploadBank(host, bytes, 0, 0).status ==
        BankUploadStatus::kNotAkao);
  CHECK(host.loads.empty());
  CHECK(host.bound.empty());
}

TEST_CASE("Bank uploads refuse destinations outside the instrument table",
          "[audio][samples]") {
  RecordingBanks host;
  host.instruments = {Instrument(0, 0), Instrument(0, 0)};
  const auto two = AkaoHeader(1, 2);
  CHECK(Samples::UploadBank(host, two, -1, 0).status ==
        BankUploadStatus::kInvalidDestination);
  CHECK(Samples::UploadBank(host, two, 256, 0).status ==
        BankUploadStatus::kInvalidDestination);
  CHECK(Samples::UploadBank(host, two, 255, 0).status ==
        BankUploadStatus::kInvalidDestination);
  // The original copies one record for a zero count.
  CHECK(Samples::UploadBank(host, AkaoHeader(1, 0), 0, 0).status ==
        BankUploadStatus::kInvalidDestination);
  CHECK(Samples::UploadBank(host, AkaoHeader(1, 0xFFFFFFFF), 0, 0).status ==
        BankUploadStatus::kInvalidDestination);
  CHECK(host.loads.empty());
  CHECK(host.bound.empty());
}

TEST_CASE("Bank uploads refuse unavailable or mismatched banks",
          "[audio][samples]") {
  RecordingBanks host;
  host.loadable = false;
  CHECK(Samples::UploadBank(host, AkaoHeader(4, 1), 0, 0).status ==
        BankUploadStatus::kBankUnavailable);
  host.loadable = true;
  host.instruments = {Instrument(0, 0), Instrument(0, 0)};
  CHECK(Samples::UploadBank(host, AkaoHeader(4, 3), 0, 0).status ==
        BankUploadStatus::kCountMismatch);
  CHECK(host.loads == std::vector<std::uint32_t>{4, 4});
  CHECK(host.bound.empty());
}

TEST_CASE("Live bank uploads mark every refused upload as failed",
          "[audio][samples]") {
  const int saved = Audio_Cutscene_legacySpuTransferBusy;
  Audio_Cutscene_legacySpuTransferBusy = 1;
  auto bytes = AkaoHeader(1, 1);
  bytes[0] = std::byte{0};
  CHECK(Samples::UploadLiveBank(bytes.data(), 0, 0, 0) == -1);
  CHECK(Audio_Cutscene_legacySpuTransferBusy == -1);
  Audio_Cutscene_legacySpuTransferBusy = 1;
  const auto valid = AkaoHeader(1, 1);
  CHECK(Samples::UploadLiveBank(valid.data(), 0, 256, 0) == -1);
  CHECK(Audio_Cutscene_legacySpuTransferBusy == -1);
  Audio_Cutscene_legacySpuTransferBusy = 1;
  CHECK(Samples::UploadLiveBank(nullptr, 0, 0, 0) == -1);
  CHECK(Audio_Cutscene_legacySpuTransferBusy == -1);
  Audio_Cutscene_legacySpuTransferBusy = saved;
}
