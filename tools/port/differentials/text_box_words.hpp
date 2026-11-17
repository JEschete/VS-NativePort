#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "Menu/TextBoxState.hpp"

namespace TextBoxWords {
inline constexpr std::size_t kBoxWords = 12;
inline constexpr std::uint32_t kHalf = 0xFFFF;
inline constexpr std::uint32_t kByte = 0xFF;

inline std::int16_t Low(std::uint32_t word)
{
    return static_cast<std::int16_t>(word & kHalf);
}
inline std::int16_t High(std::uint32_t word)
{
    return static_cast<std::int16_t>(word >> 16);
}
inline std::uint8_t ByteOf(std::uint32_t word, unsigned index)
{
    return static_cast<std::uint8_t>((word >> (index * 8)) & kByte);
}
inline std::uint32_t Halves(std::int32_t low, std::int32_t high)
{
    return (static_cast<std::uint32_t>(low) & kHalf)
         | ((static_cast<std::uint32_t>(high) & kHalf) << 16);
}
inline std::uint32_t Bytes(
    std::int32_t b0, std::int32_t b1, std::int32_t b2, std::int32_t b3)
{
    return (static_cast<std::uint32_t>(b0) & kByte)
         | ((static_cast<std::uint32_t>(b1) & kByte) << 8)
         | ((static_cast<std::uint32_t>(b2) & kByte) << 16)
         | ((static_cast<std::uint32_t>(b3) & kByte) << 24);
}

inline VS::Menu::TextBox BoxFrom(std::span<const std::uint32_t, kBoxWords> w)
{
    return {
        .flags = VS::Menu::TextBoxFlags::FromWord(w[0]),
        .opaque4 = Low(w[1]),
        .opaque6 = High(w[1]),
        // Fixture only: the original address stands in for the string.
        .text = reinterpret_cast<const char*>(static_cast<std::uintptr_t>(w[2])),
        .speed = static_cast<std::uint16_t>(Low(w[3])),
        .previous_chunks = static_cast<std::uint16_t>(High(w[3])),
        .x_indent = Low(w[4]),
        .y_indent = High(w[4]),
        .x = Low(w[5]),
        .y = High(w[5]),
        .center_x = Low(w[6]),
        .center_y = High(w[6]),
        .state = Low(w[7]),
        .brightness = ByteOf(w[7], 2),
        .reserved1f = ByteOf(w[7], 3),
        .chars_per_line = static_cast<std::int8_t>(ByteOf(w[8], 0)),
        .line_count = static_cast<std::int8_t>(ByteOf(w[8], 1)),
        .open_duration = static_cast<std::int8_t>(ByteOf(w[8], 2)),
        .reserved23 = static_cast<std::int8_t>(ByteOf(w[8], 3)),
        .frame_x = Low(w[9]),
        .frame_y = High(w[9]),
        .frame_width = Low(w[10]),
        .frame_height = High(w[10]),
        .speaker_pause = Low(w[11]),
        .speaker_animation = High(w[11]),
    };
}

inline std::array<std::uint32_t, kBoxWords> WordsOf(const VS::Menu::TextBox& b)
{
    return {
        b.flags.Word(),
        Halves(b.opaque4, b.opaque6),
        static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(b.text)),
        Halves(b.speed, b.previous_chunks),
        Halves(b.x_indent, b.y_indent),
        Halves(b.x, b.y),
        Halves(b.center_x, b.center_y),
        Halves(b.state, Bytes(b.brightness, b.reserved1f, 0, 0)),
        Bytes(b.chars_per_line, b.line_count, b.open_duration, b.reserved23),
        Halves(b.frame_x, b.frame_y),
        Halves(b.frame_width, b.frame_height),
        Halves(b.speaker_pause, b.speaker_animation),
    };
}
} // namespace TextBoxWords
