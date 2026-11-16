// Native side of the equipment row and preview model differentials
// (equipment_rows_common.py, #276). One vector per line: the mode, then the
// mode's inputs in the order the harness lists them. Prints a refusal marker,
// the observed globals and fixture fields, then the recorded host calls.
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <optional>
#include <span>
#include <vector>

#include "Menu/Inventory/EquipmentRows.hpp"

namespace {
namespace inv = VS::Menu::Inventory;
using VS::Content::ActorAssetRequest;
using VS::Content::AssetAddress;
using VS::Menu::Framework::ItemText;
using VS::Menu::Framework::MenuRow;
using VS::Menu::Framework::RowPlacement;
using VS::Menu::Framework::RowState;

enum Mode : int {
    kCreate,
    kSelected,
    kUpdate,
    kAccessory,
    kReload,
    kTwoHanded,
    kQueue,
};

enum Call : std::int64_t {
    kCallAlloc = 1,
    kCallFree,
    kCallSetRow,
    kCallGetRow,
    kCallInfo,
    kCallIdle,
    kCallLeave,
    kCallInvalid,
    kCallChange,
    kCallSfx,
    kCallCursor,
    kCallResetStats = 20,
    kCallApply,
    kCallPrimary,
    kCallPages,
    kCallRelease = 30,
    kCallAnimations,
    kCallDrawn,
    kCallBusy,
    kCallResetEquip,
    kCallReleasePreview,
    kCallClone,
    kCallResetPreview,
    kCallPlay,
    kCallAdvance,
    kCallCopy,
    kCallQueue,
};

constexpr std::int64_t kRefused = -999;
constexpr std::size_t kRows = 64;
constexpr int kFirstRow = 20;
constexpr int kVisible = 6;
constexpr std::int64_t kRecordBytes = 128;
constexpr std::int64_t kSfxChannel = 0x7E;
constexpr std::int64_t kBlendPose = 0xC54;
constexpr std::int64_t kPose = 0x704;
constexpr std::int64_t kPoseBytes = 0x550;
constexpr std::uint32_t kWeaponDrawnBit = 0x800000;
constexpr std::int64_t kArmorBytes = 40;

using Calls = std::vector<std::vector<std::int64_t>>;

template <typename Value> bool Read(Value& value)
{
    long long read = 0;
    if (!(std::cin >> read)) {
        return false;
    }
    value = static_cast<Value>(read);
    return true;
}

class ListHost final : public inv::EquipmentRowsHost {
public:
    Calls calls;
    std::vector<ItemText> storage;
    std::array<MenuRow, kRows> rows { };
    bool idle = false;
    std::int32_t cursor_return = 0;

    [[nodiscard]] std::int64_t Offset(const void* data) const
    {
        if (data == nullptr) {
            return -1;
        }
        return static_cast<const std::uint8_t*>(data)
             - reinterpret_cast<const std::uint8_t*>(storage.data());
    }

