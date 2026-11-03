#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Audio/MixControl.hpp"
#include "Audio/MixControlLive.hpp"
#include "Audio/SeqRecordsLive.hpp"
#include "Audio/SoundDriver.hpp"
#include "Audio/SoundDriverLive.hpp"
#include "Audio/VoiceMixer.hpp"
#include "Audio/VoiceMixerLive.hpp"
#include "Content/ModuleData.h"

extern "C" {
#include "Audio/Cutscene/Stream.h"
#include "Audio/Mixer/MixControl.h"
#include "Audio/Mixer/VoiceMixer.h"
#include "Audio/Seq/Channel.h"
#include "Legacy/Psx.h"
}

namespace {
namespace Driver = VS::Audio::Driver;
namespace Mixer = VS::Audio::Mixer;
namespace Seq = VS::Audio::Seq;
using Driver::DriverRefusal;
using Driver::VoiceRegister;
using Log = std::vector<std::string_view>;

constexpr std::uint32_t kNever = std::numeric_limits<std::uint32_t>::max();
constexpr std::int32_t kMinimum = std::numeric_limits<std::int32_t>::min();

// The engaged value of an optional the test expects to be present.
template <typename T>
T& Present(std::optional<T>& value) {
  if (!value.has_value()) {
    throw std::logic_error("expected an engaged optional");
  }
  return *value;
}

struct RegisterWrite {
  std::uint32_t voice = 0;
  VoiceRegister reg = VoiceRegister::kVolumeLeft;
  std::uint16_t value = 0;

  friend bool operator==(const RegisterWrite&, const RegisterWrite&) = default;
};

// A sound driver host over a register array, a transfer state and SDK calls
// that fail a set number of times before they succeed.
class FakeDriver final : public Driver::SoundDriverHost {
 public:
  std::array<std::uint16_t, std::size_t{Driver::kVoiceCount} * 8> registers{};
  std::vector<RegisterWrite> writes;
  Log calls;
  std::map<std::string_view, std::uint32_t> failures;
  std::map<std::string_view, std::uint32_t> attempts;
  std::int32_t transfer_state = Driver::kTransferIdle;
  // The poll on which a busy transfer ends; empty when it never does.
  std::optional<std::uint32_t> idle_after;
  std::uint32_t polls = 0;
  Driver::TransferCallback callback = Driver::TransferCallback::kNone;
  std::size_t written = 0;
  std::size_t read = 0;
  std::uint16_t timer_target = 0;
  std::int32_t tick_event = Driver::kNoTickEvent;
  std::int32_t opened_event = 7;
  std::vector<std::int32_t> event_arguments;
  std::vector<std::pair<std::uint32_t, std::int32_t>> rates;
  std::vector<std::array<std::uint32_t, 4>> volumes;
  std::vector<DriverRefusal> refusals;

  std::uint16_t& Register(std::uint32_t voice, VoiceRegister reg) {
    return registers.at((std::size_t{voice} * 8) +
                        static_cast<std::size_t>(reg));
  }

  std::uint16_t ReadVoiceRegister(std::uint32_t voice,
                                  VoiceRegister reg) override {
    return Register(voice, reg);
  }

  void WriteVoiceRegister(std::uint32_t voice, VoiceRegister reg,
                          std::uint16_t value) override {
    writes.push_back({voice, reg, value});
    Register(voice, reg) = value;
  }

  void SetVoiceSampleRate(std::uint32_t voice,
                          std::int32_t sample_rate) override {
    calls.emplace_back("SetVoiceSampleRate");
    rates.emplace_back(voice, sample_rate);
  }

  void SetVoiceVolume(std::uint32_t voice, std::uint32_t left,
                      std::uint32_t right, std::uint32_t scale) override {
    calls.emplace_back("SetVoiceVolume");
    volumes.push_back({voice, left, right, scale});
  }

  std::int32_t TransferState() override {
    ++polls;
    if (idle_after && polls >= *idle_after) {
      transfer_state = Driver::kTransferIdle;
    }
    return transfer_state;
  }

  void SetTransferState(std::int32_t state) override {
    calls.emplace_back("SetTransferState");
    transfer_state = state;
  }

  void SetTransferCallback(Driver::TransferCallback next) override {
    calls.emplace_back("SetTransferCallback");
    callback = next;
  }

  void WriteSampleMemory(std::span<const std::byte> data) override {
    calls.emplace_back("WriteSampleMemory");
    written = data.size();
  }

  void ReadSampleMemory(std::span<std::byte> data) override {
    calls.emplace_back("ReadSampleMemory");
    read = data.size();
  }

  void ResetPlayback() override { calls.emplace_back("ResetPlayback"); }

  bool SetTickTimer(std::uint16_t target) override {
    timer_target = target;
    return Attempt("SetTickTimer");
  }

  bool StartTickTimer() override { return Attempt("StartTickTimer"); }
  bool StopTickTimer() override { return Attempt("StopTickTimer"); }

  std::int32_t OpenTickEvent() override {
    return Attempt("OpenTickEvent") ? opened_event : Driver::kNoTickEvent;
  }

  std::int32_t TickEvent() override { return tick_event; }

  void SetTickEvent(std::int32_t event) override {
    calls.emplace_back("SetTickEvent");
    tick_event = event;
  }

  bool EnableTickEvent(std::int32_t event) override {
    event_arguments.push_back(event);
    return Attempt("EnableTickEvent");
  }

  bool DisableTickEvent(std::int32_t event) override {
    event_arguments.push_back(event);
    return Attempt("DisableTickEvent");
  }

  bool CloseTickEvent(std::int32_t event) override {
    event_arguments.push_back(event);
    return Attempt("CloseTickEvent");
  }

  void ResetOutput() override { calls.emplace_back("ResetOutput"); }

  void Refuse(DriverRefusal refusal) override { refusals.push_back(refusal); }

 private:
  bool Attempt(std::string_view name) {
    calls.push_back(name);
    ++attempts[name];
    const auto failing = failures.find(name);
    if (failing == failures.end() || failing->second == 0) return true;
    if (failing->second != kNever) --failing->second;
    return false;
  }
};
}  // namespace

TEST_CASE("Sample transfers arm the completion callback before the SPU copy",
          "[audio-mixer-driver][audio]") {
  FakeDriver host;
  Driver::ArmSampleTransferCallback(host);
  CHECK(host.transfer_state == Driver::kTransferBusy);
  CHECK(host.callback == Driver::TransferCallback::kCompleteTransfer);
  Driver::CompleteSampleTransfer(host);
  CHECK(host.transfer_state == Driver::kTransferIdle);
  CHECK(host.callback == Driver::TransferCallback::kNone);
  CHECK(host.calls == Log{"SetTransferState", "SetTransferCallback",
                          "SetTransferCallback", "SetTransferState"});
}

TEST_CASE("Sample memory copies arm the transfer before reaching the SPU",
          "[audio-mixer-driver][audio]") {
  FakeDriver host;
  std::array<std::byte, 16> buffer{};
  Driver::WriteSampleMemory(host, buffer);
  CHECK(host.calls ==
        Log{"SetTransferState", "SetTransferCallback", "WriteSampleMemory"});
  CHECK(host.written == 16);
  CHECK(host.transfer_state == Driver::kTransferBusy);
  CHECK(host.callback == Driver::TransferCallback::kCompleteTransfer);

  host.calls.clear();
  Driver::ReadSampleMemory(host, std::span(buffer).first(8));
  CHECK(host.calls ==
        Log{"SetTransferState", "SetTransferCallback", "ReadSampleMemory"});
  CHECK(host.read == 8);
  CHECK(host.refusals.empty());
}

TEST_CASE("Sample transfer waits give up after the poll limit",
          "[audio-mixer-driver][audio]") {
  FakeDriver ending;
  ending.transfer_state = Driver::kTransferBusy;
  ending.idle_after = 5;
  CHECK(Driver::WaitForSampleTransfer(ending));
  CHECK(ending.polls == 5);
  CHECK(ending.refusals.empty());

  // Only the busy state keeps the original spinning.
  FakeDriver other;
  other.transfer_state = 2;
  CHECK(Driver::WaitForSampleTransfer(other));
  CHECK(other.polls == 1);

  FakeDriver stuck;
  stuck.transfer_state = Driver::kTransferBusy;
  CHECK_FALSE(Driver::WaitForSampleTransfer(stuck));
  CHECK(stuck.polls == Driver::kTransferPollLimit);
  CHECK(stuck.refusals ==
        std::vector<DriverRefusal>{DriverRefusal::kTransferNeverEnded});
}

TEST_CASE("Driver start retries the tick timer and its event in order",
          "[audio-mixer-driver][audio]") {
  FakeDriver host;
  host.failures["SetTickTimer"] = 2;
  host.failures["OpenTickEvent"] = 1;
  host.failures["EnableTickEvent"] = 3;
  CHECK(Driver::Start(host));
  CHECK(host.timer_target == 0x44E8);
  CHECK(host.tick_event == 7);
  CHECK(host.event_arguments == std::vector<std::int32_t>(4, 7));
  CHECK(host.calls == Log{"ResetPlayback", "SetTickTimer", "SetTickTimer",
                          "SetTickTimer", "StartTickTimer", "OpenTickEvent",
                          "SetTickEvent", "OpenTickEvent", "SetTickEvent",
                          "EnableTickEvent", "EnableTickEvent",
                          "EnableTickEvent", "EnableTickEvent"});
  CHECK(host.refusals.empty());
}

TEST_CASE("Driver start refuses once an SDK call fails every attempt",
          "[audio-mixer-driver][audio]") {
  FakeDriver timer;
  timer.failures["StartTickTimer"] = kNever;
  CHECK_FALSE(Driver::Start(timer));
  CHECK(timer.attempts["StartTickTimer"] == Driver::kSdkRetryLimit);
  CHECK_FALSE(timer.attempts.contains("OpenTickEvent"));
  CHECK(timer.refusals ==
        std::vector<DriverRefusal>{DriverRefusal::kTickTimerUnavailable});

  FakeDriver event;
  event.failures["OpenTickEvent"] = kNever;
  CHECK_FALSE(Driver::Start(event));
  CHECK(event.attempts["OpenTickEvent"] == Driver::kSdkRetryLimit);
  CHECK(event.tick_event == Driver::kNoTickEvent);
  CHECK_FALSE(event.attempts.contains("EnableTickEvent"));
  CHECK(event.refusals ==
        std::vector<DriverRefusal>{DriverRefusal::kTickEventUnavailable});
}

TEST_CASE("Driver stop resets the output only after every SDK call succeeds",
          "[audio-mixer-driver][audio]") {
  FakeDriver host;
  host.tick_event = 5;
  host.failures["StopTickTimer"] = 1;
  CHECK(Driver::Stop(host));
  CHECK(host.calls == Log{"StopTickTimer", "StopTickTimer", "DisableTickEvent",
                          "CloseTickEvent", "ResetOutput"});
  CHECK(host.event_arguments == std::vector<std::int32_t>{5, 5});
  CHECK(host.refusals.empty());

  FakeDriver failing;
  failing.tick_event = 5;
  failing.failures["DisableTickEvent"] = kNever;
  CHECK_FALSE(Driver::Stop(failing));
  CHECK(failing.attempts["DisableTickEvent"] == Driver::kSdkRetryLimit);
  CHECK_FALSE(failing.attempts.contains("CloseTickEvent"));
  CHECK(std::ranges::count(failing.calls, std::string_view{"ResetOutput"}) ==
        0);
  CHECK(failing.refusals ==
        std::vector<DriverRefusal>{DriverRefusal::kTickEventUnavailable});
}

TEST_CASE("Voice register writes pack addresses and envelope fields",
          "[audio-mixer-driver][audio]") {
  FakeDriver host;
  // Addresses are written in 8-byte units, truncated to the halfword.
  Driver::SetVoiceStartAddress(host, 3, 0x12345);
  Driver::SetVoiceLoopAddress(host, 3, 0x80008);
  CHECK(host.Register(3, VoiceRegister::kStartAddress) == 0x2468);
  CHECK(host.Register(3, VoiceRegister::kLoopAddress) == 0x0001);

  host.Register(5, VoiceRegister::kEnvelopeLower) = 0xABEF;
  Driver::SetVoiceAttackRateAndMode(host, 5, 0x12, 4);
  CHECK(host.Register(5, VoiceRegister::kEnvelopeLower) == 0x92EF);
  // Modes below 4 select the linear attack.
  Driver::SetVoiceAttackRateAndMode(host, 5, 0x12, 1);
  CHECK(host.Register(5, VoiceRegister::kEnvelopeLower) == 0x12EF);
  Driver::SetVoiceDecayRate(host, 5, 0xA);
  CHECK(host.Register(5, VoiceRegister::kEnvelopeLower) == 0x12AF);
  Driver::SetVoiceSustainLevel(host, 5, 3);
  CHECK(host.Register(5, VoiceRegister::kEnvelopeLower) == 0x12A3);
  CHECK(host.refusals.empty());
}

TEST_CASE("Voice envelope writes pack the sustain fields into halfwords",
          "[audio-mixer-driver][audio]") {
  FakeDriver host;
  host.Register(5, VoiceRegister::kEnvelopeUpper) = 0x7FFF;
  Driver::SetVoiceSustainRateAndMode(host, 5, 0x15, 2);
  CHECK(host.Register(5, VoiceRegister::kEnvelopeUpper) == 0x457F);
  Driver::SetVoiceSustainRateAndMode(host, 5, 0x15, 1);
  CHECK(host.Register(5, VoiceRegister::kEnvelopeUpper) == 0x057F);

  // The packed word is stored as a halfword (sh).
  Driver::SetVoiceAttackRateAndMode(host, 6, 0x1FF, 0);
  CHECK(host.Register(6, VoiceRegister::kEnvelopeLower) == 0xFF00);

  Driver::SetVoiceEnvelopeLower(host, 7, 0xBEEF);
  Driver::SetVoiceEnvelopeUpper(host, 7, 0xCAFE);
  CHECK(host.Register(7, VoiceRegister::kEnvelopeLower) == 0xBEEF);
  CHECK(host.Register(7, VoiceRegister::kEnvelopeUpper) == 0xCAFE);
  CHECK(host.refusals.empty());
}

TEST_CASE("Voice register writes refuse voices outside the 24 SPU voices",
          "[audio-mixer-driver][audio]") {
  FakeDriver host;
  Driver::SetVoiceStartAddress(host, 24, 0x1000);
  Driver::SetVoiceLoopAddress(host, -1, 0x1000);
  Driver::SetVoiceEnvelopeLower(host, 24, 1);
  Driver::SetVoiceEnvelopeUpper(host, 99, 1);
  Driver::SetVoiceAttackRateAndMode(host, 24, 1, 4);
  Driver::SetVoiceDecayRate(host, -5, 1);
  Driver::SetVoiceSustainLevel(host, 24, 1);
  Driver::SetVoiceSustainRateAndMode(host, 24, 1, 2);
  CHECK(host.writes.empty());
  CHECK(host.refusals ==
        std::vector<DriverRefusal>(8, DriverRefusal::kInvalidVoice));
}

TEST_CASE("Full voice parameter writes scale the volumes at 32 bits",
          "[audio-mixer-driver][audio]") {
  FakeDriver host;
  Driver::VoiceParameters parameters;
  parameters.dirty_flags = 0x1FFFF;
  parameters.sample_start = 0x2000;
  parameters.loop_start = 0x2FF8;
  parameters.sample_rate = 0x1000;
  parameters.adsr_lower = 0x80FF;
  parameters.adsr_upper = 0x1FC0;
  parameters.volume_left = 0x3FFF;
  parameters.volume_right = -0x100;
  Driver::WriteVoiceParameters(host, 2, parameters, 0x40);
  CHECK(parameters.dirty_flags == 0);
  CHECK(host.writes == std::vector<RegisterWrite>{
                           {2, VoiceRegister::kVolumeLeft, 0x1FFF},
                           {2, VoiceRegister::kVolumeRight, 0x7F80},
                           {2, VoiceRegister::kSampleRate, 0x1000},
                           {2, VoiceRegister::kStartAddress, 0x0400},
                           {2, VoiceRegister::kEnvelopeLower, 0x80FF},
                           {2, VoiceRegister::kEnvelopeUpper, 0x1FC0},
                           {2, VoiceRegister::kLoopAddress, 0x05FF},
                       });

  // Without a scale the volumes are masked to 15 bits as they are.
  Driver::WriteVoiceParameters(host, 2, parameters, 0);
  CHECK(host.Register(2, VoiceRegister::kVolumeLeft) == 0x3FFF);
  CHECK(host.Register(2, VoiceRegister::kVolumeRight) == 0x7F00);

  // The product wraps at 32 bits before the arithmetic shift (mult, sra 7).
  parameters.volume_left = 0x7FFF;
  Driver::WriteVoiceParameters(host, 2, parameters, 0x20000);
  CHECK(host.Register(2, VoiceRegister::kVolumeLeft) == 0x7C00);
}

