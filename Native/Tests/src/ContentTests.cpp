#include "Content/Disc.hpp"

#include <catch2/catch_test_macros.hpp>

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
