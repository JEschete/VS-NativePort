// Time-attack results heading, rating, rating wipe, time, counting time, new
// record label, leaderboard and leaderboard row (F03032-F03039) against the
// original MENUF.PRG instructions. Each line: operation, x, y, timer, slot,
// completed time, new record slot, course, the 8 x 3 trial scores and the 127
// atlas entries (u, v, w, h, page, palette). Prints: status, the call count,
// then per call kind, x, y, texture index, brightness or timer, palette, time,
// record flag, wipe edge and the eight color bytes.
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <vector>

#include "Menu/Results/TimeAttackRating.hpp"

namespace {
namespace r = VS::Menu::Results;
namespace render = VS::Render;

constexpr std::size_t kAtlasEntries = 127;
constexpr std::size_t kCallWords = 17;

using Call = std::array<std::int64_t, kCallWords>;

void PutColors(Call& call, const r::TimeAttackColors& colors)
{
    std::size_t next = 9;
    for (const auto& color : colors) {
        call[next++] = color.r;
        call[next++] = color.g;
        call[next++] = color.b;
        call[next++] = color.step;
    }
}

class Host final : public r::TimeAttackResultsDrawHost {
public:
    std::vector<Call> calls;

    void DrawFlashFade(r::ResultsPoint origin, std::size_t texture_index,
        const r::TimeAttackColors& colors) override
    {
        Call call { 0, origin.x, origin.y, static_cast<std::int64_t>(texture_index) };
        PutColors(call, colors);
        calls.push_back(call);
    }
    void DrawFade(r::ResultsPoint origin, std::size_t texture_index,
        std::int32_t brightness) override
    {
        calls.push_back({ 1, origin.x, origin.y, static_cast<std::int64_t>(texture_index),
            brightness });
    }
    void DrawFadeClut(r::ResultsPoint origin, std::size_t texture_index,
        std::int32_t brightness, std::int32_t clut) override
    {
        calls.push_back({ 2, origin.x, origin.y, static_cast<std::int64_t>(texture_index),
            brightness, clut });
    }
    void DrawTime(r::ResultsPoint origin, std::int32_t timer, std::int32_t time) override
    {
        calls.push_back({ 3, origin.x, origin.y, 0, timer, 0, time, 0 });
    }
    void DrawWipe(r::ResultsPoint origin, std::size_t texture_index,
        const r::TimeAttackColors& colors, std::int32_t wipe_edge_x) override
    {
        Call call { 4, origin.x, origin.y, static_cast<std::int64_t>(texture_index), 0, 0, 0, 0,
            wipe_edge_x };
        PutColors(call, colors);
        calls.push_back(call);
    }
    void DrawRankedTime(r::ResultsPoint origin, std::int32_t timer, std::int32_t time,
        bool is_new_record) override
    {
        calls.push_back({ 3, origin.x, origin.y, 0, timer, 0, time, is_new_record ? 1 : 0 });
    }
};

bool RunVector()
{
    std::int64_t operation = 0, x = 0, y = 0, timer = 0, slot = 0, time = 0, new_slot = 0, course = 0;
    if (!(std::cin >> operation >> x >> y >> timer >> slot >> time >> new_slot >> course))
        return false;
    std::array<r::TrialScoreRow, r::kTimeAttackCourses> scores { };
    for (auto& row : scores) {
        for (auto& score : row) {
            std::int64_t read = 0;
            std::cin >> read;
            score = static_cast<std::uint32_t>(read);
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
    const auto boss = static_cast<std::uint8_t>(course);
    Host host;
    r::ResultsDrawResult result { };
    switch (operation) {
    case 0:
        result = r::DrawTimeAttackResultsHeading(atlas, i32(x), i32(y), i32(timer), host);
        break;
    case 1:
        result = r::DrawTimeAttackRating(atlas, i32(x), i32(y), i32(timer), i32(time), host);
        break;
    case 2:
        result = r::DrawTimeAttackRatingWipe(atlas, i32(x), i32(y), i32(timer), i32(time), host);
        break;
    case 3:
        result = r::DrawTimeAttackTime(atlas, i32(x), i32(y), i32(timer), i32(time), host);
        break;
    case 4:
        result = r::DrawCountingTimeAttackTime(atlas, i32(x), i32(y), i32(timer), i32(time), host);
        break;
    case 5:
        result = r::DrawNewRecord(atlas, i32(x), i32(y), i32(timer), host);
        break;
    case 6:
        result = r::DrawLeaderboard(atlas, i32(x), i32(y), i32(timer), boss, i32(new_slot), scores,
            host);
        break;
    case 7:
        result = r::DrawLeaderboardEntry(atlas, i32(x), i32(y), i32(slot), i32(timer), boss,
            i32(new_slot), scores, host);
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
