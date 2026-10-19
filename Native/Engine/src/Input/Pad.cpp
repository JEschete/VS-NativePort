#include "Input/Pad.hpp"

#include <SDL3/SDL.h>

#include <string>

#include "Core/Log.hpp"

namespace VS::Input {

namespace {

constexpr u32 RepeatDelay = 20;
constexpr u32 RepeatPeriod = 4;

struct NamedBit {
  u32 bit;
  const char* name;
};
constexpr NamedBit Names[] = {
    {Up, "Up"},         {Down, "Down"},
    {Left, "Left"},     {Right, "Right"},
    {Cross, "Cross"},   {Circle, "Circle"},
    {Square, "Square"}, {Triangle, "Triangle"},
    {L1, "L1"},         {R1, "R1"},
    {L2, "L2"},         {R2, "R2"},
    {L3, "L3"},         {R3, "R3"},
    {Start, "Start"},   {Select, "Select"},
};

u8 AxisToByte(Sint16 v) {
  return static_cast<u8>((static_cast<i32>(v) + 32768) >> 8);
}

}  // namespace

std::string DescribeButtons(u32 mask) {
  std::string out;
  for (const auto& n : Names) {
    if (mask & n.bit) {
      if (!out.empty()) {
        out += '+';
      }
      out += n.name;
    }
  }
  return out.empty() ? "-" : out;
}

u32 ParseButtons(std::string_view names) {
  u32 mask = 0;
  while (!names.empty()) {
    const usize plus = names.find('+');
    const std::string_view name = names.substr(0, plus);
    for (const auto& n : Names) {
      if (name == n.name) {
        mask |= n.bit;
      }
    }
    names = plus == std::string_view::npos ? std::string_view{}
                                           : names.substr(plus + 1);
  }
  return mask;
}

InputSystem::~InputSystem() {
  if (m_pad_) {
    SDL_CloseGamepad(m_pad_);
  }
}

void InputSystem::HandleEvent(const SDL_Event& ev) {
  if (ev.type == SDL_EVENT_GAMEPAD_ADDED && !m_pad_) {
    m_pad_ = SDL_OpenGamepad(ev.gdevice.which);
    if (m_pad_) {
      VS_LOG_INFO("input", "gamepad connected: {}", GamepadName());
    }
  } else if (ev.type == SDL_EVENT_GAMEPAD_REMOVED && m_pad_ &&
             SDL_GetGamepadID(m_pad_) == ev.gdevice.which) {
    VS_LOG_INFO("input", "gamepad disconnected: {}", GamepadName());
    SDL_CloseGamepad(m_pad_);
    m_pad_ = nullptr;
  }
}

std::string InputSystem::GamepadName() const {
  const char* n = m_pad_ ? SDL_GetGamepadName(m_pad_) : nullptr;
  return n ? n : "none";
}

u32 InputSystem::SampleKeyboard() const {
  // Default layout; rebinding arrives with the options/config work (plan §6.4).
  struct Bind {
    SDL_Scancode key;
    u32 bit;
  };
  static constexpr Bind Binds[] = {
      {SDL_SCANCODE_UP, Up},        {SDL_SCANCODE_DOWN, Down},
      {SDL_SCANCODE_LEFT, Left},    {SDL_SCANCODE_RIGHT, Right},
      {SDL_SCANCODE_X, Cross},      {SDL_SCANCODE_C, Circle},
      {SDL_SCANCODE_Z, Square},     {SDL_SCANCODE_S, Triangle},
      {SDL_SCANCODE_Q, L1},         {SDL_SCANCODE_E, R1},
      {SDL_SCANCODE_1, L2},         {SDL_SCANCODE_3, R2},
      {SDL_SCANCODE_RETURN, Start}, {SDL_SCANCODE_BACKSPACE, Select},
  };
  const bool* keys = SDL_GetKeyboardState(nullptr);
  u32 mask = 0;
  for (const auto& b : Binds) {
    if (keys[b.key]) {
      mask |= b.bit;
    }
  }
  return mask;
}

u32 InputSystem::SampleGamepad(Sticks& sticks) const {
  if (!m_pad_) {
    return 0;
  }
  struct Bind {
    SDL_GamepadButton button;
    u32 bit;
  };
  // SDL face buttons are positional: South=Cross, East=Circle, West=Square,
  // North=Triangle.
  static constexpr Bind Binds[] = {
      {SDL_GAMEPAD_BUTTON_DPAD_UP, Up},
      {SDL_GAMEPAD_BUTTON_DPAD_DOWN, Down},
      {SDL_GAMEPAD_BUTTON_DPAD_LEFT, Left},
      {SDL_GAMEPAD_BUTTON_DPAD_RIGHT, Right},
      {SDL_GAMEPAD_BUTTON_SOUTH, Cross},
      {SDL_GAMEPAD_BUTTON_EAST, Circle},
      {SDL_GAMEPAD_BUTTON_WEST, Square},
      {SDL_GAMEPAD_BUTTON_NORTH, Triangle},
      {SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, L1},
      {SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, R1},
      {SDL_GAMEPAD_BUTTON_LEFT_STICK, L3},
      {SDL_GAMEPAD_BUTTON_RIGHT_STICK, R3},
      {SDL_GAMEPAD_BUTTON_START, Start},
      {SDL_GAMEPAD_BUTTON_BACK, Select},
  };
  u32 mask = 0;
  for (const auto& b : Binds) {
    if (SDL_GetGamepadButton(m_pad_, b.button)) {
      mask |= b.bit;
    }
  }
  constexpr Sint16 TriggerThreshold = 16384;
  if (SDL_GetGamepadAxis(m_pad_, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) >
      TriggerThreshold) {
    mask |= L2;
  }
  if (SDL_GetGamepadAxis(m_pad_, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) >
      TriggerThreshold) {
    mask |= R2;
  }
  sticks.lx = AxisToByte(SDL_GetGamepadAxis(m_pad_, SDL_GAMEPAD_AXIS_LEFTX));
  sticks.ly = AxisToByte(SDL_GetGamepadAxis(m_pad_, SDL_GAMEPAD_AXIS_LEFTY));
  sticks.rx = AxisToByte(SDL_GetGamepadAxis(m_pad_, SDL_GAMEPAD_AXIS_RIGHTX));
  sticks.ry = AxisToByte(SDL_GetGamepadAxis(m_pad_, SDL_GAMEPAD_AXIS_RIGHTY));
  return mask;
}

const PadState& InputSystem::Update(u32 tick_speed) {
  Sticks sticks;
  u32 raw = m_devices_ ? (SampleKeyboard() | SampleGamepad(sticks)) : 0;
  raw |= m_injected_;
  m_state_.analog = m_pad_ != nullptr && m_devices_;
  m_state_.sticks = sticks;
  if (m_state_.analog) {
    // Original analog-mode behaviour: left stick acts as the d-pad.
    if (sticks.lx < 16) raw |= Left;
    if (sticks.lx >= 241) raw |= Right;
    if (sticks.ly < 16) raw |= Up;
    if (sticks.ly >= 241) raw |= Down;
  }
  Step(raw, tick_speed);
  return m_state_;
}

void InputSystem::Step(u32 raw, u32 tick_speed) {
  const u32 previous = m_state_.held;
  m_state_.held = raw;
  m_state_.pressed = ~previous & raw;
  m_state_.released = previous & ~raw;
  m_state_.repeat = m_state_.pressed;
  for (u32 i = 0; i < 32; ++i) {
    if (raw & (1u << i)) {
      m_held_ticks_[i] += tick_speed;
      if (m_held_ticks_[i] >= RepeatDelay &&
          ((m_held_ticks_[i] - RepeatDelay) % RepeatPeriod) == 0) {
        m_state_.repeat |= 1u << i;
      }
    } else {
      m_held_ticks_[i] = 0;
    }
  }
}

}  // namespace VS::Input
