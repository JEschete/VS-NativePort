// Time-attack heading, course, boss name, best time, countdown and palette
// pulse (F03018-F03023) against the original MENUF.PRG instructions. Each
// line: operation, x, y, timer, course, blend weight, the 8 first-place trial
// scores, the 160 clutBuf words and the 127 atlas entries (u, v, w, h, page,
// palette). Prints: status, the call count, then per call kind, x, y, texture
// index, brightness or timer, palette, time, record flag and the eight color
// bytes; then the 160 clutBuf words, the upload count and per upload the area
// (x, y, w, h) and its 160 words.
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <vector>

#include "Menu/Results/TimeAttackHeading.hpp"

namespace {
namespace r = VS::Menu::Results;
namespace render = VS::Render;

constexpr std::size_t kAtlasEntries = 127;
constexpr std::size_t kCallWords = 16;

using Call = std::array<std::int64_t, kCallWords>;

class Host final : public r::TimeAttackDrawHost {
public:
    std::vector<Call> calls;

    void DrawFlashFade(r::ResultsPoint origin, std::size_t texture_index,
        const r::TimeAttackColors& colors) override
    {
        Call call { 0, origin.x, origin.y, static_cast<std::int64_t>(texture_index) };
        std::size_t next = 8;
        for (const auto& color : colors) {
            call[next++] = color.r;
            call[next++] = color.g;
            call[next++] = color.b;
            call[next++] = color.step;
        }
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
};

class Target final : public r::ResultsClutTarget {
public:
    std::vector<std::vector<std::int64_t>> uploads;

    void Upload(const render::ScreenRect& area, std::span<const std::uint16_t> colors) override
    {
        std::vector<std::int64_t> upload { area.x, area.y, area.width, area.height };
        upload.insert(upload.end(), colors.begin(), colors.end());
        uploads.push_back(upload);
    }
};

bool RunVector()
{
    std::int64_t operation = 0, x = 0, y = 0, timer = 0, course = 0, weight = 0;
    if (!(std::cin >> operation >> x >> y >> timer >> course >> weight))
        return false;
    std::array<std::uint32_t, r::kTimeAttackCourses> scores { };
    for (auto& score : scores) {
        std::int64_t read = 0;
        std::cin >> read;
        score = static_cast<std::uint32_t>(read);
    }
    std::array<std::uint16_t, r::kResultsClutWords> clut { };
    for (auto& word : clut) {
        std::int64_t read = 0;
        std::cin >> read;
        word = static_cast<std::uint16_t>(read);
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
    Target target;
    r::ResultsDrawResult result { };
    switch (operation) {
    case 0:
        result = r::DrawTimeAttackHeading(atlas, i32(x), i32(y), i32(timer), host);
        break;
    case 1:
        result = r::DrawCourseNumber(atlas, i32(x), i32(y), i32(timer), boss, host);
        break;
    case 2:
        result = r::DrawBossName(atlas, i32(x), i32(y), i32(timer), boss, host);
        break;
    case 3:
        result = r::DrawBestTime(atlas, i32(x), i32(y), i32(timer), boss, scores, host);
        break;
    case 4:
        result = r::DrawStartCountdown(atlas, i32(x), i32(y), i32(timer), host);
        break;
    case 5:
        r::PulseTimeAttackClut(clut, i32(weight), target);
        break;
    default:
        return false;
    }
    std::cout << static_cast<int>(result) << ' ' << host.calls.size();
    for (const auto& call : host.calls)
        for (const auto word : call)
            std::cout << ' ' << word;
    for (const auto word : clut)
        std::cout << ' ' << word;
    std::cout << ' ' << target.uploads.size();
    for (const auto& upload : target.uploads)
        for (const auto word : upload)
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
