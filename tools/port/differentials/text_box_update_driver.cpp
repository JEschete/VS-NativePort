#include <array>
#include <cstdint>
#include <iostream>
#include <span>
#include <vector>

#include "Menu/TextBoxUpdate.hpp"
#include "text_box_words.hpp"

namespace {
namespace menu = VS::Menu;
using Call = std::array<std::int64_t, 4>;

class Host final : public menu::TextBoxUpdateHost {
public:
    std::array<menu::TextBox, menu::kTextBoxCount> boxes { };
    std::vector<Call> calls;
    std::int32_t tick = 0;
    std::uint8_t buffer = 0;
    std::uint8_t active = 0;
    std::uint8_t timer = 0;

    menu::TextBox ReadBox(std::size_t index) const override { return boxes.at(index); }
    void WriteBox(std::size_t index, const menu::TextBox& box) override
    {
        boxes.at(index) = box;
    }
    std::int32_t TickSpeed() const override { return tick; }
    void BlendText(std::size_t index, std::int32_t scale) override
    {
        calls.push_back({ 0, static_cast<std::int64_t>(index), scale, 0 });
    }
    void DrawInformationBackdrop(std::size_t index) override
    {
        calls.push_back({ 1, static_cast<std::int64_t>(index), 0, 0 });
    }
    void DrawFrame(std::size_t index) override
    {
        calls.push_back({ 2, static_cast<std::int64_t>(index), 0, 0 });
    }
    void DrawProgressiveText(std::size_t index) override
    {
        calls.push_back({ 3, static_cast<std::int64_t>(index), 0, 0 });
    }
    void DrawScaledText(std::size_t index, std::int32_t scale) override
    {
        calls.push_back({ 4, static_cast<std::int64_t>(index), scale, 0 });
    }
    void SubmitOrdering() override { calls.push_back({ 5, -6, active, 33 }); }
    std::uint8_t OrderingBufferIndex() const override { return buffer; }
    void BeginOrderingBuffer(std::uint8_t index) override
    {
        buffer = index;
        active = index;
        calls.push_back({ 6, index, 34, 0 });
    }
    bool TimerVisible() const override { return timer == 1; }
    void DrawTimer() override { calls.push_back({ 7, 0, 0, 0 }); }
    void DrawOverlay() override { calls.push_back({ 8, 0, 0, 0 }); }
    void AdvanceRandomBits() override { calls.push_back({ 9, 0, 0, 0 }); }
};

bool RunVector()
{
    Host host;
    unsigned buffer = 0;
    unsigned active = 0;
    unsigned timer = 0;
    if (!(std::cin >> host.tick >> buffer >> active >> timer))
        return false;
    host.buffer = static_cast<std::uint8_t>(buffer);
    host.active = static_cast<std::uint8_t>(active);
    host.timer = static_cast<std::uint8_t>(timer);
    for (auto& box : host.boxes) {
        std::array<std::uint32_t, TextBoxWords::kBoxWords> words { };
        for (auto& word : words)
            std::cin >> word;
        box = TextBoxWords::BoxFrom(words);
    }
    const auto status = menu::UpdateTextBoxes(host);
    std::cout << static_cast<int>(status) << ' ' << static_cast<unsigned>(host.buffer)
              << ' ' << static_cast<unsigned>(host.active);
    for (const auto& box : host.boxes) {
        for (auto word : TextBoxWords::WordsOf(box))
            std::cout << ' ' << word;
    }
    std::cout << ' ' << host.calls.size();
    for (const auto& call : host.calls) {
        for (auto value : call)
            std::cout << ' ' << value;
    }
    std::cout << '\n';
    return true;
}
} // namespace

int main()
{
    while (RunVector()) { }
}
