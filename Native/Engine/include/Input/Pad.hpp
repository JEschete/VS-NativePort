#pragma once

#include <array>
#include <limits>
#include <string>
#include <string_view>

#include "Core/Types.hpp"
#include "Input/Actions.hpp"

union SDL_Event;
struct SDL_Gamepad;

namespace VS::Input {

struct Sticks {
  static constexpr u8 kCenter = 128;
  u8 lx = kCenter, ly = kCenter, rx = kCenter,
     ry = kCenter;  // 0..255, 128 = centre (original convention)
};

struct PadState {
  u32 held = 0;      // vs_main_buttonsState
  u32 pressed = 0;   // vs_main_buttonsPressed
  u32 released = 0;  // vs_main_buttonsReleased
  u32 repeat = 0;    // vs_main_buttonRepeat: pressed, then auto-repeat after 20
                     // ticks every 4
  Sticks sticks;
  bool analog = false;
};

// Gamepad motor levels: the large (low-frequency) and small (high-frequency)
// motors, 0 = off.
struct Rumble {
  u16 low_frequency = 0;
  u16 high_frequency = 0;

  friend bool operator==(const Rumble&, const Rumble&) = default;
};

std::string DescribeButtons(u32 mask);
// "Cross" or "Up+Cross" (names as DescribeButtons writes them) -> mask; unknown
// names are ignored.
u32 ParseButtons(std::string_view names);

// Gathers keyboard + first gamepad each logical tick and derives edge/repeat
// state. Adopts another connected gamepad when the active one disconnects.
class InputSystem {
 public:
  InputSystem() = default;
  ~InputSystem();
  InputSystem(const InputSystem&) = delete;
  InputSystem& operator=(const InputSystem&) = delete;

  void HandleEvent(const SDL_Event& ev);  // hot-plug
  // Samples devices and advances one game tick. `tickSpeed` mirrors
  // vs_gametime_tickspeed.
  const PadState& Update(u32 tick_speed = 1);
  const PadState& State() const { return m_state_; }
  bool HasGamepad() const { return m_pad_ != nullptr; }
  std::string GamepadName() const;

  // Pure edge/repeat derivation (unit tested); `raw` is the sampled held mask.
  void Step(u32 raw, u32 tick_speed);

  // Buttons held in addition to the devices from the next Update on (scripted
  // input for tests and captures).
  void Inject(u32 mask) { m_injected_ = mask; }
  // Ignore keyboard and gamepad (scripted runs that must not be disturbed by a
  // person at the machine).
  void SetDevicesEnabled(bool enabled) { m_devices_ = enabled; }
  // The motor levels the next Update drives the gamepad at; ignored while the
  // devices are disabled.
  void RequestRumble(Rumble rumble) { m_rumble_request_ = rumble; }

 private:
  bool OpenGamepad(u32 instance_id);
  void UpdateActuators();
  void CloseGamepad();
  u32 SampleKeyboard() const;
  u32 SampleGamepad(Sticks& sticks) const;

  SDL_Gamepad* m_pad_ = nullptr;
  PadState m_state_;
  std::array<u32, std::numeric_limits<u32>::digits> m_held_ticks_{};
  u32 m_injected_ = 0;
  bool m_devices_ = true;
  Rumble m_rumble_request_;
  Rumble m_rumble_;
  u64 m_rumble_updated_ = 0;
  bool m_rumble_error_ = false;
};

}  // namespace VS::Input