TEST_CASE("Full voice parameter writes refuse voices outside the SPU",
          "[audio-mixer-driver][audio]") {
  FakeDriver host;
  Driver::VoiceParameters parameters;
  parameters.dirty_flags = 0x13;
  Driver::WriteVoiceParameters(host, 24, parameters, 0);
  CHECK(parameters.dirty_flags == 0x13);
  CHECK(host.writes.empty());
  CHECK(host.refusals ==
        std::vector<DriverRefusal>{DriverRefusal::kInvalidVoice});
}

TEST_CASE("Dirty voice parameter writes follow the original flag order",
          "[audio-mixer-driver][audio]") {
  FakeDriver host;
  Driver::VoiceParameters parameters;
  parameters.dirty_flags = 0x10 | 0x3 | 0x80 | 0x10000 | 0x6600 | 0x9900;
  parameters.sample_rate = 0x1234;
  parameters.volume_left = -2;
  parameters.volume_right = 0x100;
  parameters.volume_scale = 0x7F;
  parameters.sample_start = 0x4000;
  parameters.loop_start = 0x4010;
  parameters.adsr_upper = 0x5555;
  parameters.adsr_lower = 0xAAAA;
  Driver::WriteDirtyVoiceParameters(host, 4, parameters);
  CHECK(parameters.dirty_flags == 0);
  CHECK(host.calls == Log{"SetVoiceSampleRate", "SetVoiceVolume"});
  CHECK(host.rates ==
        std::vector<std::pair<std::uint32_t, std::int32_t>>{{4U, 0x1234}});
  const std::array<std::uint32_t, 4> volume{4U, 0xFFFFFFFEU, 0x100U, 0x7FU};
  REQUIRE(host.volumes.size() == 1);
  CHECK(host.volumes[0] == volume);
  CHECK(host.writes == std::vector<RegisterWrite>{
                           {4, VoiceRegister::kStartAddress, 0x0800},
                           {4, VoiceRegister::kLoopAddress, 0x0802},
                           {4, VoiceRegister::kEnvelopeUpper, 0x5555},
                           {4, VoiceRegister::kEnvelopeLower, 0xAAAA},
                       });
}

TEST_CASE("Dirty voice parameter writes stop or refuse without work to do",
          "[audio-mixer-driver][audio]") {
  Driver::VoiceParameters parameters;
  // The writes stop once no flags remain.
  FakeDriver rate_only;
  parameters.dirty_flags = 0x10;
  Driver::WriteDirtyVoiceParameters(rate_only, 4, parameters);
  CHECK(rate_only.calls == Log{"SetVoiceSampleRate"});
  CHECK(rate_only.writes.empty());

  FakeDriver refused;
  parameters.dirty_flags = 0;
  Driver::WriteDirtyVoiceParameters(refused, 30, parameters);
  CHECK(refused.refusals.empty());
  parameters.dirty_flags = 0x80;
  Driver::WriteDirtyVoiceParameters(refused, 24, parameters);
  CHECK(parameters.dirty_flags == 0x80);
  CHECK(refused.writes.empty());
  CHECK(refused.refusals ==
        std::vector<DriverRefusal>{DriverRefusal::kInvalidVoice});
}

namespace {
using Driver::ChannelKind;
using Mixer::LayerId;
using Mixer::MixRefusal;

class Refusals final : public Mixer::RefusalSink {
 public:
  std::vector<MixRefusal> list;
  void Refuse(MixRefusal refusal) override { list.push_back(refusal); }
};

struct Configured {
  Mixer::ChannelRef ref;
  std::uint32_t voice = 0;
  std::int16_t left = 0;
  std::int16_t right = 0;

  friend bool operator==(const Configured&, const Configured&) = default;
};

// A mixer host that records the voice output and clears a configured
// channel's dirty flags as the voice output does.
class FakeMixer final : public Mixer::MixerHost {
 public:
  std::array<std::int16_t, Mixer::kVoiceCount> envelope_levels{};
  std::vector<std::uint32_t> envelope_reads;
  std::vector<Configured> configured;
  std::vector<std::pair<std::uint32_t, LayerId>> owners;
  std::vector<std::pair<std::int16_t, std::int16_t>> depths;
  std::vector<std::int32_t> clocks;
  std::vector<std::uint32_t> reverb_voices;
  std::vector<std::uint32_t> noise_voices;
  std::vector<std::uint32_t> fm_voices;
  std::vector<std::uint32_t> key_on;
  std::vector<std::uint32_t> key_off;
  std::vector<MixRefusal> refusals;

  void Refuse(MixRefusal refusal) override { refusals.push_back(refusal); }

  std::int16_t Envelope(std::uint32_t voice) override {
    envelope_reads.push_back(voice);
    return envelope_levels.at(voice);
  }

  void ConfigureChannel(Mixer::ChannelRef ref,
                        Mixer::MixChannel& channel) override {
    configured.push_back({ref, channel.assigned_voice, channel.volume_left,
                          channel.volume_right});
    channel.state.voice.dirty_flags = 0;
  }

  void SetVoiceOwner(std::uint32_t voice, LayerId layer) override {
    owners.emplace_back(voice, layer);
  }

  void SetReverbDepth(std::int16_t left, std::int16_t right) override {
    depths.emplace_back(left, right);
  }

  void SetNoiseClock(std::int32_t clock) override { clocks.push_back(clock); }

  void SetReverbVoices(std::uint32_t voices) override {
    reverb_voices.push_back(voices);
  }

  void SetNoiseVoices(std::uint32_t voices) override {
    noise_voices.push_back(voices);
  }

  void SetFrequencyModulationVoices(std::uint32_t voices) override {
    fm_voices.push_back(voices);
  }

  void KeyOn(std::uint32_t voices) override { key_on.push_back(voices); }
  void KeyOff(std::uint32_t voices) override { key_off.push_back(voices); }
};

// A pan table whose gain grows linearly from the left.
constexpr std::array<std::int16_t, Mixer::kPanGainCount> kLinearPan = [] {
  std::array<std::int16_t, Mixer::kPanGainCount> gains{};
  for (std::size_t i = 0; i < gains.size(); ++i) {
    gains[i] = static_cast<std::int16_t>(i * 0x80);
  }
  return gains;
}();

Mixer::PanGains Gains() { return Mixer::PanGains(kLinearPan); }

Mixer::MixLevels Levels(std::int32_t master, std::int32_t pitch_scale,
                        std::uint32_t behavior) {
  return {master, pitch_scale, behavior, Gains()};
}

// Two samples and a loop marker back to the first.
constexpr std::array<std::int16_t, 5> kWave{0x4000, -0x4000, 0, 0, -2};

Mixer::LfoCursor WaveAt(std::size_t position) { return {kWave, position}; }

void Slide(Mixer::MixChannel& channel, Mixer::SlideState& state, Refusals& sink,
           ChannelKind kind = ChannelKind::Music,
           std::uint32_t channel_bit = 1) {
  Mixer::UpdateChannelSlidesAndDelays(channel, state, channel_bit, kind, sink);
}

// Noise ends on the next tick, FM on the one after.
Mixer::MixChannel TimedChannel() {
  Mixer::MixChannel music;
  music.state.noise_timer = 1;
  music.state.fm_timer = 2;
  music.state.vibrato_delay_left = 1;
  music.state.tremolo_delay_left = 3;
  return music;
}

// Vibrato, tremolo and auto-pan depths one step from their targets.
Mixer::MixChannel DepthSliding() {
  Mixer::MixChannel channel;
  Seq::ChannelState& fields = channel.state;
  fields.pitch_base = 0x1000;
  fields.volume = 0x7F0000;
  fields.volume_balance = 0x7F00;
  fields.vibrato_depth = 0x8100;
  fields.vibrato_depth_slide_step = 0x100;
  fields.vibrato_depth_slide_length = 1;
  fields.vibrato_rate_counter = 3;
  // At the loop marker: the read lands on the first sample.
  channel.vibrato = WaveAt(2);
  fields.tremolo_depth = 0x7E00;
  fields.tremolo_depth_slide_step = 0x100;
  fields.tremolo_depth_slide_length = 1;
  fields.tremolo_rate_counter = 2;
  channel.tremolo = WaveAt(0);
  fields.auto_pan_depth = 0x3F00;
  fields.auto_pan_depth_slide_step = 0x100;
  fields.auto_pan_depth_slide_length = 1;
  channel.auto_pan = WaveAt(1);
  return channel;
}
}  // namespace

TEST_CASE("Channel slides dirty the volume only when audible bits change",
          "[audio-mixer-driver][audio]") {
  Refusals sink;
  Mixer::SlideState state;
  Mixer::MixChannel channel;
  Seq::ChannelState& fields = channel.state;
  fields.volume_balance = 0x1000;
  fields.volume_balance_slide_step = 0x80;
  fields.volume_balance_slide_length = 2;
  Slide(channel, state, sink);
  CHECK(fields.volume_balance == 0x1080);
  CHECK(fields.volume_balance_slide_length == 1);
  CHECK(fields.voice.dirty_flags == 0);
  Slide(channel, state, sink);
  CHECK(fields.volume_balance == 0x1100);
  CHECK(fields.voice.dirty_flags == Seq::kVolumeDirty);
  // Sound effects keep their balance.
  fields.voice.dirty_flags = 0;
  fields.volume_balance_slide_length = 1;
  Slide(channel, state, sink, ChannelKind::SoundEffect);
  CHECK(fields.volume_balance == 0x1100);
  CHECK(fields.volume_balance_slide_length == 1);
  CHECK(fields.voice.dirty_flags == 0);
}

TEST_CASE("Channel volume slides wrap at 32 bits and dirty on bits 21-31",
          "[audio-mixer-driver][audio]") {
  Refusals sink;
  Mixer::SlideState state;
  Mixer::MixChannel volume;
  volume.state.volume = 0x7FFFFFFF;
  volume.state.volume_slide_step = 1;
  volume.state.volume_slide_length = 1;
  Slide(volume, state, sink);
  CHECK(volume.state.volume == kMinimum);
  CHECK(volume.state.voice.dirty_flags == Seq::kVolumeDirty);
  volume.state.voice.dirty_flags = 0;
  volume.state.volume = 0;
  volume.state.volume_slide_step = 0x1000;
  volume.state.volume_slide_length = 1;
  Slide(volume, state, sink);
  CHECK(volume.state.volume == 0x1000);
  CHECK(volume.state.voice.dirty_flags == 0);
  CHECK(sink.list.empty());
}

TEST_CASE("Channel pan slides dirty the volume when the high byte changes",
          "[audio-mixer-driver][audio]") {
  Refusals sink;
  Mixer::SlideState state;
  Mixer::MixChannel pan;
  pan.state.pan = 0x40F0;
  pan.state.pan_slide_step = 0x10;
  pan.state.pan_slide_length = 2;
  Slide(pan, state, sink);
  CHECK(pan.state.pan == 0x4100);
  CHECK(pan.state.voice.dirty_flags == Seq::kVolumeDirty);
  pan.state.voice.dirty_flags = 0;
  pan.state.pan_slide_step = 1;
  Slide(pan, state, sink);
  CHECK(pan.state.pan == 0x4101);
  CHECK(pan.state.pan_slide_length == 0);
  CHECK(pan.state.voice.dirty_flags == 0);
  CHECK(sink.list.empty());
}

TEST_CASE("Channel timers toggle the noise and modulation voices they own",
          "[audio-mixer-driver][audio]") {
  Refusals sink;
  Mixer::SlideState state;
  state.active.noise_channels = 0x4;
  Mixer::MixChannel music = TimedChannel();
  Slide(music, state, sink, ChannelKind::Music, 0x4);
  CHECK(state.active.noise_channels == 0);
  CHECK(state.active.fm_channels == 0);
  CHECK(state.driver_update_flags == 0x110);
  CHECK(music.state.noise_timer == 0);
  CHECK(music.state.fm_timer == 1);
  CHECK(music.state.vibrato_delay_left == 0);
  CHECK(music.state.tremolo_delay_left == 2);
  CHECK(sink.list.empty());
}

TEST_CASE("Channel modulation timers toggle their voice on the expiring tick",
          "[audio-mixer-driver][audio]") {
  Refusals sink;
  Mixer::SlideState state;
  state.active.noise_channels = 0x4;
  Mixer::MixChannel music = TimedChannel();
  Slide(music, state, sink, ChannelKind::Music, 0x4);
  state.driver_update_flags = 0;
  Slide(music, state, sink, ChannelKind::Music, 0x4);
  CHECK(state.active.fm_channels == 0x4);
  CHECK(state.active.noise_channels == 0);
  CHECK(state.driver_update_flags == 0x100);
  CHECK(sink.list.empty());
}

TEST_CASE("Sound-effect timers toggle the voice scheduler's masks",
          "[audio-mixer-driver][audio]") {
  Refusals sink;
  Mixer::SlideState effects;
  Mixer::MixChannel effect;
  effect.state.noise_timer = 1;
  effect.state.fm_timer = 1;
  Slide(effect, effects, sink, ChannelKind::SoundEffect, 1U << 13);
  CHECK(effects.scheduler.noise_voices == 1U << 13);
  CHECK(effects.scheduler.fm_voices == 1U << 13);
  CHECK(effects.active.noise_channels == 0);
  CHECK(effects.active.fm_channels == 0);
  CHECK(effects.driver_update_flags == 0x110);
  CHECK(sink.list.empty());
}

TEST_CASE("Pitch slides dirty the sample rate when the whole pitch changes",
          "[audio-mixer-driver][audio]") {
  Refusals sink;
  Mixer::SlideState state;
  Mixer::MixChannel channel;
  channel.pitch_slide = {0xF000, 0x2000, 2};
  Slide(channel, state, sink);
  CHECK(channel.pitch_slide.slide == 0x11000);
  CHECK(channel.pitch_slide.ticks_left == 1);
  CHECK(channel.state.voice.dirty_flags == Seq::kSampleRateDirty);
  channel.state.voice.dirty_flags = 0;
  Slide(channel, state, sink);
  CHECK(channel.pitch_slide.slide == 0x13000);
  CHECK(channel.pitch_slide.ticks_left == 0);
  CHECK(channel.state.voice.dirty_flags == 0);
  Slide(channel, state, sink);
  CHECK(channel.pitch_slide.slide == 0x13000);
}

TEST_CASE("Depth slides rescale the LFOs and peek their waveforms",
          "[audio-mixer-driver][audio]") {
  Refusals sink;
  Mixer::SlideState state;
  Mixer::MixChannel channel = DepthSliding();
  Seq::ChannelState& fields = channel.state;
  Slide(channel, state, sink);
  // (2 * 0x1000) >> 7 with the full-pitch bit; (0x40 * 0x4000) >> 16 doubled.
  CHECK(fields.vibrato_depth == 0x8200);
  CHECK(fields.vibrato_base == 0x40);
  CHECK(fields.vibrato_pitch == 0x20);
  CHECK(channel.vibrato.position == 2);
  CHECK(fields.voice.dirty_flags ==
        (Seq::kVolumeDirty | Seq::kSampleRateDirty));
  CHECK(sink.list.empty());
}

TEST_CASE("Tremolo and auto-pan depth slides rescale from the peeked sample",
          "[audio-mixer-driver][audio]") {
  Refusals sink;
  Mixer::SlideState state;
  Mixer::MixChannel channel = DepthSliding();
  Seq::ChannelState& fields = channel.state;
  Slide(channel, state, sink);
  // (((0x7E * 0x7F) << 9) >> 16) * 0x4000 >> 15.
  CHECK(fields.tremolo_depth == 0x7F00);
  CHECK(fields.tremolo_volume == 0x3E);
  CHECK(channel.tremolo.position == 0);
  // (0x40 * -0x4000) >> 15.
  CHECK(fields.auto_pan_depth == 0x4000);
  CHECK(fields.auto_pan_offset == -32);
  CHECK(sink.list.empty());
}