    std::span<ItemText> AllocateText(std::size_t count) override
    {
        calls.push_back({ kCallAlloc, static_cast<std::int64_t>(count) * kRecordBytes });
        storage.assign(count, ItemText { });
        return storage;
    }
    void FreeText(std::span<ItemText> text) override
    {
        calls.push_back({ kCallFree, Offset(text.data()) });
    }
    MenuRow& SetRow(
        const RowPlacement& placement, std::span<const std::uint8_t> text) override
    {
        calls.push_back({ kCallSetRow, placement.row, placement.x, placement.y,
            placement.w, placement.background_width, Offset(text.data()) });
        return rows.at(static_cast<std::size_t>(placement.row));
    }
    MenuRow& Row(int row) override
    {
        calls.push_back({ kCallGetRow, row });
        return rows.at(static_cast<std::size_t>(row));
    }
    void ShowInformation(std::span<const std::uint8_t> message) override
    {
        calls.push_back({ kCallInfo, Offset(message.data()) });
    }
    bool RowsIdle() override
    {
        calls.push_back({ kCallIdle });
        return idle;
    }
    void PlayLeaveSound() override { calls.push_back({ kCallLeave }); }
    void PlayInvalidSound() override { calls.push_back({ kCallInvalid }); }
    void PlayChangeSound() override { calls.push_back({ kCallChange }); }
    void PlayConfirmSound(std::int32_t effect) override
    {
        calls.push_back({ kCallSfx, kSfxChannel, effect });
    }
    std::int32_t DrawCursor(std::uint8_t animation, std::int32_t position) override
    {
        calls.push_back({ kCallCursor, animation, position });
        return cursor_return;
    }
};

bool ReadRows(ListHost& host)
{
    for (int i = 0; i < kVisible; ++i) {
        MenuRow& row = host.rows.at(static_cast<std::size_t>(kFirstRow + i));
        std::uint8_t state = 0;
        if (!Read(state) || !Read(row.gradient_state) || !Read(row.is_scrollable)
            || !Read(row.selected) || !Read(row.unselectable) || !Read(row.row_icon)
            || !Read(row.material) || !Read(row.outset_icon) || !Read(row.target0)) {
            return false;
        }
        row.state = static_cast<RowState>(state);
    }
    return true;
}

void PrintRows(const ListHost& host)
{
    for (int i = 0; i < kVisible; ++i) {
        const MenuRow& row = host.rows.at(static_cast<std::size_t>(kFirstRow + i));
        std::cout << ' ' << static_cast<int>(row.state) << ' '
                  << static_cast<int>(row.gradient_state) << ' '
                  << static_cast<int>(row.is_scrollable) << ' '
                  << static_cast<int>(row.selected) << ' '
                  << static_cast<int>(row.unselectable) << ' '
                  << static_cast<int>(row.row_icon) << ' '
                  << static_cast<int>(row.material) << ' '
                  << static_cast<int>(row.outset_icon) << ' ' << row.target0;
    }
}

void PrintGlobals(const ListHost& host, const inv::EquipmentRowsState& state)
{
    std::cout << ' ' << host.Offset(state.text.data()) << ' ' << state.selected_row << ' '
              << static_cast<int>(state.step) << ' ' << static_cast<int>(state.row_count)
              << ' ' << static_cast<int>(state.first_row) << ' '
              << static_cast<int>(state.equipped_item) << ' '
              << static_cast<int>(state.cursor_animation);
}

void PrintCalls(const Calls& calls)
{
    std::cout << ' ' << calls.size();
    for (const auto& call : calls) {
        for (const std::int64_t word : call) {
            std::cout << ' ' << word;
        }
    }
    std::cout << '\n';
}

int RunCreate()
{
    std::size_t count = 0;
    inv::EquipmentRowsState state;
    if (!Read(count) || !Read(state.selected_row) || !Read(state.first_row)
        || !Read(state.equipped_item)) {
        return 1;
    }
    std::vector<std::vector<std::uint8_t>> names(count);
    std::vector<std::vector<std::uint8_t>> descriptions(count);
    std::vector<inv::EquipmentRowSource> sources(count);
    for (std::size_t i = 0; i < count; ++i) {
        int present = 0;
        if (!Read(sources[i].flags) || !Read(present)) {
            return 1;
        }
        names[i].resize(present != 0 ? inv::kEquipmentNameBytes : 0);
        for (std::uint8_t& item : names[i]) {
            if (!Read(item)) {
                return 1;
            }
        }
        if (!Read(present)) {
            return 1;
        }
        descriptions[i].resize(present != 0 ? inv::kEquipmentDescriptionBytes : 0);
        for (std::uint8_t& item : descriptions[i]) {
            if (!Read(item)) {
                return 1;
            }
        }
        sources[i].name = names[i];
        sources[i].description = descriptions[i];
    }
    ListHost host;
    if (!ReadRows(host)) {
        return 1;
    }
    const bool created = inv::CreateEquipmentRows(state, host, sources);
    std::cout << (created ? 0 : kRefused);
    PrintGlobals(host, state);
    for (const ItemText& record : state.text) {
        std::array<std::uint32_t, sizeof(ItemText) / 4> words { };
        std::memcpy(words.data(), &record, sizeof(ItemText));
        for (const std::uint32_t word : words) {
            std::cout << ' ' << word;
        }
    }
    PrintRows(host);
    PrintCalls(host.calls);
    return 0;
}

int RunSelected()
{
    int text = 0;
    inv::EquipmentRowsState state;
    if (!Read(text) || !Read(state.selected_row)) {
        return 1;
    }
    std::array<ItemText, 1> record { };
    if (text != 0) {
        state.text = record;
    }
    std::cout << inv::SelectedEquipmentRow(state);
    PrintCalls({ });
    return 0;
}

int RunUpdate()
{
    std::size_t count = 0;
    inv::EquipmentRowsState state;
    inv::EquipmentChoiceInput input;
    std::int32_t has_sfx = 0;
    int idle = 0;
    ListHost host;
    if (!Read(count) || !Read(state.first_row) || !Read(state.selected_row)
        || !Read(state.step) || !Read(state.equipped_item)
        || !Read(state.cursor_animation) || !Read(input.pressed) || !Read(input.repeat)
        || !Read(has_sfx) || !Read(idle) || !Read(host.cursor_return)) {
        return 1;
    }
    state.row_count = static_cast<std::uint8_t>(count);
    host.idle = idle != 0;
    host.storage.assign(count, ItemText { });
    for (ItemText& record : host.storage) {
        if (!Read(record.flags)) {
            return 1;
        }
    }
    state.text = host.storage;
    if (!ReadRows(host)) {
        return 1;
    }
    const bool updated = inv::UpdateEquipmentChoices(state, host, input, has_sfx);
    std::cout << (updated ? 0 : kRefused);
    PrintGlobals(host, state);
    PrintRows(host);
    PrintCalls(host.calls);
    return 0;
}

class AccessoryHost final : public inv::AccessoryPreviewHost {
public:
    Calls calls;
    std::int32_t armor = 0;
    inv::AccessoryPreview preview;
    inv::AccessoryStatColumns columns;
    std::uint8_t subtype = 0;
    std::uint8_t status = 0;

