#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Content/DataStore.hpp"
#include "Save/NativeFilePresentation.hpp"

extern "C" {
#include "Core/Text/TextCode.h"
#include "Field/Map.h"
#include "Field/Mpd.h"
#include "Field/Znd.h"
#include "Menu/Files.h"
#include "Save/MemoryCard/Resources.h"
#include "Title/SaveFiles.h"
}

namespace {
namespace fs = std::filesystem;
namespace Save = VS::Game::Save;

template <typename T>
void Append(std::vector<std::uint8_t>& bytes, const T& value) {
  const auto* start = reinterpret_cast<const std::uint8_t*>(&value);
  bytes.insert(bytes.end(), start, start + sizeof(value));
}

template <typename T>
void Replace(std::vector<std::uint8_t>& bytes, std::size_t offset,
             const T& value) {
  REQUIRE(offset <= bytes.size());
  REQUIRE(sizeof(value) <= bytes.size() - offset);
  std::memcpy(bytes.data() + offset, &value, sizeof(value));
}

struct ContentFixture {
  VS::Content::DataStore::ScopedOverride restore;
  fs::path root;
  Save::Document document;
  std::array<char, 32> name{};
  std::vector<std::uint8_t> arm;
  std::vector<std::uint8_t> zone;
  std::vector<std::uint8_t> mpd;

  ContentFixture() {
    static std::atomic<unsigned> counter = 0;
    const auto tick =
        std::chrono::steady_clock::now().time_since_epoch().count();
    root = fs::current_path() / "build" / "quality" /
           ("native-file-presentation-" + std::to_string(tick) + "-" +
            std::to_string(counter.fetch_add(1)));
    fs::create_directories(root / "disc" / "SMALL");
    fs::create_directories(root / "disc" / "MAP");
    document.state.location.zone = 9;
    document.state.location.room = 5;
    document.state.location.scene = 1;
    // Real ARM text framing, embedded zero digit, spacing and high-bit glyph.
    const std::array<unsigned char, 14> encoded{
        0xF8, 0, 0xFB, 4, 0xFB, 6, 0x20, 0xFA, 3, 0, 0x8F, 0x53, 0x28, 0xE7};
    std::transform(
        encoded.begin(), encoded.end(), name.begin(),
        [](unsigned char value) { return static_cast<char>(value); });
    Append(arm, std::uint32_t{1});
    // Empty room geometry: vertex count and five primitive counts.
    Append(arm, Field_MapRoom{0, 24, 9, 6});
    for (int index = 0; index < 6; ++index) Append(arm, std::uint32_t{0});
    Field_MapRoomName room_name{};
    std::copy(name.begin(), name.end(), room_name.name);
    room_name.prev = -1;
    room_name.next = -1;
    Append(arm, room_name);
    Append(zone, Field_ZndHeader{28, 48, 0, 0, 0, 0, 0});
    for (int index = 0; index < 6; ++index) {
      Append(zone, std::uint32_t{300});
      Append(zone, std::uint32_t{0});
    }
    Field_MpdHeader header{};
    header.roomSectionOffset = sizeof(header);
    Append(mpd, header);
    std::array<std::uint32_t, 24> lengths{};
    lengths[8] = sizeof(Field_MpdFloorTrigger);
    Append(mpd, lengths);
    Field_MpdFloorTrigger trigger{};
    trigger.kind = 16;
    trigger.activation = 2;
    Append(mpd, trigger);
  }

  ~ContentFixture() {
    try {
      restore.Restore();
      std::error_code error;
      fs::remove_all(root, error);
      if (error) std::abort();
    } catch (...) {
      std::abort();
    }
  }

  void Write(std::string_view path, std::span<const std::uint8_t> bytes) {
    std::ofstream output(root / "disc" / fs::path(path), std::ios::binary);
    REQUIRE(output);
    output.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
    REQUIRE(output);
  }

  void Open() {
    Replace(zone, sizeof(Field_ZndHeader) + 5ull * 8 + 4,
            static_cast<std::uint32_t>(mpd.size()));
    Write("SMALL/SCEN001.ARM", arm);
    Write("MAP/ZONE009.ZND", zone);
    Write("MAP/MAP014.MPD", mpd);
    {
      std::ofstream manifest(root / "files.tsv");
      REQUIRE(manifest);
      manifest << "lba\tsize\tform\tpath\n"
               << "100\t" << arm.size() << "\traw\tSMALL/SCEN001.ARM\n"
               << "200\t" << zone.size() << "\traw\tMAP/ZONE009.ZND\n"
               << "300\t" << mpd.size() << "\traw\tMAP/MAP014.MPD\n";
      REQUIRE(manifest);
    }
    const auto opened = VS::Content::DataStore::Open(root);
    REQUIRE(opened.has_value());
    const auto* store = VS::Content::DataStore::Get();
    REQUIRE(store != nullptr);
    REQUIRE(store->Entries().size() == 3);
    REQUIRE(store->Find("SMALL/SCEN001.ARM") != nullptr);
  }
};
}  // namespace

