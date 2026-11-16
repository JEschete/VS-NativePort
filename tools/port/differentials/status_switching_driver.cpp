// Native side of the status actor switching differentials (status_switching_common.py, #276).
// One vector per line: mode, the five MENU4.PRG int globals from _previousActor, the five state
// bytes from _renderBasicStatsState, _switchCurrentActor's state word, the HUD slide offset, the
// ordering-table word, the three label positions, widths and texture u values, four argument
// words, the eight words vs_battle_renderValue returns and 15 actor slots (present, state flags,
// kind, five vitals halfwords, the analyzed word, six limb name indices, the accessory id).
// Prints the result, the globals, the name row's selected byte and the calls (seven words each).
#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <iostream>
#include <optional>
#include <vector>

#include "Menu/Status/ActorSwitching.hpp"

namespace {
namespace st = VS::Menu::Status;

enum Mode : int {
    kInit,
    kNext,
    kSwitch,
    kUnknown,
    kBar,
    kVitals,
    kLimbs,
    kRows,
};
enum CallKind : std::int32_t {
    kCallHud = 1,
    kCallFlags,
    kCallSound,
    kCallMenuItem,
    kCallRelease,
    kCallLoad,
    kCallSprite,
    kCallLabel,
    kCallValue,
    kCallGauge,
};

constexpr std::size_t kSlots = 15;
constexpr std::size_t kValueReturns = 8;
constexpr std::uint32_t kAnalyzedBit = 1U << 17;
constexpr std::uint32_t kChainBytes = 0xC;

using Call = std::array<std::int32_t, 7>;

struct Slot {
    int present = 0;
    std::uint32_t flags = 0;
    std::uint16_t kind = 0;
    std::array<std::int16_t, 5> vitals {};
    std::uint32_t analyzed = 0;
    std::array<std::uint8_t, st::kStatusLimbSlots> limbs {};
    std::uint16_t accessory = 0;
};

std::vector<Call> calls;

class DriverVitalsHost final : public st::VitalsHost {
public:
    std::deque<std::int32_t> returns;
    std::int32_t chain = 0;

    void SetHudHidden(std::int32_t hidden) override { calls.push_back({ kCallHud, hidden }); }
    std::int32_t DrawValue(std::int32_t font, std::int32_t xy, std::int32_t value) override
    {
        calls.push_back({ kCallValue, font, xy, value, chain });
        if (returns.empty())
            return 0;
        const std::int32_t left = returns.front();
        returns.pop_front();
        return left;
    }
    void QueueSprite(const st::VitalsSprite& sprite) override
    {
        calls.push_back({ kCallSprite, sprite.brightness, sprite.xy, sprite.wh, chain,
            static_cast<std::int32_t>(sprite.uv_clut) });
    }
    void QueueLabelSprite(const st::VitalsSprite& sprite) override
    {
        calls.push_back({ kCallLabel, sprite.brightness, sprite.xy, sprite.wh, chain,
            static_cast<std::int32_t>(sprite.uv_clut) });
    }
    void DrawGauge(std::int32_t color, std::int32_t fill, std::int32_t xy) override
    {
        calls.push_back({ kCallGauge, color, fill, chain, xy });
    }
};

class DriverRoster final : public st::InspectableActorRoster {
public:
    explicit DriverRoster(const std::array<Slot, kSlots>& slots)
        : slots_(slots)
    {
    }
    std::uint32_t StateFlags(std::int32_t slot) override
    {
        calls.push_back({ kCallFlags, slot });
        return slot >= 0 && slot < static_cast<std::int32_t>(kSlots)
            ? slots_[static_cast<std::size_t>(slot)].flags
            : 0;
    }
    std::optional<std::uint16_t> Kind(std::int32_t slot) override
    {
        if (slot < 0 || slot >= static_cast<std::int32_t>(kSlots))
            return std::nullopt;
        const Slot& actor = slots_[static_cast<std::size_t>(slot)];
        if (actor.present == 0)
            return std::nullopt;
        return actor.kind;
    }

private:
    const std::array<Slot, kSlots>& slots_;
};

class DriverSwitchHost final : public st::ActorSwitchHost {
public:
    int selected = 0;

