#pragma once

#include "vs/core/Types.hpp"

#include <array>
#include <string>

union SDL_Event;
struct SDL_Gamepad;

namespace VS::Input {

// Button bits as the game logic sees them. Values match the original pad word
// (libetc PADxxx) so ported logic keeps its masks; the hardware behind them is SDL.
// Player 1 uses bits 0-15, player 2 bits 16-31 (as in vs_main_processPadState).
enum Button : u32 {
    L2 = 1u << 0,
    R2 = 1u << 1,
    L1 = 1u << 2,
    R1 = 1u << 3,
    Triangle = 1u << 4,
    Circle = 1u << 5,
    Cross = 1u << 6,
    Square = 1u << 7,
    Select = 1u << 8,
    L3 = 1u << 9,
    R3 = 1u << 10,  // also set by the game when the left stick is deflected
    Start = 1u << 11,
    Up = 1u << 12,
    Right = 1u << 13,
    Down = 1u << 14,
    Left = 1u << 15,
};

struct Sticks {
    u8 lx = 128, ly = 128, rx = 128, ry = 128;  // 0..255, 128 = centre (original convention)
};

struct PadState {
    u32 held = 0;      // vs_main_buttonsState
    u32 pressed = 0;   // vs_main_buttonsPressed
    u32 released = 0;  // vs_main_buttonsReleased
    u32 repeat = 0;    // vs_main_buttonRepeat: pressed, then auto-repeat after 20 ticks every 4
    Sticks sticks;
    bool analog = false;
};

std::string DescribeButtons(u32 mask);

// Gathers keyboard + first gamepad each logical tick and derives edge/repeat state.
class InputSystem {
public:
    InputSystem() = default;
    ~InputSystem();
    InputSystem(const InputSystem&) = delete;
    InputSystem& operator=(const InputSystem&) = delete;

    void HandleEvent(const SDL_Event& ev);  // hot-plug
    // Samples devices and advances one game tick. `tickSpeed` mirrors vs_gametime_tickspeed.
    const PadState& Update(u32 tickSpeed = 1);
    const PadState& State() const { return m_state; }
    bool HasGamepad() const { return m_pad != nullptr; }
    std::string GamepadName() const;

    // Pure edge/repeat derivation (unit tested); `raw` is the sampled held mask.
    void Step(u32 raw, u32 tickSpeed);

private:
    u32 SampleKeyboard() const;
    u32 SampleGamepad(Sticks& sticks) const;

    SDL_Gamepad* m_pad = nullptr;
    PadState m_state;
    std::array<u32, 32> m_heldTicks{};
};

} // namespace VS::Input
