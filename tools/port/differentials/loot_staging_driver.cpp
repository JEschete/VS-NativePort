#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>

#include "Menu/Loot/LootStaging.hpp"

namespace {
using VS::Items::InventoryContents;

constexpr std::size_t kInventoryBytes = 0xF00;
constexpr std::size_t kWeapon = 0;
constexpr std::size_t kBlade = 1;
constexpr std::size_t kGrip = 2;
constexpr std::size_t kShield = 3;
constexpr std::size_t kArmor = 4;
constexpr std::size_t kGem = 5;
constexpr std::size_t kMisc = 6;

constexpr std::array<std::size_t, 7> kCapacities { 8, 16, 16, 8, 16, 48, 64 };
constexpr std::array<std::size_t, 7> kOffsets { 0x000, 0x280, 0x540, 0x100, 0x640, 0x8C0,
    0xE00 };
constexpr std::array<std::size_t, 7> kStrides { 0x20, 0x2C, 0x10, 0x30, 0x28, 0x1C,
    0x04 };
constexpr std::array<std::size_t, 7> kIdOffsets { 1, 0, 0, 4, 0, 0, 0 };
constexpr std::array<std::size_t, 7> kIdSizes { 1, 1, 2, 1, 1, 2, 2 };

void SetInteger(
    std::uint8_t* bytes, std::size_t offset, std::uint32_t value, std::size_t size)
{
    for (std::size_t i = 0; i < size; ++i) {
        bytes[offset + i] = static_cast<std::uint8_t>(value >> (i * 8));
    }
}

void FillPattern(std::uint8_t* bytes, std::size_t size, std::uint8_t salt)
{
    for (std::size_t i = 0; i < size; ++i) {
        bytes[i] = static_cast<std::uint8_t>(salt + i * 7);
    }
}

void FillCategory(
    std::uint8_t* target, std::size_t category, std::size_t count, std::uint8_t salt)
{
    for (std::size_t row = 0; row < count; ++row) {
        const auto offset = kOffsets[category] + row * kStrides[category];
        auto* record = target + offset;
        FillPattern(
            record, kStrides[category], static_cast<std::uint8_t>(salt + row * 17));
        SetInteger(target, offset + kIdOffsets[category],
            category == kWeapon ? 1 : static_cast<std::uint32_t>(row + 1),
            kIdSizes[category]);
    }
}

std::vector<std::uint8_t> SourceBytes(
    std::size_t kind, std::uint8_t salt, std::uint8_t gem_mask)
{
    constexpr std::array<std::size_t, 7> kSizes { 0xAC, 0x2C, 0x10, 0x80, 0x28, 0x1C,
        0x04 };
    std::vector<std::uint8_t> source(kSizes.at(kind));
    FillPattern(source.data(), source.size(), salt);
    switch (kind) {
    case kBlade:
        SetInteger(source.data(), 0, salt, 1);
        break;
    case kGrip:
        SetInteger(source.data(), 0, 0x100 + salt, 2);
        break;
    case kGem:
        SetInteger(source.data(), 0, 0x200 + salt, 2);
        break;
    case kWeapon:
        SetInteger(source.data(), 4, salt, 1);
        SetInteger(source.data(), 0x30, 0x100 + salt, 2);
        for (std::size_t gem = 0; gem < 3; ++gem) {
            SetInteger(source.data(), 0x40 + gem * 0x1C,
                gem_mask & (1 << gem) ? 0x200 + salt + gem : 0, 2);
        }
        break;
    case kArmor:
        SetInteger(source.data(), 0, salt, 1);
        SetInteger(source.data(), 0x27, 0xFE, 1);
        break;
    case kShield:
        SetInteger(source.data(), 4, salt, 1);
        SetInteger(source.data(), 0x2A, 0xE2, 1);
        for (std::size_t gem = 0; gem < 3; ++gem) {
            SetInteger(source.data(), 0x2C + gem * 0x1C,
                gem_mask & (1 << gem) ? 0x200 + salt + gem : 0, 2);
        }
        break;
    case kMisc:
        SetInteger(source.data(), 0, 0x100 + salt, 2);
        source[2] = 0xA5;
        source[3] = static_cast<std::uint8_t>(salt % 100 + 1);
        break;
    default:
        std::abort();
    }
    return source;
}

template <typename Record> Record AsRecord(const std::vector<std::uint8_t>& bytes)
{
    if (bytes.size() != sizeof(Record))
        std::abort();
    Record record { };
    std::memcpy(&record, bytes.data(), sizeof(record));
    return record;
}

void PrintResult(int result, const InventoryContents& inventory)
{
    std::cout << result;
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(&inventory);
    for (std::size_t i = 0; i < sizeof(inventory); ++i) {
        std::cout << ' ' << static_cast<unsigned>(bytes[i]);
    }
    std::cout << '\n';
}
} // namespace

int main()
{
    static_assert(sizeof(InventoryContents) == kInventoryBytes);
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    std::array<int, 11> input { };
    while (std::cin >> input[0]) {
        for (std::size_t i = 1; i < input.size(); ++i) {
            if (!(std::cin >> input[i]))
                return 1;
        }

        const auto kind = static_cast<std::size_t>(input[0]);
        const auto salt = static_cast<std::uint8_t>(input[1]);
        const auto owner = input[2];
        const auto gem_mask = static_cast<std::uint8_t>(input[3]);
        const auto source_bytes = SourceBytes(kind, salt, gem_mask);
        InventoryContents target { };
        auto* target_bytes = reinterpret_cast<std::uint8_t*>(&target);
        for (std::size_t category = 0; category < kCapacities.size(); ++category) {
            FillCategory(target_bytes, category,
                static_cast<std::size_t>(input[4 + category]), salt);
        }

        int result = 0;
        switch (kind) {
        case kBlade:
            result = VS::Menu::Loot::StageBlade(
                AsRecord<VS::Items::InventoryBlade>(source_bytes), owner, target);
            break;
        case kGrip:
            result = VS::Menu::Loot::StageGrip(
                AsRecord<VS::Items::InventoryGrip>(source_bytes), owner, target);
            break;
        case kGem:
            result = VS::Menu::Loot::StageGem(
                AsRecord<VS::Items::InventoryGem>(source_bytes), owner, target);
            break;
        case kWeapon:
            result = VS::Menu::Loot::StageWeapon(
                AsRecord<VS::Items::LootedWeapon>(source_bytes), target);
            break;
        case kArmor:
            result = VS::Menu::Loot::StageArmor(
                AsRecord<VS::Items::InventoryArmor>(source_bytes), target);
            break;
        case kShield:
            result = VS::Menu::Loot::StageShield(
                AsRecord<VS::Items::LootedShield>(source_bytes), target);
            break;
        case kMisc:
            result = VS::Menu::Loot::StageMiscItem(
                AsRecord<VS::Items::LootedItem>(source_bytes), target);
            break;
        default:
            return 2;
        }
        PrintResult(result, target);
    }
    return 0;
}