TEST_CASE("Vibrato depth slides without the full-pitch bit scale 15/256",
          "[audio-mixer-driver][audio]") {
  Refusals sink;
  Mixer::SlideState state;
  // Without the full-pitch bit the depth scales 15/256 of the pitch; a rate
  // counter about to step peeks nothing.
  Mixer::MixChannel quiet;
  quiet.state.pitch_base = 0x1000;
  quiet.state.vibrato_depth = 0x0100;
  quiet.state.vibrato_depth_slide_step = 0x100;
  quiet.state.vibrato_depth_slide_length = 1;
  quiet.state.vibrato_rate_counter = 1;
  quiet.vibrato = WaveAt(0);
  Slide(quiet, state, sink);
  CHECK(quiet.state.vibrato_base == 3);
  CHECK(quiet.state.vibrato_pitch == 0);
  CHECK(quiet.state.voice.dirty_flags == 0);

  Mixer::MixChannel empty;
  empty.state.tremolo_depth_slide_length = 1;
  Slide(empty, state, sink);
  CHECK(empty.state.tremolo_volume == 0);
  CHECK(sink.list == std::vector<MixRefusal>{MixRefusal::kWaveformOutside});
}

namespace {
// Volume 0x7F, balance 0x7F and a pan of 0x40 give a level of 0x7E.
Mixer::MixChannel Audible() {
  Mixer::MixChannel channel;
  channel.state.volume = 0x7F0000;
  channel.state.volume_balance = 0x7F00;
  channel.state.pan = 0x4000;
  channel.state.voice.dirty_flags = Seq::kVolumeDirty;
  return channel;
}

Mixer::MixChannel Pitched() {
  Mixer::MixChannel channel;
  channel.state.pitch_base = 0x1000;
  channel.state.vibrato_pitch = 0x10;
  channel.state.voice.dirty_flags = Seq::kSampleRateDirty;
  channel.pitch_slide.slide = 0x50000;
  return channel;
}

std::uint16_t MusicPitch(Mixer::MixChannel channel, std::int32_t scale) {
  Refusals sink;
  Mixer::UpdateMusicChannelModulation(channel, std::nullopt,
                                      Levels(0, scale, 0), sink);
  return channel.sample_rate;
}

std::unique_ptr<Mixer::MixerState> NewMixer() {
  return std::make_unique<Mixer::MixerState>();
}
}  // namespace

TEST_CASE("Music modulation mixes the master volume through the pan gains",
          "[audio-mixer-driver][audio]") {
  Refusals sink;
  Mixer::MixChannel channel = Audible();
  Mixer::UpdateMusicChannelModulation(channel, std::nullopt,
                                      Levels(0x7F0000, 0, 0), sink);
  // ((0x7E * 0x7F) >> 7) times the gains at pan 0x40 and 0xBF, >> 15.
  CHECK(channel.volume_left == 0x1F);
  CHECK(channel.volume_right == 0x5D);
  CHECK(channel.state.voice.dirty_flags == Seq::kVolumeDirty);

  // Only bits 16-22 of the master volume count.
  Mixer::MixChannel high = Audible();
  Mixer::UpdateMusicChannelModulation(high, std::nullopt,
                                      Levels(0xFF0000, 0, 0), sink);
  CHECK(high.volume_left == 0x1F);
  CHECK(high.volume_right == 0x5D);
  CHECK(sink.list.empty());
}

TEST_CASE("Music modulation mixes mono and keeps a clean volume's mix",
          "[audio-mixer-driver][audio]") {
  Refusals sink;
  Mixer::MixChannel mono = Audible();
  Mixer::UpdateMusicChannelModulation(
      mono, std::nullopt, Levels(0x7F0000, 0, Mixer::kMonoMix), sink);
  CHECK(mono.volume_left == 0x3E);
  CHECK(mono.volume_right == 0x3E);

  // A clean volume keeps the previous mix.
  Mixer::MixChannel clean = Audible();
  clean.state.voice.dirty_flags = 0;
  clean.volume_left = 5;
  Mixer::UpdateMusicChannelModulation(clean, std::nullopt,
                                      Levels(0x7F0000, 0, 0), sink);
  CHECK(clean.volume_left == 5);
  CHECK(clean.volume_right == 0);
  CHECK(sink.list.empty());
}

TEST_CASE("Music pitch adds the vibrato and slide before the pitch scale",
          "[audio-mixer-driver][audio]") {
  CHECK(MusicPitch(Pitched(), 0) == 0x1015);
  // Scales below 0x80 add pitch * scale / 128; the rest give
  // pitch * scale / 256 (byte 2 of the Q16.16 scale).
  CHECK(MusicPitch(Pitched(), 0x400000) == 0x181F);
  CHECK(MusicPitch(Pitched(), 0xC00000) == 0x0C0F);
  CHECK(MusicPitch(Pitched(), -0x10000) == 0x1004);

  // The fine pitch adds after the scale and the rate keeps 14 bits.
  Mixer::MixChannel fine = Pitched();
  fine.state.fine_pitch_delta = 0x3000;
  CHECK(MusicPitch(fine, 0) == 0x15);

  Mixer::MixChannel clean = Pitched();
  clean.state.voice.dirty_flags = 0;
  clean.sample_rate = 0x777;
  CHECK(MusicPitch(clean, 0) == 0x777);
}

TEST_CASE("Side chains read the sample rate of the channel record before",
          "[audio-mixer-driver][audio]") {
  Refusals sink;
  Mixer::MixChannel pitch;
  pitch.state.update_flags = Seq::kPitchSidechainActive;
  Mixer::UpdateMusicChannelModulation(pitch, std::uint16_t{0x800},
                                      Levels(0, 0, 0), sink);
  CHECK(pitch.sample_rate == 0x800);
  CHECK(pitch.state.voice.dirty_flags == Seq::kSampleRateDirty);

  Mixer::MixChannel volume = Audible();
  volume.state.update_flags = Seq::kVolumeSidechainActive;
  volume.state.voice.dirty_flags = 0;
  Mixer::UpdateMusicChannelModulation(volume, std::uint16_t{0x3000},
                                      Levels(0x7F0000, 0, Mixer::kMonoMix),
                                      sink);
  CHECK(volume.state.voice.dirty_flags == Seq::kVolumeDirty);
  // (((0x6000 * 0x7F) >> 7) * 0x7F >> 7) halved by the centre gain.
  CHECK(volume.volume_left == 0x2F40);
  CHECK(volume.volume_right == 0x2F40);
  CHECK(sink.list.empty());
}

TEST_CASE("Volume side chains take twice the rate as a signed halfword",
          "[audio-mixer-driver][audio]") {
  Refusals sink;
  // Twice the rate is taken as a signed halfword (sll 1, sll 16, sra 16).
  Mixer::MixChannel wrapped = Audible();
  wrapped.state.update_flags = Seq::kVolumeSidechainActive;
  Mixer::UpdateMusicChannelModulation(wrapped, std::uint16_t{0x4000},
                                      Levels(0x7F0000, 0, Mixer::kMonoMix),
                                      sink);
  CHECK(wrapped.volume_left == -16129);
  CHECK(sink.list.empty());
}

TEST_CASE("Side chains on the first channel record are refused",
          "[audio-mixer-driver][audio]") {
  Refusals sink;
  Mixer::MixChannel first;
  first.state.update_flags =
      Seq::kPitchSidechainActive | Seq::kVolumeSidechainActive;
  first.sample_rate = 0x123;
  Mixer::UpdateMusicChannelModulation(first, std::nullopt, Levels(0, 0, 0),
                                      sink);
  CHECK(first.sample_rate == 0x123);
  CHECK(first.state.voice.dirty_flags == 0);
  CHECK(sink.list ==
        std::vector<MixRefusal>(2, MixRefusal::kSidechainWithoutPrevious));
}

TEST_CASE("Sound effect modulation mixes the effect volume pan and pitch",
          "[audio-mixer-driver][audio]") {
  Refusals sink;
  Mixer::MixChannel effect = Audible();
  effect.state.pan = 0x2000;
  effect.state.effect_volume = 0x4000;
  effect.effect_pan = 0x2000;
  effect.state.pitch_base = 0x1000;
  effect.state.voice.dirty_flags |= Seq::kSampleRateDirty;
  effect.effect_pitch_scale = 0xC000;
  Mixer::MixChannel fixed = effect;
  Mixer::MixChannel unscaled = effect;
  Mixer::UpdateSoundEffectModulation(effect, std::nullopt, Levels(0, 0, 0),
                                     sink);
  // (0x7E * 0x40) >> 7 at pan index 0x40 + 0x80 and its mirror.
  CHECK(effect.volume_left == 0x2F);
  CHECK(effect.volume_right == 0xF);
  CHECK(effect.sample_rate == 0xC00);

  // Sound effects ignore the music master volume and pitch scale.
  Mixer::UpdateSoundEffectModulation(unscaled, std::nullopt,
                                     Levels(0x100000, 0x400000, 0), sink);
  CHECK(unscaled == effect);

  fixed.effect_tags = Mixer::kFixedMixEffect;
  Mixer::UpdateSoundEffectModulation(fixed, std::nullopt, Levels(0, 0, 0),
                                     sink);
  CHECK(fixed.volume_left == 0x3F);
  CHECK(fixed.volume_right == 0x3E);
  CHECK(fixed.sample_rate == 0x1000);
  CHECK(sink.list.empty());
}

TEST_CASE("LFO modulation steps through the waveform and its loop marker",
          "[audio-mixer-driver][audio]") {
  Refusals sink;
  const Mixer::MixLevels levels = Levels(0, 0, 0);
  Mixer::MixChannel channel;
  channel.state.update_flags = Seq::kVibratoActive;
  channel.state.vibrato_base = 0x40;
  channel.state.vibrato_rate = 1;
  channel.state.vibrato_rate_counter = 1;
  channel.vibrato = WaveAt(0);
  std::vector<std::int16_t> pitches;
  for (int step = 0; step < 3; ++step) {
    Mixer::UpdateMusicChannelModulation(channel, std::nullopt, levels, sink);
    pitches.push_back(channel.state.vibrato_pitch);
  }
  // Negative pitches are not doubled.
  CHECK(pitches == std::vector<std::int16_t>{0x20, -0x10, 0x20});
  CHECK(channel.vibrato.position == 1);
  CHECK(channel.state.vibrato_rate_counter == 1);
  CHECK(sink.list.empty());
}

TEST_CASE("Tremolo and auto-pan modulation step their own waveforms",
          "[audio-mixer-driver][audio]") {
  Refusals sink;
  const Mixer::MixLevels levels = Levels(0, 0, 0);
  Mixer::MixChannel other = Audible();
  other.state.voice.dirty_flags = 0;
  other.state.update_flags = Seq::kTremoloActive | Seq::kAutoPanActive;
  other.state.tremolo_depth = 0x7F00;
  other.state.tremolo_rate_counter = 1;
  other.state.auto_pan_depth = 0x4000;
  other.state.auto_pan_rate_counter = 1;
  other.tremolo = WaveAt(0);
  other.auto_pan = WaveAt(1);
  Mixer::UpdateMusicChannelModulation(other, std::nullopt, levels, sink);
  CHECK(other.state.tremolo_volume == 0x3E);
  CHECK(other.state.auto_pan_offset == -32);
  CHECK(other.tremolo.position == 1);
  CHECK(other.auto_pan.position == 2);
  CHECK(other.state.voice.dirty_flags == Seq::kVolumeDirty);
  CHECK(sink.list.empty());
}

TEST_CASE("Music waits out the vibrato delay and sound effects do not",
          "[audio-mixer-driver][audio]") {
  Refusals sink;
  const Mixer::MixLevels levels = Levels(0, 0, 0);
  // Music waits out the vibrato delay; sound effects do not.
  Mixer::MixChannel delayed;
  delayed.state.update_flags = Seq::kVibratoActive;
  delayed.state.vibrato_base = 0x40;
  delayed.state.vibrato_rate_counter = 1;
  delayed.state.vibrato_delay_left = 1;
  delayed.vibrato = WaveAt(0);
  Mixer::MixChannel effect = delayed;
  Mixer::UpdateMusicChannelModulation(delayed, std::nullopt, levels, sink);
  CHECK(delayed.state.vibrato_pitch == 0);
  CHECK(delayed.state.vibrato_rate_counter == 1);
  CHECK(delayed.vibrato.position == 0);
  Mixer::UpdateSoundEffectModulation(effect, std::nullopt, levels, sink);
  CHECK(effect.state.vibrato_pitch == 0x20);
  CHECK(effect.vibrato.position == 1);
  CHECK(sink.list.empty());
}

TEST_CASE("LFO reads outside the waveform are refused and keep the level",
          "[audio-mixer-driver][audio]") {
  Refusals sink;
  Mixer::MixChannel past;
  past.state.update_flags = Seq::kTremoloActive;
  past.state.tremolo_rate_counter = 1;
  past.state.tremolo_volume = 9;
  past.tremolo = WaveAt(kWave.size());
  Mixer::UpdateMusicChannelModulation(past, std::nullopt, Levels(0, 0, 0),
                                      sink);
  CHECK(past.state.tremolo_volume == 9);
  CHECK(past.tremolo.position == kWave.size());

  constexpr std::array<std::int16_t, 4> kJumpOut{0x100, 0, 0, 5};
  Mixer::MixChannel jump;
  jump.state.update_flags = Seq::kAutoPanActive;
  jump.state.auto_pan_depth = 0x4000;
  jump.state.auto_pan_rate_counter = 1;
  jump.auto_pan = {kJumpOut, 1};
  Mixer::UpdateMusicChannelModulation(jump, std::nullopt, Levels(0, 0, 0),
                                      sink);
  CHECK(jump.auto_pan.position == 1);
  CHECK(jump.state.auto_pan_offset == 0);
  CHECK(sink.list == std::vector<MixRefusal>(2, MixRefusal::kWaveformOutside));
}

TEST_CASE("Free voice searches begin at the active layer's search start",
          "[audio-mixer-driver][audio]") {
  Refusals sink;
  auto state = NewMixer();
  state->envelopes.fill(0x100);
  state->envelopes[5] = 0;
  state->envelopes[9] = 0;
  state->primary.config.voice_search_start = 6;
  CHECK(Mixer::FindFreeVoice(*state, false, sink) == 9);
  CHECK(Mixer::FindFreeVoice(*state, true, sink) == 5);
  state->envelopes[9] = 1;
  CHECK(Mixer::FindFreeVoice(*state, false, sink) == Mixer::kNoVoice);
  CHECK(sink.list.empty());
}

TEST_CASE("Voice searches refuse starts outside the voices or a missing layer",
          "[audio-mixer-driver][audio]") {
  Refusals sink;
  auto state = NewMixer();
  state->envelopes.fill(0x100);
  state->envelopes[5] = 0;
  state->envelopes[9] = 1;
  state->primary.config.voice_search_start = 24;
  CHECK(Mixer::FindFreeVoice(*state, false, sink) == Mixer::kNoVoice);
  state->primary.config.voice_search_start = -1;
  CHECK(Mixer::StealQuietestVoice(*state, false, sink) == Mixer::kNoVoice);
  CHECK(Mixer::FindFreeVoice(*state, true, sink) == 5);
  state->primary.config.voice_search_start = 0;
  state->active = LayerId::kSecondary;
  CHECK(Mixer::FindFreeVoice(*state, false, sink) == Mixer::kNoVoice);
  CHECK(sink.list == std::vector<MixRefusal>{
                         MixRefusal::kVoiceSearchOutside,
                         MixRefusal::kVoiceSearchOutside,
                         MixRefusal::kSecondaryLayerMissing,
                     });
}

TEST_CASE("Voice stealing takes the quietest voice and unassigns it",
          "[audio-mixer-driver][audio]") {
  Refusals sink;
  auto state = NewMixer();
  state->envelopes.fill(Mixer::kReservedEnvelope);
  state->envelopes[3] = 0x10;
  state->envelopes[7] = 0x20;
  state->envelopes[12] = 0x20;
  state->primary.config.voice_search_start = 6;
  state->secondary.emplace();
  state->primary.channels[2].assigned_voice = 7;
  Present(state->secondary).channels[4].assigned_voice = 7;
  state->primary.channels[3].assigned_voice = 12;
  // The first of two equally quiet voices wins.
  CHECK(Mixer::StealQuietestVoice(*state, false, sink) == 7);
  CHECK(state->primary.channels[2].assigned_voice == Mixer::kNoVoice);
  CHECK(Present(state->secondary).channels[4].assigned_voice ==
        Mixer::kNoVoice);
  CHECK(state->primary.channels[3].assigned_voice == 12);
  CHECK(Mixer::StealQuietestVoice(*state, true, sink) == 3);

  state->envelopes.fill(Mixer::kReservedEnvelope);
  CHECK(Mixer::StealQuietestVoice(*state, true, sink) == Mixer::kNoVoice);
  CHECK(state->primary.channels[3].assigned_voice == 12);
  CHECK(sink.list.empty());
}

