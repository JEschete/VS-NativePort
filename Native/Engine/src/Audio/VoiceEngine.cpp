#include "Audio/VoiceEngine.hpp"

#include <sfizz.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <memory>
#include <string>
#include <utility>

#include "Core/Log.hpp"
namespace VS::Audio {
namespace {
constexpr int kEnvelopeMaximum = 32767;
constexpr int kEnvelopeClock = 44100;
constexpr int kCounterUnit = 32768;
constexpr float kOutputMaximum = 1.0F - 1.0F / kCounterUnit;
constexpr int kFastShift = 11;
constexpr int kExponentialThreshold = 24576;
constexpr float kBendRangeCents = 14400;
constexpr float kCentsPerOctave = 1200;
constexpr int kSampleKey = 60;
constexpr unsigned kPreloadFrames = 262144;
constexpr int kMaximumVelocity = 127;
constexpr int kMaximumStep = 7;
constexpr int kEnvelopeFractionBits = 15;
constexpr int kExponentialSlowdown = 4;

EnvelopeRate Rate(int shift, int step, bool exponential, bool decreasing,
                  bool hold = false) {
  return {shift, step, exponential, decreasing, hold};
}

void SetPitch(sfizz_synth_t* synth, float rate, int delay = 0) {
  if (rate > 0) {
    const float bend = kCentsPerOctave * std::log2(rate) / kBendRangeCents;
    sfizz_send_hd_pitch_wheel(synth, delay, std::clamp(bend, -1.0F, 1.0F));
  }
}
}  // namespace

// ADSR packing: SetVoiceAdsr* setters in SLUS_010.40/25AC.c:1262-1313.
EnvelopeDefinition DecodeAkaoEnvelope(u16 lower, u16 upper) {
  constexpr int kShiftMask = 31;
  constexpr int kStepMask = 3;
  constexpr int kLevelUnit = 2048;
  constexpr int kExponentialMode = 0x8000;
  constexpr int kSustainDecrease = 0x4000;
  constexpr int kReleaseExponential = 0x20;
  constexpr int kDecayShiftOffset = 4;
  constexpr int kLevelMask = 15;
  const int attack_shift = (lower >> 10) & kShiftMask;
  const int attack_step = (lower >> 8) & kStepMask;
  const int sustain_shift = (upper >> 8) & kShiftMask;
  const int sustain_step = (upper >> 6) & kStepMask;
  const int release_shift = upper & kShiftMask;
  return {Rate(attack_shift, attack_step, (lower & kExponentialMode) != 0,
               false, attack_shift == kShiftMask && attack_step == kStepMask),
          Rate((lower >> kDecayShiftOffset) & kLevelMask, 0, true, true),
          Rate(sustain_shift, sustain_step, (upper & kExponentialMode) != 0,
               (upper & kSustainDecrease) != 0,
               sustain_shift == kShiftMask && sustain_step == kStepMask),
          Rate(release_shift, 0, (upper & kReleaseExponential) != 0, true,
               release_shift == kShiftMask),
          std::min(((lower & kLevelMask) + 1) * kLevelUnit, kEnvelopeMaximum)};
}

void VoiceEnvelope::Start(const EnvelopeDefinition& definition) {
  m_definition_ = definition;
  m_phase_ = Phase::kAttack;
  m_level_ = 0;
  m_counter_ = 0;
  m_clock_ = 0;
}

void VoiceEnvelope::Release() {
  if (m_phase_ != Phase::kDone && m_phase_ != Phase::kRelease) {
    m_phase_ = Phase::kRelease;
    m_counter_ = 0;
  }
}

void VoiceEnvelope::Tick() {
  if (m_phase_ == Phase::kDone) {
    return;
  }
  const EnvelopeRate* rate = &m_definition_.attack;
  switch (m_phase_) {
    case Phase::kDecay:
      rate = &m_definition_.decay;
      break;
    case Phase::kSustain:
      rate = &m_definition_.sustain;
      break;
    case Phase::kRelease:
      rate = &m_definition_.release;
      break;
    default:
      break;
  }
  if (rate->hold) {
    return;
  }
  int step = kMaximumStep - rate->step;
  if (rate->decreasing) {
    step = -step - 1;
  }
  step *= 1 << std::max(0, kFastShift - rate->shift);
  int increment = kCounterUnit >> std::max(0, rate->shift - kFastShift);
  if (rate->exponential) {
    if (rate->decreasing) {
      step = (step * m_level_) >> kEnvelopeFractionBits;
    } else if (m_level_ > kExponentialThreshold) {
      if (rate->shift < kFastShift - 1) {
        step /= kExponentialSlowdown;
      } else if (rate->shift >= kFastShift) {
        increment /= kExponentialSlowdown;
      } else {
        step /= 2;
        increment /= 2;
      }
    }
  }
  m_counter_ += std::max(increment, 1);
  if (m_counter_ >= kCounterUnit) {
    m_counter_ = 0;
    m_level_ = std::clamp(m_level_ + step, 0, kEnvelopeMaximum);
  }
  if (m_phase_ == Phase::kAttack && m_level_ == kEnvelopeMaximum) {
    m_phase_ = Phase::kDecay;
  } else if (m_phase_ == Phase::kDecay &&
             m_level_ <= m_definition_.sustain_level) {
    m_phase_ = Phase::kSustain;
  } else if (m_phase_ == Phase::kRelease && m_level_ == 0) {
    m_phase_ = Phase::kDone;
  }
}

float VoiceEnvelope::Advance() {
  m_clock_ += kEnvelopeClock;
  while (m_clock_ >= VoiceEngine::kSampleRate) {
    m_clock_ -= VoiceEngine::kSampleRate;
    Tick();
  }
  return static_cast<float>(m_level_) / kCounterUnit;
}

namespace {
using Synth = std::unique_ptr<sfizz_synth_t, decltype(&sfizz_free)>;
constexpr usize kCommandCapacity = 4096;
constexpr unsigned kReportLevelBits = 16;
constexpr u64 kReportActiveBit = 0x8000;
constexpr u64 kReportLevelMask = kReportActiveBit - 1;
static_assert(std::atomic<usize>::is_always_lock_free);
static_assert(std::atomic<u64>::is_always_lock_free);

struct MixControls {
  float rate = 1;
  float left = 1;
  float right = 1;
  EnvelopeDefinition envelope;
  bool noise = false;
  bool frequency_modulation = false;
  bool reverb = false;
  bool silent = false;
  bool paused = false;
};

MixControls Mix(const VoiceControls& controls) {
  return {controls.rate,     controls.left,   controls.right,
          controls.envelope, controls.noise,  controls.frequency_modulation,
          controls.reverb,   controls.silent, controls.paused};
}

// Noise and preceding-voice pitch rules: PSX-SPX, Sound Processing Unit.
struct Noise {
  unsigned clock = 0;
  unsigned phase = 0;
  int timer = 0;
  u16 level = 0;

