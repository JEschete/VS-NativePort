// Native side of tools\port\differentials\sphere_target_labels.py.
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <iostream>
#include <span>
#include <sstream>
#include <string>
#include <vector>

#include "Actors/AppearanceLive.hpp"
#include "Battle/SphereContainmentLive.hpp"
#include "Battle/SphereTargetLabelsLive.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Render/GameFontLive.hpp"
#include "Render/SpritesLive.hpp"
#include "Render/UiPacketsLive.hpp"

extern "C" void Core_Errorf(const char*, const char*, ...) { std::abort(); }

extern "C" {
#include "Battle/BattleTypes.h"
#include "Battle/Sphere.h"
#include "Battle/TargetVolume.h"
#include "Battle/TargetingState.h"
#include "Core/Gte.h"
#include "Menu/Framework/Menus.h"
}

extern "C" {
Battle_TargetSphereSetupView* Battle_targetSphere = nullptr;
u_char Battle_targetColumnTopOffset = 0;
char Menu_Framework_rowAnimationSteps[36] = { };
}

namespace {
using Value = long long;
using Word = std::uint32_t;

constexpr std::size_t kLabels = 24;
constexpr std::size_t kSteps = 16;
constexpr std::size_t kRecords = 8;
constexpr std::size_t kLabelFields = 10;
constexpr std::size_t kRecordFields = 5;
constexpr std::size_t kOrderingSlots = 512;
constexpr std::ptrdiff_t kOrderingOrigin = 300;
constexpr Word kPrimitiveStart = 0x80700000;
constexpr Word kBoxBytes = 0x38;
constexpr Value kUnknownSlot = 0x7FFF;

enum Event : Value {
    kLine = 1,
    kTpage,
    kGradient,
    kText,
    kBox,
    kView,
    kProject,
    kVolume,
    kEnable,
    kDisable,
};

struct Vector {
    Value function = 0;
    Value anchor = 0;
    Value ring = 0;
    Value pulse_head = 0;
    Value column_top = 0;
    Value project_dx = 0;
    Value project_dy = 0;
    Value label_index = 0;
    Value count = 0;
    std::array<Value, kSteps> steps { };
    std::array<std::array<Value, kLabelFields>, kLabels> labels { };
    std::array<std::array<Value, kRecordFields>, kRecords> records { };

    bool Read(std::istream& input)
    {
        if (!(input >> function >> anchor >> ring >> pulse_head >> column_top
                >> project_dx >> project_dy >> label_index >> count)) {
            return false;
        }
        for (Value& step : steps) {
            if (!(input >> step))
                return false;
        }
        for (auto& label : labels) {
            for (Value& field : label) {
                if (!(input >> field))
                    return false;
            }
        }
        for (auto& record : records) {
            for (Value& field : record) {
                if (!(input >> field))
                    return false;
            }
        }
        return function >= 0 && function < 6 && label_index >= 0
            && static_cast<std::size_t>(label_index) < kLabels;
    }
};

struct Harness {
    std::vector<Value> events;
    std::array<u_long, kOrderingSlots> ordering { };
    std::array<std::array<char, 4>, kLabels> texts { };
    Word cursor = kPrimitiveStart;
    Value project_dx = 0;
    Value project_dy = 0;
    SVECTOR projected { };

    u_long* Origin() { return ordering.data() + kOrderingOrigin; }

    Value Slot(const u_long* chain)
    {
        if (chain == nullptr)
            return kUnknownSlot;
        return static_cast<Value>(chain - Origin());
    }

    Value TextIndex(const char* text) const
    {
        for (std::size_t index = 0; index < texts.size(); ++index) {
            if (text == texts[index].data())
                return static_cast<Value>(index);
        }
        return -1;
    }

    void Add(std::initializer_list<Value> values) { events.insert(events.end(), values); }
};

Harness harness;

Value Unsigned(int value) { return std::bit_cast<Word>(value); }

Word GradientStop(Word gradient, Word color_index, Word brightness)
{
    return (gradient * 0x01000193U) ^ (color_index * 0x9E3779B1U) ^ (brightness << 7U)
         ^ 0x2C5A9E11U;
}
} // namespace