TEST_CASE("Channel voice masks gather the voices of the masked channels",
          "[audio-mixer-driver][audio]") {
  Mixer::MusicChannels channels;
  channels[1].assigned_voice = 4;
  channels[3].assigned_voice = 4;
  channels[5].assigned_voice = 9;
  CHECK(Mixer::IsVoiceAssignedToChannels(channels, 0x8, 4));
  CHECK_FALSE(Mixer::IsVoiceAssignedToChannels(channels, 0x20, 4));
  CHECK_FALSE(Mixer::IsVoiceAssignedToChannels(channels, 0x5, 4));

  // Channels without a voice add nothing; the filter applies to the whole
  // accumulated mask.
  std::uint32_t voices = 0x1;
  Mixer::AccumulateFilteredVoiceMask(channels, voices, 0x2B, ~0U);
  CHECK(voices == 0x211);
  voices = 0;
  Mixer::AccumulateFilteredVoiceMask(channels, voices, 0x22, ~0x10U);
  CHECK(voices == 0x200);
  voices = 0x1;
  Mixer::AccumulateFilteredVoiceMask(channels, voices, 0x20, ~0x1U);
  CHECK(voices == 0x200);
}

namespace {
// Two layers whose allocated, keyed and channel voices are all reserved.
std::unique_ptr<Mixer::MixerState> ReservingMixer() {
  auto state = NewMixer();
  Seq::MusicLayerState& primary = state->primary.config;
  primary.active_channels = 0x3;
  primary.allocated_voices = 0x6;
  primary.keyed_voices = 0x400;
  state->primary.channels[7].assigned_voice = 5;
  state->primary.channels[10].assigned_voice = 6;
  Mixer::MusicLayer& secondary = state->secondary.emplace();
  secondary.config.active_channels = 0x30;
  secondary.config.allocated_voices = 0x10;
  secondary.config.keyed_voices = 0x800;
  secondary.channels[11].assigned_voice = 7;
  secondary.channels[2].assigned_voice = 5;
  return state;
}
}  // namespace

TEST_CASE("Voice availability reserves allocated and keyed channel voices",
          "[audio-mixer-driver][audio]") {
  FakeMixer host;
  host.envelope_levels.fill(0x100);
  host.envelope_levels[5] = 0;
  auto state = ReservingMixer();
  Mixer::RefreshVoiceAvailability(*state, 0x100, host);

  for (const std::uint32_t voice : {1U, 4U, 8U, 6U, 7U}) {
    CHECK(state->envelopes[voice] == Mixer::kReservedEnvelope);
  }
  CHECK(state->envelopes[0] == 0x100);
  CHECK(state->envelopes[5] == 0);
  CHECK(state->primary.channels[7].assigned_voice == Mixer::kNoVoice);
  CHECK(Present(state->secondary).channels[2].assigned_voice ==
        Mixer::kNoVoice);
  // Reserved voices skip the envelope read.
  CHECK(host.envelope_reads.size() == Mixer::kVoiceCount - 3);
  CHECK(std::ranges::find(host.envelope_reads, 4U) ==
        host.envelope_reads.end());
  CHECK(host.refusals.empty());
}

TEST_CASE("Saved music fades its master volume and restores it afterwards",
          "[audio-mixer-driver][audio]") {
  Seq::MusicLayerState layer;
  layer.master_volume = 0x7F0000;
  Mixer::MasterFade fade{0x400000, 0};
  Mixer::ApplySavedMusicMasterFade(layer, fade);
  CHECK(layer.master_volume == 0x400000);
  CHECK(fade.saved_value == 0x7F0000);
  Mixer::RestoreSavedMusicVolume(layer, fade);
  CHECK(layer.master_volume == 0x7F0000);

  // The division by 127 truncates toward zero.
  layer.master_volume = -0x10000;
  fade.value = 0x10000;
  Mixer::ApplySavedMusicMasterFade(layer, fade);
  CHECK(layer.master_volume == -0x200);
  CHECK(fade.saved_value == -0x10000);
}

namespace {
using Seq::ChannelGroup;
using Owners = std::vector<std::pair<std::uint32_t, LayerId>>;
using Words = std::vector<std::uint32_t>;

// Keys channels 0 and 5 with channel 0 holding voice 0 and voice 9 free.
std::uint32_t KeyPendingChannels(Mixer::MixerState& state, FakeMixer& host) {
  state.envelopes.fill(0x100);
  state.envelopes[9] = 0;
  Seq::MusicLayerState& primary = state.primary.config;
  primary.master_volume = 0x7F0000;
  primary.pending_key_on = 0x21;
  state.muted_channels = 0x4;
  for (const std::size_t index : {0U, 2U, 4U, 5U}) {
    state.primary.channels[index] = Audible();
  }
  state.primary.channels[2].assigned_voice = 14;
  std::uint32_t key_on = 0;
  Mixer::ApplyMusicChannelVoices(state, Gains(), LayerId::kPrimary, 0x3D, 0x1,
                                 key_on, host);
  return key_on;
}
}  // namespace

TEST_CASE("Music channel voices key pending channels onto free voices",
          "[audio-mixer-driver][audio]") {
  FakeMixer host;
  auto state = NewMixer();
  const std::uint32_t key_on = KeyPendingChannels(*state, host);
  // Channel 0 keeps its own voice, channel 5 takes the free voice 9, the
  // muted channel 2 plays silent and channel 4 has no voice to configure.
  CHECK(key_on == 0x201);
  CHECK(host.configured == std::vector<Configured>{
                               {{ChannelGroup::kMusic, 0}, 0, 0x1F, 0x5D},
                               {{ChannelGroup::kMusic, 2}, 14, 0, 0},
                               {{ChannelGroup::kMusic, 5}, 9, 0x1F, 0x5D},
                           });
  CHECK(host.owners ==
        Owners{{0U, LayerId::kPrimary}, {9U, LayerId::kPrimary}});
  CHECK(state->envelopes[9] == Mixer::kReservedEnvelope);
  CHECK(host.refusals.empty());
}

TEST_CASE("Music channel voices leave pending keys for the commit to clear",
          "[audio-mixer-driver][audio]") {
  FakeMixer host;
  auto state = NewMixer();
  KeyPendingChannels(*state, host);
  const Seq::MusicLayerState& primary = state->primary.config;
  CHECK(state->primary.channels[0].assigned_voice == 0);
  CHECK(state->primary.channels[4].assigned_voice == Mixer::kNoVoice);
  CHECK(state->primary.channels[4].state.voice.dirty_flags ==
        Seq::kVolumeDirty);
  CHECK(state->driver_update_flags == Seq::kVoiceModeUpdate);
  CHECK(primary.pending_key_on == 0x21);
  CHECK(primary.status_flags == 0);
  CHECK(host.refusals.empty());
}

TEST_CASE("Music channel voices steal the quietest voice when none is free",
          "[audio-mixer-driver][audio]") {
  FakeMixer host;
  auto state = NewMixer();
  state->envelopes.fill(Mixer::kReservedEnvelope);
  state->envelopes[11] = 0x10;
  state->primary.config.pending_key_on = 0x20;
  state->primary.channels[5] = Audible();
  std::uint32_t key_on = 0;
  Mixer::ApplyMusicChannelVoices(*state, Gains(), LayerId::kPrimary, 0x20, 0,
                                 key_on, host);
  CHECK(key_on == 0x800);
  CHECK(state->primary.config.status_flags == Mixer::kVoiceStolen);
  CHECK(state->primary.channels[5].assigned_voice == 11);
  CHECK(state->envelopes[11] == Mixer::kReservedEnvelope);
  CHECK(host.configured.size() == 1);
  CHECK(host.refusals.empty());
}

namespace {
// Every voice reserved and channel 5 pending on voice 3.
std::unique_ptr<Mixer::MixerState> FullMixer() {
  auto full = NewMixer();
  full->envelopes.fill(Mixer::kReservedEnvelope);
  full->primary.config.pending_key_on = 0x20;
  full->primary.channels[5] = Audible();
  full->primary.channels[5].assigned_voice = 3;
  return full;
}
}  // namespace

TEST_CASE("Music channel voices give up when no voice can be stolen",
          "[audio-mixer-driver][audio]") {
  FakeMixer none;
  auto full = FullMixer();
  std::uint32_t key_on = 0;
  Mixer::ApplyMusicChannelVoices(*full, Gains(), LayerId::kPrimary, 0x20, 0,
                                 key_on, none);
  CHECK(key_on == 0);
  CHECK(full->primary.config.status_flags ==
        (Mixer::kVoiceStolen | Mixer::kVoiceUnavailable));
  CHECK(full->primary.channels[5].assigned_voice == Mixer::kNoVoice);
  CHECK(none.configured.empty());
  CHECK(none.owners.empty());
  CHECK(none.refusals.empty());
}

TEST_CASE("Music channel voices refuse a missing secondary layer",
          "[audio-mixer-driver][audio]") {
  auto full = FullMixer();
  std::uint32_t key_on = 0;
  FakeMixer missing;
  Mixer::ApplyMusicChannelVoices(*full, Gains(), LayerId::kSecondary, 0x20, 0,
                                 key_on, missing);
  CHECK(missing.refusals ==
        std::vector<MixRefusal>{MixRefusal::kSecondaryLayerMissing});
  CHECK(missing.configured.empty());
}

namespace {
// Three keyed primary channels with reverb, noise and a pending voice mode.
void CommitPrimaryLayer(Mixer::MixerState& state, FakeMixer& host) {
  host.envelope_levels.fill(0x100);
  host.envelope_levels[4] = 0;
  host.envelope_levels[5] = 0;
  Seq::MusicLayerState& primary = state.primary.config;
  primary.active_channels = 0x7;
  primary.active_notes = 0x7;
  primary.allocated_voices = 0x1;
  primary.keyed_voices = 0x2;
  primary.pending_key_on = 0x7;
  primary.noise_channels = 0x4;
  primary.reverb_channels = 0x1;
  primary.reverb_depth = 0x7FFF0000;
  primary.noise_clock = 0x1F;
  primary.master_volume = 0x7F0000;
  state.driver_update_flags = 0x90;
  state.scheduler.noise_voices = 0x2000;
  for (std::size_t index = 0; index < 3; ++index) {
    state.primary.channels[index] = Audible();
  }
  Mixer::CommitPlaybackVoices(state, Gains(), host);
}
}  // namespace

TEST_CASE("Voice commits key the primary layer and service driver updates",
          "[audio-mixer-driver][audio]") {
  FakeMixer host;
  auto state = NewMixer();
  CommitPrimaryLayer(*state, host);
  // The keyed channel goes first and searches from voice 0; the allocated
  // channel 0 keeps its own voice.
  CHECK(host.configured == std::vector<Configured>{
                               {{ChannelGroup::kMusic, 1}, 4, 0x1F, 0x5D},
                               {{ChannelGroup::kMusic, 0}, 0, 0x1F, 0x5D},
                               {{ChannelGroup::kMusic, 2}, 5, 0x1F, 0x5D},
                           });
  CHECK(host.key_on == Words{0x31});
  // The reverb depth is the halfword of depth >> 12.
  CHECK(host.depths ==
        std::vector<std::pair<std::int16_t, std::int16_t>>{{-16, -16}});
  CHECK(host.clocks == std::vector<std::int32_t>{0x1F});
  CHECK(host.noise_voices == Words{0x2020});
  CHECK(host.refusals.empty());
}

TEST_CASE("Voice commits write the voice modes and clear the pending work",
          "[audio-mixer-driver][audio]") {
  FakeMixer host;
  auto state = NewMixer();
  CommitPrimaryLayer(*state, host);
  CHECK(host.reverb_voices == Words{0x1});
  CHECK(host.fm_voices == Words{0});
  CHECK(state->voice_modes == Mixer::VoiceModes{0x1, 0x2020, 0});
  CHECK(state->driver_update_flags == 0);
  CHECK(state->primary.config.pending_key_on == 0);
  CHECK(host.envelope_reads.size() == Mixer::kVoiceCount - 1);
  CHECK(host.refusals.empty());
}

namespace {
// A faded saved layer, one primary channel and one effect voice to key.
void CommitSavedLayer(Mixer::MixerState& state, FakeMixer& host) {
  Mixer::MusicLayer& secondary = state.secondary.emplace();
  Seq::MusicLayerState& saved = secondary.config;
  saved.active_channels = 0x3;
  saved.active_notes = 0x3;
  saved.keyed_voices = 0x1;
  saved.pending_key_on = 0x3;
  saved.master_volume = 0x7F0000;
  secondary.channels[0] = Audible();
  secondary.channels[1] = Audible();
  Seq::MusicLayerState& primary = state.primary.config;
  primary.active_channels = 0x1;
  primary.active_notes = 0x1;
  primary.pending_key_on = 0x1;
  primary.master_volume = 0x7F0000;
  state.primary.channels[0] = Audible();
  state.mix_behavior = Seq::kSavedMusicFadeActive;
  state.master_fade.value = 0x400000;
  state.scheduler.active_voices = Mixer::kFirstEffectVoiceBit;
  state.scheduler.keyed_voices = Mixer::kFirstEffectVoiceBit;
  state.scheduler.pending_key_on = Mixer::kFirstEffectVoiceBit;
  state.effects[0] = Audible();
  state.effects[0].assigned_voice = 12;
  Mixer::CommitPlaybackVoices(state, Gains(), host);
}
}  // namespace

TEST_CASE("Voice commits play the saved layer first under its master fade",
          "[audio-mixer-driver][audio]") {
  FakeMixer host;
  auto state = NewMixer();
  CommitSavedLayer(*state, host);
  // The saved layer mixes at the faded master volume 0x40.
  CHECK(host.configured ==
        std::vector<Configured>{
            {{ChannelGroup::kSecondaryMusic, 0}, 0, 0xF, 0x2F},
            {{ChannelGroup::kSecondaryMusic, 1}, 1, 0xF, 0x2F},
            {{ChannelGroup::kMusic, 0}, 2, 0x1F, 0x5D},
            {{ChannelGroup::kSoundEffects, 0}, 12, 0, 0},
        });
  CHECK(host.owners == Owners{{0U, LayerId::kSecondary},
                              {1U, LayerId::kSecondary},
                              {2U, LayerId::kPrimary}});
  CHECK(host.key_on == Words{0x1007});
  CHECK(Present(state->secondary).config.master_volume == 0x7F0000);
  CHECK(state->master_fade.saved_value == 0x7F0000);
  CHECK(host.refusals.empty());
}

TEST_CASE("Voice commits clear every layer's pending keys after the fade",
          "[audio-mixer-driver][audio]") {
  FakeMixer host;
  auto state = NewMixer();
  CommitSavedLayer(*state, host);
  CHECK(state->active == LayerId::kPrimary);
  CHECK(Present(state->secondary).config.pending_key_on == 0);
  CHECK(state->primary.config.pending_key_on == 0);
  CHECK(state->scheduler.pending_key_on == 0);
  CHECK(state->driver_update_flags == 0);
  CHECK(host.refusals.empty());
}

namespace {
// Effect voices 0, 12 and 13 with channel 1 holding no voice.
void CommitLowEffectVoices(Mixer::MixerState& state, FakeMixer& host) {
  state.scheduler.active_voices = 0x3001;
  state.scheduler.keyed_voices = 0x3001;
  state.scheduler.pending_key_on = 0x3001;
  state.effects[0] = Audible();
  state.effects[0].assigned_voice = 12;
  state.effects[1] = Audible();
  Mixer::CommitPlaybackVoices(state, Gains(), host);
}
}  // namespace

TEST_CASE("Voice commits refuse effect voices below the effect channels",
          "[audio-mixer-driver][audio]") {
  FakeMixer host;
  auto state = NewMixer();
  CommitLowEffectVoices(*state, host);
  CHECK(host.configured == std::vector<Configured>{
                               {{ChannelGroup::kSoundEffects, 0}, 12, 0, 0},
                           });
  CHECK(host.key_on == Words{0x3001});
  CHECK(host.refusals == std::vector<MixRefusal>{
                             MixRefusal::kEffectVoiceOutsideChannels,
                             MixRefusal::kChannelWithoutVoice,
                         });
  CHECK(state->scheduler.pending_key_on == 0);
  CHECK(host.envelope_reads.empty());
}

