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
#include "text_box_words.hpp"

namespace {
namespace m = VS::Menu;
using TextBoxWords::kBoxWords;
using TextBoxWords::BoxFrom;
using TextBoxWords::WordsOf;
using TextBoxWords::ByteOf;
using TextBoxWords::Bytes;
constexpr std::size_t kSelectorWords = 4;
constexpr std::size_t kAnchorWords = 32;
constexpr std::size_t kArgumentWords = 8;

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
