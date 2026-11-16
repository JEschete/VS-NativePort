// Native driver for the MENUE manual page differentials (manual_page_*.py).
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <span>
#include <vector>

#include "Menu/ManualPage.hpp"

namespace {
namespace manual = VS::Menu::manual;

template <typename T> bool Read(T& value)
{
    long long raw = 0;
    if (!(std::cin >> raw)) {
        return false;
    }
    value = static_cast<T>(raw);
    return true;
}

template <std::size_t Words>
void PrintPacket(std::int32_t slot, const std::array<std::uint32_t, Words>& body)
{
    std::printf(" 1 %d %zu", slot, Words);
    for (const std::uint32_t word : body) {
        std::printf(" %u", word);
    }
}

// Prints each primitive as 1, its slot, its body length and the body words.
class Printer final : public manual::PagePrimitiveTarget {
public:
    void DrawGradientQuad(std::int32_t slot, const manual::GradientQuad& quad) override
    {
        PrintPacket(slot, manual::PackGradientQuad(quad));
    }
    void DrawTexturedQuad(std::int32_t slot, const manual::TexturedQuad& quad) override
    {
        PrintPacket(slot, manual::PackTexturedQuad(quad));
    }
    void DrawTile(std::int32_t slot, const manual::PageTile& tile) override
    {
        PrintPacket(slot, manual::PackPageTile(tile));
    }
    void DrawGradientLine(std::int32_t slot, const manual::GradientLine& line) override
    {
        PrintPacket(slot, manual::PackGradientLine(line));
    }
    void DrawFlatQuad(std::int32_t slot, const manual::FlatQuad& quad) override
    {
        PrintPacket(slot, manual::PackFlatQuad(quad));
    }
    void SetTexturePage(std::int32_t slot, std::uint16_t page) override
    {
        PrintPacket(slot, manual::PackTexturePage(page));
    }
};

std::uint32_t PackHalves(std::int16_t low, std::int16_t high)
{
    return static_cast<std::uint32_t>(static_cast<std::uint16_t>(low))
         | (static_cast<std::uint32_t>(static_cast<std::uint16_t>(high)) << 16U);
}

// A draw area prints as a two-word packet in the content slot holding the raw rectangle,
// as the differential's SetDrawArea stub stores it; diagram lines print 2, an
// illustration 3 and its halfword offset.
class ContentPrinter final : public manual::PageContentTarget {
public:
    void SetDrawArea(const VS::Render::ScreenRect& area) override
    {
        std::printf(" 1 %d 2 %u %u", manual::kPageContentSlot, PackHalves(area.x, area.y),
            PackHalves(area.width, area.height));
    }
    void DrawDiagramLines() override { std::printf(" 2"); }
    void DrawIllustration(std::size_t offset) override { std::printf(" 3 %zu", offset); }
};

bool ReadRect(manual::PageRect& rect)
{
    return Read(rect.x) && Read(rect.y) && Read(rect.width) && Read(rect.height);
}

bool ReadArrow(manual::PageArrowState& state)
{
    int square = 0;
    if (!Read(state.animation_state) || !Read(square)) {
        return false;
    }
    state.square_held = square != 0;
    return true;
}

// Each result prints first: 0 drawn, 1 refused.
int Result(manual::PageDrawResult result)
{
    return result == manual::PageDrawResult::kDrawn ? 0 : 1;
}

bool Content()
{
    VS::Render::ScreenRect clip;
    std::size_t count = 0;
    if (!Read(clip.x) || !Read(clip.y) || !Read(clip.width) || !Read(clip.height)
        || !Read(count)) {
        return false;
    }
    std::vector<std::int16_t> text(count);
    for (std::int16_t& value : text) {
        if (!Read(value)) {
            return false;
        }
    }
    // The events print as they happen, so a refusal is decided before any of them.
    class Counter final : public manual::PageContentTarget {
    public:
        void SetDrawArea(const VS::Render::ScreenRect&) override { }
        void DrawDiagramLines() override { }
        void DrawIllustration(std::size_t) override { }
    } counter;
    const manual::PageDrawResult result = manual::DrawPageContent(text, clip, counter);
    std::printf("%d", Result(result));
    if (result == manual::PageDrawResult::kDrawn) {
        ContentPrinter printer;
        manual::DrawPageContent(text, clip, printer);
    }
    return true;
}

// Primitive draws whose refusal is decided before their first primitive.
class Silent final : public manual::PagePrimitiveTarget {
public:
    void DrawGradientQuad(std::int32_t, const manual::GradientQuad&) override { }
    void DrawTexturedQuad(std::int32_t, const manual::TexturedQuad&) override { }
    void DrawTile(std::int32_t, const manual::PageTile&) override { }
    void DrawGradientLine(std::int32_t, const manual::GradientLine&) override { }
    void DrawFlatQuad(std::int32_t, const manual::FlatQuad&) override { }
    void SetTexturePage(std::int32_t, std::uint16_t) override { }
};

template <typename Draw> void PrintDraw(Draw draw)
{
    Silent silent;
    const manual::PageDrawResult result = draw(silent);
    std::printf("%d", Result(result));
    if (result == manual::PageDrawResult::kDrawn) {
        Printer printer;
        draw(printer);
    }
}
} // namespace

int main()
{
    int operation = 0;
    while (std::cin >> operation) {
        manual::PageRect rect;
        manual::PageArrowState arrow;
        if (operation == 1) {
            if (!Content()) {
                return 2;
            }
        } else if (operation == 2) {
            std::int32_t gradient = 0;
            if (!ReadRect(rect) || !Read(gradient)) {
                return 2;
            }
            PrintDraw([&](manual::PagePrimitiveTarget& target) {
                return manual::DrawPageBackground(rect, gradient, target);
            });
        } else if (operation == 3 || operation == 4) {
            if (!ReadArrow(arrow)) {
                return 2;
            }
            PrintDraw([&](manual::PagePrimitiveTarget& target) {
                if (operation == 3) {
                    manual::DrawUpperScrollFade(arrow, target);
                } else {
                    manual::DrawLowerScrollFade(arrow, target);
                }
                return manual::PageDrawResult::kDrawn;
            });
        } else if (operation == 5) {
            int type = 0;
            if (!Read(type) || !ReadArrow(arrow)) {
                return 2;
            }
            PrintDraw([&](manual::PagePrimitiveTarget& target) {
                manual::DrawPageArrow(
                    type == 0 ? manual::PageArrow::kUp : manual::PageArrow::kDown, arrow,
                    target);
                return manual::PageDrawResult::kDrawn;
            });
        } else if (operation == 6) {
            if (!ReadRect(rect)) {
                return 2;
            }
            PrintDraw([&](manual::PagePrimitiveTarget& target) {
                return manual::DrawControlsBackground(rect, target);
            });
        } else {
            return 2;
        }
        std::printf("\n");
    }
    return 0;
}
