// Results fading textures, time and prompt (F03040-F03042, F03044) against the
// original MENUF.PRG instructions. Each line: operation, x, y, three
// arguments, the ordering entry's link, the stale primitive words and the 127
// atlas entries (u, v, w, h, page, palette). Prints: status, the final
// ordering entry, the packet count, every packet word, then the text count
// and (string, xy, color, chain) per text.
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <string_view>
#include <vector>

#include "Menu/Results/ResultsDrawing.hpp"
#include "Render/UiTexturedQuads.hpp"

namespace {
namespace r = VS::Menu::Results;
namespace render = VS::Render;

constexpr std::uint32_t kPrimitives = 0x80180000;
constexpr std::uint32_t kPacketBytes = 40;
constexpr std::size_t kCapacity = 12;
constexpr std::size_t kAtlasEntries = 127;

// The PS1 primitive cursor and the ordering entry seven words before the
// table: AddPrim links each packet in front of the entry.
class Target final : public r::ResultsDrawTarget {
public:
    std::vector<std::uint32_t> words;
    std::uint32_t ordering = 0;
    std::size_t packets = 0;
    std::vector<std::array<std::int64_t, 4>> texts;

    bool DrawQuad(const render::UiTexturedQuad& quad) override
    {
        if (packets >= kCapacity)
            return false;
        render::UiTexturedQuadPacket storage {};
        const auto first = packets * render::kUiTexturedQuadWords;
        for (std::size_t i = 0; i < storage.size(); ++i)
            storage[i] = words[first + i];
        const auto packet = render::BuildUiTexturedQuadPacket(quad, storage, ordering);
        for (std::size_t i = 0; i < packet.size(); ++i)
            words[first + i] = packet[i];
        const auto address = kPrimitives + kPacketBytes * static_cast<std::uint32_t>(packets);
        ordering = (ordering & 0xFF000000u) | (address & 0xFFFFFFu);
        ++packets;
        return true;
    }
    void DrawText(std::string_view text, std::int32_t xy, std::uint32_t color) override
    {
        const std::int64_t id = text == "PRESS" ? 0 : text == "BUTTON" ? 1 : 2;
        texts.push_back({ id, xy, static_cast<std::int32_t>(color), 0 });
    }
};

bool RunVector()
{
    std::int64_t operation = 0, x = 0, y = 0, a = 0, b = 0, c = 0, link = 0;
    if (!(std::cin >> operation >> x >> y >> a >> b >> c >> link))
        return false;
    Target target;
    target.ordering = static_cast<std::uint32_t>(link);
    target.words.resize(kCapacity * render::kUiTexturedQuadWords);
    for (auto& word : target.words) {
        std::int64_t value = 0;
        std::cin >> value;
        word = static_cast<std::uint32_t>(value);
    }
    std::vector<render::TextureRegion> atlas(kAtlasEntries);
    for (auto& entry : atlas) {
        std::int64_t f[6] {};
        for (auto& value : f)
            std::cin >> value;
        entry = { static_cast<std::uint8_t>(f[0]), static_cast<std::uint8_t>(f[1]),
            static_cast<std::uint8_t>(f[2]), static_cast<std::uint8_t>(f[3]),
            static_cast<std::uint16_t>(f[4]), static_cast<std::uint16_t>(f[5]) };
    }
    const auto i32 = [](std::int64_t value) { return static_cast<std::int32_t>(value); };
    r::ResultsDrawResult result {};
    switch (operation) {
    case 0: result = r::DrawFadingTexture(atlas, i32(x), i32(y), i32(a), i32(b), target); break;
    case 1:
        result = r::DrawFadingTextureClut(atlas, i32(x), i32(y), i32(a), i32(b), i32(c), target);
        break;
    case 2: result = r::DrawTime(atlas, i32(x), i32(y), i32(a), i32(b), c != 0, target); break;
    case 3: result = r::DrawPressButtonPrompt(i32(x), i32(y), i32(a), i32(b), target); break;
    default: return false;
    }
    std::cout << static_cast<int>(result) << ' ' << target.ordering << ' ' << target.packets;
    for (std::size_t i = 0; i < target.packets * render::kUiTexturedQuadWords; ++i)
        std::cout << ' ' << target.words[i];
    std::cout << ' ' << target.texts.size();
    for (const auto& text : target.texts)
        for (const auto value : text)
            std::cout << ' ' << value;
    std::cout << '\n';
    return true;
}
} // namespace

int main()
{
    while (RunVector()) { }
    return 0;
}
