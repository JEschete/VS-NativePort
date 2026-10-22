#pragma once

#include <array>
#include <expected>
#include <filesystem>
#include <memory>
#include <span>
#include <string>

#include "Audio/Reverb.hpp"
#include "Core/Types.hpp"

namespace VS::Audio {

struct EnvelopeRate {
  int shift = 0;
  int step = 0;
  bool exponential = false;
  bool decreasing = false;
  bool hold = false;
};

struct EnvelopeDefinition {
  EnvelopeRate attack;
  EnvelopeRate decay;
  EnvelopeRate sustain;
  EnvelopeRate release;
  int sustain_level = 0;
};

EnvelopeDefinition DecodeAkaoEnvelope(u16 lower, u16 upper);

class VoiceEnvelope {
 public:
  void Start(const EnvelopeDefinition& definition);
  void Configure(const EnvelopeDefinition& definition) {
    m_definition_ = definition;
  }
  void Release();
  float Advance();
  int Level() const { return m_level_; }
  bool Finished() const { return m_phase_ == Phase::kDone; }

 private:
  enum class Phase { kAttack, kDecay, kSustain, kRelease, kDone };
  void Tick();
  EnvelopeDefinition m_definition_;
  Phase m_phase_ = Phase::kDone;
  int m_level_ = 0;
  int m_counter_ = 0;
  int m_clock_ = 0;
};

struct VoiceControls {
  std::filesystem::path instrument;
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

// The existing sequencer owns allocation. This engine owns sample playback
// and mixing, not a second event sequencer or a register/sample-memory model.
// Controls have one producer; Render has one consumer. Stop the consumer
// before destruction. Programs are loaded and retired on the producer.
class VoiceEngine {
 public:
  static constexpr usize kVoices = 24;
  static constexpr usize kFramesPerTick = 200;
  static constexpr int kSampleRate = 48000;

  VoiceEngine();
  ~VoiceEngine();
  VoiceEngine(const VoiceEngine&) = delete;
  VoiceEngine& operator=(const VoiceEngine&) = delete;
  void Configure(usize voice, const VoiceControls& controls);
  std::expected<void, std::string> NoteOn(usize voice);
  void NoteOff(usize voice);
  void Stop(usize voice);
  void SetRelease(usize voice, const EnvelopeRate& release);
  void SetNoiseClock(unsigned clock);
  void SetMonoOutput(bool mono);
  void SetReverb(const ReverbControls& controls, bool clear = false);
  void Reset();
  // Publish a complete control tick; Render never observes a partial batch.
  void CommitTick();
  void Render(std::span<float, kFramesPerTick * 2> stereo);
  int EnvelopeLevel(usize voice) const;
  bool IsActive(usize voice) const;

 private:
  struct State;
  std::unique_ptr<State> m_state_;
};

}  // namespace VS::Audio
