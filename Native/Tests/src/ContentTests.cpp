#include "Content/Disc.hpp"
#include "Content/DataAccess.h"
#include "Content/DataStore.hpp"
#include "Content/ModuleData.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>
#include <filesystem>

using namespace VS;

namespace {
std::filesystem::path FindRepoDisc()
{
    for (auto dir = std::filesystem::current_path(); dir.has_parent_path() && dir != dir.parent_path();
         dir = dir.parent_path()) {
        if (auto cue = dir / "disks" / "SLUS-01040.cue"; std::filesystem::exists(cue)) {
            return cue;
        }
    }
    return {};
}
} // namespace

TEST_CASE("NormalizeDiscPath", "[content]")
{
    CHECK(Content::NormalizeDiscPath("\\map\\zone009.znd;1") == "MAP/ZONE009.ZND");
    CHECK(Content::NormalizeDiscPath("/SLUS_010.40") == "SLUS_010.40");
}

TEST_CASE("Module data copies validate both source and destination extents", "[content][module]")
{
    const std::array<unsigned char, 8> source{0, 1, 2, 3, 4, 5, 6, 7};
    std::array<unsigned char, 4> destination{9, 9, 9, 9};
    Content_ModuleDataEntry entry{"fixture", 2, 4, destination.data(), 4};
    REQUIRE(Content::CopyModuleDataEntry(entry, source));
    CHECK(destination == std::array<unsigned char, 4>{2, 3, 4, 5});
    const auto saved = destination;
    entry.size = 5;
    CHECK_FALSE(Content::CopyModuleDataEntry(entry, source));
    entry.size = 4;
    entry.offset = 6;
    CHECK_FALSE(Content::CopyModuleDataEntry(entry, source));
    entry.offset = UINT32_MAX;
    CHECK_FALSE(Content::CopyModuleDataEntry(entry, source));
    CHECK(destination == saved);
}

TEST_CASE("Module relocation maps native strides and rejects unsafe interiors", "[content][module]")
{
    std::array<std::uintptr_t, 3> pointers{};
    std::array<unsigned char, 8> bytes{};
    std::array<unsigned char, 16> expanded{};
    const std::array<Content_ModuleSymbol, 4> symbols{{
        {"fixture", 0x1000, 12, pointers.data(), sizeof pointers, 4, sizeof pointers[0]},
        {"fixture", 0x2000, 8, expanded.data(), sizeof expanded, 0, 0},
        {"SLUS_010.40", 0x3000, 8, bytes.data(), sizeof bytes, 1, 1},
        {"fixture", 0x4000, 0, expanded.data(), 0, 0, 0},
    }};
    CHECK(Content::ResolveModuleAddress(symbols, "fixture", 0x1004) == &pointers[1]);
    CHECK(Content::ResolveModuleAddress(symbols, "fixture", 0x1002) == nullptr);
    CHECK(Content::ResolveModuleAddress(symbols, "fixture", 0x100C) == nullptr);
    CHECK(Content::ResolveModuleAddress(symbols, "fixture", 0x2000) == expanded.data());
    CHECK(Content::ResolveModuleAddress(symbols, "fixture", 0x2004) == nullptr);
    CHECK(Content::ResolveModuleAddress(symbols, "fixture", 0x3007) == &bytes[7]);
    CHECK(Content::ResolveModuleAddress(symbols, "fixture", 0x3008) == nullptr);
    CHECK(Content::ResolveModuleAddress(symbols, "fixture", 0x4000) == expanded.data());
    CHECK(Content::ResolveModuleAddress(symbols, "fixture", 0x4004) == nullptr);
    CHECK(Content::ResolveModuleAddress(symbols, "other", 0x1000) == nullptr);
}

TEST_CASE("Pointer table relocation preserves destinations on invalid input", "[content][module]")
{
    std::array<int, 2> values{};
    std::array<void*, 3> destination{&values[0], &values[0], &values[0]};
    std::array<unsigned, 3> addresses{0x1000, 0, 0x1004};
    const std::array<Content_ModuleSymbol, 1> symbols{{
        {"fixture", 0x1000, sizeof values, values.data(), sizeof values, sizeof(int), sizeof(int)},
    }};
    const std::span<const unsigned char> image{reinterpret_cast<const unsigned char*>(addresses.data()), sizeof addresses};
    Content_ModulePointerTable table{"fixture", 0, 3, destination.data(), 3};
    REQUIRE(Content::RelocateModulePointerTable(table, image, symbols));
    CHECK(destination == std::array<void*, 3>{&values[0], nullptr, &values[1]});
    const auto saved = destination;
    addresses[2] = 0x2000;
    CHECK_FALSE(Content::RelocateModulePointerTable(table, image, symbols));
    CHECK(destination == saved);
    addresses[2] = 0x1004;
    table.capacity = 2;
    CHECK_FALSE(Content::RelocateModulePointerTable(table, image, symbols));
    CHECK(destination == saved);
    table.capacity = 3;
    table.offset = UINT32_MAX;
    CHECK_FALSE(Content::RelocateModulePointerTable(table, image, symbols));
    CHECK(destination == saved);
}

TEST_CASE("Extracted US modules initialize every required data and dispatch entry", "[content][module][disc]")
{
    const auto cue = FindRepoDisc();
    if (cue.empty()) {
        SKIP("Repository disc location is unavailable");
    }
    const auto root = cue.parent_path().parent_path() / "Native" / "data";
    if (!std::filesystem::exists(root / Content::DataStore::ManifestName)) {
        SKIP("Extracted Native/data is unavailable");
    }
    REQUIRE(Content::DataStore::Open(root).has_value());
    REQUIRE(Content_LoadModuleData() == 0);
    for (unsigned index = 0; index < Content_modulePointerCount; ++index) {
        const auto& table = Content_modulePointers[index];
        INFO(table.module << " offset " << table.offset);
        CHECK(table.count <= table.capacity);
    }
}

TEST_CASE("Disc image: ISO9660 directory and file read", "[content][disc]")
{
    const auto cue = FindRepoDisc();
    if (cue.empty()) {
        SKIP("disks\\SLUS-01040.cue not present");
    }
    auto disc = Content::Disc::Open(cue);
    REQUIRE(disc.has_value());
    CHECK((*disc)->IsImage());
    auto exe = (*disc)->Find("SLUS_010.40");
    REQUIRE(exe.has_value());
    auto data = (*disc)->ReadFile(*exe);
    REQUIRE(data.has_value());
    REQUIRE(data->size() > 8);
    CHECK(std::string(reinterpret_cast<const char*>(data->data()), 8) == "PS-X EXE");
    CHECK((*disc)->Find("BATTLE/BATTLE.PRG").has_value());
}
