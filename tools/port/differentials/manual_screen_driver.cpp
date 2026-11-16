// Native side of the manual screen differentials (manual_screen_common.py,
// #276). One vector per line, starting with the mode; argv[1] is MENUE.PRG,
// read for _helpFileCdFiles.
//
// The screen modes (func_80102E9C, _menuReady, vs_menuE_exec): the step byte,
// open delay, init flag, help result, the replies of _initMenuState,
// _showMenu and vs_mainmenu_ready, the background fade step, the current
// menu, the menu hide flag and the HUD slide. Prints the return value, the
// step, open delay, init flag, help result, current menu, hide flag and HUD
// slide, then the calls.
//
// The help modes (_initMenuState, _showMenu): the help step, scroll, line
// count, arrow frame, load counter, the text and asset pointers, both disc
// request pointers and their states, the _topMenu reply, both cursor memory
// bytes, the asset header's two words, the page text box state, the
// _getRoundedLineCount reply, the pressed, previous and repeated buttons, both
// settings bytes and the display state's 18 halfwords. Prints the return
// value, the help statics, the text box state and the display state, then the
// calls. Every printed value is an unsigned 32-bit word, as the original's
// registers and memory are read.
#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <optional>
#include <string_view>
#include <vector>

#include "Menu/ManualScreen.hpp"

namespace {
namespace manual = VS::Menu::manual;

enum Mode : int { kClear, kReady, kScreen, kInit, kPages };
enum CallKind : std::int64_t {
    kCallClear = 1,
    kCallReady,
    kCallFade,
    kCallInitHelp,
    kCallPages,
    kCallField,
    kCallHud,
    kCallPriority,
    kCallCommand,
    kCallBzero,
    kCallReserve,
    kCallRetire,
    kCallInfoInit,
    kCallDismiss,
    kCallTopics,
    kCallFree,
    kCallAlloc,
    kCallSlot,
    kCallEnqueue,
    kCallFreeSlot,
    kCallSprites,
    kCallCluts,
    kCallTextBox,
    kCallLines,
    kCallSfx,
    kCallButton,
    kCallText,
    kCallControlsBackground,
    kCallHide,
    kCallPageBackground,
    kCallFadeUp,
    kCallFadeDown,
    kCallContent,
    kCallCopy,
    // A call the original never makes.
    kCallRefuse = 90,
};

// The fixture addresses manual_screen_common.py uses.
constexpr std::uint32_t kDisplayState = 0x800F1BC8U;
constexpr std::uint32_t kDisplayStateBytes = 0x24;
constexpr std::uint32_t kAllocations = 0x80540000U;
constexpr std::uint32_t kAllocationStride = 0x1000;
constexpr std::uint32_t kSlots = 0x80530000U;
constexpr std::uint32_t kSlotStride = 0x10;
constexpr std::size_t kDisplayHalfwords = kDisplayStateBytes / 2;
// vs_battle_manualDisplayState: currentManual, then scrollPositions from the
// third halfword.
constexpr std::size_t kScrollMemoryHalfword = 2;

// _helpFileCdFiles in MENUE.PRG: an {lba, size} word pair per file.
constexpr std::size_t kHelpFileTableOffset = 0x2F8;
constexpr std::size_t kHelpFiles =
    std::size_t { manual::kTopicCount } * manual::kHelpFilesPerTopic;

struct HelpFileEntry {
    std::uint32_t lba = 0;
    std::uint32_t size = 0;
};
using HelpFileTable = std::array<HelpFileEntry, kHelpFiles>;

using Call = std::vector<std::int64_t>;

template <typename Value> bool Read(Value& value)
{
    long long read = 0;
    if (!(std::cin >> read))
        return false;
    value = static_cast<Value>(read);
    return true;
}

std::int64_t Word(std::int64_t value)
{
    return static_cast<std::int64_t>(static_cast<std::uint32_t>(value));
}

std::optional<HelpFileTable> LoadHelpFiles(const char* path)
{
    std::ifstream file(path, std::ios::binary);
    file.seekg(static_cast<std::streamoff>(kHelpFileTableOffset));
    std::array<unsigned char, kHelpFiles * 8> bytes { };
    if (!file.read(reinterpret_cast<char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()))) {
        return std::nullopt;
    }
    const auto word = [&bytes](std::size_t at) {
        return static_cast<std::uint32_t>(bytes[at])
             | (static_cast<std::uint32_t>(bytes[at + 1]) << 8)
             | (static_cast<std::uint32_t>(bytes[at + 2]) << 16)
             | (static_cast<std::uint32_t>(bytes[at + 3]) << 24);
    };
    HelpFileTable table { };
    for (std::size_t i = 0; i < table.size(); ++i) {
        table[i] = { .lba = word(i * 8), .size = word((i * 8) + 4) };
    }
    return table;
}