  float Advance() {
    phase += kEnvelopeClock;
    if (phase >= VoiceEngine::kSampleRate) {
      phase -= VoiceEngine::kSampleRate;
      const int step = 4 + static_cast<int>(clock & 3);
      timer -= step;
      if (timer < 0) {
        const unsigned parity = ((level >> 15) ^ (level >> 12) ^ (level >> 11) ^
                                 (level >> 10) ^ 1) &
                                1;
        level = static_cast<u16>((level << 1) | parity);
        const int period = 0x20000 >> (clock >> 2);
        timer += period;
        if (timer < 0) timer += period;
      }
    }
    return static_cast<float>(std::bit_cast<i16>(level)) / kCounterUnit;
  }
};

float ModulatedRate(float rate, float previous) {
  constexpr int kUnityPitch = 4096;
  constexpr int kMaximumPitch = 16384;
  const auto amplitude = std::clamp(
      static_cast<int>(std::clamp(previous, -1.0F, 1.0F) * kCounterUnit),
      -kCounterUnit, kEnvelopeMaximum);
  const auto pitch = static_cast<int>(rate * kUnityPitch);
  return static_cast<float>(
             std::clamp((static_cast<i64>(pitch) * (amplitude + kCounterUnit)) /
                            kCounterUnit,
                        i64{0}, i64{kMaximumPitch})) /
         kUnityPitch;
}

std::expected<Synth, std::string> LoadProgram(const std::filesystem::path& path,
                                              usize voice) {
  const auto started = std::chrono::steady_clock::now();
  Synth synth{sfizz_create_synth(), sfizz_free};
  if (!synth) return std::unexpected("sfizz_create_synth failed");
  sfizz_set_sample_rate(synth.get(), VoiceEngine::kSampleRate);
  sfizz_set_samples_per_block(synth.get(), VoiceEngine::kFramesPerTick);
  sfizz_set_num_voices(synth.get(), 1);
  sfizz_set_preload_size(synth.get(), kPreloadFrames);
  sfizz_set_volume(synth.get(), 0);
  if (!sfizz_load_file(synth.get(), path.string().c_str())) {
    return std::unexpected("sfizz cannot load " + path.string());
  }
  VS_LOG_DEBUG("audio", "instrument load: voice={} duration_ms={} path={}",
               voice,
               std::chrono::duration<double, std::milli>(
                   std::chrono::steady_clock::now() - started)
                   .count(),
               path.string());
  return synth;
}
}  // namespace

struct VoiceEngine::State {
  enum class Kind {
    kConfigure,
    kOn,
    kOff,
    kStop,
    kNoise,
    kReverb,
    kMono,
    kTick
  };
  struct Command {
    Kind kind = Kind::kTick;
    usize voice = 0;
    u64 epoch = 0;
    u64 generation = 0;
    MixControls controls;
    bool replace_program = false;
    unsigned noise_clock = 0;
    ReverbControls reverb;
    bool clear_reverb = false;
    bool mono = false;
    Synth synth{nullptr, sfizz_free};
  };
  struct Pending {
    VoiceControls controls;
    std::filesystem::path loaded;
    VoiceEnvelope envelope;
    u64 generation = 0;
    bool active = false;
  };
  struct Voice {
    Synth synth{nullptr, sfizz_free};
    MixControls controls;
    VoiceEnvelope envelope;
    u64 generation = 0;
    bool active = false;
    float held_left = 0;
    float held_right = 0;
  };

