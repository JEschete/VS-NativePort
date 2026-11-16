// Cube-puzzle initialization, palette pulse and screen transitions (F03024,
// F03046-F03051) against the original MENUF.PRG instructions. Each line:
// operation, load step, request state, the map completion, score, timer,
// screen state, animation and buff reel words, par, cap, the puzzle clock's
// hundredths, seconds and minutes, the maximum flag, the buttons, the misc
// score, the pulse weight, whether there is a room and its zone and map, the
// 160 clutBuf words, the file size and its byte runs (offset, length, bytes).
// Prints: status, the 14 state words, the 160 clutBuf words, the rows (each
// its length and words), then the file bytes changed as (offset, value) pairs.
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <optional>
#include <span>
#include <vector>

#include "Menu/Results/CubePuzzleLifecycle.hpp"
#include "Menu/Results/ResultsPalette.hpp"

namespace {
namespace r = VS::Menu::Results;
namespace render = VS::Render;

using Row = std::vector<std::int64_t>;

enum RowKind : std::int64_t {
    kReserveRow,
    kReadRow,
    kUploadRow,
    kReleaseRow,
    kFreeRow,
    kPlayRow,
    kStopRow,
    kRetireRow,
    kPulseRow,
    kMessageRow,
    kRankRow,
    kPromptRow,
    kRefuseRow,
    kClutRow,
};

constexpr std::int64_t kIqFile = 3;
constexpr std::int64_t kEscFile = 4;
constexpr std::int16_t kLoadedState = 4;

struct Rows {
    std::vector<Row> rows;
};

class LoadHost final : public r::CubePuzzleLoadHost {
public:
    LoadHost(Rows& out, std::vector<std::byte>& asset, const std::vector<std::byte>& pristine,
        std::int16_t slot_state, std::optional<r::CubePuzzleRoom> room)
        : out_(out)
        , asset_(asset)
        , pristine_(pristine)
        , slot_state_(slot_state)
        , room_(room)
    {
    }

    void ReserveTexturePages(std::uint32_t first_page, std::uint32_t page_count,
        std::int32_t owner) override
    {
        out_.rows.push_back({ kReserveRow, first_page, page_count, owner });
    }
    void BeginAssetRead(r::CubePuzzleAsset asset) override
    {
        out_.rows.push_back({ kReadRow, asset == r::CubePuzzleAsset::kIq ? kIqFile : kEscFile, 1, 1, 1 });
    }
    bool AssetReadLoaded() override { return slot_state_ == kLoadedState; }
    std::span<std::byte> LoadedAsset() override { return asset_; }
    void UploadImage(const render::ScreenRect& area, std::span<const std::byte> pixels) override
    {
        const auto offset = static_cast<std::int64_t>(pixels.data() - asset_.data());
        Row row { kUploadRow, area.x, area.y, area.width, area.height, offset,
            static_cast<std::int64_t>(pixels.size()), 0 };
        std::int64_t changed = 0;
        for (std::size_t index = 0; index < pixels.size(); ++index) {
            const auto at = static_cast<std::size_t>(offset) + index;
            if (asset_[at] != pristine_[at]) {
                row.push_back(static_cast<std::int64_t>(at));
                row.push_back(std::to_integer<std::int64_t>(asset_[at]));
                ++changed;
            }
        }
        row[7] = changed;
        out_.rows.push_back(row);
    }
    std::optional<r::CubePuzzleRoom> CurrentRoom() override { return room_; }
    void ReleaseAssetRead() override { out_.rows.push_back({ kReleaseRow, 1 }); }
    void FreeAssetBuffer() override { out_.rows.push_back({ kFreeRow, 1 }); }
    void Refuse(r::CubePuzzleRefusal refusal) override
    {
        out_.rows.push_back({ kRefuseRow, static_cast<std::int64_t>(refusal) });
    }

private:
    Rows& out_;
    std::vector<std::byte>& asset_;
    const std::vector<std::byte>& pristine_;
    std::int16_t slot_state_;
    std::optional<r::CubePuzzleRoom> room_;
};

class ScreenHost final : public r::CubePuzzleScreenHost {
public:
    explicit ScreenHost(Rows& out)
        : out_(out)
    {
    }

    void PlaySfx(std::int32_t group, std::int32_t effect) override
    {
        out_.rows.push_back({ kPlayRow, group, effect });
    }
    void StopSfx(std::int32_t group, std::int32_t effect) override
    {
        out_.rows.push_back({ kStopRow, group, effect });
    }
    void RetireTexturePages(std::uint32_t first_page, std::uint32_t page_count,
        std::int32_t owner) override
    {
        out_.rows.push_back({ kRetireRow, first_page, page_count, owner });
    }
    void PulsePalette(std::int32_t blend_weight) override
    {
        out_.rows.push_back({ kPulseRow, blend_weight });
    }
    void DrawMessage(r::CubePuzzleMessage message, r::ResultsPoint origin,
        std::int32_t timer) override
    {
        out_.rows.push_back({ kMessageRow, static_cast<std::int64_t>(message), origin.x, origin.y, timer });
    }
    void DrawRank(r::ResultsPoint origin, std::int32_t timer, std::int32_t rank) override
    {
        out_.rows.push_back({ kRankRow, origin.x, origin.y, timer, rank });
    }
    void DrawPrompt(r::ResultsPoint origin, std::int32_t slide_timer,
        std::int32_t bounce_timer) override
    {
        out_.rows.push_back({ kPromptRow, origin.x, origin.y, slide_timer, bounce_timer });
    }

private:
    Rows& out_;
};

class ClutTarget final : public r::ResultsClutTarget {
public:
    ClutTarget(Rows& out, const std::uint16_t* clut)
        : out_(out)
        , clut_(clut)
    {
    }