void PrintCalls(const std::vector<Call>& calls)
{
    std::cout << ' ' << calls.size();
    for (const Call& call : calls) {
        for (const std::int64_t word : call)
            std::cout << ' ' << word;
    }
    std::cout << '\n';
}

struct ScreenVector {
    std::uint8_t step = 0;
    manual::ScreenState state;
    std::int32_t init_reply = 0;
    std::int32_t pages_reply = 0;
    std::int32_t ready_reply = 0;
    std::int32_t fade_step = 0;
    std::uint8_t current_menu = 0;
    std::uint8_t hide_menu = 0;
    std::int32_t hud_slide = 0;
};

class RecordingScreenHost final : public manual::ScreenHost {
public:
    explicit RecordingScreenHost(ScreenVector& vector)
        : vector_(vector)
    {
    }

    std::vector<Call> calls;

    void ClearMenuExcept(std::int32_t row) override
    {
        calls.push_back({ kCallClear, Word(row) });
    }
    std::int32_t MenuRowsIdle() override
    {
        calls.push_back({ kCallReady });
        return vector_.ready_reply;
    }
    void SetBackgroundFade(std::int32_t fade) override
    {
        calls.push_back({ kCallFade, Word(fade) });
    }
    std::int32_t InitializeHelp() override
    {
        calls.push_back({ kCallInitHelp });
        return vector_.init_reply;
    }
    std::int32_t UpdateHelpPages() override
    {
        calls.push_back({ kCallPages });
        return vector_.pages_reply;
    }
    void SetFieldRenderingEnabled(std::int32_t enabled) override
    {
        calls.push_back({ kCallField, Word(enabled) });
    }
    void SetHudHidden(std::int32_t hidden) override
    {
        calls.push_back({ kCallHud, Word(hidden) });
    }
    void SetHudSlideOffset(std::int32_t offset) override { vector_.hud_slide = offset; }
    void SetBackgroundPriority(std::int32_t layer, std::int32_t brightness) override
    {
        calls.push_back({ kCallPriority, Word(layer), Word(brightness) });
    }
    void SetMenuCommand(std::int32_t command) override
    {
        calls.push_back({ kCallCommand, Word(command) });
    }
    std::int32_t BackgroundFadeStep() const override { return vector_.fade_step; }
    void SetCurrentMenu(std::int32_t menu) override
    {
        vector_.current_menu = static_cast<std::uint8_t>(menu);
    }
    void RevealMenu() override { vector_.hide_menu = 0; }

private:
    ScreenVector& vector_;
};

