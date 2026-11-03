#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>
#include <span>

#include "Input/Controller.hpp"

// Live binding for Controller.hpp. The ports, buttons, sticks and settings
// stay in the legacy globals the copied code reads (ControllerLiveState.h);
// each adapter loads them into the typed state, runs the core and stores them
// back. libpad (LibPad.h) is the host. Port IDs are libpad's: 0x00 and 0x10.
// The vibration slots are typed state owned here. Invalid ports, motors and
// patterns are refused, reported and leave the state unchanged.
namespace VS::Input {
using LegacyPadPacket = std::span<const char, kPadPacketBytes>;

[[nodiscard]] VibrationState& LiveVibrationState() noexcept;
// Requests refused since start-up.
[[nodiscard]] std::uint64_t RefusedLiveControllerRequestCount() noexcept;

void InvalidateLiveControllerConnections();
void UnlockLiveAnalogModeSwitch();
void ConfigureLiveControllerActuators(int port_id, LegacyPadPacket packet);
int ReadLiveControllerPort(int port_id, LegacyPadPacket packet);
void UpdateLiveControllerConnection(int port_id, LegacyPadPacket packet);
void SetLiveMotorCommand(int port_index, int motor, std::uint8_t command);
// `pattern` is trusted through the end of both of its streams.
int StartLiveVibrationPattern(const char* pattern, std::uint32_t request,
                              int owner);
int StopLiveVibrationPatterns(std::uint32_t selector, int owner);
void ResetLiveVibrationPatterns();
void UpdateLiveVibrationPatterns();
void PlayLiveVibrationPreset(int index);
// Returns 1, as the original did. A soft reset does not return.
int UpdateLiveControllers();
}  // namespace VS::Input