namespace VS::Render {
u_long* LiveFrameOrderingEntry(LiveOrderingBase base, int slot)
{
    if (base != LiveOrderingBase::kOverlay)
        std::abort();
    const std::ptrdiff_t index = kOrderingOrigin + slot;
    if (index < 0 || index >= static_cast<std::ptrdiff_t>(kOrderingSlots))
        return nullptr;
    return harness.ordering.data() + index;
}

void InsertLiveThreeWordPrimitive(u_long* before, int rgb0, int xy, int wh)
{
    harness.Add(
        { kLine, harness.Slot(before), Unsigned(rgb0), Unsigned(xy), Unsigned(wh) });
}

void InsertLiveDrawModeCommand(int tpage, u_long* before)
{
    harness.Add({ kTpage, harness.Slot(before), Unsigned(tpage) });
}

std::uint32_t LivePacketQueue::ChainLink() const
{
    return static_cast<std::uint32_t>(*chain_);
}

bool LivePacketQueue::CanEmit(std::size_t) const { return chain_ != nullptr; }

// Records the box and links the chain to it as the original's cursor would.
bool LivePacketQueue::Emit(std::span<const std::uint32_t> packet)
{
    harness.events.push_back(kBox);
    harness.events.push_back(harness.Slot(chain_));
    for (const std::uint32_t word : packet)
        harness.events.push_back(word);
    *chain_ = harness.cursor & 0xFFFFFFU;
    harness.cursor += kBoxBytes;
    return true;
}

void DrawLiveCompactTextColor(const char* text, int xy, int color, u_long* chain)
{
    harness.Add({ kText, harness.Slot(chain), harness.TextIndex(text), Unsigned(xy),
        Unsigned(color) });
}

void LoadLiveFrameViewMatrix() { harness.Add({ kView }); }
} // namespace VS::Render

namespace VS::Menu::Framework {
int LiveGradientStopColor(u_int gradient, u_int color_index, int brightness)
{
    const auto bright = std::bit_cast<Word>(brightness);
    harness.Add({ kGradient, gradient, color_index, bright });
    return std::bit_cast<int>(GradientStop(gradient, color_index, bright));
}
} // namespace VS::Menu::Framework

extern "C" {
void Core_Gte_LoadV0(const SVECTOR* v)
{
    harness.projected = *v;
    harness.Add({ kProject, v->vx, v->vy, v->vz });
}

void Core_Gte_Rtps(void) { }

void Core_Gte_StoreSxy2(long* out)
{
    const Word x = static_cast<Word>(harness.projected.vx + harness.project_dx);
    const Word y = static_cast<Word>(harness.projected.vy + harness.project_dy);
    *out = std::bit_cast<long>((x & 0xFFFFU) | (y << 16U));
}
}

namespace VS::Battle {
int IsLivePointInCurrentSphereVolume(
    int volume_index, std::span<const std::uint16_t> point)
{
    if (point.size() != 3)
        std::abort();
    harness.Add({ kVolume, volume_index, point[0], point[1], point[2] });
    return (point[0] ^ point[1] ^ point[2]) & 1;
}
} // namespace VS::Battle

namespace VS::Actors {
int EnableLiveModelPalettePulse(int actor_id, int pulse_flags, int pulse_slot)
{
    harness.Add(
        { kEnable, Unsigned(actor_id), Unsigned(pulse_flags), Unsigned(pulse_slot) });
    return 0;
}

int DisableLiveModelPalettePulse(int actor_id, int pulse_slot)
{
    harness.Add({ kDisable, Unsigned(actor_id), Unsigned(pulse_slot) });
    return 0;
}
} // namespace VS::Actors

