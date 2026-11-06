#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstddef>
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
// The caller retains these bytes until the slot is cleared or replaced,
// including while a pattern is paused. Both streams must end inside the span.
int StartLiveVibrationPattern(std::span<const std::byte> pattern,
                              std::uint32_t request, int owner);
// Copies the validated streams before replacing a slot. The native slot owns
// the copy through pauses and state snapshots; refusal leaves it unchanged.
int StartLiveOwnedVibrationPattern(std::span<const std::byte> pattern,
                                   std::uint32_t request, int owner);
// Compatibility import: `pattern` is trusted through both stream terminators.
// New callers with a known source extent must use the bounded overload.
int StartLiveVibrationPattern(const char* pattern, std::uint32_t request,
                              int owner);
int StopLiveVibrationPatterns(std::uint32_t selector, int owner);
void ResetLiveVibrationPatterns();
void UpdateLiveVibrationPatterns();
void PlayLiveVibrationPreset(int index);
// Returns 1, as the original did. A soft reset does not return.
int UpdateLiveControllers();
}  // namespace VS::Input