TEST_CASE("Voice commits do nothing when the active layer is missing",
          "[audio-mixer-driver][audio]") {
  // A commit whose active layer is a missing secondary does nothing.
  FakeMixer host;
  auto state = NewMixer();
  CommitLowEffectVoices(*state, host);
  FakeMixer missing;
  auto orphan = NewMixer();
  orphan->active = LayerId::kSecondary;
  orphan->scheduler.active_voices = Mixer::kFirstEffectVoiceBit;
  orphan->scheduler.keyed_voices = Mixer::kFirstEffectVoiceBit;
  orphan->scheduler.pending_key_on = Mixer::kFirstEffectVoiceBit;
  orphan->effects[0] = state->effects[0];
  Mixer::CommitPlaybackVoices(*orphan, Gains(), missing);
  CHECK(missing.refusals ==
        std::vector<MixRefusal>{MixRefusal::kSecondaryLayerMissing});
  CHECK(missing.configured.empty());
  CHECK(missing.key_on.empty());
  CHECK(orphan->scheduler.pending_key_on == Mixer::kFirstEffectVoiceBit);
}

namespace {
// Both layers and the scheduler queue key-offs; voice 14 plays an effect.
void QueueReleases(Mixer::MixerState& state) {
  Mixer::MusicLayer& secondary = state.secondary.emplace();
  secondary.config.pending_key_off = 0x3;
  secondary.config.keyed_voices = 0x1;
  secondary.channels[0].assigned_voice = 6;
  secondary.channels[1].assigned_voice = 8;
  Seq::MusicLayerState& primary = state.primary.config;
  primary.pending_key_off = 0x6;
  primary.keyed_voices = 0x4;
  state.primary.channels[1].assigned_voice = 0;
  state.primary.channels[2].assigned_voice = 14;
  state.scheduler.active_voices = 0x4000;
  state.scheduler.pending_key_off = 0x10000;
}

// Queues key-offs only for voices a paused effect or the stream owns.
void QueuePausedReleases(Mixer::MixerState& state) {
  state.scheduler.active_voices = 0;
  state.scheduler.paused_effect_voices = 0x1;
  state.stream_voices = 0x100;
  state.primary.config.pending_key_off = 0x2;
  Present(state.secondary).config.pending_key_off = 0x2;
}
}  // namespace

TEST_CASE("Key-off requests release keyed channels first and filter voices",
          "[audio-mixer-driver][audio]") {
  FakeMixer host;
  auto state = NewMixer();
  QueueReleases(*state);
  Mixer::ReleasePendingVoices(*state, host);
  // Voice 14 belongs to a playing sound effect.
  CHECK(host.key_off == Words{0x10141});
  CHECK(Present(state->secondary).config.pending_key_off == 0);
  CHECK(state->primary.config.pending_key_off == 0);
  CHECK(state->scheduler.pending_key_off == 0);

  host.key_off.clear();
  Mixer::ReleasePendingVoices(*state, host);
  CHECK(host.key_off.empty());
  CHECK(host.refusals.empty());
}

TEST_CASE("Key-off requests filter paused effect and stream voices",
          "[audio-mixer-driver][audio]") {
  FakeMixer host;
  auto state = NewMixer();
  QueueReleases(*state);
  Mixer::ReleasePendingVoices(*state, host);
  host.key_off.clear();
  // Paused effect and stream voices are filtered too.
  QueuePausedReleases(*state);
  Mixer::ReleasePendingVoices(*state, host);
  CHECK(host.key_off.empty());
  CHECK(state->primary.config.pending_key_off == 0);
  CHECK(Present(state->secondary).config.pending_key_off == 0);
  CHECK(host.refusals.empty());
}

TEST_CASE("Key-off requests refuse a missing active secondary layer",
          "[audio-mixer-driver][audio]") {
  FakeMixer host;
  auto state = NewMixer();
  QueueReleases(*state);
  Mixer::ReleasePendingVoices(*state, host);
  QueuePausedReleases(*state);
  Mixer::ReleasePendingVoices(*state, host);
  FakeMixer missing;
  state->active = LayerId::kSecondary;
  state->secondary.reset();
  state->scheduler.pending_key_off = 0x1000;
  Mixer::ReleasePendingVoices(*state, missing);
  CHECK(missing.key_off.empty());
  CHECK(missing.refusals ==
        std::vector<MixRefusal>{MixRefusal::kSecondaryLayerMissing});
  CHECK(host.refusals.empty());
}

TEST_CASE("Voice mode masks map the requested layer channels to voices",
          "[audio-mixer-driver][audio]") {
  Refusals sink;
  auto state = NewMixer();
  state->primary.config.active_channels = 0x3;
  state->primary.config.keyed_voices = 0x2;
  state->primary.channels[0].assigned_voice = 3;
  state->primary.channels[1].assigned_voice = 4;
  state->secondary.emplace();
  Present(state->secondary).config.active_channels = 0x1;
  Present(state->secondary).channels[0].assigned_voice = 7;
  state->stream_voices = 0x8;
  // Voice 3 plays the cutscene stream.
  CHECK(Mixer::BuildVoiceModeMask(*state, {0x1, 0x3, 0x800000}, sink) ==
        0x800090);
  CHECK(state->driver_update_flags == Seq::kVoiceModeUpdate);
  CHECK(Mixer::BuildVoiceModeMask(*state, {0, 0x2, 0}, sink) == 0x10);
  CHECK(sink.list.empty());

  state->active = LayerId::kSecondary;
  state->secondary.reset();
  CHECK(Mixer::BuildVoiceModeMask(*state, {0x1, 0x3, 0x5}, sink) == 0x5);
  CHECK(sink.list ==
        std::vector<MixRefusal>{MixRefusal::kSecondaryLayerMissing});
}

namespace {
using Mixer::CommandRefusal;
using Mixer::EffectId;

class CommandRefusals final : public Mixer::CommandRefusalSink {
 public:
  std::vector<CommandRefusal> list;

  void Refuse(CommandRefusal refusal) override { list.push_back(refusal); }
};

struct VoiceCall {
  std::string_view what;
  std::uint32_t voice = 0;
  std::array<std::uint32_t, 3> values{};

  friend bool operator==(const VoiceCall&, const VoiceCall&) = default;
};

struct MusicRelease {
  ChannelGroup layer = ChannelGroup::kMusic;
  std::uint32_t filter = 0;

  friend bool operator==(const MusicRelease&, const MusicRelease&) = default;
};

// A control host that records the layer releases, volume invalidations and
// voice writes a command asks for.
class FakeControl final : public Mixer::ControlHost {
 public:
  std::vector<MusicRelease> released;
  std::vector<ChannelGroup> invalidated;
  std::vector<VoiceCall> voice_calls;
  std::vector<CommandRefusal> refusals;

  void ReleaseMusic(Mixer::CommandState& /*state*/, ChannelGroup layer,
                    std::uint32_t music_id_filter) override {
    released.push_back({layer, music_id_filter});
  }

  void InvalidateMusicVolumes(Mixer::CommandState& /*state*/,
                              ChannelGroup layer) override {
    invalidated.push_back(layer);
  }

  void InvalidateSoundEffectVolumes(Mixer::CommandState& /*state*/) override {
    invalidated.push_back(ChannelGroup::kSoundEffects);
  }

  void SetVoiceVolume(std::uint32_t voice, std::uint32_t left,
                      std::uint32_t right, std::uint32_t scale) override {
    voice_calls.push_back({"volume", voice, {left, right, scale}});
  }

  void SetVoiceSampleRate(std::uint32_t voice, std::int32_t rate) override {
    voice_calls.push_back(
        {"rate", voice, {static_cast<std::uint32_t>(rate), 0, 0}});
  }

  void SetVoiceAttack(std::uint32_t voice, std::int32_t rate,
                      std::uint32_t mode) override {
    voice_calls.push_back(
        {"attack", voice, {static_cast<std::uint32_t>(rate), mode, 0}});
  }

  void SetVoiceSustain(std::uint32_t voice, std::int32_t rate,
                       std::uint32_t mode) override {
    voice_calls.push_back(
        {"sustain", voice, {static_cast<std::uint32_t>(rate), mode, 0}});
  }

  void Refuse(CommandRefusal refusal) override { refusals.push_back(refusal); }
};

// The writes a paused voice's release makes, voice by voice.
std::vector<VoiceCall> Silenced(const std::vector<std::uint32_t>& voices) {
  std::vector<VoiceCall> calls;
  for (const std::uint32_t voice : voices) {
    calls.push_back({"volume", voice, {0, 0, 0}});
    calls.push_back({"rate", voice, {0, 0, 0}});
    calls.push_back({"attack", voice, {0x7F, 1, 0}});
    calls.push_back({"sustain", voice, {0x7F, 3, 0}});
  }
  return calls;
}

std::vector<std::uint32_t> VoiceRange(std::uint32_t first, std::uint32_t end) {
  std::vector<std::uint32_t> voices;
  for (std::uint32_t voice = first; voice < end; ++voice) {
    voices.push_back(voice);
  }
  return voices;
}

constexpr std::uintptr_t kTopBit = std::bit_floor(~std::uintptr_t{0});

Seq::MusicCommand Command(std::uint32_t param1, std::uint32_t param2 = 0,
                          std::uint32_t param3 = 0) {
  Seq::MusicCommand command;
  command.param1 = param1;
  command.param2 = param2;
  command.param3 = param3;
  return command;
}

// Effects 0-2 play; effect 1 is a fixed mix effect and effect 3 is silent.
Mixer::CommandState PlayingEffects() {
  Mixer::CommandState state;
  state.scheduler.active_voices = 0x7000;
  state.scheduler.keyed_voices = 0x7000;
  state.effects[0].tags = 4;
  state.effects[1].tags = static_cast<std::int32_t>(Mixer::kFixedMixEffect) | 3;
  state.effects[1].effect_id = EffectId{kTopBit | 0x10};
  state.effects[2].effect_id = EffectId{0x10};
  state.effects[3].tags = 4;
  return state;
}
}  // namespace

TEST_CASE("Sound-effect pitch scales change only the selected playing effects",
          "[audio-mixer-driver][audio]") {
  Mixer::CommandState state = PlayingEffects();
  state.effects[0].pitch_scale_length = 5;
  Mixer::CommandState expected = state;
  // The command byte is unsigned (lbu) and becomes the high pitch byte.
  Mixer::SetSoundEffectPitchScale(state, {EffectId{}, 4}, 0x1A5);
  expected.effects[0].pitch_scale = 0xA500;
  expected.effects[0].pitch_scale_length = 0;
  expected.effects[0].dirty_flags = Seq::kSampleRateDirty;
  CHECK(state == expected);

  // Without tags the effect identity selects, compared at its full width.
  Mixer::SetSoundEffectPitchScale(state, {EffectId{0x10}, 0}, 0x22);
  expected.effects[2].pitch_scale = 0x2200;
  expected.effects[2].dirty_flags = Seq::kSampleRateDirty;
  CHECK(state == expected);
  Mixer::SetSoundEffectPitchScale(state, {EffectId{kTopBit | 0x10}, 0}, 0x33);
  expected.effects[1].pitch_scale = 0x3300;
  expected.effects[1].dirty_flags = Seq::kSampleRateDirty;
  CHECK(state == expected);

  // Tags select any overlap, but silent channels are never touched.
  Mixer::SetSoundEffectPitchScale(state, {EffectId{}, 6}, 0x44);
  expected.effects[0].pitch_scale = 0x4400;
  expected.effects[1].pitch_scale = 0x4400;
  CHECK(state == expected);
  CHECK(state.effects[3].pitch_scale == 0);
}

TEST_CASE("Sound-effect pitch slides divide at the original 16-bit widths",
          "[audio-mixer-driver][audio]") {
  CommandRefusals sink;
  Mixer::CommandState state = PlayingEffects();
  Mixer::EffectControl& effect = state.effects[0];
  const Mixer::EffectSelection selection{EffectId{}, 4};

  effect.pitch_scale = 0x1000;
  Mixer::SlideSoundEffectPitchScale(state, selection, 8, 0x30, sink);
  CHECK(effect.pitch_scale_step == 0x400);
  CHECK(effect.pitch_scale_length == 8);
  CHECK(effect.pitch_scale == 0x1000);

  effect.pitch_scale = 0x5000;
  Mixer::SlideSoundEffectPitchScale(state, selection, 0x10, 0, sink);
  CHECK(effect.pitch_scale_step == 0xFFFFFB00);
  CHECK(effect.pitch_scale_length == 0x10);

  // The distance wraps at 16 bits: 0x7F00 - 0x8000.
  effect.pitch_scale = 0x18000;
  Mixer::SlideSoundEffectPitchScale(state, selection, 1, 0x7F, sink);
  CHECK(effect.pitch_scale_step == 0xFFFFFF00);
  CHECK(effect.pitch_scale_length == 1);
  CHECK(sink.list.empty());
}

TEST_CASE("Sound-effect pitch slides over zero or 0xFFFF ticks",
          "[audio-mixer-driver][audio]") {
  CommandRefusals sink;
  Mixer::CommandState state = PlayingEffects();
  Mixer::EffectControl& effect = state.effects[0];
  const Mixer::EffectSelection selection{EffectId{}, 4};

  // A zero length runs over one tick.
  effect.pitch_scale = 0x1000;
  Mixer::SlideSoundEffectPitchScale(state, selection, 0, 0x30, sink);
  CHECK(effect.pitch_scale_step == 0x2000);
  CHECK(effect.pitch_scale_length == 1);

  // A 16-bit length of 0xFFFF divides by -1 (div of a sign-extended half).
  Mixer::SlideSoundEffectPitchScale(state, selection, 0xFFFF, 0x30, sink);
  CHECK(effect.pitch_scale_step == 0xFFFFE000);
  CHECK(effect.pitch_scale_length == 0xFFFF);
  CHECK(sink.list.empty());
}

TEST_CASE("Sound-effect pitch slides refuse a zero 16-bit length",
          "[audio-mixer-driver][audio]") {
  CommandRefusals sink;
  Mixer::CommandState state = PlayingEffects();
  Mixer::EffectControl& effect = state.effects[0];
  effect.pitch_scale = 0x1000;
  // A length whose low half is zero is refused and divides by one.
  Mixer::SlideSoundEffectPitchScale(state, {EffectId{}, 4}, 0x10000, 0x30,
                                    sink);
  CHECK(effect.pitch_scale_step == 0x2000);
  CHECK(effect.pitch_scale_length == 0);
  CHECK(sink.list ==
        std::vector<CommandRefusal>{CommandRefusal::kZeroLengthSlide});
  // Only the selected playing effect slid.
  CHECK(state.effects[2].pitch_scale_length == 0);
  CHECK(state.effects[3].pitch_scale_length == 0);
}

TEST_CASE("World sound-effect pitch commands skip fixed mix effects",
          "[audio-mixer-driver][audio]") {
  Mixer::CommandState state = PlayingEffects();
  state.effects[3].tags = static_cast<std::int32_t>(Mixer::kFixedMixEffect);
  state.effects[12].pitch_scale = 0x77;

  // Every world channel takes the pitch, playing or not.
  Mixer::SetWorldSoundEffectPitchScale(state, Command(0x122));
  for (const std::size_t i : {0U, 2U, 4U, 11U}) {
    CHECK(state.effects[i].pitch_scale == 0x2200);
    CHECK(state.effects[i].dirty_flags == Seq::kSampleRateDirty);
  }
  CHECK(state.effects[1].pitch_scale == 0);
  CHECK(state.effects[1].dirty_flags == 0);
  CHECK(state.effects[3].pitch_scale == 0);
  CHECK(state.effects[12].pitch_scale == 0x77);
  CHECK(state.effects[12].dirty_flags == 0);
}