    void PlaySwitchSound() override { calls.push_back({ kCallSound }); }
    void ShowActorName(std::int32_t slot, const st::ActorNameRow& row) override
    {
        calls.push_back({ kCallMenuItem, row.row, row.x, row.y, row.width, row.background_width, slot });
        selected = row.selected ? 1 : 0;
    }
    void ReleaseActorPreview() override { calls.push_back({ kCallRelease }); }
    void LoadActorPreview(std::int32_t slot) override { calls.push_back({ kCallLoad, slot }); }
};

template <typename Value> bool Read(Value& value)
{
    long long read = 0;
    if (!(std::cin >> read))
        return false;
    value = static_cast<Value>(read);
    return true;
}

template <typename Value, std::size_t Size> bool ReadAll(std::array<Value, Size>& values)
{
    for (Value& value : values) {
        if (!Read(value))
            return false;
    }
    return true;
}

bool ReadSlot(Slot& slot)
{
    return Read(slot.present) && Read(slot.flags) && Read(slot.kind) && ReadAll(slot.vitals)
        && Read(slot.analyzed) && ReadAll(slot.limbs) && Read(slot.accessory);
}

std::optional<st::ActorVitals> VitalsAt(const std::array<Slot, kSlots>& slots, std::int32_t actor)
{
    const std::int32_t slot = static_cast<std::int32_t>(static_cast<std::uint32_t>(actor) - 1U);
    if (slot < 0 || slot >= static_cast<std::int32_t>(kSlots))
        return std::nullopt;
    const Slot& record = slots[static_cast<std::size_t>(slot)];
    if (record.present == 0)
        return std::nullopt;
    return st::ActorVitals { .current_hp = record.vitals[0],
        .max_hp = record.vitals[1],
        .current_mp = record.vitals[2],
        .max_mp = record.vitals[3],
        .risk = static_cast<std::uint16_t>(record.vitals[4]),
        .analyzed = (record.analyzed & kAnalyzedBit) != 0 };
}

int RunVector(int mode)
{
    std::array<std::int32_t, 5> globals {};
    std::array<std::uint8_t, 5> state_bytes {};
    std::uint32_t phase = 0;
    std::int32_t hud = 0;
    std::uint32_t ordering_table = 0;
    st::VitalsLabelLayout labels;
    std::array<std::int32_t, 4> arguments {};
    std::array<std::int32_t, kValueReturns> returns {};
    std::array<Slot, kSlots> slots {};
    if (!ReadAll(globals) || !ReadAll(state_bytes) || !Read(phase) || !Read(hud) || !Read(ordering_table)
        || !ReadAll(labels.positions) || !ReadAll(labels.widths) || !ReadAll(labels.texture_u)
        || !ReadAll(arguments) || !ReadAll(returns))
        return 1;
    for (Slot& slot : slots) {
        if (!ReadSlot(slot))
            return 1;
    }
    calls.clear();
    DriverVitalsHost vitals_host;
    vitals_host.returns.assign(returns.begin(), returns.end());
    vitals_host.chain = static_cast<std::int32_t>(ordering_table - kChainBytes);
    DriverSwitchHost switch_host;
    st::VitalsPanelState panel { .phase = state_bytes[0],
        .slide = state_bytes[1],
        .delay = state_bytes[2],
        .shown_actor = state_bytes[3] };
    st::ActorSwitchState switching { .phase = phase,
        .selected_actor = globals[1],
        .animation_step = globals[4],
        .draw_background_first = globals[2],
        .fade_screen = globals[3],
        .delay_screen_update = state_bytes[4] };
    std::int32_t result = 0;
    switch (mode) {
    case kInit:
        st::BeginVitalsTransition(panel, arguments[0], vitals_host);
        break;
    case kNext: {
        DriverRoster roster(slots);
        const std::optional<std::int32_t> next = st::FindNextInspectableActor(roster, arguments[0], arguments[1]);
        if (!next.has_value())
            return 1;
        result = *next;
        break;
    }
    case kSwitch:
        result = st::AnimateActorSwitch(switching, arguments[0], switch_host) ? 1 : 0;
        break;
    case kUnknown:
        vitals_host.chain = arguments[2];
        st::DrawUnanalyzedValue(vitals_host, arguments[0], arguments[1] != 0);
        break;
    case kBar:
        st::DrawVitalBar(vitals_host, arguments[0], arguments[1], arguments[2], arguments[3]);
        break;
    case kVitals: {
        const st::VitalsFrame frame { .selected = VitalsAt(slots, globals[1]),
            .previous = VitalsAt(slots, globals[0]),
            .hud_slide_offset = hud,
            .labels = labels };
        if (!st::DrawVitals(panel, frame, vitals_host))
            return 1;
        break;
    }
    case kLimbs:
        if (arguments[0] < 0 || arguments[0] >= static_cast<std::int32_t>(kSlots))
            return 1;
        result = st::LimbCount(slots[static_cast<std::size_t>(arguments[0])].limbs);
        break;
    case kRows: {
        const auto slot = static_cast<std::size_t>(static_cast<std::uint32_t>(globals[1]) - 1U);
        if (slot >= kSlots)
            return 1;
        result = st::EquipmentRowCount(slots[slot].limbs, slots[slot].accessory);
        break;
    }
    default:
        return 1;
    }

    std::cout << result << ' ' << globals[0] << ' ' << switching.selected_actor << ' '
              << switching.draw_background_first << ' ' << switching.fade_screen << ' '
              << switching.animation_step << ' ' << int { panel.phase } << ' ' << int { panel.slide } << ' '
              << int { panel.delay } << ' ' << int { panel.shown_actor } << ' '
              << int { switching.delay_screen_update } << ' ' << switching.phase << ' ' << switch_host.selected
              << ' ' << calls.size();
    for (const Call& call : calls) {
        for (const std::int32_t field : call)
            std::cout << ' ' << field;
    }
    std::cout << '\n';
    return 0;
}
} // namespace

int main()
{
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    int mode = 0;
    while (Read(mode)) {
        if (RunVector(mode) != 0)
            return 1;
    }
    return 0;
}
