// Native side of the screen overlay lifetime differentials
// (screen_overlay_lifetime_common.py, #276). One vector per line: mode, the controller (0
// detached, 1 the defaults, 2 another), its loading byte, pages-reserved byte, feedback
// layer, warp scale, warp growth, vignette word and request number (0 null, 1 queued, 2
// earlier), the load argument, the allocator's request number (0 refused) and the two
// request states. Prints the lifetime as the same controller number (an attach from
// detached selects the defaults), the controller words and request number (3 for a
// refused request's completed record), then the port calls as [count, kind and arguments
// of each call...].
#include <array>
#include <cstdint>
#include <iostream>
#include <vector>

#include "Effects/ScreenDistortion/ScreenOverlayLifetime.hpp"

// The cores pass requests through without reading them; the driver's loader reads the
// state halfword the live loader does.
struct Content_DiscQueueSlot {
    std::int16_t state;
};

namespace {
namespace sd = VS::Effects::ScreenDistortion;

enum Mode : std::uint8_t { kReset, kLoad, kDraw, kAcquire, kRelease };

enum Call : std::uint8_t {
    kAllocate = 1,
    kEnqueue,
    kFree,
    kWait,
    kAcquireWorkspace,
    kReleaseWorkspace,
    kRetire,
    kWarp,
    kTiles,
    kVignette,
    kCompleteRefused,
};

constexpr std::int16_t kLoaded = 4;
constexpr int kOtherRequest = 99;

using Calls = std::vector<std::vector<std::int64_t>>;

struct Requests {
    // Null, queued, earlier, completed.
    std::array<Content_DiscQueueSlot, 3> records { };

    [[nodiscard]] Content_DiscQueueSlot* At(std::int64_t number)
    {
        return number <= 0 || number > 3 ? nullptr : &records.at(number - 1);
    }
    [[nodiscard]] int Number(const Content_DiscQueueSlot* request) const
    {
        if (request == nullptr) {
            return 0;
        }
        for (std::size_t index = 0; index < records.size(); ++index) {
            if (request == &records.at(index)) {
                return static_cast<int>(index) + 1;
            }
        }
        return kOtherRequest;
    }
};

class RecordingLoader final : public sd::ScreenOverlayLoader {
public:
    RecordingLoader(Requests& requests, Calls& calls, std::int64_t allocate)
        : requests_(requests)
        , calls_(calls)
        , allocate_(allocate)
    {
    }

    void AcquireWorkspace(int owner_tag) override
    {
        calls_.push_back({ kAcquireWorkspace, owner_tag });
    }
    void ReleaseWorkspace(int owner_tag) override
    {
        calls_.push_back({ kReleaseWorkspace, owner_tag });
    }
    Content_DiscQueueSlot* AllocateOverlayRead() override
    {
        calls_.push_back({ kAllocate });
        return requests_.At(allocate_);
    }
    void EnqueueOverlayRead(Content_DiscQueueSlot* read) override
    {
        calls_.push_back({ kEnqueue, requests_.Number(read) });
    }
    Content_DiscQueueSlot* CompleteRefusedOverlayRead() override
    {
        calls_.push_back({ kCompleteRefused });
        return requests_.At(3);
    }
    bool OverlayReadLoaded(const Content_DiscQueueSlot* read) override
    {
        return read != nullptr && read->state == kLoaded;
    }
    void ReleaseOverlayRead(Content_DiscQueueSlot* read) override
    {
        calls_.push_back({ kFree, requests_.Number(read) });
    }
    void WaitForOverlayReady() override { calls_.push_back({ kWait }); }

private:
    Requests& requests_;
    Calls& calls_;
    std::int64_t allocate_;
};

class RecordingPainter final : public sd::ScreenOverlayPainter {
public:
    explicit RecordingPainter(Calls& calls)
        : calls_(calls)
    {
    }

    void WarpPolygons(sd::RadialWarpSettings /*settings*/) override
    {
        calls_.push_back({ kWarp });
    }
    void DrawFeedbackTiles(std::uint16_t /*feedback_layer*/) override
    {
        calls_.push_back({ kTiles });
    }
    void DrawVignette() override { calls_.push_back({ kVignette }); }

private:
    Calls& calls_;
};

class RecordingPages final : public sd::FeedbackTexturePages {
public:
    explicit RecordingPages(Calls& calls)
        : calls_(calls)
    {
    }

    void Reserve(const sd::TexturePageLease& /*lease*/) override { }
    void Retire(const sd::TexturePageLease& lease) override
    {
        calls_.push_back({ kRetire, static_cast<std::int32_t>(lease.first_page),
            static_cast<std::int32_t>(lease.page_count), lease.owner_tag });
    }

private:
    Calls& calls_;
};
} // namespace

int main()
{
    std::int64_t mode = 0;
    std::int64_t controller = 0;
    std::int64_t loading = 0;
    std::int64_t reserved = 0;
    std::int64_t layer = 0;
    std::int64_t scale = 0;
    std::int64_t growth = 0;
    std::int64_t vignette = 0;
    std::int64_t request = 0;
    std::int64_t begin = 0;
    std::int64_t allocate = 0;
    std::int64_t queued_state = 0;
    std::int64_t earlier_state = 0;
    while (std::cin >> mode >> controller >> loading >> reserved >> layer >> scale
           >> growth >> vignette >> request >> begin >> allocate >> queued_state
           >> earlier_state) {
        Requests requests;
        requests.records[0].state = static_cast<std::int16_t>(queued_state);
        requests.records[1].state = static_cast<std::int16_t>(earlier_state);
        Calls calls;
        RecordingLoader loader(requests, calls, allocate);
        RecordingPainter painter(calls);
        RecordingPages pages(calls);
        sd::ScreenOverlayLifetime lifetime { .attached = controller != 0,
            .loading = static_cast<std::uint8_t>(loading),
            .controls = { .feedback_pages_reserved = static_cast<std::uint8_t>(reserved),
                .feedback_layer = static_cast<std::uint16_t>(layer),
                .radial_warp = { .scale = static_cast<std::uint16_t>(scale),
                    .growth = static_cast<std::int16_t>(growth) },
                .vignette_enabled = static_cast<std::int32_t>(vignette) },
            .read = requests.At(request) };

        switch (mode) {
        case kReset:
            sd::ResetScreenOverlay(lifetime);
            break;
        case kLoad:
            sd::LoadScreenOverlay(lifetime, static_cast<std::int32_t>(begin), loader);
            break;
        case kDraw:
            sd::DrawScreenOverlay(lifetime, loader, painter);
            break;
        case kAcquire:
            sd::AcquireScreenOverlay(lifetime, loader);
            break;
        case kRelease:
            sd::ReleaseScreenOverlay(lifetime, loader, pages);
            break;
        default:
            break;
        }

        const std::int64_t attached_controller =
            lifetime.attached ? (controller != 0 ? controller : 1) : 0;
        std::cout << attached_controller << ' ' << int { lifetime.loading } << ' '
                  << int { lifetime.controls.feedback_pages_reserved } << ' '
                  << lifetime.controls.feedback_layer << ' '
                  << lifetime.controls.radial_warp.scale << ' '
                  << lifetime.controls.radial_warp.growth << ' '
                  << lifetime.controls.vignette_enabled << ' '
                  << requests.Number(lifetime.read) << ' ' << calls.size();
        for (const auto& call : calls) {
            for (const std::int64_t item : call) {
                std::cout << ' ' << item;
            }
        }
        std::cout << '\n';
    }
    return 0;
}