TEST_CASE("World sound-effect pitch slides move only playing world channels",
          "[audio-mixer-driver][audio]") {
  CommandRefusals sink;
  Mixer::CommandState state = PlayingEffects();
  state.effects[3].tags = static_cast<std::int32_t>(Mixer::kFixedMixEffect);
  state.effects[12].pitch_scale = 0x77;
  Mixer::SetWorldSoundEffectPitchScale(state, Command(0x122));

  // The slide moves only the playing world channels.
  Mixer::SlideWorldSoundEffectPitchScale(state, Command(4, 0x2A), sink);
  for (const std::size_t i : {0U, 2U}) {
    CHECK(state.effects[i].pitch_scale_step == 0x200);
    CHECK(state.effects[i].pitch_scale_length == 4);
  }
  CHECK(state.effects[1].pitch_scale_length == 0);
  CHECK(state.effects[4].pitch_scale_length == 0);
  CHECK(sink.list.empty());
}

TEST_CASE("Tempo scale commands slide between signed command bytes",
          "[audio-mixer-driver][audio]") {
  CommandRefusals sink;
  Seq::LevelSlide tempo{0, 0x55, 9};
  // The command byte is signed (lb).
  Mixer::SetTempoScale(tempo, -2);
  CHECK(tempo.value == -0x20000);
  CHECK(tempo.step == 0x55);
  CHECK(tempo.length == 0);

  Mixer::SlideTempoScale(tempo, Command(4, 2), sink);
  CHECK(tempo.value == -0x20000);
  CHECK(tempo.step == 0x10000);
  CHECK(tempo.length == 4);
  CHECK(sink.list.empty());
}

TEST_CASE("Tempo scale slides run zero lengths over one tick and wrap at 16",
          "[audio-mixer-driver][audio]") {
  CommandRefusals sink;
  Seq::LevelSlide tempo{-0x20000, 0x10000, 4};
  Mixer::SlideTempoScale(tempo, Command(0, 2), sink);
  CHECK(tempo.step == 0x40000);
  CHECK(tempo.length == 1);
  // The 32-bit length divides; its 16-bit copy wraps.
  Mixer::SlideTempoScale(tempo, Command(0x10000, 2), sink);
  CHECK(tempo.step == 4);
  CHECK(tempo.length == 0);
  CHECK(sink.list.empty());
}

TEST_CASE("Tempo scale slides refuse a step that overflows the division",
          "[audio-mixer-driver][audio]") {
  CommandRefusals sink;
  Seq::LevelSlide tempo{kMinimum, 4, 0};
  Mixer::SlideTempoScale(tempo, Command(0xFFFFFFFF, 0), sink);
  CHECK(tempo.step == kMinimum);
  CHECK(tempo.length == -1);
  CHECK(sink.list ==
        std::vector<CommandRefusal>{CommandRefusal::kSlideStepOverflow});
}

TEST_CASE("Tempo scale ramps start from the first signed command byte",
          "[audio-mixer-driver][audio]") {
  CommandRefusals sink;
  Seq::LevelSlide tempo{kMinimum, kMinimum, -1};
  Mixer::SetTempoScaleRamp(tempo, Command(8, 0x10, 0x20), sink);
  CHECK(tempo.value == 0x100000);
  CHECK(tempo.step == 0x20000);
  CHECK(tempo.length == 8);
  Mixer::SetTempoScaleRamp(tempo, Command(8, 0, 0x20), sink);
  CHECK(tempo.value == 0);
  CHECK(tempo.step == 0x200000);
  CHECK(tempo.length == 1);
  CHECK(sink.list.empty());
}

TEST_CASE("Tempo scale ramps wrap signed starts and refuse zero lengths",
          "[audio-mixer-driver][audio]") {
  CommandRefusals sink;
  Seq::LevelSlide tempo{0, 0x200000, 1};
  Mixer::SetTempoScaleRamp(tempo, Command(8, 0x1FF, 0), sink);
  CHECK(tempo.value == -0x10000);
  CHECK(tempo.step == 0x2000);
  CHECK(sink.list.empty());

  // A zero length with a nonzero start is refused and divides by one.
  Mixer::SetTempoScaleRamp(tempo, Command(0, 0x10, 0x20), sink);
  CHECK(tempo.value == 0x100000);
  CHECK(tempo.step == 0x100000);
  CHECK(tempo.length == 0);
  CHECK(sink.list ==
        std::vector<CommandRefusal>{CommandRefusal::kZeroLengthSlide});
}

TEST_CASE("Music pitch scale commands share the tempo slide arithmetic",
          "[audio-mixer-driver][audio]") {
  CommandRefusals sink;
  Seq::LevelSlide pitch{0, 0x55, 9};
  Mixer::SetMusicPitchScale(pitch, 3);
  CHECK(pitch.value == 0x30000);
  CHECK(pitch.step == 0x55);
  CHECK(pitch.length == 0);

  Mixer::SlideMusicPitchScale(pitch, Command(2, 0xFF), sink);
  CHECK(pitch.value == 0x30000);
  CHECK(pitch.step == -0x20000);
  CHECK(pitch.length == 2);
  CHECK(sink.list.empty());
}

TEST_CASE("Music pitch scale ramps share the tempo ramp arithmetic",
          "[audio-mixer-driver][audio]") {
  CommandRefusals sink;
  Seq::LevelSlide pitch{0x30000, -0x20000, 2};
  Mixer::SetMusicPitchScaleRamp(pitch, Command(4, 1, 5), sink);
  CHECK(pitch.value == 0x10000);
  CHECK(pitch.step == 0x10000);
  CHECK(pitch.length == 4);
  CHECK(sink.list.empty());

  Mixer::SetMusicPitchScaleRamp(pitch, Command(0, 1, 5), sink);
  CHECK(pitch.step == 0x40000);
  CHECK(pitch.length == 0);
  CHECK(sink.list ==
        std::vector<CommandRefusal>{CommandRefusal::kZeroLengthSlide});
}

TEST_CASE("Music stops release the active and secondary layers",
          "[audio-mixer-driver][audio]") {
  FakeControl host;
  Mixer::CommandState state;
  state.active.emplace();
  Present(state.active).status_flags = 0x10001;
  state.has_secondary = true;
  state.music_queued = true;
  Mixer::StopMusicAndCancelReplacement(state, host);
  CHECK(host.released == std::vector<MusicRelease>{
                             {ChannelGroup::kMusic, 0},
                             {ChannelGroup::kSecondaryMusic, 0},
                         });
  CHECK(Present(state.active).status_flags == 0x1);
  CHECK_FALSE(state.music_queued);

  host.released.clear();
  Mixer::StopMatchingMusic(state, Command(0x55), host);
  CHECK(host.released == std::vector<MusicRelease>{
                             {ChannelGroup::kMusic, 0x55},
                             {ChannelGroup::kSecondaryMusic, 0x55},
                         });
  // Filter 0 stops only the active layer.
  host.released.clear();
  Mixer::StopMatchingMusic(state, Command(0), host);
  CHECK(host.released == std::vector<MusicRelease>{{ChannelGroup::kMusic, 0}});
  CHECK(host.refusals.empty());
}

TEST_CASE("Music stops without an active layer still cancel the queue",
          "[audio-mixer-driver][audio]") {
  // Without an active layer the queue is still cancelled.
  FakeControl missing;
  Mixer::CommandState silent;
  silent.music_queued = true;
  Mixer::StopMusicAndCancelReplacement(silent, missing);
  Mixer::StopMatchingMusic(silent, Command(0x55), missing);
  CHECK(missing.released.empty());
  CHECK_FALSE(silent.music_queued);
  CHECK(missing.refusals == std::vector<CommandRefusal>{
                                CommandRefusal::kActiveLayerMissing,
                                CommandRefusal::kActiveLayerMissing,
                            });
}

namespace {
// Stops world effects 0 and 2 with effect 1 fixed and effect 3 silent.
Mixer::CommandState StoppedWorldEffects() {
  Mixer::CommandState state = PlayingEffects();
  state.effects[0].effect_id = EffectId{0x20};
  state.effects[0].update_flags = 0x9;
  state.effects[2].update_flags = 0x9;
  state.effects[3].effect_id = EffectId{0x30};
  state.scheduler.reverb_voices = 0x7000;
  Mixer::StopWorldSoundEffects(state);
  return state;
}
}  // namespace

TEST_CASE("World sound-effect stops release the playing world voices",
          "[audio-mixer-driver][audio]") {
  const Mixer::CommandState state = StoppedWorldEffects();
  CHECK(state.scheduler.pending_key_off == 0x5000);
  CHECK(state.scheduler.active_voices == 0x2000);
  CHECK(state.scheduler.keyed_voices == 0x2000);
  CHECK(state.scheduler.reverb_voices == 0x2000);
  CHECK(state.effects[1] == PlayingEffects().effects[1]);
}

TEST_CASE("World sound-effect stops clear the stopped effects only",
          "[audio-mixer-driver][audio]") {
  const Mixer::CommandState state = StoppedWorldEffects();
  for (const std::size_t i : {0U, 2U}) {
    CHECK(state.effects[i].tags == 0);
    CHECK(state.effects[i].effect_id == EffectId{});
    CHECK(state.effects[i].update_flags == 0);
  }
  // A silent channel keeps its effect.
  CHECK(state.effects[3].effect_id == EffectId{0x30});
  CHECK(state.driver_update_flags ==
        (Seq::kVoiceModeUpdate | Seq::kNoiseClockUpdate));
}

TEST_CASE("Output mode commands invalidate every playing layer's volumes",
          "[audio-mixer-driver][audio]") {
  FakeControl host;
  Mixer::CommandState state;
  state.active.emplace();
  state.has_secondary = true;
  Mixer::EnableMonoOutput(state, host);
  CHECK(state.mix_behavior == Mixer::kMonoOutput);
  CHECK(host.invalidated == std::vector<ChannelGroup>{
                                ChannelGroup::kMusic,
                                ChannelGroup::kSecondaryMusic,
                                ChannelGroup::kSoundEffects,
                            });
  CHECK(host.refusals.empty());

  // Without layers only the sound effects are invalidated.
  FakeControl missing;
  Mixer::CommandState silent;
  silent.mix_behavior = Mixer::kMonoOutput;
  Mixer::EnableStereoOutput(silent, missing);
  CHECK(silent.mix_behavior == Mixer::kStereoOutput);
  CHECK(missing.invalidated ==
        std::vector<ChannelGroup>{ChannelGroup::kSoundEffects});
  CHECK(missing.refusals ==
        std::vector<CommandRefusal>{CommandRefusal::kActiveLayerMissing});
}

TEST_CASE("Mute masks dirty every music volume and jump thresholds keep a half",
          "[audio-mixer-driver][audio]") {
  CommandRefusals sink;
  Mixer::CommandState state;
  state.music_dirty_flags[5] = Seq::kSampleRateDirty;
  Mixer::SetMusicMuteMask(state, Command(0x80000001));
  CHECK(state.muted_music_channels == kMinimum + 1);
  CHECK(std::ranges::all_of(state.music_dirty_flags, [](std::uint32_t dirty) {
    return (dirty & Seq::kVolumeDirty) == Seq::kVolumeDirty;
  }));
  CHECK(state.music_dirty_flags[5] ==
        (Seq::kSampleRateDirty | Seq::kVolumeDirty));

  Mixer::SetMusicJumpThreshold(state, Command(0x12345), sink);
  CHECK(sink.list ==
        std::vector<CommandRefusal>{CommandRefusal::kActiveLayerMissing});
  sink.list.clear();
  state.active.emplace();
  // The threshold is stored with sh.
  Mixer::SetMusicJumpThreshold(state, Command(0x12345), sink);
  CHECK(Present(state.active).jump_threshold == 0x2345);
  CHECK(sink.list.empty());
}

namespace {
// Music channels 4-5 play; voices 0-1 stream and 12-23 play sound effects.
Mixer::CommandState PlayingMusic() {
  Mixer::CommandState state;
  state.active.emplace().active_channels = 0x30;
  state.scheduler.active_voices = 0xFFF000;
  state.stream_voices = 0x3;
  state.stream_control_flags = 0x4;
  return state;
}
}  // namespace

TEST_CASE("Music pauses silence free voices and resumes dirty paused channels",
          "[audio-mixer-driver][audio]") {
  FakeControl host;
  Mixer::CommandState state = PlayingMusic();
  // Voices 0-1 stream and 12-23 play sound effects.
  Mixer::PauseMusic(state, host);
  CHECK(host.voice_calls == Silenced(VoiceRange(2, 12)));
  CHECK(Present(state.active).paused_channels == 0x30);
  CHECK(Present(state.active).active_channels == 0);
  CHECK(state.stream_control_flags == (0x4 | Mixer::kMusicPaused));

  // A layer with no playing channels keeps its paused channels.
  host.voice_calls.clear();
  Mixer::PauseMusic(state, host);
  CHECK(host.voice_calls.empty());
  CHECK(Present(state.active).paused_channels == 0x30);
  CHECK(host.refusals.empty());
}

TEST_CASE("Music resumes dirty the paused channels and clear the pause flag",
          "[audio-mixer-driver][audio]") {
  FakeControl host;
  Mixer::CommandState state = PlayingMusic();
  Mixer::PauseMusic(state, host);
  Mixer::PauseMusic(state, host);
  CommandRefusals sink;
  Mixer::ResumeMusic(state, sink);
  for (std::size_t i = 0; i < Seq::kMusicChannelCount; ++i) {
    CAPTURE(i);
    CHECK(state.music_dirty_flags[i] ==
          (i == 4 || i == 5 ? Mixer::kResumeDirty : 0U));
  }
  CHECK(Present(state.active).paused_channels == 0);
  CHECK(Present(state.active).active_channels == 0x30);
  CHECK(state.driver_update_flags == Seq::kVoiceModeUpdate);
  CHECK(state.stream_control_flags == 0x4);
  CHECK(sink.list.empty());
}

TEST_CASE("Music pauses without an active layer still change the pause flag",
          "[audio-mixer-driver][audio]") {
  CommandRefusals sink;
  // Without an active layer the pause flag still changes.
  FakeControl missing;
  Mixer::CommandState silent;
  Mixer::PauseMusic(silent, missing);
  CHECK(silent.stream_control_flags == Mixer::kMusicPaused);
  Mixer::ResumeMusic(silent, sink);
  CHECK(silent.stream_control_flags == 0);
  CHECK(missing.voice_calls.empty());
  CHECK(missing.refusals ==
        std::vector<CommandRefusal>{CommandRefusal::kActiveLayerMissing});
  CHECK(sink.list ==
        std::vector<CommandRefusal>{CommandRefusal::kActiveLayerMissing});
}

namespace {
// Pauses then resumes effects 0-2 with voice 0 below the effect channels.
Mixer::CommandState ResumedWorldEffects(CommandRefusals& sink) {
  FakeControl host;
  Mixer::CommandState state = PlayingEffects();
  state.scheduler.active_voices = 0x7001;
  Mixer::PauseWorldSoundEffects(state, host);
  Mixer::ResumeWorldSoundEffects(state, sink);
  return state;
}
}  // namespace

TEST_CASE("World sound-effect pauses skip fixed effects and voices below them",
          "[audio-mixer-driver][audio]") {
  FakeControl host;
  Mixer::CommandState state = PlayingEffects();
  // Voice 0 lies below the sound-effect channels; the original never ends
  // its voice loop over it.
  state.scheduler.active_voices = 0x7001;
  Mixer::PauseWorldSoundEffects(state, host);
  CHECK(state.scheduler.paused_effect_voices == 0x5001);
  CHECK(state.scheduler.active_voices == 0x2000);
  CHECK(host.voice_calls == Silenced({12, 14}));
  CHECK(host.refusals == std::vector<CommandRefusal>{
                             CommandRefusal::kEffectVoiceOutsideChannels});
  CHECK(state.stream_control_flags == Mixer::kEffectsPaused);
}

TEST_CASE("World sound-effect resumes dirty the paused world effects",
          "[audio-mixer-driver][audio]") {
  CommandRefusals sink;
  const Mixer::CommandState state = ResumedWorldEffects(sink);
  CHECK(sink.list == std::vector<CommandRefusal>{
                         CommandRefusal::kEffectVoiceOutsideChannels});
  CHECK(state.effects[0].dirty_flags == Mixer::kResumeDirty);
  CHECK(state.effects[1].dirty_flags == 0);
  CHECK(state.effects[2].dirty_flags == Mixer::kResumeDirty);
}

TEST_CASE("World sound-effect resumes restore the paused voices",
          "[audio-mixer-driver][audio]") {
  CommandRefusals sink;
  const Mixer::CommandState state = ResumedWorldEffects(sink);
  CHECK(state.scheduler.paused_effect_voices == 0);
  CHECK(state.scheduler.active_voices == 0x7001);
  CHECK(state.driver_update_flags == Seq::kVoiceModeUpdate);
  CHECK(state.stream_control_flags == 0);
}