    bool HasAccessory(std::int32_t slot) override { return slot >= 0 && slot < armor; }
    void ResetStats() override { calls.push_back({ kCallResetStats }); }
    inv::AccessoryPreview LoadAccessory(std::int32_t slot) override
    {
        calls.push_back({ kCallApply, slot * kArmorBytes });
        return preview;
    }
    void StoreStatColumns(const inv::AccessoryStatColumns& stored) override
    {
        columns = stored;
    }
    void SetPrimaryStats(std::int32_t strength, std::int32_t intelligence,
        std::int32_t agility, std::int32_t known) override
    {
        calls.push_back({ kCallPrimary, strength, intelligence, agility, known });
    }
    void SetStatusItem(std::uint8_t item_subtype, std::uint8_t item) override
    {
        subtype = item_subtype;
        status = item;
    }
    void SetStatPages(std::int32_t pages) override
    {
        calls.push_back({ kCallPages, pages });
    }
};

template <typename Array> bool ReadAll(Array& items)
{
    for (auto& item : items) {
        if (!Read(item)) {
            return false;
        }
    }
    return true;
}

int RunAccessory()
{
    AccessoryHost host;
    std::int32_t id = 0;
    if (!Read(host.armor) || !Read(id) || !Read(host.preview.strength)
        || !Read(host.preview.intelligence) || !Read(host.preview.agility)
        || !ReadAll(host.preview.types) || !ReadAll(host.preview.classes)
        || !ReadAll(host.preview.affinities) || !ReadAll(host.columns.classes)
        || !ReadAll(host.columns.affinities) || !ReadAll(host.columns.types)
        || !Read(host.subtype) || !Read(host.status)) {
        return 1;
    }
    const bool set = inv::SetAccessoryPreviewStats(host, id);
    std::cout << (set ? 0 : kRefused);
    for (const auto* column :
        { &host.columns.classes, &host.columns.affinities, &host.columns.types }) {
        for (const std::int16_t item : *column) {
            std::cout << ' ' << item;
        }
    }
    std::cout << ' ' << static_cast<int>(host.subtype) << ' '
              << static_cast<int>(host.status);
    PrintCalls(host.calls);
    return 0;
}

class ModelHost final : public inv::EquipmentModelHost {
public:
    Calls calls;
    std::uint8_t sequence = 0;
    std::uint8_t animation = 0;
    std::int32_t flags_word = 0;
    std::array<int, 2> weapons { };
    std::array<std::uint8_t, 2> weapon_flags { };
    std::int32_t busy = 0;
    std::array<std::int32_t, 2> slots { };