  std::array<Pending, kVoices> pending;
  std::array<Voice, kVoices> voices;
  std::array<std::atomic<u64>, kVoices> reports{};
  std::array<Command, kCommandCapacity> commands;
  std::atomic<usize> written{0};
  std::atomic<usize> read{0};
  usize next = 0;
  usize reclaimed = 0;
  std::atomic<u64> epoch{1};
  u64 rendering_epoch = 0;
  u64 generation = 0;
  Noise noise;
  Reverb reverb;
  bool mono = false;

  static void RenderSamples(Voice& voice,
                            const std::array<float, kFramesPerTick>& previous,
                            std::array<float, kFramesPerTick>& left,
                            std::array<float, kFramesPerTick>& right,
                            bool modulated) {
    if (!voice.synth || voice.controls.silent) {
      left.fill(0);
      right.fill(0);
      return;
    }
    std::array<bool, kFramesPerTick> advancing{};
    int count = 0;
    for (usize frame = 0; frame < kFramesPerTick; ++frame) {
      const auto rate =
          modulated ? ModulatedRate(voice.controls.rate, previous[frame])
                    : voice.controls.rate;
      if (rate <= 0) continue;
      if (modulated || count == 0) SetPitch(voice.synth.get(), rate, count);
      advancing[frame] = true;
      ++count;
    }
    std::array<float, kFramesPerTick> source_left{};
    std::array<float, kFramesPerTick> source_right{};
    std::array<float*, 2> channels{source_left.data(), source_right.data()};
    if (count != 0) {
      sfizz_render_block(voice.synth.get(), channels.data(), 2, count);
    }
    usize sample = 0;
    for (usize frame = 0; frame < kFramesPerTick; ++frame) {
      if (advancing[frame]) {
        voice.held_left = source_left[sample];
        voice.held_right = source_right[sample++];
      }
      left[frame] = voice.held_left;
      right[frame] = voice.held_right;
    }
  }

  void Push(Command command) {
    const auto consumed = read.load(std::memory_order_acquire);
    while (reclaimed != consumed) {
      commands[reclaimed++ % commands.size()].synth.reset();
    }
    if (next - consumed == commands.size()) {
      VS_LOG_ERROR("audio", "native voice command queue exhausted");
      std::abort();
    }
    command.epoch = epoch.load(std::memory_order_relaxed);
    // Consumed cells return retired programs to the control thread.
    commands[next % commands.size()] = std::move(command);
    ++next;
  }

  void Apply(Command& command) {
    auto& voice = voices[command.voice];
    switch (command.kind) {
      case Kind::kConfigure:
        voice.controls = command.controls;
        voice.envelope.Configure(voice.controls.envelope);
        if (voice.active && voice.synth) {
          SetPitch(voice.synth.get(), voice.controls.rate);
        }
        break;
      case Kind::kOn:
        if (command.replace_program) voice.synth.swap(command.synth);
        if (voice.synth) {
          sfizz_all_sound_off(voice.synth.get());
          SetPitch(voice.synth.get(), voice.controls.rate);
          sfizz_send_note_on(voice.synth.get(), 0, kSampleKey,
                             kMaximumVelocity);
        }
        voice.held_left = voice.held_right = 0;
        voice.envelope.Start(voice.controls.envelope);
        voice.generation = command.generation;
        voice.active = true;
        break;
      case Kind::kOff:
        voice.envelope.Release();
        break;
      case Kind::kStop:
        if (voice.synth) sfizz_all_sound_off(voice.synth.get());
        voice.envelope = {};
        voice.active = false;
        break;
      case Kind::kNoise:
        noise.clock = command.noise_clock;
        break;
      case Kind::kReverb:
        reverb.Configure(command.reverb, command.clear_reverb);
        break;
      case Kind::kMono:
        mono = command.mono;
        break;
      case Kind::kTick:
        break;
    }
  }