TEST_CASE("World sound-effect pauses with nothing playing change the flags",
          "[audio-mixer-driver][audio]") {
  CommandRefusals sink;
  // With nothing playing or paused only the flags change.
  FakeControl idle;
  Mixer::CommandState quiet;
  Mixer::PauseWorldSoundEffects(quiet, idle);
  CHECK(quiet.stream_control_flags == Mixer::kEffectsPaused);
  Mixer::ResumeWorldSoundEffects(quiet, sink);
  CHECK(quiet.stream_control_flags == 0);
  CHECK(quiet.driver_update_flags == 0);
  CHECK(idle.voice_calls.empty());
  CHECK(idle.refusals.empty());
}

namespace {
class FakeReverb final : public Mixer::ReverbHost {
 public:
  std::int32_t mode = 0;
  Log calls;

  std::int32_t ReverbMode() override { return mode; }

  void EnableReverb(bool enabled) override {
    calls.emplace_back(enabled ? "enable" : "disable");
  }

  void SetReverbMode(std::int32_t value) override {
    calls.emplace_back("set");
    mode = value;
  }
};

// A dispatch host over two sequence headers and a saved layer.
class FakeDispatch final : public Mixer::DispatchHost {
 public:
  std::optional<Mixer::AkaoHeader> first;
  std::optional<Mixer::AkaoHeader> second;
  std::optional<Seq::MusicLayerState> saved;
  std::int32_t mode = 0;
  Log calls;
  Words handlers;
  Words replaced;
  std::vector<CommandRefusal> refusals;

  std::int32_t ReverbMode() override { return mode; }

  void EnableReverb(bool enabled) override {
    calls.emplace_back(enabled ? "enable" : "disable");
  }

  void SetReverbMode(std::int32_t value) override {
    calls.emplace_back("set");
    mode = value;
  }

  void BeginCommand() override { calls.emplace_back("begin"); }

  void EndCommand() override { calls.emplace_back("end"); }

  std::optional<Mixer::AkaoHeader> SequenceAt(
      Mixer::ArgumentSlot slot) override {
    return slot == Mixer::ArgumentSlot::kFirst ? first : second;
  }

  void CopyFirstArgument() override { calls.emplace_back("copy"); }

  void QueueFirstSequence() override { calls.emplace_back("queue"); }

  void ReplaceFirstArgument(std::uint32_t music_id) override {
    calls.emplace_back("replace");
    replaced.push_back(music_id);
  }

  void LoadSequence(Mixer::DispatchState& /*state*/,
                    Mixer::ArgumentSlot slot) override {
    calls.emplace_back(slot == Mixer::ArgumentSlot::kFirst ? "load first"
                                                           : "load second");
  }

  void SaveActiveLayer(Mixer::DispatchState& state) override {
    calls.emplace_back("save");
    saved = state.active;
  }

  void AttachSavedLayer(Mixer::DispatchState& state) override {
    calls.emplace_back("attach");
    state.secondary = saved;
  }

  void RunHandler(Mixer::DispatchState& /*state*/,
                  std::uint32_t command) override {
    calls.emplace_back("run");
    handlers.push_back(command);
  }

  void Refuse(CommandRefusal refusal) override { refusals.push_back(refusal); }

  void Clear() {
    calls.clear();
    handlers.clear();
  }
};

constexpr std::uint32_t kNoTrack = 0xFFFFFFFFU;
constexpr Mixer::CommandWords kArguments{0x21, 0x22, 0x23, 0x24, 0x25};

// Track 3 plays and both layers' transition latches are set.
Mixer::DispatchState Dispatching() {
  Mixer::DispatchState state;
  state.arguments = kArguments;
  state.control_latches = 0x7701;
  state.active.emplace();
  Present(state.active).music_id = 3;
  return state;
}
}  // namespace

TEST_CASE("Reverb mode changes disable the reverb around the new mode",
          "[audio-mixer-driver][audio]") {
  FakeReverb host;
  host.mode = 3;
  Mixer::SetReverbMode(host, 3);
  CHECK(host.calls.empty());
  Mixer::SetReverbMode(host, 5);
  CHECK(host.calls == Log{"disable", "set", "enable"});
  CHECK(host.mode == 5);
}

TEST_CASE("AKAO headers give the sequence id and the reverb mode",
          "[audio-mixer-driver][audio]") {
  constexpr std::array<std::uint8_t, 12> kHeader{
      'A', 'K', 'A', 'O', 0x34, 0x12, 0x77, 0x77, 0x05, 0x01, 0x99, 0x99};
  const auto bytes = std::span(kHeader);
  CHECK(Mixer::ReadAkaoHeader(bytes) == Mixer::AkaoHeader{0x1234, 0x105});
  CHECK(Mixer::ReadAkaoHeader(bytes.first(Mixer::kAkaoHeaderBytes)) ==
        Mixer::AkaoHeader{0x1234, 0x105});
  CHECK_FALSE(Mixer::ReadAkaoHeader(bytes.first(9)).has_value());
  CHECK_FALSE(Mixer::ReadAkaoHeader({}).has_value());
  std::array<std::uint8_t, 12> wrong = kHeader;
  wrong[3] = 'X';
  CHECK_FALSE(Mixer::ReadAkaoHeader(wrong).has_value());
}

TEST_CASE("Track start commands set the reverb and the start parameters",
          "[audio-mixer-driver][audio]") {
  FakeDispatch host;
  host.first = Mixer::AkaoHeader{9, 4};
  Mixer::DispatchState state = Dispatching();
  // Only the low command byte selects.
  CHECK(Mixer::DispatchCommand(state, host, 0x110) == 9);
  CHECK(state.control_latches == 0x1);
  CHECK(host.calls ==
        Log{"begin", "disable", "set", "enable", "copy", "run", "end"});
  CHECK(host.handlers == Words{0x10});
  CHECK(host.mode == 4);
  CHECK(state.parameters == Mixer::CommandWords{0, 9, 0, 0, 0});
  CHECK(Present(state.active).music_id == 3);
  CHECK(host.refusals.empty());
}

namespace {
// Starts track 9 with command 0x110 and clears the host's call log.
Mixer::DispatchState StartedTrack(FakeDispatch& host) {
  host.first = Mixer::AkaoHeader{9, 4};
  Mixer::DispatchState state = Dispatching();
  static_cast<void>(Mixer::DispatchCommand(state, host, 0x110));
  host.Clear();
  return state;
}

// Replays the loop-count start 0x12 and the layer start 0x19.
void StartLoopAndLayer(Mixer::DispatchState& state, FakeDispatch& host) {
  state.arguments.param2 = 0x31;
  static_cast<void>(Mixer::DispatchCommand(state, host, 0x12));
  state.arguments.param2 = 0x41;
  static_cast<void>(Mixer::DispatchCommand(state, host, 0x19));
  host.Clear();
}
}  // namespace

TEST_CASE("Track start commands keep earlier parameters for later starts",
          "[audio-mixer-driver][audio]") {
  FakeDispatch host;
  Mixer::DispatchState state = StartedTrack(host);
  state.arguments.param2 = 0x31;
  CHECK(Mixer::DispatchCommand(state, host, 0x12) == 9);
  CHECK(host.calls == Log{"begin", "copy", "run", "end"});
  CHECK(host.handlers == Words{0x12});
  CHECK(state.parameters == Mixer::CommandWords{0, 9, 0x31, 0, 0});

  host.Clear();
  state.arguments.param2 = 0x41;
  CHECK(Mixer::DispatchCommand(state, host, 0x19) == 9);
  CHECK(host.handlers == Words{0x19});
  CHECK(state.parameters == Mixer::CommandWords{0, 9, 0x31, 0x41, 0});
}

TEST_CASE("Track crossfade commands play a zero loop count once",
          "[audio-mixer-driver][audio]") {
  FakeDispatch host;
  Mixer::DispatchState state = StartedTrack(host);
  StartLoopAndLayer(state, host);
  // A zero loop count plays once.
  state.arguments.param2 = 0;
  CHECK(Mixer::DispatchCommand(state, host, 0x1A) == 9);
  CHECK(host.handlers == Words{0x1A});
  CHECK(state.parameters == Mixer::CommandWords{0, 9, 0x31, 1, 0x22});
  state.arguments.param2 = 6;
  CHECK(Mixer::DispatchCommand(state, host, 0x1A) == 9);
  CHECK(state.parameters.ext_param1 == 6);
  CHECK(host.refusals.empty());
}

TEST_CASE("Track start commands skip the playing track and missing sequences",
          "[audio-mixer-driver][audio]") {
  FakeDispatch host;
  host.mode = 4;
  host.first = Mixer::AkaoHeader{9, 4};
  Mixer::DispatchState state = Dispatching();
  Present(state.active).music_id = 9;
  // The same track without a secondary layer runs handler 0.
  CHECK(Mixer::DispatchCommand(state, host, 0x10) == 0);
  CHECK(host.calls == Log{"begin", "run", "end"});
  CHECK(host.handlers == Words{0});
  CHECK(state.control_latches == 0x1);
  CHECK(state.parameters == Mixer::CommandWords{});

  // A secondary layer playing another track restarts it.
  host.Clear();
  state.secondary.emplace();
  Present(state.secondary).music_id = 11;
  CHECK(Mixer::DispatchCommand(state, host, 0x10) == 9);
  CHECK(host.handlers == Words{0x10});
  CHECK(host.refusals.empty());
}

namespace {
// Track 9 skipped and then restarted over a secondary layer playing 11.
Mixer::DispatchState RestartedTrack(FakeDispatch& host) {
  host.mode = 4;
  host.first = Mixer::AkaoHeader{9, 4};
  Mixer::DispatchState state = Dispatching();
  Present(state.active).music_id = 9;
  static_cast<void>(Mixer::DispatchCommand(state, host, 0x10));
  state.secondary.emplace().music_id = 11;
  static_cast<void>(Mixer::DispatchCommand(state, host, 0x10));
  host.Clear();
  return state;
}
}  // namespace

TEST_CASE("Track start commands run handler 0 for a missing sequence",
          "[audio-mixer-driver][audio]") {
  FakeDispatch host;
  Mixer::DispatchState state = RestartedTrack(host);
  // A first argument naming no sequence runs handler 0.
  host.first.reset();
  state.control_latches = 0x7701;
  CHECK(Mixer::DispatchCommand(state, host, 0x10) == kNoTrack);
  CHECK(host.calls == Log{"begin", "run", "end"});
  CHECK(host.handlers == Words{0});
  CHECK(state.control_latches == 0x7701);
  CHECK(host.refusals.empty());
}

TEST_CASE("Track start commands refuse a missing active layer",
          "[audio-mixer-driver][audio]") {
  FakeDispatch host;
  Mixer::DispatchState state = RestartedTrack(host);
  host.first.reset();
  state.control_latches = 0x7701;
  static_cast<void>(Mixer::DispatchCommand(state, host, 0x10));
  host.Clear();
  host.first = Mixer::AkaoHeader{9, 4};
  state.active.reset();
  CHECK(Mixer::DispatchCommand(state, host, 0x10) == kNoTrack);
  CHECK(host.handlers == Words{0});
  CHECK(state.control_latches == 0x7701);
  CHECK(host.refusals ==
        std::vector<CommandRefusal>{CommandRefusal::kActiveLayerMissing});
}

TEST_CASE("Queued track commands mark the layer and always run their handler",
          "[audio-mixer-driver][audio]") {
  FakeDispatch host;
  host.first = Mixer::AkaoHeader{9, 4};
  Mixer::DispatchState state = Dispatching();
  Present(state.active).status_flags = 0x1;
  CHECK(Mixer::DispatchCommand(state, host, 0x14) == 9);
  CHECK(Present(state.active).status_flags == (0x1 | Mixer::kMusicQueued));
  CHECK(host.calls == Log{"begin", "queue", "run", "end"});
  CHECK(host.handlers == Words{0x14});
  CHECK(state.control_latches == 0x7701);
  CHECK(host.refusals.empty());
}

TEST_CASE("Queued track commands run their handler without a sequence",
          "[audio-mixer-driver][audio]") {
  FakeDispatch host;
  host.first = Mixer::AkaoHeader{9, 4};
  Mixer::DispatchState state = Dispatching();
  Present(state.active).status_flags = 0x1;
  static_cast<void>(Mixer::DispatchCommand(state, host, 0x14));

  host.Clear();
  host.first.reset();
  CHECK(Mixer::DispatchCommand(state, host, 0x14) == kNoTrack);
  CHECK(host.calls == Log{"begin", "run", "end"});
  CHECK(host.handlers == Words{0x14});

  host.Clear();
  host.first = Mixer::AkaoHeader{9, 4};
  state.active.reset();
  CHECK(Mixer::DispatchCommand(state, host, 0x14) == kNoTrack);
  CHECK(host.handlers == Words{0});
  CHECK(host.refusals ==
        std::vector<CommandRefusal>{CommandRefusal::kActiveLayerMissing});
}

namespace {
// Track 3 plays four channels, two of them pushed to the saved layer.
Mixer::DispatchState PairedTracks(FakeDispatch& host) {
  host.first = Mixer::AkaoHeader{9, 4};
  host.second = Mixer::AkaoHeader{11, 6};
  Mixer::DispatchState state = Dispatching();
  Present(state.active).active_channels = 0xF;
  Present(state.active).push_release_mask = 0x3;
  return state;
}
}  // namespace

TEST_CASE("Track pair commands save the second track as the secondary layer",
          "[audio-mixer-driver][audio]") {
  FakeDispatch host;
  Mixer::DispatchState state = PairedTracks(host);
  CHECK(Mixer::DispatchCommand(state, host, 0x16) == 0);
  CHECK(host.calls == Log{"begin", "disable", "set", "enable", "load second",
                          "save", "replace", "load first", "attach", "run",
                          "end"});
  CHECK(host.replaced == Words{9});
  CHECK(host.handlers == Words{0});
  CHECK(host.mode == 6);
  CHECK(state.arguments.param2 == 11);
  CHECK(host.refusals.empty());
}

TEST_CASE("Track pair commands split the channels between the two layers",
          "[audio-mixer-driver][audio]") {
  FakeDispatch host;
  Mixer::DispatchState state = PairedTracks(host);
  static_cast<void>(Mixer::DispatchCommand(state, host, 0x16));
  REQUIRE(state.secondary.has_value());
  CHECK(Present(state.secondary).music_id == 11);
  CHECK(Present(state.secondary).active_channels == 0xC);
  CHECK(Present(state.active).music_id == 9);
  CHECK(Present(state.active).active_channels == 0xF);
  CHECK(state.control_latches == (0x1 | Mixer::kSecondarySilenced));
  CHECK(host.refusals.empty());
}

TEST_CASE("Track pair commands without a second sequence change nothing",
          "[audio-mixer-driver][audio]") {
  // A missing second sequence changes nothing but runs handler 0.
  FakeDispatch partial;
  partial.first = Mixer::AkaoHeader{9, 4};
  Mixer::DispatchState untouched = Dispatching();
  CHECK(Mixer::DispatchCommand(untouched, partial, 0x16) == 0);
  CHECK(partial.calls == Log{"begin", "run", "end"});
  CHECK(untouched == Dispatching());
}

TEST_CASE("Track pair commands refuse a missing active layer",
          "[audio-mixer-driver][audio]") {
  FakeDispatch orphan;
  orphan.first = Mixer::AkaoHeader{9, 4};
  orphan.second = Mixer::AkaoHeader{11, 6};
  Mixer::DispatchState silent = Dispatching();
  silent.active.reset();
  CHECK(Mixer::DispatchCommand(silent, orphan, 0x16) == 0);
  CHECK(orphan.calls == Log{"begin", "run", "end"});
  CHECK(silent.control_latches == 0x7701);
  CHECK(orphan.refusals ==
        std::vector<CommandRefusal>{CommandRefusal::kActiveLayerMissing});
}

TEST_CASE("Layer resume commands latch the pending resume of a silenced layer",
          "[audio-mixer-driver][audio]") {
  FakeDispatch host;
  Mixer::DispatchState state;
  const std::array<std::pair<std::uint32_t, std::uint32_t>, 4> latches{{
      {0x100, 0x4100},
      {0x1000, 0x1400},
      {0x1100, 0x5100},
      {0x1, 0x1},
  }};
  for (const auto& [before, after] : latches) {
    CAPTURE(before);
    state.control_latches = before;
    CHECK(Mixer::DispatchCommand(state, host, 0x17) == 0);
    CHECK(state.control_latches == after);
  }
  CHECK(host.handlers == Words{0, 0, 0, 0});
  CHECK(std::ranges::count(host.calls, std::string_view{"copy"}) == 0);
}