    bool HasPreviewActor() override { return true; }
    std::uint8_t PreviewSequenceModel() override { return sequence; }
    std::uint8_t PreviewAnimation() override { return animation; }
    bool HasWeaponModel(int slot) override
    {
        return slot >= 2 && slot < 4
            && weapons.at(static_cast<std::size_t>(slot - 2)) != 0;
    }
    std::optional<AssetAddress> EquipmentModelSlot(int index) override
    {
        return static_cast<AssetAddress>(
            static_cast<std::uint32_t>(slots.at(static_cast<std::size_t>(index))));
    }
    void ReleaseWeaponModel(int slot) override
    {
        calls.push_back({ kCallRelease, slot });
    }
    void QueueWeaponAnimations(std::int32_t sequence_model) override
    {
        calls.push_back({ kCallAnimations, sequence_model });
    }
    void QueueAsset(const ActorAssetRequest& request) override
    {
        calls.push_back({ kCallQueue, request.kind, request.target_slot, request.model_id,
            static_cast<std::int32_t>(static_cast<std::uint32_t>(request.destination)),
            request.sub_kind, request.texture_slot, request.material });
    }
    void SetWeaponDrawn(bool drawn) override
    {
        calls.push_back({ kCallDrawn, 1, drawn ? 1 : 0, 1 });
    }
    bool AssetQueueBusy() override
    {
        calls.push_back({ kCallBusy });
        return busy != 0;
    }
    void ResetEquipmentPose() override { calls.push_back({ kCallResetEquip }); }
    void ReleasePreview() override { calls.push_back({ kCallReleasePreview }); }
    void ClonePreview(int source_slot) override
    {
        calls.push_back({ kCallClone, source_slot });
    }
    void ResetPreviewPose() override { calls.push_back({ kCallResetPreview }); }
    void SheathePreviewWeapon() override
    {
        flags_word = static_cast<std::int32_t>(
            static_cast<std::uint32_t>(flags_word) & ~kWeaponDrawnBit);
    }
    void PlayPreviewAnimation(int played) override
    {
        calls.push_back({ kCallPlay, played, 1, 0, 0 });
    }
    void AdvancePreviewSkeleton() override
    {
        calls.push_back({ kCallAdvance, 0, kBlendPose, 1 });
    }
    void CopyPreviewPoseToBlend() override
    {
        calls.push_back({ kCallCopy, kPose, kBlendPose, kPoseBytes });
    }
    void ClearWeaponModelFlag(int slot) override
    {
        weapon_flags.at(static_cast<std::size_t>(slot - 2)) = 0;
    }
};

int RunModels(int mode)
{
    ModelHost host;
    std::uint8_t step = 0;
    inv::EquipmentModelChoice choice;
    if (!Read(step) || !Read(choice.category) || !Read(choice.model_id)
        || !Read(choice.material) || !Read(host.sequence) || !Read(host.animation)
        || !Read(host.flags_word) || !ReadAll(host.weapons) || !ReadAll(host.weapon_flags)
        || !Read(host.busy) || !ReadAll(host.slots)) {
        return 1;
    }
    if (mode == kTwoHanded) {
        std::cout << (inv::IsTwoHandedWeapon(choice.category) ? 1 : 0);
        PrintCalls({ });
        return 0;
    }
    if (mode == kQueue) {
        const bool queued =
            inv::QueueEquipmentModel(host, choice.model_id, choice.material);
        std::cout << (queued ? 0 : kRefused);
        PrintCalls(host.calls);
        return 0;
    }
    const inv::ModelReload reload = inv::ReloadEquipmentModels(step, host, choice);
    std::cout << (reload == inv::ModelReload::kRefused ? kRefused : 0) << ' '
              << (reload == inv::ModelReload::kDone ? 0 : 1) << ' '
              << static_cast<int>(step) << ' ' << host.flags_word << ' '
              << static_cast<int>(host.weapon_flags[0]) << ' '
              << static_cast<int>(host.weapon_flags[1]);
    PrintCalls(host.calls);
    return 0;
}

int RunVector(int mode)
{
    switch (mode) {
    case kCreate:
        return RunCreate();
    case kSelected:
        return RunSelected();
    case kUpdate:
        return RunUpdate();
    case kAccessory:
        return RunAccessory();
    case kReload:
    case kTwoHanded:
    case kQueue:
        return RunModels(mode);
    default:
        return 1;
    }
}
} // namespace

int main()
{
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);
    int mode = 0;
    while (Read(mode)) {
        if (RunVector(mode) != 0) {
            return 1;
        }
    }
    return 0;
}
