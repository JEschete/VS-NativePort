#include "Input/Pad.hpp"

#include <SDL3/SDL.h>

#include <array>
#include <limits>
#include <memory>
#include <span>
#include <string>

#include "Core/Log.hpp"
#include "Input/Actions.hpp"
#include "Input/Bindings.hpp"

namespace VS::Input {

namespace {

constexpr u32 kRepeatDelay = 20;
constexpr u32 kRepeatPeriod = 4;
constexpr i32 kAxisCenter = -std::numeric_limits<Sint16>::min();
constexpr int kAxisByteShift = std::numeric_limits<u8>::digits;
constexpr u8 kDpadLowThreshold = 16;
constexpr u8 kDpadHighThreshold = 241;
constexpr u32 kRumbleDurationMs = 250;
constexpr u64 kRumbleRefreshMs = 100;
constexpr Rumble kStillMotors{};

// SDL's button for each digital GamepadControl, kSouth..kBack.
constexpr std::array<SDL_GamepadButton, kGamepadButtonCount> kGamepadButtons{
    SDL_GAMEPAD_BUTTON_SOUTH,         SDL_GAMEPAD_BUTTON_EAST,
    SDL_GAMEPAD_BUTTON_WEST,          SDL_GAMEPAD_BUTTON_NORTH,
    SDL_GAMEPAD_BUTTON_DPAD_UP,       SDL_GAMEPAD_BUTTON_DPAD_DOWN,
    SDL_GAMEPAD_BUTTON_DPAD_LEFT,     SDL_GAMEPAD_BUTTON_DPAD_RIGHT,
    SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER,
    SDL_GAMEPAD_BUTTON_LEFT_STICK,    SDL_GAMEPAD_BUTTON_RIGHT_STICK,
    SDL_GAMEPAD_BUTTON_START,         SDL_GAMEPAD_BUTTON_BACK,
};

u8 AxisToByte(Sint16 value) {
  return static_cast<u8>((static_cast<i32>(value) + kAxisCenter) >>
                         kAxisByteShift);
}

}  // namespace

std::string DescribeButtons(u32 mask) {
  std::string out;
  for (const Action action : kActions) {
    if ((mask & ActionBit(action)) != 0) {
      if (!out.empty()) {
        out += '+';
      }
      out += ActionName(action);
    }
  }
  return out.empty() ? "-" : out;
}

u32 ParseButtons(std::string_view names) {
  u32 mask = 0;
  while (!names.empty()) {
    const usize plus = names.find('+');
    if (const auto action = ParseAction(names.substr(0, plus))) {
      mask |= ActionBit(*action);
    }
    names = plus == std::string_view::npos ? std::string_view{}
                                           : names.substr(plus + 1);
  }
  return mask;
}

InputSystem::~InputSystem() { CloseGamepad(); }

bool InputSystem::OpenGamepad(u32 instance_id) {
  m_pad_ = SDL_OpenGamepad(instance_id);
  if (!m_pad_) {
    VS::Core::LogWarn("input", "opening gamepad {}: {}", instance_id,
                      SDL_GetError());
    return false;
  }
  VS::Core::LogInfo("input", "gamepad connected: {}", GamepadName());
  return true;
}

void InputSystem::CloseGamepad() {
  if (m_pad_) {
    if (m_rumble_ != kStillMotors && !SDL_RumbleGamepad(m_pad_, 0, 0, 0)) {
      SDL_LogWarn(SDL_LOG_CATEGORY_INPUT, "stopping gamepad rumble: %s",
                  SDL_GetError());
    }
    SDL_CloseGamepad(m_pad_);
    m_pad_ = nullptr;
  }
  m_rumble_ = {};
  m_rumble_updated_ = 0;
  m_rumble_error_ = false;
}

void InputSystem::HandleEvent(const SDL_Event& ev) {
  if (ev.type == SDL_EVENT_GAMEPAD_ADDED && !m_pad_) {
    OpenGamepad(ev.gdevice.which);
  } else if (ev.type == SDL_EVENT_GAMEPAD_REMOVED && m_pad_ &&
             SDL_GetGamepadID(m_pad_) == ev.gdevice.which) {
    VS::Core::LogInfo("input", "gamepad disconnected: {}", GamepadName());
    CloseGamepad();
    int count = 0;
    const std::unique_ptr<SDL_JoystickID, decltype(&SDL_free)> devices(
        SDL_GetGamepads(&count), SDL_free);
    if (!devices) {
      VS::Core::LogWarn("input", "listing gamepads: {}", SDL_GetError());
      return;
    }
    for (int index = 0; index < count; ++index) {
      if (OpenGamepad(devices.get()[index])) break;
    }
  }
}

void InputSystem::UpdateActuators() {
  if (!m_pad_) return;
  const Rumble levels = m_devices_ ? m_rumble_request_ : kStillMotors;
  const bool changed = levels != m_rumble_;
  const bool active = levels != kStillMotors;
  const u64 now = SDL_GetTicks();
  if ((!changed && (!active || now - m_rumble_updated_ < kRumbleRefreshMs)) ||
      (m_rumble_error_ && now - m_rumble_updated_ < kRumbleRefreshMs)) {
    return;
  }
  m_rumble_updated_ = now;
  if (!SDL_RumbleGamepad(m_pad_, levels.low_frequency, levels.high_frequency,
                         active ? kRumbleDurationMs : 0)) {
    if (!m_rumble_error_) {
      VS::Core::LogWarn("input", "gamepad rumble on {}: {}", GamepadName(),
                        SDL_GetError());
    }
    m_rumble_error_ = true;
    return;
  }
  m_rumble_ = levels;
  m_rumble_error_ = false;
}

std::string InputSystem::GamepadName() const {
  const char* name = m_pad_ ? SDL_GetGamepadName(m_pad_) : nullptr;
  return name ? name : "none";
}

u32 InputSystem::SampleKeyboard() const {
  int count = 0;
  const bool* keys = SDL_GetKeyboardState(&count);
  if (!keys || count <= 0) {
    return 0;
  }
  return ResolveKeyboard(m_bindings_,
                         std::span<const bool>(keys, static_cast<usize>(count)))
      .PadWord();
}

u32 InputSystem::SampleGamepad(Sticks& sticks) const {
  if (!m_pad_) {
    return 0;
  }
  GamepadSnapshot snapshot;
  for (usize index = 0; index < kGamepadButtons.size(); ++index) {
    snapshot.buttons.at(index) =
        SDL_GetGamepadButton(m_pad_, kGamepadButtons.at(index));
  }
  snapshot.left_trigger =
      SDL_GetGamepadAxis(m_pad_, SDL_GAMEPAD_AXIS_LEFT_TRIGGER);
  snapshot.right_trigger =
      SDL_GetGamepadAxis(m_pad_, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER);
  sticks.lx = AxisToByte(SDL_GetGamepadAxis(m_pad_, SDL_GAMEPAD_AXIS_LEFTX));
  sticks.ly = AxisToByte(SDL_GetGamepadAxis(m_pad_, SDL_GAMEPAD_AXIS_LEFTY));
  sticks.rx = AxisToByte(SDL_GetGamepadAxis(m_pad_, SDL_GAMEPAD_AXIS_RIGHTX));
  sticks.ry = AxisToByte(SDL_GetGamepadAxis(m_pad_, SDL_GAMEPAD_AXIS_RIGHTY));
  return ResolveGamepad(m_bindings_, snapshot).PadWord();
}

const PadState& InputSystem::Update(u32 tick_speed) {
  UpdateActuators();
  Sticks sticks;
  u32 raw = m_devices_ ? (SampleKeyboard() | SampleGamepad(sticks)) : 0;
  raw |= m_injected_;
  m_state_.analog = m_pad_ != nullptr && m_devices_;
  m_state_.sticks = sticks;
  if (m_state_.analog) {
    // Original analog-mode behaviour: left stick acts as the d-pad.
    if (sticks.lx < kDpadLowThreshold) raw |= Left;
    if (sticks.lx >= kDpadHighThreshold) raw |= Right;
    if (sticks.ly < kDpadLowThreshold) raw |= Up;
    if (sticks.ly >= kDpadHighThreshold) raw |= Down;
  }
  Step(raw, tick_speed);
  return m_state_;
}

// Public input API: a held-button mask and logical tick increment.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void InputSystem::Step(u32 raw, u32 tick_speed) {
  const u32 previous = m_state_.held;
  m_state_.held = raw;
  m_state_.pressed = ~previous & raw;
  m_state_.released = previous & ~raw;
  m_state_.repeat = m_state_.pressed;
  for (usize index = 0; index < m_held_ticks_.size(); ++index) {
    if (raw & (1u << index)) {
      m_held_ticks_[index] += tick_speed;
      if (m_held_ticks_[index] >= kRepeatDelay &&
          ((m_held_ticks_[index] - kRepeatDelay) % kRepeatPeriod) == 0) {
        m_state_.repeat |= 1u << index;
      }
    } else {
      m_held_ticks_[index] = 0;
    }
  }
}

}  // namespace VS::Input