namespace {
// Replays earlier commands and clears the host's call log.
void Replay(Mixer::DispatchState& state, FakeDispatch& host,
            std::initializer_list<std::uint32_t> commands) {
  for (const std::uint32_t command : commands) {
    static_cast<void>(Mixer::DispatchCommand(state, host, command));
  }
  host.Clear();
}
}  // namespace

TEST_CASE("Paired and default commands run their handlers in order",
          "[audio-mixer-driver][audio]") {
  FakeDispatch host;
  Mixer::DispatchState state = Dispatching();
  CHECK(Mixer::DispatchCommand(state, host, 0xD8) == 0);
  CHECK(host.calls == Log{"begin", "copy", "run", "run", "end"});
  CHECK(host.handlers == Words{0xD0, 0xD4});
  CHECK(state.parameters == Mixer::CommandWords{});

  host.Clear();
  CHECK(Mixer::DispatchCommand(state, host, 0xD9) == 0);
  CHECK(host.handlers == Words{0xD1, 0xD5});
  CHECK(state.parameters == Mixer::CommandWords{0x21, 0, 0, 0, 0});
}

TEST_CASE("Paired ramp and pause commands run their handlers in order",
          "[audio-mixer-driver][audio]") {
  FakeDispatch host;
  Mixer::DispatchState state = Dispatching();
  Replay(state, host, {0xD8, 0xD9});
  CHECK(Mixer::DispatchCommand(state, host, 0xDA) == 0);
  CHECK(host.handlers == Words{0xD2, 0xD6});
  CHECK(state.parameters == Mixer::CommandWords{0x21, 0x22, 0, 0, 0});

  host.Clear();
  CHECK(Mixer::DispatchCommand(state, host, 0x99) == 0);
  CHECK(host.calls == Log{"begin", "run", "run", "run", "end"});
  CHECK(host.handlers == Words{0x9B, 0x9D, 0x9F});
}

TEST_CASE("Paired resume commands run their handlers in order",
          "[audio-mixer-driver][audio]") {
  FakeDispatch host;
  Mixer::DispatchState state = Dispatching();
  Replay(state, host, {0xD8, 0xD9, 0xDA, 0x99});
  CHECK(Mixer::DispatchCommand(state, host, 0x98) == 0);
  CHECK(host.handlers == Words{0x9A, 0x9C, 0x9E});
  CHECK(host.refusals.empty());
}

TEST_CASE("Default commands copy every argument to the parameters",
          "[audio-mixer-driver][audio]") {
  FakeDispatch host;
  Mixer::DispatchState state = Dispatching();
  Replay(state, host, {0xD8, 0xD9, 0xDA, 0x99, 0x98});
  // Any other command copies every argument to the parameters.
  CHECK(Mixer::DispatchCommand(state, host, 0x180) == 0);
  CHECK(host.calls == Log{"begin", "copy", "run", "end"});
  CHECK(host.handlers == Words{0x80});
  CHECK(state.parameters == kArguments);
  CHECK(state.control_latches == 0x7701);
  CHECK(host.refusals.empty());
}

TEST_CASE("Live SPU voice writes reach the legacy registers and refuse others",
          "[audio-mixer-driver][audio]") {
  constexpr std::size_t kHalves =
      LEGACY_SPU_REGISTER_BYTES / sizeof(std::uint16_t);
  const std::span registers(static_cast<std::uint16_t*>(LegacySpu_Registers()),
                            kHalves);
  const std::vector<std::uint16_t> saved(registers.begin(), registers.end());
  const std::uint64_t refused = Seq::LiveRefusalCount();
  // Addresses are stored in 8-byte units.
  Driver::LiveSetVoiceStartAddress(3, 0x12345);
  Driver::LiveSetVoiceLoopAddress(3, 0x8000);
  CHECK(registers[(3 * 8) + 3] == 0x2468);
  CHECK(registers[(3 * 8) + 7] == 0x1000);
  CHECK(Seq::LiveRefusalCount() == refused);

  // Voice 24's registers would be the SPU's main volume and reverb block.
  const std::vector<std::uint16_t> before(registers.begin(), registers.end());
  Driver::LiveSetVoiceStartAddress(24, 0x10);
  Driver::LiveSetVoiceLoopAddress(-1, 0x10);
  Driver::LiveSetVoiceAttackRateAndMode(24, 0x7F, 1);
  Driver::LiveSetVoiceDecayRate(24, 0xF);
  Driver::LiveSetVoiceSustainLevel(24, 0xF);
  Driver::LiveSetVoiceSustainRateAndMode(24, 0x7F, 3);
  CHECK(std::ranges::equal(registers, before));
  CHECK(Seq::LiveRefusalCount() == refused + 6);
  std::ranges::copy(saved, registers.begin());
}

TEST_CASE("Live sample transfers arm and complete the legacy transfer state",
          "[audio-mixer-driver][audio]") {
  const auto transfer = [] {
    return static_cast<std::int32_t>(Audio_Cutscene_legacySpuTransferBusy);
  };
  const std::int32_t saved = transfer();
  const std::uint64_t refused = Seq::LiveRefusalCount();
  Driver::LiveArmSampleTransferCallback();
  CHECK(transfer() == Driver::kTransferBusy);
  Driver::LiveCompleteSampleTransfer();
  CHECK(transfer() == Driver::kTransferIdle);
  Driver::LiveWaitForSampleTransfer();
  CHECK(Seq::LiveRefusalCount() == refused);

  // A transfer without a buffer is refused before the SPU is touched.
  Driver::LiveWriteSampleMemory(nullptr, 4);
  Driver::LiveReadSampleMemory(nullptr, 4);
  CHECK(Seq::LiveRefusalCount() == refused + 2);
  CHECK(transfer() == Driver::kTransferIdle);
  Audio_Cutscene_legacySpuTransferBusy = saved;
}

TEST_CASE("Live mixer commands refuse missing parameters",
          "[audio-mixer-driver][audio]") {
  const std::array<Audio_CommandHandler, 13> handlers{
      &Mixer::LiveSetSoundEffectPitchScale,
      &Mixer::LiveSlideSoundEffectPitchScale,
      &Mixer::LiveSetWorldSoundEffectPitchScale,
      &Mixer::LiveSlideWorldSoundEffectPitchScale,
      &Mixer::LiveSetTempoScale,
      &Mixer::LiveSlideTempoScale,
      &Mixer::LiveSetTempoScaleRamp,
      &Mixer::LiveSetMusicPitchScale,
      &Mixer::LiveSlideMusicPitchScale,
      &Mixer::LiveSetMusicPitchScaleRamp,
      &Mixer::LiveStopMatchingMusic,
      &Mixer::LiveSetMusicMuteMask,
      &Mixer::LiveSetMusicJumpThreshold,
  };
  const std::uint64_t refused = Seq::LiveRefusalCount();
  for (const Audio_CommandHandler handler : handlers) {
    handler(nullptr);
  }
  Mixer::LiveIgnoreCommand(nullptr);
  CHECK(Seq::LiveRefusalCount() == refused + handlers.size());
}

namespace {
// Restores the legacy tempo and music pitch slides a test changes.
struct LiveSlideFixture {
  std::array<int, 4> values{Audio_Mixer_tempoScale, Audio_Mixer_tempoScaleStep,
                            Audio_Mixer_musicPitchScale,
                            Audio_Mixer_musicPitchScaleStep};
  std::array<std::int16_t, 2> lengths{Audio_Mixer_tempoScaleSlideLength,
                                      Audio_Mixer_musicPitchScaleSlideLength};

  LiveSlideFixture() = default;
  ~LiveSlideFixture() {
    Audio_Mixer_tempoScale = values[0];
    Audio_Mixer_tempoScaleStep = values[1];
    Audio_Mixer_musicPitchScale = values[2];
    Audio_Mixer_musicPitchScaleStep = values[3];
    Audio_Mixer_tempoScaleSlideLength = lengths[0];
    Audio_Mixer_musicPitchScaleSlideLength = lengths[1];
  }

  LiveSlideFixture(const LiveSlideFixture&) = delete;
  LiveSlideFixture& operator=(const LiveSlideFixture&) = delete;
};
}  // namespace

TEST_CASE("Live tempo and music pitch commands update the legacy slides",
          "[audio-mixer-driver][audio]") {
  const LiveSlideFixture fixture;
  const std::uint64_t refused = Seq::LiveRefusalCount();
  Audio_CommandParams params{};
  // Only the low byte of the first word is read, signed.
  params.param1 = 0x1FE;
  Mixer::LiveSetTempoScale(&params);
  CHECK(Audio_Mixer_tempoScale == -0x20000);
  CHECK(Audio_Mixer_tempoScaleSlideLength == 0);
  params.param1 = 4;
  params.param2 = 2;
  Mixer::LiveSlideTempoScale(&params);
  CHECK(Audio_Mixer_tempoScale == -0x20000);
  CHECK(Audio_Mixer_tempoScaleStep == 0x10000);
  CHECK(Audio_Mixer_tempoScaleSlideLength == 4);
  CHECK(Seq::LiveRefusalCount() == refused);
}

TEST_CASE("Live music pitch commands update the legacy pitch slide",
          "[audio-mixer-driver][audio]") {
  const LiveSlideFixture fixture;
  const std::uint64_t refused = Seq::LiveRefusalCount();
  Audio_CommandParams params{};
  params.param1 = 3;
  Mixer::LiveSetMusicPitchScale(&params);
  CHECK(Audio_Mixer_musicPitchScale == 0x30000);
  params = {};
  params.param2 = 0x10;
  params.param3 = 0x20;
  Mixer::LiveSetMusicPitchScaleRamp(&params);
  CHECK(Audio_Mixer_musicPitchScale == 0x100000);
  CHECK(Audio_Mixer_musicPitchScaleStep == 0x100000);
  CHECK(Audio_Mixer_musicPitchScaleSlideLength == 0);
  CHECK(Seq::LiveRefusalCount() == refused + 1);
}

namespace {
// Restores the music layer pointers a test rebinds.
struct LiveLayerFixture {
  Audio_MusicConfig* active = Audio_Seq_activeMusicConfig;
  Audio_MusicConfig* secondary = Audio_Seq_secondaryMusicConfig;
  Audio_SeqChannel* channels = Audio_Seq_secondaryMusicChannels;

  LiveLayerFixture() = default;
  ~LiveLayerFixture() {
    Audio_Seq_activeMusicConfig = active;
    Audio_Seq_secondaryMusicConfig = secondary;
    Audio_Seq_secondaryMusicChannels = channels;
  }

  LiveLayerFixture(const LiveLayerFixture&) = delete;
  LiveLayerFixture& operator=(const LiveLayerFixture&) = delete;
};
}  // namespace

TEST_CASE("Live mixer passes refuse missing channels and unbound layers",
          "[audio-mixer-driver][audio]") {
  const LiveLayerFixture fixture;
  const std::uint64_t refused = Seq::LiveRefusalCount();
  Audio_SeqChannel channel{};
  Mixer::LiveUpdateChannelSlidesAndDelays(nullptr, 1, ChannelKind::Music);
  Mixer::LiveUpdateMusicChannelModulation(nullptr);
  Mixer::LiveUpdateSoundEffectModulation(nullptr);
  CHECK(Seq::LiveRefusalCount() == refused + 3);

  Audio_Seq_activeMusicConfig = nullptr;
  Mixer::LiveUpdateChannelSlidesAndDelays(&channel, 1, ChannelKind::Music);
  Mixer::LiveUpdateMusicChannelModulation(&channel);
  Mixer::LiveCommitPlaybackVoices();
  Mixer::LiveReleasePendingVoices();
  CHECK(Seq::LiveRefusalCount() == refused + 7);

  // A secondary layer must be a record of its own with channels of its own.
  Audio_MusicConfig other{};
  Audio_MusicConfig stray{};
  std::vector<Audio_SeqChannel> own(Seq::kMusicChannelCount);
  Audio_Seq_activeMusicConfig = &Audio_Seq_primaryMusicConfig;
  Audio_Seq_secondaryMusicConfig = &Audio_Seq_primaryMusicConfig;
  Audio_Seq_secondaryMusicChannels = own.data();
  Mixer::LiveCommitPlaybackVoices();
  Audio_Seq_secondaryMusicConfig = &other;
  Audio_Seq_secondaryMusicChannels = nullptr;
  Mixer::LiveCommitPlaybackVoices();
  Audio_Seq_secondaryMusicChannels = std::data(Audio_Seq_musicChannels);
  Mixer::LiveReleasePendingVoices();
  // The active layer must be one of the two.
  Audio_Seq_secondaryMusicChannels = own.data();
  Audio_Seq_activeMusicConfig = &stray;
  Mixer::LiveReleasePendingVoices();
  CHECK(Seq::LiveRefusalCount() == refused + 11);
  CHECK(Audio_Seq_activeMusicConfig == &stray);
}

TEST_CASE(
    "Mixer commands keep their live bindings and retired helpers keep none",
    "[audio-mixer-driver][audio]") {
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  const std::array<std::pair<std::uint32_t, Audio_CommandHandler>, 22> kept{{
      {0x80017FB0U, &Mixer::LiveSetSoundEffectPitchScale},
      {0x80018090U, &Mixer::LiveSlideSoundEffectPitchScale},
      {0x80018218U, &Mixer::LiveSetWorldSoundEffectPitchScale},
      {0x80018270U, &Mixer::LiveSlideWorldSoundEffectPitchScale},
      {0x8001833CU, &Mixer::LiveSetTempoScale},
      {0x80018358U, &Mixer::LiveSlideTempoScale},
      {0x800183C0U, &Mixer::LiveSetTempoScaleRamp},
      {0x80018434U, &Mixer::LiveSetMusicPitchScale},
      {0x80018450U, &Mixer::LiveSlideMusicPitchScale},
      {0x800184B8U, &Mixer::LiveSetMusicPitchScaleRamp},
      {0x8001852CU, &Mixer::LiveStopMusicAndCancelReplacement},
      {0x800185A0U, &Mixer::LiveStopMatchingMusic},
      {0x80018608U, &Mixer::LiveStopWorldSoundEffects},
      {0x800186E4U, &Mixer::LiveEnableStereoOutput},
      {0x80018744U, &Mixer::LiveEnableMonoOutput},
      {0x800187A4U, &Mixer::LiveSetMusicMuteMask},
      {0x800187E4U, &Mixer::LiveSetMusicJumpThreshold},
      {0x800187F8U, &Mixer::LivePauseMusic},
      {0x800188E8U, &Mixer::LiveResumeMusic},
      {0x80018984U, &Mixer::LivePauseWorldSoundEffects},
      {0x80018AA0U, &Mixer::LiveResumeWorldSoundEffects},
      {0x80018BD8U, &Mixer::LiveIgnoreCommand},
  }};
  for (const auto& [address, handler] : kept) {
    CAPTURE(address);
    CHECK(VS::Content::ResolveModuleAddress(symbols, "SLUS_010.40", address) ==
          reinterpret_cast<void*>(handler));
  }
  // The sound driver, the voice mixer, Sound_SetReverbMode and the
  // dispatcher are reached only through direct calls.
  const std::array<std::uint32_t, 34> retired{
      0x8001347CU, 0x800134A0U, 0x800134D0U, 0x80013520U, 0x8001355CU,
      0x80013938U, 0x80013A28U, 0x80013BB8U, 0x80013BD4U, 0x80013BF0U,
      0x80013C08U, 0x80013C20U, 0x80013C50U, 0x80013C78U, 0x80013CA0U,
      0x80013D04U, 0x80013DB0U, 0x80013ECCU, 0x8001436CU, 0x800147CCU,
      0x80014C58U, 0x80014D08U, 0x80014D70U, 0x80014FBCU, 0x8001503CU,
      0x80015080U, 0x800151C0U, 0x80015210U, 0x80015220U, 0x80015764U,
      0x800157C4U, 0x80015970U, 0x80018BE0U, 0x80018C30U,
  };
  for (const std::uint32_t address : retired) {
    CAPTURE(address);
    CHECK(VS::Content::ResolveModuleAddress(symbols, "SLUS_010.40", address) ==
          nullptr);
  }
}
