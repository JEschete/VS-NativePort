// Native side of the SCREFF2 screen overlay differentials (screen_distortion_common.py,
// #276). One vector per line: mode, the controller's loading byte, pages-reserved byte,
// feedback layer, warp scale, warp growth and vignette word, two argument words, the
// packet start (in words past the primitive buffer), the ordering-table head and
// feedback-slot words and the frame buffer index. Prints the controller fields after a
// setter, the ellipse point, or the packet end, word count, packet words and
// ordering-table entries after a drawing core, then the texture page calls as [count,
// kind and arguments of each call...].
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <map>
#include <span>
#include <vector>

#include "Effects/ScreenDistortion/ScreenOverlays.hpp"
#include "Render/UiPackets.hpp"

namespace {
namespace sd = VS::Effects::ScreenDistortion;

enum Mode : std::uint8_t {
    kSetLayer,
    kDrawTiles,
    kSetWarp,
    kSetVignette,
    kEllipse,
    kCorner,
    kDrawVignette,
};

constexpr std::uint32_t kPrimitives = 0x80600000;
constexpr std::uint32_t kHead = 0x80700010;
constexpr std::uint32_t kLinkMask = 0xFFFFFF;

// The PS1 words the cores reach: ordering-table entries and the primitive
// buffer, with the primitive cursor.
struct Memory {
    std::map<std::uint32_t, std::uint32_t> words;
    std::uint32_t cursor = 0;
};

class SimulatedQueue final : public VS::Render::PacketQueue {
public:
    SimulatedQueue(Memory& memory, std::uint32_t entry)
        : memory_(memory)
        , entry_(entry)
    {
    }

    [[nodiscard]] std::uint32_t ChainLink() const override
    {
        const auto found = memory_.words.find(entry_);
        return found != memory_.words.end() ? found->second : 0;
    }
    bool Emit(std::span<const std::uint32_t> packet) override
    {
        for (std::size_t index = 0; index < packet.size(); ++index) {
            memory_.words[memory_.cursor + static_cast<std::uint32_t>(4 * index)] =
                packet[index];
        }
        memory_.words[entry_] = memory_.cursor & kLinkMask;
        memory_.cursor += static_cast<std::uint32_t>(4 * packet.size());
        return true;
    }

private:
    Memory& memory_;
    std::uint32_t entry_;
};

class RecordingPages final : public sd::FeedbackTexturePages {
public:
    void Reserve(const sd::TexturePageLease& lease) override { Record(1, lease); }
    void Retire(const sd::TexturePageLease& lease) override { Record(2, lease); }

    std::vector<std::vector<std::int64_t>> calls;

private:
    void Record(int kind, const sd::TexturePageLease& lease)
    {
        calls.push_back({ kind, static_cast<std::int32_t>(lease.first_page),
            static_cast<std::int32_t>(lease.page_count), lease.owner_tag });
    }
};

std::uint32_t Word(Memory& memory, std::uint32_t address)
{
    return memory.words[address];
}

void PrintPackets(Memory& memory, std::uint32_t start, std::uint32_t end,
    std::span<const std::uint32_t> entries)
{
    const std::uint32_t count = (end - start) / 4;
    std::cout << static_cast<std::int32_t>(end - kPrimitives) << ' ' << count;
    for (std::uint32_t index = 0; index < count; ++index) {
        std::cout << ' ' << static_cast<std::int32_t>(Word(memory, start + 4 * index));
    }
    for (const std::uint32_t entry : entries) {
        std::cout << ' ' << static_cast<std::int32_t>(Word(memory, entry));
    }
}
} // namespace

int main()
{
    std::int64_t mode = 0;
    std::int64_t loading = 0;
    std::int64_t reserved = 0;
    std::int64_t layer = 0;
    std::int64_t scale = 0;
    std::int64_t growth = 0;
    std::int64_t vignette = 0;
    std::int64_t first = 0;
    std::int64_t second = 0;
    std::int64_t start_words = 0;
    std::int64_t head = 0;
    std::int64_t slot_head = 0;
    std::int64_t frame = 0;
    while (std::cin >> mode >> loading >> reserved >> layer >> scale >> growth >> vignette
           >> first >> second >> start_words >> head >> slot_head >> frame) {
        sd::ScreenOverlayControls controls { .feedback_pages_reserved =
                                                 static_cast<std::uint8_t>(reserved),
            .feedback_layer = static_cast<std::uint16_t>(layer),
            .radial_warp = { .scale = static_cast<std::uint16_t>(scale),
                .growth = static_cast<std::int16_t>(growth) },
            .vignette_enabled = static_cast<std::int32_t>(vignette) };
        const auto a0 = static_cast<std::int32_t>(first);
        const auto a1 = static_cast<std::int32_t>(second);
        Memory memory;
        const std::uint32_t start =
            kPrimitives + static_cast<std::uint32_t>(4 * start_words);
        memory.cursor = start;
        memory.words[kHead] = static_cast<std::uint32_t>(head);
        RecordingPages pages;

        switch (mode) {
        case kSetLayer:
            sd::SetFeedbackLayer(controls, a0, pages);
            break;
        case kSetWarp:
            sd::SetRadialWarp(controls, sd::RadialWarpFromWords(a0, a1));
            break;
        case kSetVignette:
            sd::SetVignetteEnabled(controls, a0);
            break;
        case kEllipse:
            std::cout << static_cast<std::int32_t>(sd::EllipsePoint(a0, a1));
            break;
        case kCorner: {
            SimulatedQueue queue(memory, kHead);
            sd::AppendVignetteCorner(static_cast<std::uint32_t>(a0), a1, queue);
            const std::uint32_t entries[] = { kHead };
            PrintPackets(memory, start, memory.cursor, entries);
            break;
        }
        case kDrawVignette: {
            SimulatedQueue queue(memory, kHead);
            sd::DrawVignette(queue);
            const std::uint32_t entries[] = { kHead };
            PrintPackets(memory, start, memory.cursor, entries);
            break;
        }
        case kDrawTiles: {
            // The harness's slot entry, independent of the core's slot.
            const std::uint32_t expected_entry =
                kHead + 4 * (static_cast<std::uint32_t>(layer) - 3);
            memory.words[expected_entry] = static_cast<std::uint32_t>(slot_head);
            const std::uint32_t slot_entry =
                kHead
                + 4
                      * static_cast<std::uint32_t>(
                          sd::FeedbackOrderingSlot(static_cast<std::uint16_t>(layer)));
            SimulatedQueue scene(memory, kHead);
            SimulatedQueue feedback(memory, slot_entry);
            sd::DrawFeedbackTiles(static_cast<std::int32_t>(frame), scene, feedback);
            const std::uint32_t entries[] = { kHead, expected_entry };
            PrintPackets(memory, start, memory.cursor, entries);
            break;
        }
        default:
            break;
        }
        if (mode == kSetLayer || mode == kSetWarp || mode == kSetVignette) {
            std::cout << loading << ' ' << int { controls.feedback_pages_reserved } << ' '
                      << controls.feedback_layer << ' ' << controls.radial_warp.scale
                      << ' ' << controls.radial_warp.growth << ' '
                      << controls.vignette_enabled;
        }
        std::cout << ' ' << pages.calls.size();
        for (const auto& call : pages.calls) {
            for (const std::int64_t item : call) {
                std::cout << ' ' << item;
            }
        }
        std::cout << '\n';
    }
    return 0;
}
