#include "Audio/Reverb.hpp"

#include <algorithm>
#include <array>
#include <bit>

#include "Audio/VoiceEngine.hpp"

namespace VS::Audio {
namespace {
constexpr float kUnit = 32768;
constexpr unsigned kFilterRate = 44100;
constexpr int kTapUnit = 4;
constexpr usize kCombFilters = 4;
constexpr usize kPresetCount = static_cast<usize>(ReverbPreset::kPipe) + 1;

constexpr float Coefficient(u16 value) {
  return static_cast<float>(std::bit_cast<i16>(value)) / kUnit;
}

struct ChannelTaps {
  int reflection;
  int feedback;
  int cross_reflection;
  int cross_feedback;
  std::array<int, kCombFilters> comb;
  std::array<int, 2> diffusion;
};

struct Preset {
  int length;
  float reflection;
  float wall;
  std::array<float, kCombFilters> comb;
  std::array<float, 2> diffusion;
  std::array<int, 2> diffusion_delay;
  std::array<ChannelTaps, 2> channel;
};

constexpr auto SampleTaps(std::array<Preset, kPresetCount> presets) {
  for (auto& preset : presets) {
    for (auto& delay : preset.diffusion_delay) delay *= kTapUnit;
    for (auto& channel : preset.channel) {
      channel.reflection *= kTapUnit;
      channel.feedback *= kTapUnit;
      channel.cross_reflection *= kTapUnit;
      channel.cross_feedback *= kTapUnit;
      for (auto& tap : channel.comb) tap *= kTapUnit;
      for (auto& tap : channel.diffusion) tap *= kTapUnit;
    }
  }
  return presets;
}

// PSX-SPX "SPU Reverb Examples" and "Reverb Formula".
// Ordering verified against SLUS_010.40 _spu_rev_param (D_80030D00),
// libspu/S_SRMT.c. Tap units are converted once to 22050-Hz sample offsets.
constexpr auto kPresets = SampleTaps({{
    {},
    {0x26C0 / 2,
     Coefficient(0x6D80),
     Coefficient(0xBA80),
     {Coefficient(0x54B8), Coefficient(0xBED0), 0, 0},
     {Coefficient(0x5800), Coefficient(0x5300)},
     {0x7D, 0x5B},
     {{{0x4D6, 0x334, 0, 0, {0x3F0, 0x374, 0, 0}, {0x1B4, 0xB8}},
       {0x333, 0x1B5, 0, 0, {0x227, 0x1EF, 0, 0}, {0x136, 0x5C}}}}},
    {0x1F40 / 2,
     Coefficient(0x70F0),
     Coefficient(0x9C00),
     {Coefficient(0x4FA8), Coefficient(0xBCE0), Coefficient(0x4410),
      Coefficient(0xC0F0)},
     {Coefficient(0x5280), Coefficient(0x4EC0)},
     {0x33, 0x25},
     {{{0x3E4, 0x31C, 0x25C, 0x18F, {0x3A4, 0x372, 0x22F, 0x1D2}, {0xB4, 0x4C}},
       {0x31B,
        0x25D,
        0x18E,
        0xB5,
        {0x2AF, 0x266, 0x135, 0xB7},
        {0x80, 0x26}}}}},
    {0x4840 / 2,
     Coefficient(0x70F0),
     Coefficient(0xB4C0),
     {Coefficient(0x4FA8), Coefficient(0xBCE0), Coefficient(0x4510),
      Coefficient(0xBEF0)},
     {Coefficient(0x5280), Coefficient(0x4EC0)},
     {0xB1, 0x7F},
     {{{0x904,
        0x76C,
        0x5EC,
        0x42F,
        {0x824, 0x7A2, 0x50F, 0x462},
        {0x264, 0x100}},
       {0x76B,
        0x5ED,
        0x42E,
        0x265,
        {0x65F, 0x616, 0x305, 0x2B7},
        {0x1B2, 0x80}}}}},
    {0x6FE0 / 2,
     Coefficient(0x6F60),
     Coefficient(0xA680),
     {Coefficient(0x4FA8), Coefficient(0xBCE0), Coefficient(0x4510),
      Coefficient(0xBEF0)},
     {Coefficient(0x5680), Coefficient(0x52C0)},
     {0xE3, 0xA9},
     {{{0xDFB,
        0xB59,
        0x8D9,
        0x5EA,
        {0xD09, 0xBD9, 0x7EC, 0x6EF},
        {0x31C, 0x154}},
       {0xB58,
        0x8DA,
        0x5E9,
        0x31D,
        {0xA3C, 0x973, 0x4B0, 0x3D2},
        {0x238, 0xAA}}}}},
    {0xADE0 / 2,
     Coefficient(0x6000),
     Coefficient(0xC000),
     {Coefficient(0x5000), Coefficient(0x4C00), Coefficient(0xB800),
      Coefficient(0xBC00)},
     {Coefficient(0x6000), Coefficient(0x5C00)},
     {0x1A5, 0x139},
     {{{0x15BA,
        0x11C0,
        0xDC0,
        0x9C2,
        {0x14C2, 0x11BC, 0xBC4, 0xA00},
        {0x5C0, 0x274}},
       {0x11BB,
        0xDC3,
        0x9C1,
        0x5C1,
        {0x10BD, 0xDC1, 0x7C1, 0x6CD},
        {0x41A, 0x13A}}}}},
    {0xF6C0 / 2,
     Coefficient(0x7E00),
     Coefficient(0xB000),
     {Coefficient(0x5000), Coefficient(0xB400), Coefficient(0xB000),
      Coefficient(0x4C00)},
     {Coefficient(0x6000), Coefficient(0x5400)},
     {0x33D, 0x231},
     {{{0x1ED6,
        0x1A32,
        0x15EE,
        0x1056,
        {0x1D14, 0x1BC2, 0x1334, 0x11F6},
        {0xAE0, 0x464}},
       {0x1A31,
        0x15EF,
        0x1055,
        0xAE1,
        {0x183B, 0x16B2, 0xF2D, 0xC5D},
        {0x7A2, 0x232}}}}},
    {0x18040 / 2,
     Coefficient(0x7FFF),
     Coefficient(0x8100),
     {Coefficient(0x7FFF), 0, 0, 0},
     {0, 0},
     {1, 1},
     {{{0x1FFF, 0x1005, 0, 0, {0x1005, 0, 0, 0}, {0x1004, 4}},
       {0xFFF, 5, 0, 0, {5, 0, 0, 0}, {0x1002, 2}}}}},
    {0x18040 / 2,
     Coefficient(0x7FFF),
     0,
     {Coefficient(0x7FFF), 0, 0, 0},
     {0, 0},
     {1, 1},
     {{{0x1FFF, 0x1005, 0, 0, {0x1005, 0, 0, 0}, {0x1004, 4}},
       {0xFFF, 5, 0, 0, {5, 0, 0, 0}, {0x1002, 2}}}}},
    {0x3C00 / 2,
     Coefficient(0x70F0),
     Coefficient(0x8500),
     {Coefficient(0x4FA8), Coefficient(0xBCE0), Coefficient(0x4510),
      Coefficient(0xBEF0)},
     {Coefficient(0x5F80), Coefficient(0x54C0)},
     {0x17, 0x13},
     {{{0x371, 0x358, 0x1D6, 0x1A0, {0x2E5, 0x2B0, 0x12D, 0x11F}, {0x58, 0x28}},
       {0x2AF, 0x26A, 0x11E, 0xE3, {0x1DF, 0x1D7, 0xB1, 0x59}, {0x40, 0x14}}}}},
}});

constexpr std::array<int, 39> kFilter = {
    -1,   0, 2,     0, -10,   0,     35,    0, -103,  0, 266,  0, -616, 0,
    1332, 0, -2960, 0, 10246, 16384, 10246, 0, -2960, 0, 1332, 0, -616, 0,
    266,  0, -103,  0, 35,    0,     -10,   0, 2,     0, -1,
};

float Saturate(float sample) {
  return std::clamp(sample, -1.0F, 1.0F - 1 / kUnit);
}

StereoSample Interpolate(StereoSample from, StereoSample to, float fraction) {
  return {from.left + (to.left - from.left) * fraction,
          from.right + (to.right - from.right) * fraction};
}

StereoSample Filter(const std::array<StereoSample, kFilter.size()>& history,
                    usize cursor) {
  StereoSample result;
  for (usize tap = 0; tap < kFilter.size(); ++tap) {
    const auto& sample =
        history[(cursor + history.size() - tap) % history.size()];
    const auto coefficient = static_cast<float>(kFilter[tap]) / kUnit;
    result.left += sample.left * coefficient;
    result.right += sample.right * coefficient;
  }
  return result;
}
}  // namespace

void Reverb::Clear() {
  m_delay_.fill(0);
  m_input_.fill({});
  m_output_.fill({});
  m_previous_input_ = m_previous_wet_ = m_wet_ = {};
  m_cursor_ = 0;
  m_filter_cursor_ = 0;
  m_phase_ = 0;
  m_half_ = false;
}

void Reverb::Configure(const ReverbControls& controls, bool clear) {
  if (clear || m_controls_.preset != controls.preset) Clear();
  m_controls_ = controls;
}

float Reverb::Read(int offset) const {
  const auto length = kPresets[static_cast<usize>(m_controls_.preset)].length;
  const auto index = (m_cursor_ + offset) % length;
  return m_delay_[index < 0 ? index + length : index];
}

void Reverb::Write(int offset, float sample) {
  if (!m_controls_.enabled) return;
  const auto length = kPresets[static_cast<usize>(m_controls_.preset)].length;
  const auto index = (m_cursor_ + offset) % length;
  m_delay_[index < 0 ? index + length : index] = Saturate(sample);
}

StereoSample Reverb::Network(StereoSample input) {
  const auto& preset = kPresets[static_cast<usize>(m_controls_.preset)];
  // The shipped presets use -1 for both input coefficients.
  const std::array<float, 2> signal{-input.left, -input.right};
  if (m_controls_.enabled) {
    for (usize side = 0; side < signal.size(); ++side) {
      const auto& taps = preset.channel[side];
      const float same = Read(taps.reflection - 1);
      const float cross = Read(taps.cross_reflection - 1);
      Write(taps.reflection, (signal[side] + Read(taps.feedback) * preset.wall -
                              same) * preset.reflection +
                                 same);
      Write(taps.cross_reflection,
            (signal[side] +
             Read(preset.channel[1 - side].cross_feedback) * preset.wall -
             cross) *
                    preset.reflection +
                cross);
    }
  }
  std::array<float, 2> output{};
  for (usize side = 0; side < output.size(); ++side) {
    const auto& taps = preset.channel[side];
    for (usize comb = 0; comb < taps.comb.size(); ++comb) {
      output[side] += Read(taps.comb[comb]) * preset.comb[comb];
    }
    for (usize stage = 0; stage < taps.diffusion.size(); ++stage) {
      const auto delayed =
          Read(taps.diffusion[stage] - preset.diffusion_delay[stage]);
      const auto value =
          Saturate(output[side] - preset.diffusion[stage] * delayed);
      Write(taps.diffusion[stage], value);
      output[side] = value * preset.diffusion[stage] + delayed;
    }
  }
  m_cursor_ = (m_cursor_ + 1) % preset.length;
  return {Saturate(output[0]), Saturate(output[1])};
}

StereoSample Reverb::Tick(StereoSample input) {
  m_input_[m_filter_cursor_] = {Saturate(input.left), Saturate(input.right)};
  StereoSample wet;
  if (m_half_) wet = Network(Filter(m_input_, m_filter_cursor_));
  m_output_[m_filter_cursor_] = {wet.left * 2, wet.right * 2};
  const auto result = Filter(m_output_, m_filter_cursor_);
  m_filter_cursor_ = (m_filter_cursor_ + 1) % kFilterTaps;
  m_half_ = !m_half_;
  return result;
}

StereoSample Reverb::Process(StereoSample input) {
  if (m_controls_.preset == ReverbPreset::kOff) return {};
  m_phase_ += kFilterRate;
  if (m_phase_ >= VoiceEngine::kSampleRate) {
    m_phase_ -= VoiceEngine::kSampleRate;
    const auto fraction = 1 - static_cast<float>(m_phase_) / kFilterRate;
    m_previous_wet_ = m_wet_;
    m_wet_ = Tick(Interpolate(m_previous_input_, input, fraction));
  }
  m_previous_input_ = input;
  const auto wet =
      Interpolate(m_previous_wet_, m_wet_,
                  static_cast<float>(m_phase_) / VoiceEngine::kSampleRate);
  return {wet.left * m_controls_.left, wet.right * m_controls_.right};
}
}  // namespace VS::Audio