bool Screen(int mode)
{
    ScreenVector vector;
    if (!(Read(vector.step) && Read(vector.state.open_delay)
            && Read(vector.state.initialized) && Read(vector.state.help_result)
            && Read(vector.init_reply) && Read(vector.pages_reply)
            && Read(vector.ready_reply) && Read(vector.fade_step)
            && Read(vector.current_menu) && Read(vector.hide_menu)
            && Read(vector.hud_slide))) {
        return false;
    }
    RecordingScreenHost host(vector);
    std::int32_t result = 0;
    switch (mode) {
    case kClear:
        manual::ClearMenuExceptRow8(host);
        break;
    case kReady:
        manual::PollMenuReady(host);
        break;
    default:
        result = manual::UpdateScreen(host, vector.step, vector.state);
        break;
    }
    std::cout << Word(result) << ' ' << static_cast<int>(vector.step) << ' '
              << static_cast<int>(vector.state.open_delay) << ' '
              << static_cast<int>(vector.state.initialized) << ' '
              << Word(vector.state.help_result) << ' '
              << static_cast<int>(vector.current_menu) << ' '
              << static_cast<int>(vector.hide_menu) << ' ' << Word(vector.hud_slide);
    PrintCalls(host.calls);
    return true;
}

struct HelpVector {
    manual::HelpPagesState state;
    // Indexed by HelpFile: the asset file, then the text.
    std::array<std::uint32_t, 2> buffers { };
    std::array<std::uint32_t, 2> queues { };
    std::array<std::int16_t, 2> queue_states { };
    std::int32_t topic_reply = 0;
    manual::TopicCursor cursor;
    manual::HelpAssetHeader header;
    std::uint16_t text_box_state = 0;
    std::int32_t line_reply = 0;
    std::uint32_t pressed = 0;
    std::uint32_t previous = 0;
    std::uint32_t repeated = 0;
    std::uint8_t setting = 0;
    std::uint8_t other_setting = 0;
    std::array<std::uint16_t, kDisplayHalfwords> display { };
};

std::size_t Index(manual::HelpFile file) { return static_cast<std::size_t>(file); }

class RecordingHelpHost final : public manual::HelpPagesHost {
public:
    RecordingHelpHost(HelpVector& vector, const HelpFileTable& table)
        : vector_(vector)
        , table_(table)
    {
    }

    std::vector<Call> calls;