TEST_CASE("Native file summary owns original ARM glyph bytes and artwork",
          "[save-presentation]") {
  ContentFixture fixture;
  fixture.Open();
  const auto described = Save::DescribeNativeFile(fixture.document);
  INFO("DescribeNativeFile: " << (described ? "success" : described.error()));
  REQUIRE(described.has_value());
  CHECK(described->room_name == fixture.name);
  CHECK(described->location_artwork == 1);
  CHECK(fixture.document.state.location.zone == 9);
  CHECK(fixture.document.state.location.room == 5);
  CHECK(fixture.document.state.location.scene == 1);
  const auto repeated = Save::DescribeNativeFile(fixture.document);
  INFO("Repeated DescribeNativeFile: " << (repeated ? "success"
                                                    : repeated.error()));
  REQUIRE(repeated.has_value());
  CHECK(repeated->room_name == described->room_name);
  CHECK(repeated->location_artwork == described->location_artwork);
}

TEST_CASE("Native file summary does not invent artwork for arbitrary rooms",
          "[save-presentation]") {
  ContentFixture fixture;
  Field_MpdFloorTrigger trigger{};
  trigger.kind = 2;
  trigger.activation = 2;
  Replace(fixture.mpd, sizeof(Field_MpdHeader) + 24ull * 4, trigger);
  fixture.Open();
  const auto described = Save::DescribeNativeFile(fixture.document);
  INFO("DescribeNativeFile: " << (described ? "success" : described.error()));
  REQUIRE(described.has_value());
  CHECK(described->room_name == fixture.name);
  CHECK(described->location_artwork == -1);
}

TEST_CASE("Native file summary rejects missing or inconsistent names",
          "[save-presentation]") {
  ContentFixture fixture;
  SECTION("The selected room must occur in its scene") {
    fixture.document.state.location.room = 4;
  }
  SECTION("Names must terminate inside their own 32 bytes") {
    std::fill(fixture.arm.end() - sizeof(Field_MapRoomName),
              fixture.arm.end() - 4, std::uint8_t{10});
  }
  SECTION("A name table must be present") {
    fixture.arm.resize(fixture.arm.size() - sizeof(Field_MapRoomName));
  }
  SECTION("Control operands may not escape the name record") {
    const auto offset = fixture.arm.size() - sizeof(Field_MapRoomName);
    auto name = std::span(fixture.arm).subspan(offset, 32);
    std::fill(name.begin(), name.end(), std::uint8_t{10});
    fixture.arm[offset + 31] = Core_TextCode_Spacing;
  }
  SECTION("A different scene must not use a stale current-room name") {
    fixture.document.state.location.scene = 2;
  }
  SECTION("Malformed room payloads are rejected by the existing ARM parser") {
    Replace(fixture.arm, 8, std::uint32_t{0xFFFFFFFF});
  }
  fixture.Open();
  const auto described = Save::DescribeNativeFile(fixture.document);
  INFO("DescribeNativeFile: " << (described ? "success" : described.error()));
  REQUIRE_FALSE(described.has_value());
  CHECK_FALSE(described.error().empty());
}

TEST_CASE("Native file summary bounds original artwork content tables",
          "[save-presentation]") {
  ContentFixture fixture;
  SECTION("ZND room arrays require complete records") {
    Replace(fixture.zone, 4, std::int32_t{47});
  }
  SECTION("MPD sections stay within the file") {
    Replace(fixture.mpd, sizeof(Field_MpdHeader) + 8ull * 4,
            std::uint32_t{0xFFFFFFFF});
  }
  SECTION("Floor-trigger arrays require complete records") {
    Replace(fixture.mpd, sizeof(Field_MpdHeader) + 8ull * 4,
            std::uint32_t{sizeof(Field_MpdFloorTrigger) - 1});
  }
  fixture.Open();
  const auto described = Save::DescribeNativeFile(fixture.document);
  INFO("DescribeNativeFile: " << (described ? "success" : described.error()));
  REQUIRE_FALSE(described.has_value());
  CHECK_FALSE(described.error().empty());
}

TEST_CASE("Native file drawing is inert outside the original asset lifetime",
          "[save-presentation]") {
  auto* saved_resources = Save_MemoryCard_uiResources;
  Save_MemoryCard_uiResources = nullptr;
  const auto before_menu = Menu_Files_fileMenuElements[0];
  const auto before_title = Title_fileMenuElements[0];
  const int menu_cursor = Menu_Files_selectCursorXy;
  const int title_cursor = Title_selectionCursorPosition;
  Save::DrawNativeFileScreen({});
  Save::NativeFileScreen title;
  title.title = true;
  title.confirmation = true;
  Save::DrawNativeFileScreen(title);
  CHECK(std::memcmp(&before_menu, &Menu_Files_fileMenuElements[0],
                    sizeof(before_menu)) == 0);
  CHECK(std::memcmp(&before_title, &Title_fileMenuElements[0],
                    sizeof(before_title)) == 0);
  CHECK(Menu_Files_selectCursorXy == menu_cursor);
  CHECK(Title_selectionCursorPosition == title_cursor);
  Save_MemoryCard_uiResources = saved_resources;
}
