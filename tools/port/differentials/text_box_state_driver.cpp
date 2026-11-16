// Text-box state, selection, anchors and lifecycle (F01329, F01330, F01343,
// F01378, F01409, F01410, F01411, F01412) against the original instructions.
// Each line: operation, eight argument words, pressed buttons, default
// choice, $v1, 96 box words, 4 selector words, 32 anchor words. Prints:
// refused, return, 96 box words, 4 selector words, call count, calls.
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <optional>
#include <span>
#include <vector>

#include "Menu/TextBoxState.hpp"

namespace {
namespace m = VS::Menu;
constexpr std::size_t kBoxWords = 12;
constexpr std::size_t kSelectorWords = 4;
constexpr std::size_t kAnchorWords = 32;
constexpr std::size_t kArgumentWords = 8;
constexpr std::uint32_t kHalf = 0xFFFF;
constexpr std::uint32_t kByte = 0xFF;

using Call = std::array<std::int64_t, 5>;

class Host final : public m::TextBoxHost {
public:
    std::vector<Call> calls;
    void AnimateSpeaker(
        std::uint8_t speaker, std::int32_t timeline, std::int32_t command) override
    {
        calls.push_back({ 0, speaker, 0, timeline, command });
    }
    void PlaySelectSound() override { calls.push_back({ 1, 0, 0, 0, 0 }); }
    void PlayChangeSound() override { calls.push_back({ 2, 0, 0, 0, 0 }); }
};

std::int16_t Low(std::uint32_t word) { return static_cast<std::int16_t>(word & kHalf); }
std::int16_t High(std::uint32_t word) { return static_cast<std::int16_t>(word >> 16); }
std::uint8_t ByteOf(std::uint32_t word, unsigned index)
{
    return static_cast<std::uint8_t>((word >> (index * 8)) & kByte);
}
std::uint32_t Halves(std::int32_t low, std::int32_t high)
{
    return (static_cast<std::uint32_t>(low) & kHalf)
         | ((static_cast<std::uint32_t>(high) & kHalf) << 16);
}
std::uint32_t Bytes(std::int32_t b0, std::int32_t b1, std::int32_t b2, std::int32_t b3)
{
    return (static_cast<std::uint32_t>(b0) & kByte)
         | ((static_cast<std::uint32_t>(b1) & kByte) << 8)
         | ((static_cast<std::uint32_t>(b2) & kByte) << 16)
         | ((static_cast<std::uint32_t>(b3) & kByte) << 24);
}

m::TextBox BoxFrom(std::span<const std::uint32_t, kBoxWords> w)
{
    return {
        .flags = m::TextBoxFlags::FromWord(w[0]),
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

std::array<std::uint32_t, kBoxWords> WordsOf(const m::TextBox& b)
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

bool RunVector()
{
    int operation = 0;
    std::array<std::int64_t, kArgumentWords> raw { };
    std::uint32_t pressed = 0;
    std::uint32_t default_choice = 0;
    std::uint32_t v1 = 0;
    if (!(std::cin >> operation))
        return false;
    for (auto& value : raw)
        std::cin >> value;
    std::cin >> pressed >> default_choice >> v1;
    std::array<std::int32_t, kArgumentWords> a { };
    for (std::size_t i = 0; i < a.size(); ++i) {
        a[i] = static_cast<std::int32_t>(static_cast<std::uint32_t>(raw[i]));
    }
    std::array<std::uint32_t, m::kTextBoxCount * kBoxWords> box_words { };
    std::array<std::uint32_t, kSelectorWords> selector_words { };
    std::array<std::uint32_t, kAnchorWords> anchor_words { };
    for (auto& word : box_words)
        std::cin >> word;
    for (auto& word : selector_words)
        std::cin >> word;
    for (auto& word : anchor_words)
        std::cin >> word;

    std::array<m::TextBox, m::kTextBoxCount> boxes { };
    for (std::size_t i = 0; i < boxes.size(); ++i) {
        boxes[i] =
            BoxFrom(std::span(box_words).subspan(i * kBoxWords).first<kBoxWords>());
    }
    m::TextBoxSelector selector { };
    for (std::size_t i = 0; i < m::kTextBoxCount; ++i) {
        selector.order[i] = ByteOf(selector_words[i / 4], i % 4);
    }
    selector.selected = ByteOf(selector_words[2], 2);
    selector.count = ByteOf(selector_words[2], 3);
    std::array<m::TextAnchorPreset, m::kTextAnchorPresetCount> presets { };
    for (std::size_t i = 0; i < presets.size(); ++i) {
        presets[i] = { m::TextAnchor::FromWord(anchor_words[i * 2]),
            m::TextAnchor::FromWord(anchor_words[i * 2 + 1]) };
    }

    Host host;
    bool refused = false;
    std::int64_t result = 0;
    const auto box_at = [&](std::int32_t id) -> m::TextBox& {
        return boxes.at(static_cast<std::size_t>(id));
    };
    switch (operation) {
    case 0: {
        const auto* text = reinterpret_cast<const char*>(
            static_cast<std::uintptr_t>(static_cast<std::uint32_t>(a[1])));
        result = m::SetTextBoxText(box_at(a[0]), host, text);
        break;
    }
    case 1: {
        const auto choice =
            m::SelectTextBoxChoice(boxes, selector, host, static_cast<std::uint8_t>(a[0]),
                { .pressed = pressed,
                    .default_choice = static_cast<std::uint8_t>(default_choice) });
        refused = !choice.has_value();
        result = choice.value_or(0);
        break;
    }
    case 2:
        m::InitInformationTextBox(boxes, a[0]);
        break;
    case 3:
        result = m::DismissAllTextBoxes(boxes, host) ? 1 : 0;
        break;
    case 4: {
        const auto index = m::TextBoxIndex(a[0]);
        refused = !index.has_value();
        result = static_cast<std::int64_t>(index.value_or(0));
        break;
    }
    case 5: {
        const auto anchor = m::TextBoxAnchor(presets,
            { .preset = a[1], .orientation = a[2], .columns = a[3], .lines = a[4] });
        refused = !anchor.has_value();
        result = anchor.has_value() ? anchor->Word() : 0;
        break;
    }
    case 6:
        m::InitTextBox(box_at(a[0]), { .flags = a[1],
                                         .x = a[2],
                                         .y = a[3],
                                         .chars_per_line = a[4],
                                         .line_count = a[5],
                                         .center_x = a[6],
                                         .center_y = a[7] });
        break;
    case 7:
        result = m::DismissTextBox(box_at(a[0]), host) ? -1 : 0;
        break;
    default:
        return false;
    }

    std::cout << (refused ? 1 : 0) << ' ' << result;
    for (const auto& box : boxes) {
        for (const std::uint32_t word : WordsOf(box))
            std::cout << ' ' << word;
    }
    selector_words[0] =
        Bytes(selector.order[0], selector.order[1], selector.order[2], selector.order[3]);
    selector_words[1] =
        Bytes(selector.order[4], selector.order[5], selector.order[6], selector.order[7]);
    selector_words[2] = Bytes(ByteOf(selector_words[2], 0), ByteOf(selector_words[2], 1),
        selector.selected, selector.count);
    for (const std::uint32_t word : selector_words)
        std::cout << ' ' << word;
    std::cout << ' ' << host.calls.size();
    for (const auto& call : host.calls) {
        for (const std::int64_t value : call)
            std::cout << ' ' << value;
    }
    std::cout << '\n';
    return true;
}
} // namespace

int main()
{
    while (RunVector()) { }
    return 0;
}