    std::uint8_t CursorMemorySetting() const override { return vector_.setting; }
    void ClearDisplayState() override
    {
        calls.push_back({ kCallBzero, kDisplayState, kDisplayStateBytes });
        vector_.display.fill(0);
    }
    void ReserveTexturePages(
        std::uint32_t first, std::uint32_t count, std::int32_t owner) override
    {
        calls.push_back({ kCallReserve, first, count, Word(owner) });
    }
    void RetireTexturePages(
        std::uint32_t first, std::uint32_t count, std::int32_t owner) override
    {
        calls.push_back({ kCallRetire, first, count, Word(owner) });
    }
    void SetBackgroundPriority(std::int32_t layer, std::int32_t brightness) override
    {
        calls.push_back({ kCallPriority, Word(layer), Word(brightness) });
    }
    void InitializeInformationBox() override { calls.push_back({ kCallInfoInit }); }
    void DismissInformationBox() override { calls.push_back({ kCallDismiss }); }
    std::int32_t UpdateTopicMenu(std::int32_t init) override
    {
        calls.push_back({ kCallTopics, Word(init) });
        return vector_.topic_reply;
    }
    manual::TopicCursor TopicMenuCursor() const override { return vector_.cursor; }
    std::uint16_t CurrentManual() const override { return vector_.display[0]; }
    void SetCurrentManual(std::uint16_t manual) override { vector_.display[0] = manual; }
    std::uint16_t ScrollMemory(std::size_t topic) const override
    {
        if (topic + kScrollMemoryHalfword >= vector_.display.size())
            return 0;
        return vector_.display[topic + kScrollMemoryHalfword];
    }
    void SetScrollMemory(std::size_t topic, std::uint16_t scroll) override
    {
        if (topic + kScrollMemoryHalfword >= vector_.display.size()) {
            calls.push_back({ kCallRefuse });
            return;
        }
        vector_.display[topic + kScrollMemoryHalfword] = scroll;
    }
    bool HelpBufferHeld(manual::HelpFile file) const override
    {
        return vector_.buffers[Index(file)] != 0;
    }
    void FreeHelpBuffer(manual::HelpFile file) override
    {
        calls.push_back({ kCallFree, vector_.buffers[Index(file)] });
    }
    void ForgetHelpBuffer(manual::HelpFile file) override
    {
        vector_.buffers[Index(file)] = 0;
    }
    void AllocateHelpBuffer(manual::HelpFile file, std::int32_t topic) override
    {
        const HelpFileEntry* const entry = Entry(file, topic);
        if (entry == nullptr)
            return;
        calls.push_back({ kCallAlloc, entry->size });
        vector_.buffers[Index(file)] =
            kAllocations + (kAllocationStride * allocations_++);
    }
    void RequestHelpFile(manual::HelpFile file, std::int32_t topic) override
    {
        const HelpFileEntry* const entry = Entry(file, topic);
        if (entry == nullptr)
            return;
        calls.push_back({ kCallSlot, entry->lba, entry->size });
        const std::uint32_t slot = kSlots + (kSlotStride * slots_++);
        calls.push_back({ kCallEnqueue, slot, vector_.buffers[Index(file)] });
        vector_.queues[Index(file)] = slot;
    }
    std::int16_t HelpFileState(manual::HelpFile file) const override
    {
        return vector_.queue_states[Index(file)];
    }
    void ReleaseHelpFile(manual::HelpFile file) override
    {
        calls.push_back({ kCallFreeSlot, vector_.queues[Index(file)] });
    }
    std::optional<manual::HelpAssetHeader> HelpAssets() const override
    {
        if (vector_.buffers[Index(manual::HelpFile::kAssets)] == 0) {
            return std::nullopt;
        }
        return vector_.header;
    }
    void UploadSpriteTiles(std::uint32_t byte_offset, std::uint32_t count) override
    {
        calls.push_back({ kCallSprites, byte_offset, count });
    }
    void UploadCluts(std::uint32_t byte_offset, std::uint32_t count) override
    {
        calls.push_back({ kCallCluts, byte_offset, count });
    }
    void InitTextBox(const manual::TextBoxPlacement& placement) override
    {
        calls.push_back({ kCallTextBox, Word(placement.id), Word(placement.flags),
            Word(placement.x), Word(placement.y), Word(placement.chars_per_line),
            Word(placement.line_count), Word(placement.center_x),
            Word(placement.center_y) });
    }
    void HideTextBox(std::int32_t id) override
    {
        calls.push_back({ kCallHide, Word(id) });
        vector_.text_box_state = 0;
    }
    std::int32_t LastPageLineOffset() override
    {
        calls.push_back(Snapshot(kCallLines));
        return vector_.line_reply;
    }
    std::uint32_t PressedButtons() const override { return vector_.pressed; }
    std::uint32_t PreviousButtons() const override { return vector_.previous; }
    std::uint32_t RepeatedButtons() const override { return vector_.repeated; }
    void PlaySfx(std::int32_t group, std::int32_t effect) override
    {
        calls.push_back({ kCallSfx, Word(group), Word(effect) });
    }
    void DrawButtonIcon(
        std::int32_t button, std::int32_t x, std::int32_t y, std::int32_t layer) override
    {
        calls.push_back({ kCallButton, Word(button), Word(x), Word(y), Word(layer * 4) });
    }
    void DrawText(std::string_view text, std::int32_t xy, std::int32_t color,
        std::int32_t layer) override
    {
        Call call { kCallText, static_cast<std::int64_t>(text.size()) };
        for (const char character : text) {
            call.push_back(static_cast<unsigned char>(character));
        }
        call.push_back(Word(xy));
        call.push_back(Word(color));
        call.push_back(Word(layer * 4));
        calls.push_back(call);
    }
    void DrawControlsBackground(const manual::ScreenRect& rect) override
    {
        calls.push_back({ kCallControlsBackground, Word(rect.x), Word(rect.y),
            Word(rect.w), Word(rect.h) });
    }
    void DrawPageBackground(const manual::ScreenRect& rect, std::int32_t color) override
    {
        calls.push_back({ kCallPageBackground, Word(rect.x), Word(rect.y), Word(rect.w),
            Word(rect.h), Word(color) });
    }
    void DrawUpperScrollFade() override { calls.push_back(Snapshot(kCallFadeUp)); }
    void DrawLowerScrollFade() override { calls.push_back(Snapshot(kCallFadeDown)); }
    void DrawPageContent() override { calls.push_back(Snapshot(kCallContent)); }
    void CopyPageText() override { calls.push_back(Snapshot(kCallCopy)); }
    void Refuse(std::string_view) override { calls.push_back({ kCallRefuse }); }

private:
    // The help statics a page drawing or text call reads.
    Call Snapshot(CallKind kind) const
    {
        const manual::HelpPagesState& state = vector_.state;
        return { kind, Word(state.step), Word(state.scroll), Word(state.line_count),
            Word(state.arrow_frame), Word(state.load_counter),
            vector_.buffers[Index(manual::HelpFile::kText)] };
    }

