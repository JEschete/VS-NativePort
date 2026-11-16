// Native driver for the MENUE manual illustration differentials (manual_illustration_*.py).
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <span>
#include <vector>

#include "Menu/ManualIllustration.hpp"

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

bool ReadHalfwords(std::vector<std::int16_t>& halfwords)
{
    std::size_t count = 0;
    if (!Read(count)) {
        return false;
    }
    halfwords.resize(count);
    for (std::int16_t& value : halfwords) {
        if (!Read(value)) {
            return false;
        }
    }
    return true;
}

class Uploader final : public manual::ImageUploader {
public:
    explicit Uploader(const std::uint32_t* base) : base_(base) { }
    void Upload(const VS::Render::ScreenRect& area, std::span<const std::uint32_t> pixels) override
    {
        uploads.push_back({ area.x, area.y, area.width, area.height,
            static_cast<int>(pixels.data() - base_), static_cast<int>(pixels.size()) });
    }
    std::vector<std::vector<int>> uploads;

private:
    const std::uint32_t* base_;
};

class Illustration final : public manual::IllustrationTarget {
public:
    void DrawTile(const manual::IllustrationTile& tile) override { tiles.push_back(tile); }
    std::vector<manual::IllustrationTile> tiles;
};

class Lines final : public manual::DiagramLineTarget {
public:
    void DrawLine(const manual::DiagramLine& line) override { lines.push_back(line); }
    std::vector<manual::DiagramLine> lines;
};

bool Upload(int operation)
{
    std::int32_t count = 0;
    if (!Read(count)) {
        return false;
    }
    const bool sprites = operation == 1;
    const std::size_t image_words = sprites ? manual::kTileWords : manual::kClutWords;
    std::vector<std::uint32_t> words(count > 0 ? static_cast<std::size_t>(count) * image_words : 1);
    Uploader uploader(words.data());
    const std::span<const std::uint32_t> buffer(words.data(), count > 0 ? words.size() : 0);
    const manual::UploadResult result = sprites
        ? manual::UploadSpriteTiles(buffer, count, uploader)
        : manual::UploadCluts(buffer, count, uploader);
    std::printf("%d %zu", static_cast<int>(result), uploader.uploads.size());
    for (const std::vector<int>& upload : uploader.uploads) {
        for (const int value : upload) {
            std::printf(" %d", value);
        }
    }
    return true;
}

void PrintHalfwords(std::span<const std::int16_t> halfwords)
{
    for (const std::int16_t value : halfwords) {
        std::printf(" %d", value);
    }
}
} // namespace

int main()
{
    int operation = 0;
    while (std::cin >> operation) {
        if (operation == 1 || operation == 2) {
            if (!Upload(operation)) {
                return 2;
            }
        } else if (operation == 5) {
            std::vector<std::int16_t> halfwords(manual::kAnimationHalfwords);
            for (std::int16_t& value : halfwords) {
                if (!Read(value)) {
                    return 2;
                }
            }
            const std::span<std::int16_t, manual::kAnimationHalfwords> state(halfwords.data(),
                manual::kAnimationHalfwords);
            manual::VisibilityAnimation animation = manual::LoadAnimation(state);
            const std::int16_t hidden = manual::AdvanceVisibilityAnimation(animation);
            manual::StoreAnimation(animation, state);
            std::printf("%d", hidden);
            PrintHalfwords(halfwords);
        } else if (operation == 6) {
            std::int32_t scroll = 0;
            std::vector<std::int16_t> record;
            if (!Read(scroll) || !ReadHalfwords(record)) {
                return 2;
            }
            Illustration target;
            const manual::DrawResult result = manual::DrawIllustration(record, scroll, target);
            std::printf("%d", static_cast<int>(result));
            PrintHalfwords(std::span(record).first(manual::kAnimationHalfwords));
            std::printf(" %zu", target.tiles.size());
            for (const manual::IllustrationTile& tile : target.tiles) {
                std::printf(" %d %d %u %u %u %d %d %u", tile.x, tile.y, unsigned { tile.u },
                    unsigned { tile.v }, unsigned { tile.clut }, tile.width, tile.height,
                    unsigned { tile.texture_page });
            }
        } else if (operation == 7) {
            std::int32_t scroll = 0;
            std::vector<std::int16_t> text;
            if (!Read(scroll) || !ReadHalfwords(text)) {
                return 2;
            }
            Lines target;
            const manual::DrawResult result = manual::DrawDiagramLines(text, scroll, target);
            std::printf("%d %zu", static_cast<int>(result), text.size());
            PrintHalfwords(text);
            std::printf(" %zu", target.lines.size());
            for (const manual::DiagramLine& line : target.lines) {
                std::printf(" %d %d %d %d %u %u %u", line.x0, line.y0, line.x1, line.y1,
                    unsigned { line.red }, unsigned { line.green }, unsigned { line.blue });
            }
        } else {
            return 2;
        }
        std::printf("\n");
    }
    return 0;
}