    void Upload(const render::ScreenRect& area, std::span<const std::uint16_t> colors) override
    {
        out_.rows.push_back({ kClutRow, area.x, area.y, area.width, area.height,
            static_cast<std::int64_t>(colors.size()), colors.data() == clut_ ? 1 : 0 });
    }

private:
    Rows& out_;
    const std::uint16_t* clut_;
};

bool Read(std::int64_t& value) { return static_cast<bool>(std::cin >> value); }

bool RunVector()
{
    std::int64_t operation = 0;
    if (!Read(operation))
        return false;
    std::int64_t in[20] { };
    for (auto& value : in)
        if (!Read(value))
            return false;
    const auto i32 = [](std::int64_t v) { return static_cast<std::int32_t>(v); };
    const auto u32 = [](std::int64_t v) { return static_cast<std::uint32_t>(v); };
    const auto u8 = [](std::int64_t v) { return static_cast<std::uint8_t>(v); };
    r::CubePuzzleScreen screen {
        .load_step = i32(in[0]),
        .timer = i32(in[4]),
        .screen_state = i32(in[5]),
        .animation = u32(in[6]),
        .on_screen_map_completion = i32(in[2]),
        .on_screen_score = i32(in[3]),
        .buff_reel_selection = u32(in[7]),
    };
    const auto slot_state = static_cast<std::int16_t>(in[1]);
    r::CubePuzzleRating rating { .par_seconds = static_cast<std::int16_t>(in[8]),
        .rank_cap = static_cast<std::int16_t>(in[9]) };
    r::CubePuzzleTimer timer { .clock = { .minutes = u8(in[12]), .seconds = u8(in[11]), .hundredths = u8(in[10]) },
        .show_maximum = u8(in[13]) };
    const bool confirm = (u32(in[14]) & 0x20U) != 0;
    std::uint32_t misc = u32(in[15]);
    const std::int32_t weight = i32(in[16]);
    std::optional<r::CubePuzzleRoom> room;
    if (in[17] != 0)
        room = r::CubePuzzleRoom { .zone_id = static_cast<std::int16_t>(in[18]),
            .map_id = static_cast<std::int16_t>(in[19]) };
    std::array<std::uint16_t, r::kResultsClutWords> clut { };
    for (auto& word : clut) {
        std::int64_t read = 0;
        if (!Read(read))
            return false;
        word = static_cast<std::uint16_t>(read);
    }
    std::int64_t size = 0, runs = 0;
    if (!Read(size) || !Read(runs) || size < 0)
        return false;
    std::vector<std::byte> asset(static_cast<std::size_t>(size));
    for (std::int64_t run = 0; run < runs; ++run) {
        std::int64_t offset = 0, length = 0;
        if (!Read(offset) || !Read(length))
            return false;
        for (std::int64_t index = 0; index < length; ++index) {
            std::int64_t byte = 0;
            if (!Read(byte))
                return false;
            if (offset + index < size)
                asset[static_cast<std::size_t>(offset + index)] = static_cast<std::byte>(byte);
        }
    }
    const std::vector<std::byte> pristine = asset;
    Rows out;
    LoadHost load(out, asset, pristine, slot_state, room);
    ScreenHost host(out);
    ClutTarget target(out, clut.data());
    std::int64_t status = 0;
    switch (operation) {
    case 0:
        r::PulseCubePuzzleClut(clut, weight, target);
        break;
    case 1:
        status = static_cast<std::int64_t>(r::InitializeCubePuzzleStart(screen, rating, clut, load));
        break;
    case 2:
        status = static_cast<std::int64_t>(r::InitializeCubePuzzleResults(screen, rating, timer, clut, load));
        break;
    case 3:
        status = static_cast<std::int64_t>(r::InitializeCubePuzzleQuit(screen, clut, load));
        break;
    case 4:
        status = static_cast<std::int64_t>(r::UpdateCubePuzzleStart(screen, confirm, host));
        break;
    case 5:
        status = static_cast<std::int64_t>(
            r::UpdateCubePuzzleResults(screen, rating, timer.clock, confirm, misc, host));
        break;
    case 6:
        status = static_cast<std::int64_t>(r::UpdateCubePuzzleQuit(screen, confirm, host));
        break;
    default:
        return false;
    }
    std::cout << status << ' ' << screen.load_step << ' ' << screen.on_screen_map_completion << ' '
              << screen.on_screen_score << ' ' << screen.timer << ' ' << screen.screen_state << ' '
              << screen.animation << ' ' << screen.buff_reel_selection << ' ' << rating.par_seconds << ' '
              << rating.rank_cap << ' ' << static_cast<int>(timer.clock.hundredths) << ' '
              << static_cast<int>(timer.clock.seconds) << ' ' << static_cast<int>(timer.clock.minutes) << ' '
              << static_cast<int>(timer.show_maximum) << ' ' << misc;
    for (const auto word : clut)
        std::cout << ' ' << word;
    std::cout << ' ' << out.rows.size();
    for (const auto& row : out.rows) {
        std::cout << ' ' << row.size();
        for (const auto word : row)
            std::cout << ' ' << word;
    }
    std::vector<std::int64_t> changed;
    for (std::size_t index = 0; index < asset.size(); ++index) {
        if (asset[index] != pristine[index]) {
            changed.push_back(static_cast<std::int64_t>(index));
            changed.push_back(std::to_integer<std::int64_t>(asset[index]));
        }
    }
    std::cout << ' ' << changed.size() / 2;
    for (const auto word : changed)
        std::cout << ' ' << word;
    std::cout << '\n';
    return true;
}
} // namespace

int main()
{
    std::ios::sync_with_stdio(false);
    while (RunVector()) { }
    return 0;
}
