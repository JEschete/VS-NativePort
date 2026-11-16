// Cube-puzzle quit, banner, rank and time drawing (F03052-F03059) against the
// original MENUF.PRG instructions. Each line: operation, x, y, timer, the par
// time or rank, the puzzle clock (minutes, seconds, hundredths) and the 127
// atlas entries (u, v, w, h, page, palette). Prints: status, the call count,
// then per call kind, x, y, texture index or timer, brightness or packed time,
// the eight color bytes and the new-record flag.
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <vector>

#include "Menu/Results/CubePuzzleText.hpp"

namespace {
namespace r = VS::Menu::Results;
namespace render = VS::Render;

constexpr std::size_t kAtlasEntries = 127;
constexpr std::size_t kCallWords = 14;

using Call = std::array<std::int64_t, kCallWords>;

class Host final : public r::CubePuzzleTextHost {
public:
    std::vector<Call> calls;

    void DrawFadingTexture(r::ResultsPoint origin, std::size_t texture_index,
        std::int32_t brightness) override
    {
        calls.push_back({ 0, origin.x, origin.y,
            static_cast<std::int64_t>(texture_index), brightness });
    }
    void DrawFlashFade(r::ResultsPoint origin, std::size_t texture_index,
        const r::CubeTextColors& colors) override
    {
        Call call { 1, origin.x, origin.y, static_cast<std::int64_t>(texture_index), 0 };
        std::size_t next = 5;
        for (const auto& color : colors) {
            call[next++] = color.r;
            call[next++] = color.g;
            call[next++] = color.b;
            call[next++] = color.step;
        }
        calls.push_back(call);
    }
    void DrawTime(r::ResultsPoint origin, std::int32_t timer, std::int32_t time) override
    {
        calls.push_back({ 2, origin.x, origin.y, timer, time });
    }
};

bool RunVector()
{
    std::int64_t operation = 0, x = 0, y = 0, timer = 0, extra = 0, minutes = 0,
                 seconds = 0, hundredths = 0;
    if (!(std::cin >> operation >> x >> y >> timer >> extra >> minutes >> seconds >> hundredths))
        return false;
    std::vector<render::TextureRegion> atlas(kAtlasEntries);
    for (auto& entry : atlas) {
        std::int64_t f[6] { };
        for (auto& field : f)
            std::cin >> field;
        entry = { static_cast<std::uint8_t>(f[0]), static_cast<std::uint8_t>(f[1]),
            static_cast<std::uint8_t>(f[2]), static_cast<std::uint8_t>(f[3]),
            static_cast<std::uint16_t>(f[4]), static_cast<std::uint16_t>(f[5]) };
    }
    const auto i32 = [](std::int64_t v) { return static_cast<std::int32_t>(v); };
    const r::CubePuzzleClock clock { static_cast<std::uint8_t>(minutes),
        static_cast<std::uint8_t>(seconds), static_cast<std::uint8_t>(hundredths) };
    Host host;
    r::ResultsDrawResult result { };
    switch (operation) {
    case 0:
        result = r::DrawGivingUp(atlas, i32(y), i32(timer), host);
        break;
    case 1:
        result = r::DrawChicken(atlas, i32(y), i32(timer), host);
        break;
    case 2:
        result = r::DrawEvolveOrDie(atlas, i32(x), i32(y), i32(timer), host);
        break;
    case 3:
        result = r::DrawFoodChain(atlas, i32(x), i32(y), i32(timer), host);
        break;
    case 4:
        result = r::DrawAverageTimeHeading(atlas, i32(x), i32(y), i32(timer), host);
        break;
    case 5:
        result = r::DrawCubePuzzleParTime(
            i32(x), i32(y), i32(timer), static_cast<std::int16_t>(extra), host);
        break;
    case 6:
        result = r::DrawCubePuzzleRank(atlas, i32(x), i32(y), i32(timer), i32(extra), host);
        break;
    case 7:
        result = r::DrawCountingCubePuzzleTime(atlas, i32(x), i32(y), i32(timer), clock, host);
        break;
    default:
        return false;
    }
    std::cout << static_cast<int>(result) << ' ' << host.calls.size();
    for (const auto& call : host.calls)
        for (const auto word : call)
            std::cout << ' ' << word;
    std::cout << '\n';
    return true;
}
} // namespace

int main()
{
    while (RunVector()) { }
    return 0;
}