    const HelpFileEntry* Entry(manual::HelpFile file, std::int32_t topic)
    {
        if (topic < 0 || topic >= manual::kTopicCount) {
            calls.push_back({ kCallRefuse });
            return nullptr;
        }
        return &table_[(static_cast<std::size_t>(topic) * manual::kHelpFilesPerTopic)
                       + Index(file)];
    }

    HelpVector& vector_;
    const HelpFileTable& table_;
    std::uint32_t allocations_ = 0;
    std::uint32_t slots_ = 0;
};

bool Help(int mode, const HelpFileTable& table)
{
    HelpVector vector;
    manual::HelpPagesState& state = vector.state;
    bool read = Read(state.step) && Read(state.scroll) && Read(state.line_count)
             && Read(state.arrow_frame) && Read(state.load_counter);
    for (std::uint32_t& buffer : vector.buffers)
        read = read && Read(buffer);
    for (std::uint32_t& queue : vector.queues)
        read = read && Read(queue);
    for (std::int16_t& queue : vector.queue_states)
        read = read && Read(queue);
    read = read && Read(vector.topic_reply) && Read(vector.cursor.row)
        && Read(vector.cursor.scroll) && Read(vector.header.sprite_tiles)
        && Read(vector.header.cluts) && Read(vector.text_box_state)
        && Read(vector.line_reply) && Read(vector.pressed) && Read(vector.previous)
        && Read(vector.repeated) && Read(vector.setting) && Read(vector.other_setting);
    for (std::uint16_t& halfword : vector.display) {
        read = read && Read(halfword);
    }
    if (!read)
        return false;
    RecordingHelpHost host(vector, table);
    const std::int32_t result = mode == kInit ? manual::InitHelpState(host, state)
                                              : manual::UpdateHelpPages(host, state);
    const std::size_t text = Index(manual::HelpFile::kText);
    const std::size_t assets = Index(manual::HelpFile::kAssets);
    std::cout << Word(result) << ' ' << Word(state.step) << ' ' << Word(state.scroll)
              << ' ' << Word(state.line_count) << ' ' << Word(state.arrow_frame) << ' '
              << Word(state.load_counter) << ' ' << vector.buffers[text] << ' '
              << vector.buffers[assets] << ' ' << vector.queues[text] << ' '
              << vector.queues[assets] << ' ' << vector.text_box_state;
    for (const std::uint16_t halfword : vector.display) {
        std::cout << ' ' << halfword;
    }
    PrintCalls(host.calls);
    return true;
}
} // namespace

int main(int argc, char** argv)
{
    if (argc < 2)
        return 2;
    const std::optional<HelpFileTable> table = LoadHelpFiles(argv[1]);
    if (!table.has_value())
        return 2;
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);
    int mode = 0;
    while (Read(mode)) {
        const bool read = mode <= kScreen ? Screen(mode) : Help(mode, *table);
        if (!read)
            return 1;
    }
    return 0;
}
