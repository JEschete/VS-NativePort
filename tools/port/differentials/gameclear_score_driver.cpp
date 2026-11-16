// Game-clear score, map completion and rank drawing (F03025-F03031) against
// the original MENUF.PRG instructions. Each line: operation, x, y, timer, the
// score or map completion, the counters (shown, counting), the rank, the 16
// rank rows (4 bytes each) and the 127 atlas entries (u, v, w, h, page,
// palette). Prints: status, the final counters, the call count, then per call
// kind, x, y, texture index, the eight color bytes and the wipe edge (sounds:
// kind, group, effect and zeros).
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <vector>

#include "Menu/Results/GameClearScore.hpp"

namespace {
namespace r = VS::Menu::Results;
namespace render = VS::Render;

constexpr std::size_t kAtlasEntries = 127;
constexpr std::size_t kRankRows = 16;
constexpr std::size_t kCallWords = 13;

using Call = std::array<std::int64_t, kCallWords>;

class Host final : public r::GameClearDrawHost {
public:
    std::vector<Call> calls;

    void DrawTintFade(r::ResultsPoint origin, std::size_t texture_index,
        const r::GameClearColors& colors) override
    {
        Draw(0, origin, texture_index, colors, 0);
    }
    void DrawFlashFade(r::ResultsPoint origin, std::size_t texture_index,
        const r::GameClearColors& colors) override
    {
        Draw(1, origin, texture_index, colors, 0);
    }
    void DrawWipe(r::ResultsPoint origin, std::size_t texture_index,
        const r::GameClearColors& colors, std::int32_t wipe_edge_x) override
    {
        Draw(2, origin, texture_index, colors, wipe_edge_x);
    }
    void PlaySfx(std::int32_t group, std::int32_t effect) override
    {
        calls.push_back({ 3, group, effect });
    }
    void StopSfx(std::int32_t group, std::int32_t effect) override
    {
        calls.push_back({ 4, group, effect });
    }

private:
    void Draw(std::int64_t kind, r::ResultsPoint origin, std::size_t index,
        const r::GameClearColors& colors, std::int32_t edge)
    {
        Call call { kind, origin.x, origin.y, static_cast<std::int64_t>(index) };
        std::size_t next = 4;
        for (const auto& color : colors) {
            call[next++] = color.r;
            call[next++] = color.g;
            call[next++] = color.b;
            call[next++] = color.step;
        }
        call[next] = edge;
        calls.push_back(call);
    }
};

bool RunVector()
{
    std::int64_t operation = 0, x = 0, y = 0, timer = 0, value = 0, shown = 0,
                 counting = 0, rank = 0;
    if (!(std::cin >> operation >> x >> y >> timer >> value >> shown >> counting >> rank))
        return false;
    std::vector<r::RiskbreakerRankRow> ranks(kRankRows);
    for (auto& row : ranks) {
        for (auto& byte : row) {
            std::int64_t read = 0;
            std::cin >> read;
            byte = static_cast<std::uint8_t>(read);
        }
    }
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
    const auto u32 = [](std::int64_t v) { return static_cast<std::uint32_t>(v); };
    Host host;
    r::CountingReadout readout { .shown = i32(shown), .counting = i32(counting) };
    r::ResultsDrawResult result { };
    switch (operation) {
    case 0:
        result = r::DrawCongratulations(atlas, i32(x), i32(y), i32(timer), host);
        break;
    case 1:
        result = r::DrawScore(atlas, i32(x), i32(y), i32(timer), u32(value), host);
        break;
    case 2:
        result = r::DrawCountingScore(
            atlas, i32(x), i32(y), i32(timer), u32(value), readout, host);
        break;
    case 3:
        result =
            r::DrawMapCompletion(atlas, i32(x), i32(y), i32(timer), u32(value), host);
        break;
    case 4:
        result = r::DrawCountingMapCompletion(
            atlas, i32(x), i32(y), i32(timer), u32(value), readout, host);
        break;
    case 5:
        result = r::DrawRiskbreakerRankHeading(atlas, i32(x), i32(y), i32(timer), host);
        break;
    case 6:
        result = r::DrawRiskbreakerRank(
            atlas, i32(x), i32(y), i32(timer), i32(rank), ranks, host);
        break;
    default:
        return false;
    }
    std::cout << static_cast<int>(result) << ' ' << readout.shown << ' '
              << readout.counting << ' ' << host.calls.size();
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