namespace {
Battle_TargetSphere sphere;

void Load(const Vector& value)
{
    sphere = { };
    harness.events.clear();
    harness.ordering.fill(0);
    for (std::ptrdiff_t slot = 0; slot < 3; ++slot) {
        harness.Origin()[slot] = 0x00A00000U + static_cast<u_long>(slot) * 0x1111U;
    }
    harness.cursor = kPrimitiveStart;
    harness.project_dx = value.project_dx;
    harness.project_dy = value.project_dy;
    Battle_targetSphere = reinterpret_cast<Battle_TargetSphereSetupView*>(&sphere);
    Battle_targetColumnTopOffset = static_cast<u_char>(value.column_top);
    for (std::size_t index = 0; index < kSteps; ++index) {
        Menu_Framework_rowAnimationSteps[index] =
            static_cast<char>(static_cast<std::uint8_t>(value.steps[index]));
    }
    for (std::size_t index = 0; index < kLabels; ++index) {
        const auto& fields = value.labels[index];
        Battle_SphereTargetLabel& label = sphere.labels[index];
        label.state = static_cast<char>(fields[0]);
        label.depth = static_cast<char>(fields[1]);
        label.gradient = static_cast<char>(fields[2]);
        label.ringSize = static_cast<char>(fields[3]);
        label.screenX = static_cast<short>(fields[4]);
        label.offsetY = static_cast<u_char>(fields[5]);
        label.fade = static_cast<char>(fields[6]);
        label.worldPosition = { static_cast<short>(fields[7]),
            static_cast<short>(fields[8]), static_cast<short>(fields[9]), 0 };
        label.text = harness.texts[index].data();
    }
}

std::array<Battle_TargetLabelLine, kRecords> Records(const Vector& value)
{
    std::array<Battle_TargetLabelLine, kRecords> records { };
    for (std::size_t index = 0; index < kRecords; ++index) {
        const auto& fields = value.records[index];
        records[index].opaque0[0] = static_cast<u_short>(fields[0]);
        records[index].opaque0[1] = static_cast<u_short>(fields[1]);
        records[index].opaque0[2] = static_cast<u_short>(fields[2]);
        records[index].opaque9 = static_cast<u_char>(fields[3]);
        records[index].opaqueA = static_cast<signed char>(fields[4]);
    }
    return records;
}

void PrintLabels()
{
    for (const Battle_SphereTargetLabel& label : sphere.labels) {
        std::printf("%u %u %u %u %d %u %u %d %d %d %lld ",
            static_cast<std::uint8_t>(label.state),
            static_cast<std::uint8_t>(label.depth),
            static_cast<std::uint8_t>(label.gradient),
            static_cast<std::uint8_t>(label.ringSize), label.screenX, label.offsetY,
            static_cast<std::uint8_t>(label.fade), label.worldPosition.vx,
            label.worldPosition.vy, label.worldPosition.vz,
            harness.TextIndex(label.text));
    }
}

void Run(const Vector& value)
{
    Load(value);
    int settled = 0;
    auto records = Records(value);
    switch (value.function) {
    case 0:
        VS::Battle::DrawRightTargetLabelLinkEntry(static_cast<int>(value.anchor),
            static_cast<int>(value.ring), static_cast<int>(value.pulse_head));
        break;
    case 1:
        VS::Battle::DrawLeftTargetLabelLinkEntry(static_cast<int>(value.anchor),
            static_cast<int>(value.ring), static_cast<int>(value.pulse_head));
        break;
    case 2:
        settled = VS::Battle::AreTargetLabelsSettledEntry();
        break;
    case 3:
        VS::Battle::DrawSphereTargetLabelEntry(
            &sphere.labels[static_cast<std::size_t>(value.label_index)]);
        break;
    case 4:
        VS::Battle::UpdateSphereTargetLabelsEntry();
        break;
    case 5:
        VS::Battle::UpdateSphereTargetHighlightsEntry(
            records.data(), static_cast<int>(value.count));
        break;
    default:
        std::abort();
    }
    std::printf("%zu ", harness.events.size());
    for (const Value event : harness.events)
        std::printf("%lld ", event);
    if (value.function == 2)
        std::printf("%d ", settled);
    if (value.function >= 2 && value.function <= 4)
        PrintLabels();
    std::printf("\n");
}
} // namespace

int main()
{
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty())
            continue;
        std::istringstream input(line);
        Vector value;
        if (!value.Read(input))
            return 2;
        Run(value);
    }
}