  void Consume() {
    const auto current_epoch = epoch.load(std::memory_order_acquire);
    if (rendering_epoch != current_epoch) {
      for (auto& voice : voices) {
        if (voice.synth) sfizz_all_sound_off(voice.synth.get());
        voice.active = false;
        voice.envelope = {};
      }
      rendering_epoch = current_epoch;
      noise = {};
      reverb.Configure({}, true);
      mono = false;
    }
    auto cursor = read.load(std::memory_order_relaxed);
    const auto end = written.load(std::memory_order_acquire);
    while (cursor != end) {
      auto& command = commands[cursor % commands.size()];
      if (command.epoch > rendering_epoch) break;
      const bool current = command.epoch == rendering_epoch;
      const bool boundary = current && command.kind == Kind::kTick;
      if (current && !boundary) Apply(command);
      read.store(++cursor, std::memory_order_release);
      if (boundary) break;
    }
  }
};

VoiceEngine::VoiceEngine() : m_state_(std::make_unique<State>()) {}
VoiceEngine::~VoiceEngine() = default;

void VoiceEngine::Configure(usize index, const VoiceControls& controls) {
  auto& pending = m_state_->pending.at(index);
  pending.controls = controls;
  pending.envelope.Configure(controls.envelope);
  State::Command command;
  command.kind = State::Kind::kConfigure;
  command.voice = index;
  command.controls = Mix(controls);
  m_state_->Push(std::move(command));
}

std::expected<void, std::string> VoiceEngine::NoteOn(usize index) {
  auto& pending = m_state_->pending.at(index);
  Stop(index);
  if (pending.controls.instrument.empty() && !pending.controls.noise &&
      !pending.controls.silent) {
    return std::unexpected("voice has no imported instrument");
  }
  State::Command command;
  command.kind = State::Kind::kOn;
  command.voice = index;
  if (pending.loaded != pending.controls.instrument) {
    if (!pending.controls.instrument.empty()) {
      auto synth = LoadProgram(pending.controls.instrument, index);
      if (!synth) {
        pending.loaded.clear();
        return std::unexpected(synth.error());
      }
      command.synth = std::move(*synth);
    }
    command.replace_program = true;
    pending.loaded = pending.controls.instrument;
  }
  pending.generation = ++m_state_->generation;
  command.generation = pending.generation;
  pending.envelope.Start(pending.controls.envelope);
  pending.active = true;
  m_state_->Push(std::move(command));
  return {};
}

void VoiceEngine::NoteOff(usize index) {
  m_state_->pending.at(index).envelope.Release();
  State::Command command;
  command.kind = State::Kind::kOff;
  command.voice = index;
  m_state_->Push(std::move(command));
}

void VoiceEngine::Stop(usize index) {
  auto& pending = m_state_->pending.at(index);
  pending.active = false;
  pending.envelope = {};
  State::Command command;
  command.kind = State::Kind::kStop;
  command.voice = index;
  m_state_->Push(std::move(command));
}

void VoiceEngine::SetRelease(usize index, const EnvelopeRate& release) {
  auto controls = m_state_->pending.at(index).controls;
  controls.envelope.release = release;
  Configure(index, controls);
}

void VoiceEngine::SetNoiseClock(unsigned clock) {
  constexpr unsigned kMaximumClock = 63;
  if (clock > kMaximumClock) {
    VS_LOG_ERROR("audio", "invalid native noise clock {}", clock);
    std::abort();
  }
  State::Command command;
  command.kind = State::Kind::kNoise;
  command.noise_clock = clock;
  m_state_->Push(std::move(command));
}

void VoiceEngine::SetReverb(const ReverbControls& controls, bool clear) {
  if (controls.preset < ReverbPreset::kOff ||
      controls.preset > ReverbPreset::kPipe || !std::isfinite(controls.left) ||
      !std::isfinite(controls.right)) {
    VS_LOG_ERROR("audio", "invalid native reverb controls");
    std::abort();
  }
  State::Command command;
  command.kind = State::Kind::kReverb;
  command.reverb = controls;
  command.clear_reverb = clear;
  m_state_->Push(std::move(command));
}

void VoiceEngine::SetMonoOutput(bool mono) {
  State::Command command;
  command.kind = State::Kind::kMono;
  command.mono = mono;
  m_state_->Push(std::move(command));
}

void VoiceEngine::Reset() {
  for (auto& pending : m_state_->pending) pending = {};
  const auto published = m_state_->written.load(std::memory_order_relaxed);
  while (m_state_->next != published) {
    m_state_->commands[--m_state_->next % kCommandCapacity].synth.reset();
  }
  m_state_->epoch.fetch_add(1, std::memory_order_release);
}

void VoiceEngine::CommitTick() {
  for (auto& pending : m_state_->pending) {
    if (!pending.active || pending.controls.paused) continue;
    for (usize frame = 0; frame < kFramesPerTick; ++frame) {
      pending.envelope.Advance();
    }
  }
  m_state_->Push(State::Command{});
  m_state_->written.store(m_state_->next, std::memory_order_release);
}

void VoiceEngine::Render(std::span<float, kFramesPerTick * 2> stereo) {
  m_state_->Consume();
  std::ranges::fill(stereo, 0);
  std::array<float, kFramesPerTick> left{};
  std::array<float, kFramesPerTick> right{};
  std::array<float, kFramesPerTick> previous{};
  std::array<float, kFramesPerTick> noise{};
  std::array<StereoSample, kFramesPerTick> sends{};
  for (auto& sample : noise) sample = m_state_->noise.Advance();
  for (usize index = 0; index < kVoices; ++index) {
    auto& voice = m_state_->voices[index];
    if (!voice.active || voice.controls.paused) {
      previous.fill(0);
      continue;
    }
    State::RenderSamples(voice, previous, left, right,
                         index != 0 && voice.controls.frequency_modulation &&
                             !voice.controls.noise);
    for (usize frame = 0; frame < kFramesPerTick; ++frame) {
      const float gain = voice.envelope.Advance();
      if (voice.controls.noise) left[frame] = right[frame] = noise[frame];
      previous[frame] = (left[frame] + right[frame]) * gain / 2;
      const float mixed_left = left[frame] * gain * voice.controls.left;
      const float mixed_right = right[frame] * gain * voice.controls.right;
      stereo[frame * 2] += mixed_left;
      stereo[frame * 2 + 1] += mixed_right;
      if (voice.controls.reverb) {
        sends[frame].left += mixed_left;
        sends[frame].right += mixed_right;
      }
    }
    if (voice.envelope.Finished() ||
        (voice.synth && !voice.controls.noise && !voice.controls.silent &&
         sfizz_get_num_active_voices(voice.synth.get()) == 0)) {
      if (voice.synth) sfizz_all_sound_off(voice.synth.get());
      voice.active = false;
    }
  }
  for (usize frame = 0; frame < kFramesPerTick; ++frame) {
    const auto wet = m_state_->reverb.Process(sends[frame]);
    stereo[frame * 2] += wet.left;
    stereo[frame * 2 + 1] += wet.right;
    if (m_state_->mono) {
      const auto combined = (stereo[frame * 2] + stereo[frame * 2 + 1]) / 2;
      stereo[frame * 2] = stereo[frame * 2 + 1] = combined;
    }
    // The original SPU saturates the final dry+wet mix before main volume.
    stereo[frame * 2] = std::clamp(stereo[frame * 2], -1.0F, kOutputMaximum);
    stereo[frame * 2 + 1] =
        std::clamp(stereo[frame * 2 + 1], -1.0F, kOutputMaximum);
  }
  for (usize index = 0; index < kVoices; ++index) {
    const auto& voice = m_state_->voices[index];
    const auto level = voice.active ? voice.envelope.Level() : 0;
    m_state_->reports[index].store((voice.generation << kReportLevelBits) |
                                       static_cast<u64>(level) |
                                       (voice.active ? kReportActiveBit : 0),
                                   std::memory_order_release);
  }
}

int VoiceEngine::EnvelopeLevel(usize index) const {
  const auto& pending = m_state_->pending.at(index);
  if (!pending.active) return 0;
  const auto report = m_state_->reports[index].load(std::memory_order_acquire);
  if ((report >> kReportLevelBits) != pending.generation) {
    return pending.envelope.Level();
  }
  return static_cast<int>(report & kReportLevelMask);
}

bool VoiceEngine::IsActive(usize index) const {
  const auto& pending = m_state_->pending.at(index);
  if (!pending.active) return false;
  const auto report = m_state_->reports[index].load(std::memory_order_acquire);
  return (report >> kReportLevelBits) != pending.generation ||
         (report & kReportActiveBit) != 0;
}
}  // namespace VS::Audio
