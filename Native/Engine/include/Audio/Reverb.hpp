#pragma once

#include <array>

#include "Core/Types.hpp"

namespace VS::Audio {

enum class ReverbPreset {
  kOff,
  kRoom,
  kStudioSmall,
  kStudioMedium,
  kStudioLarge,
  kHall,
  kSpace,
  kEcho,
  kDelay,
  kPipe
};

struct ReverbControls {
  ReverbPreset preset = ReverbPreset::kOff;
  bool enabled = false;
  float left = 0;
  float right = 0;
};

struct StereoSample {
  float left = 0;
  float right = 0;
};

// Native float delay network, running at the original 22050-Hz effect rate.
// Its state is independent of sample memory, voice programs and SDK registers.
class Reverb {
 public:
  void Configure(const ReverbControls& controls, bool clear);
  StereoSample Process(StereoSample input);

 private:
  static constexpr usize kMaximumDelay = 0x18040 / 2;
  static constexpr usize kFilterTaps = 39;
  float Read(int offset) const;
  void Write(int offset, float sample);
  StereoSample Network(StereoSample input);
  StereoSample Tick(StereoSample input);
  void Clear();

  ReverbControls m_controls_;
  std::array<float, kMaximumDelay> m_delay_{};
  std::array<StereoSample, kFilterTaps> m_input_{};
  std::array<StereoSample, kFilterTaps> m_output_{};
  StereoSample m_previous_input_;
  StereoSample m_previous_wet_;
  StereoSample m_wet_;
  int m_cursor_ = 0;
  usize m_filter_cursor_ = 0;
  unsigned m_phase_ = 0;
  bool m_half_ = false;
};

}  // namespace VS::Audio
