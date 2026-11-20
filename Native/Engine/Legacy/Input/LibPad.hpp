#pragma once

// libpad surface (plan §6.5). The game registers two receive buffers with
// PadInitDirect and reads them every frame; LibPad.cpp fills them from
// Engine\Input (SDL3 gamepad and keyboard) in the original pad format, so the
// game's own pad decoding runs unchanged until it is rewritten against
// Input::PadState.

#include <cstdint>

namespace VS::Legacy {

// Controller word bits, as the game reads them from the receive buffer
// (libpad PADLup ... PADselect; L1, R1, R2, Start and Select are PADn, PADl,
// PADm, PADh and PADk).
inline constexpr std::int32_t kPadLup = 1 << 12;
inline constexpr std::int32_t kPadLdown = 1 << 14;
inline constexpr std::int32_t kPadLleft = 1 << 15;
inline constexpr std::int32_t kPadLright = 1 << 13;
inline constexpr std::int32_t kPadRup = 1 << 4;
inline constexpr std::int32_t kPadRdown = 1 << 6;
inline constexpr std::int32_t kPadRleft = 1 << 7;
inline constexpr std::int32_t kPadRright = 1 << 5;
inline constexpr std::int32_t kPadL1 = 1 << 2;
inline constexpr std::int32_t kPadR1 = 1 << 3;
inline constexpr std::int32_t kPadR2 = 1 << 1;
inline constexpr std::int32_t kPadStart = 1 << 11;
inline constexpr std::int32_t kPadSelect = 1 << 8;

// PadGetState: the pad answered and its mode is settled (PadStateStable).
inline constexpr std::int32_t kPadStateStable = 6;
// PadInfoMode terms (InfoModeCurID, InfoModeCurExID, InfoModeCurExOffs).
inline constexpr std::int32_t kInfoModeCurId = 1;
inline constexpr std::int32_t kInfoModeCurExId = 2;
inline constexpr std::int32_t kInfoModeCurExOffs = 3;

void PadInitDirect(std::uint8_t* port1, std::uint8_t* port2);
void PadStartCom();
std::int32_t PadGetState(std::int32_t port);
std::int32_t PadInfoMode(std::int32_t port, std::int32_t term,
                         std::int32_t offs);
std::int32_t PadSetActAlign(std::int32_t port, std::uint8_t* table);
std::int32_t PadSetMainMode(std::int32_t port, std::int32_t offs,
                            std::int32_t lock);
void PadSetAct(std::int32_t port, std::uint8_t* table, std::int32_t len);

}  // namespace VS::Legacy
