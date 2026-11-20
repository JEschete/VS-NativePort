#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <span>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Audio/Driver/SoundDriver.hpp"
#include "Audio/Mixer/VoiceMixer.hpp"
#include "Audio/Seq/Channel.hpp"
#include "Audio/SfxChannelGroups.hpp"
#include "Audio/SoundPlaybackLive.hpp"
#include "Boot/GameLoop.hpp"
#include "Content/DataStore.hpp"
#include "Content/ModuleData.hpp"
#include "Core/Log.hpp"
#include "Ending/EndingLive.hpp"
#include "Field/MinimapScene.hpp"
#include "Field/Mpd.hpp"
#include "Field/RoomLoading.hpp"
#include "Input/ControllerLiveState.hpp"
#include "Items/Categories.hpp"
#include "Items/InventoryLive.hpp"
#include "Menu/FilesLiveState.hpp"
#include "Save/FileMenuLive.hpp"
#include "Save/NativeContainerLive.hpp"
#include "Save/NativeFilePresentation.hpp"
#include "Save/NativeMenu.hpp"
#include "Save/RuntimeSnapshot.hpp"
#include "State/Ashley.hpp"
#include "State/Location.hpp"
#include "State/PlayTime.hpp"
#include "State/Progress.hpp"
#include "State/RoomMemory.hpp"
#include "State/StateFlags.hpp"
#include "Title/SaveFiles.hpp"

using VS::Legacy::kPadLdown;
using VS::Legacy::kPadLleft;
using VS::Legacy::kPadLright;
using VS::Legacy::kPadLup;
using VS::Legacy::kPadRdown;
using VS::Legacy::kPadRright;

namespace {
namespace fs = std::filesystem;
namespace Save = VS::Game::Save;

constexpr std::size_t kSoundCount = 12;
std::array<unsigned, kSoundCount>* sound_calls = nullptr;

void CountSound(VS::Audio::CommandParams* parameters) {
  if (sound_calls != nullptr && parameters->param1 < sound_calls->size()) {
    ++(*sound_calls)[parameters->param1];
  }
}

struct MenuFixture {
  fs::path root;
  VS::Input::Settings settings = VS::Input::settings;
  int active = VS::Boot::isGameplayActive;
  decltype(VS::Input::buttonsPressed) pressed = VS::Input::buttonsPressed;
  int repeat = VS::Input::buttonRepeat;
  std::array<std::uint32_t, 12> sound_offsets{};
  std::array<std::uint16_t, 12> sound_selectors{};
  VS::Audio::Playback::ScopedLiveEffectTables sound_tables{
      {.program_offsets = sound_offsets,
       .bank_selectors = sound_selectors,
       .program_data = {}}};
  VS::Audio::CommandHandler sound_handler =
      VS::Audio::Mixer::kCommandHandlers[0x20];
  VS::Audio::LegacyCommandArguments sound_arguments =
      VS::Audio::Driver::commandArguments;
  VS::Audio::CommandParams sound_params = VS::Audio::Mixer::commandParams;
  int sound_group = VS::Audio::nextSfxChannelGroup7E;
  std::array<unsigned, kSoundCount> sounds{};
  std::array<unsigned, kSoundCount>* previous_calls = sound_calls;

  MenuFixture() {
    static std::atomic<unsigned> counter = 0;
    const auto tick =
        std::chrono::steady_clock::now().time_since_epoch().count();
    root = fs::current_path() / "build" / "quality" / "native-menu-tests" /
           ("vs-native-menu-" + std::to_string(tick) + "-" +
            std::to_string(counter.fetch_add(1)));
    fs::create_directories(root.parent_path());
    REQUIRE(fs::create_directory(root));
    Save::ConfigureDirectory(root);
    VS::Boot::isGameplayActive = 0;
    VS::Input::buttonsPressed = 0;
    VS::Input::buttonRepeat = 0;
    sound_selectors.fill(0x8000);
    VS::Audio::Mixer::kCommandHandlers[0x20] = CountSound;
    VS::Audio::nextSfxChannelGroup7E = 2;
    sound_calls = &sounds;
  }

  ~MenuFixture() {
    try {
      Save::ShutdownFileMenus();
      VS::Input::settings = settings;
      VS::Boot::isGameplayActive = active;
      VS::Input::buttonsPressed = pressed;
      VS::Input::buttonRepeat = repeat;
      VS::Audio::Mixer::kCommandHandlers[0x20] = sound_handler;
      VS::Audio::Driver::commandArguments = sound_arguments;
      VS::Audio::Mixer::commandParams = sound_params;
      VS::Audio::nextSfxChannelGroup7E = sound_group;
      sound_calls = previous_calls;
      std::error_code error;
      fs::remove_all(root, error);
      if (error) {
        VS::Core::LogError("test", "Cannot clean native menu fixture: {}",
                           error.message());
        std::abort();
      }
    } catch (...) {
      std::abort();
    }
  }
};

template <typename T>
void Put(std::vector<std::uint8_t>& bytes, std::size_t offset, const T& value) {
  if (bytes.size() < offset + sizeof(value)) {
    bytes.resize(offset + sizeof(value));
  }
  std::memcpy(bytes.data() + offset, &value, sizeof(value));
}

struct MenuContentFixture {
  VS::Content::DataStore::ScopedOverride service;
  MenuFixture& menu;
  std::array<unsigned, 8> previous_bounds{};
  fs::path root;
  std::string manifest = "lba\tsize\tform\tpath\n";

  explicit MenuContentFixture(MenuFixture& fixture)
      : menu(fixture), root(menu.root / "content") {
    constexpr std::array<unsigned, 8> kBounds{96,  127, 254, 261,
                                              308, 323, 458, 512};
    std::copy_n(VS::Items::itemCategoryUpperIds, previous_bounds.size(),
                previous_bounds.begin());
    std::copy(kBounds.begin(), kBounds.end(), VS::Items::itemCategoryUpperIds);
    AddMaps();
    AddAreaMap();
    AddBattleBounds(kBounds);
    {
      std::ofstream output(root / "files.tsv");
      output << manifest;
      REQUIRE(output.good());
    }
    const auto opened = VS::Content::DataStore::Open(root);
    INFO((opened ? "content open" : opened.error()));
    REQUIRE(opened.has_value());
  }

  void AddMaps() {
    std::vector<std::uint8_t> room;
    VS::Field::MpdHeader header{};
    header.room_section_offset = sizeof(header);
    std::array<std::uint32_t, 24> lengths{};
    lengths[1] = sizeof(VS::Field::MpdCollisionHeader) + 4 + 16;
    lengths[10] = sizeof(VS::Field::MpdEnvironment);
    Put(room, sizeof(header), lengths);
    const std::size_t collision = sizeof(header) + sizeof(lengths);
    Put(room, collision, VS::Field::MpdCollisionHeader{1, 1, 0, 1});
    room.resize(collision + lengths[1]);
    VS::Field::MpdEnvironment environment{};
    environment.scene_id = 1;
    Put(room, room.size(), environment);
    header.enemy_section_offset = static_cast<int>(room.size());
    Put(room, 0, header);
    Add("MAP/MAP009.MPD", room, 200);

    std::vector<std::uint8_t> zone;
    Put(zone, 0, VS::Field::ZoneFileHeader{28, 8, 0, 0, 0, 0, 0});
    Put(zone, 28, std::uint32_t{200});
    Put(zone, 32, static_cast<std::uint32_t>(room.size()));
    Add("MAP/ZONE009.ZND", zone, 100);
    std::vector<std::uint8_t> start;
    lengths[1] =
        sizeof(VS::Field::MpdCollisionHeader) + std::size_t{8} * 8 * 4 + 16;
    Put(start, sizeof(header), lengths);
    Put(start, collision, VS::Field::MpdCollisionHeader{8, 8, 0, 1});
    start.resize(collision + lengths[1]);
    environment.scene_id = 0;
    Put(start, start.size(), environment);
    header.enemy_section_offset = static_cast<int>(start.size());
    Put(start, 0, header);
    Add("MAP/MAP001.MPD", start, 201);
    Put(zone, 28, std::uint32_t{201});
    Put(zone, 32, static_cast<std::uint32_t>(start.size()));
    Add("MAP/ZONE001.ZND", zone, 101);
  }

  void AddAreaMap() {
    std::vector<std::uint8_t> arm;
    Put(arm, 0, std::uint32_t{1});
    Put(arm, 4, VS::Field::MapRoomRecord{0, 24, 9, 1});
    arm.resize(4 + sizeof(VS::Field::MapRoomRecord) + 24);
    VS::Field::MapRoomName name{};
    name.name[0] = 10;
    name.name[1] = static_cast<char>(0xE7);
    Put(arm, arm.size(), name);
    Add("SMALL/SCEN001.ARM", arm, 300);
  }

  void AddBattleBounds(const std::array<unsigned, 8>& bounds) {
    bool found = false;
    for (const auto& slice : VS::Content::ModuleDataEntries()) {
      if (slice.target != &VS::Items::itemCategoryUpperIds) continue;
      std::vector<std::uint8_t> battle;
      Put(battle, slice.offset, bounds);
      Add(slice.module, battle, 10000);
      found = true;
      break;
    }
    REQUIRE(found);
  }

  ~MenuContentFixture() {
    service.Restore();
    std::copy(previous_bounds.begin(), previous_bounds.end(),
              VS::Items::itemCategoryUpperIds);
  }

  void Add(std::string_view name, std::span<const std::uint8_t> bytes,
           unsigned sector) {
    const auto path = root / "disc" / fs::path(name);
    fs::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    output.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
    REQUIRE(output.good());
    manifest += std::to_string(sector) + "\t" + std::to_string(bytes.size()) +
                "\traw\t" + std::string(name) + "\n";
  }

  static Save::Document Document() {
    Save::Document document;
    document.generation = 1;
    document.saved_unix_ms = 1;
    auto& state = document.state;
    state.location.zone = 9;
    state.location.scene = 1;
    state.ashley.current_hp = 250;
    state.ashley.max_hp = 250;
    state.ashley.current_mp = 50;
    state.ashley.max_mp = 50;
    state.ashley.camera_distance = 1536;
    state.camera.distance = 1536;
    state.camera.zoom_mode = 1;
    state.room_cache[0].recency = 1;
    state.room_cache[0].zone = 9;
    return document;
  }

  void Store(const Save::Document& document,
             std::string_view name = "slot00.vssave") {
    const auto valid = Save::ValidateContent(document.state);
    INFO((valid ? "valid content" : valid.error()));
    REQUIRE(valid.has_value());
    const auto encoded = Save::Encode(document);
    INFO((encoded ? "encoded document" : encoded.error()));
    REQUIRE(encoded.has_value());
    std::ofstream output(menu.root / fs::path(name), std::ios::binary);
    output << *encoded;
    REQUIRE(output.good());
  }
};

std::string Text(const char* text) {
  REQUIRE(text != nullptr);
  std::string result;
  for (int i = 0; i < 49; ++i) {
    const auto c = static_cast<unsigned char>(text[i]);
    if (c == 0xE7) return result;
    if (c < 10) {
      result += static_cast<char>('0' + c);
    } else if (c < 36) {
      result += static_cast<char>('A' + c - 10);
    } else if (c < 62) {
      result += static_cast<char>('a' + c - 36);
    } else if (c == 0x8F) {
      result += ' ';
    } else if (c == 0x9E) {
      result += ':';
    } else if (c == 0xA1) {
      result += '/';
    } else if (c == 0xA7) {
      result += '-';
    } else {
      FAIL("Unexpected game glyph in native menu");
    }
  }
  FAIL("Native file prompt is not terminated within its one-line buffer");
  return {};
}

void Buttons(int pressed, int repeat = 0) {
  VS::Input::buttonsPressed = pressed;
  VS::Input::buttonRepeat = repeat;
}

void ConfirmSave() {
  Buttons(kPadRright);
  CHECK(VS::Menu::UpdateLiveDataSaveMenu(0) == 0);
  Buttons(0, kPadLup);
  CHECK(VS::Menu::UpdateLiveDataSaveMenu(0) == 0);
  Buttons(kPadRright);
  CHECK(VS::Menu::UpdateLiveDataSaveMenu(0) == 0);
  Buttons(0);
  CHECK(VS::Menu::UpdateLiveDataSaveMenu(0) == 0);
}

Save::NativeFileScreen Screen() {
  const auto screen = Save::FileMenuPresentation();
  REQUIRE(screen);
  return *screen;
}

int SelectedSlot() {
  const auto screen = Screen();
  REQUIRE(screen.files.size() == 3);
  REQUIRE(screen.selected_row >= 0);
  REQUIRE(screen.selected_row < 3);
  return screen.files[screen.selected_row].slot;
}

void Move(int direction, int (*update)(int)) {
  Buttons(0, direction);
  REQUIRE(update(0) == 0);
  Buttons(0);
  REQUIRE(update(0) == 0);
}

void ChooseYes(int (*update)(int)) {
  Buttons(kPadRright);
  REQUIRE(update(0) == 0);
  REQUIRE(Screen().confirmation);
  CHECK_FALSE(Screen().affirmative);
  Move(kPadLup, update);
  REQUIRE(Screen().affirmative);
  Buttons(kPadRright);
  REQUIRE(update(0) == 0);
}

void Tick(int (*update)(int)) { REQUIRE(update(0) == 0); }

void BeginPicker(int (*update)(int), int init) {
  REQUIRE(update(init) == 0);
  Buttons(0);
  Tick(update);
}

void Press(int buttons, int (*update)(int)) {
  Buttons(buttons);
  Tick(update);
}

void CheckSounds(const MenuFixture& fixture,
                 std::array<unsigned, 5> menu_sounds) {
  std::array<unsigned, kSoundCount> expected{};
  std::copy(menu_sounds.begin(), menu_sounds.end(), expected.begin() + 4);
  CHECK(fixture.sounds == expected);
}

void CheckStagedBank(const VS::Items::ContainerData* bank) {
  CHECK(VS::Game::Save::ContainerData() == bank);
  CHECK(bank->misc[0].id == 323);
  CHECK(bank->misc[0].count == 2);
}

void CheckSelectedSlot(int slot) { CHECK(SelectedSlot() == slot); }

void CheckTitlePickerWrapAndCancel() {
  Move(kPadLdown, VS::Menu::UpdateLiveLoadMenu);
  CHECK(SelectedSlot() == 0);
  Move(kPadLup, VS::Menu::UpdateLiveLoadMenu);
  CHECK(SelectedSlot() == 99);
  CHECK(Screen().files.front().slot == 97);
  CHECK(Screen().files.back().slot == 99);
  Buttons(kPadRdown);
  CHECK(VS::Menu::UpdateLiveLoadMenu(0) == -1);
  CHECK_FALSE(Save::FileMenuPresentation());
}

void CheckContainerTail(const VS::Items::ContainerData* bank) {
  CHECK(VS::Game::Save::ContainerData() == bank);
  CHECK(VS::Game::Save::ContainerData()->misc[255].id == 308);
  CHECK(VS::Game::Save::ContainerData()->misc[255].count == 2);
}

void CheckVerticalConsent() {
  Move(kPadLleft, VS::Game::Save::SelectContainer);
  CHECK_FALSE(Screen().affirmative);
  Move(kPadLright, VS::Game::Save::SelectContainer);
  CHECK_FALSE(Screen().affirmative);
  Move(kPadLup, VS::Game::Save::SelectContainer);
  CHECK(Screen().affirmative);
  Move(kPadLdown, VS::Game::Save::SelectContainer);
  CHECK_FALSE(Screen().affirmative);
}

void CheckCanceledConsent(const fs::path& root) {
  CHECK_FALSE(Screen().confirmation);
  CHECK(fs::is_empty(root));
}

void CheckFirstTwoFiles(std::array<Save::FileStatus, 2> expected) {
  const auto screen = Screen();
  REQUIRE(screen.files.size() == 3);
  CHECK(screen.files[0].status == expected[0]);
  CHECK(screen.files[1].status == expected[1]);
}

void CheckFailedSave(const MenuFixture& fixture) {
  REQUIRE(Text(VS::Menu::Files::memoryCardMessage) == "Save failed - see log");
  CHECK(fixture.sounds[8] == 0);
  CHECK(VS::Input::settings.cumulative_save_count ==
        fixture.settings.cumulative_save_count);
  CHECK(VS::Input::settings.current_game_save_count ==
        fixture.settings.current_game_save_count);
  CHECK(VS::Input::settings.save_file_generation ==
        fixture.settings.save_file_generation);
  CHECK_FALSE(fs::exists(fixture.root / "slot00.vssave"));
}
}  // namespace

TEST_CASE("Native Continue scrolls all one hundred files vertically",
          "[save-menu]") {
  MenuFixture fixture;
  CHECK(VS::Game::Save::InitialTitleSelection() == 0);
  BeginPicker(VS::Menu::UpdateLiveLoadMenu, 2);
  CHECK(SelectedSlot() == 0);
  CHECK(Screen().summaries.size() == 3);
  CHECK(Text(VS::Title::memoryCardMessage) == "Select a file");
  REQUIRE(VS::Title::memoryCardMessage != nullptr);
  for (int slot = 1; slot < 100; ++slot) {
    Move(kPadLdown, VS::Menu::UpdateLiveLoadMenu);
    CHECK(SelectedSlot() == slot);
    CHECK(Screen().files.front().slot == std::max(0, slot - 2));
  }
  CheckTitlePickerWrapAndCancel();
}

TEST_CASE("Native file screens have three cards without generic row panels",
          "[save-menu]") {
  MenuFixture fixture;
  BeginPicker(VS::Menu::UpdateLiveLoadMenu, 2);
  REQUIRE(Screen().files.size() == 3);
  CHECK(Screen().title);
  CHECK_FALSE(Screen().save);
  for (int i = 0; i < 10; ++i) {
    CHECK(VS::Title::fileMenuElements[i].state ==
          static_cast<std::uint8_t>(VS::Menu::Files::ElementState::kInactive));
  }
  for (const auto& file : Screen().files) {
    CHECK(file.status == Save::FileStatus::kEmpty);
    CHECK_FALSE(file.document);
  }
  Move(kPadLleft, VS::Menu::UpdateLiveLoadMenu);
  CheckSelectedSlot(0);
  Move(kPadLright, VS::Menu::UpdateLiveLoadMenu);
  CheckSelectedSlot(0);
}

TEST_CASE("Native Save cannot bypass production save-point eligibility",
          "[save-menu]") {
  MenuFixture fixture;
  const auto before = VS::Input::settings;
  REQUIRE(VS::Menu::UpdateLiveDataSaveMenu(1) == 0);
  REQUIRE(VS::Menu::UpdateLiveDataSaveMenu(0) == 0);
  ConfirmSave();
  CHECK(Text(VS::Menu::Files::memoryCardMessage) == "Save failed - see log");
  CHECK(!fs::exists(fixture.root / "slot00.vssave"));
  CHECK(VS::Input::settings.cumulative_save_count ==
        before.cumulative_save_count);
  CHECK(VS::Input::settings.current_game_save_count ==
        before.current_game_save_count);
  CHECK(VS::Input::settings.save_file_generation ==
        before.save_file_generation);
}

TEST_CASE("Native picker never replaces an invalid existing file",
          "[save-menu]") {
  MenuFixture fixture;
  const auto path = fixture.root / "slot00.vssave";
  {
    std::ofstream output(path, std::ios::binary);
    output << "unsupported or damaged save";
    REQUIRE(output.good());
  }
  CHECK(VS::Game::Save::InitialTitleSelection() == 1);
  REQUIRE(VS::Menu::UpdateLiveDataSaveMenu(1) == 0);
  REQUIRE(VS::Menu::UpdateLiveDataSaveMenu(0) == 0);
  CHECK(Screen().files.front().status == Save::FileStatus::kInvalid);
  CHECK(Text(VS::Menu::Files::memoryCardMessage) ==
        "Invalid data - not overwritten");
  Buttons(kPadRright);
  CHECK(VS::Menu::UpdateLiveDataSaveMenu(0) == 0);
  CHECK(Text(VS::Menu::Files::memoryCardMessage) == "No usable native save");
  std::ifstream input(path, std::ios::binary);
  CHECK(std::string(std::istreambuf_iterator<char>(input), {}) ==
        "unsupported or damaged save");
}

TEST_CASE("Container selection owns typed storage beyond picker reentry",
          "[save-menu]") {
  MenuFixture fixture;
  BeginPicker(VS::Game::Save::SelectContainer, 1);
  Press(kPadRright, VS::Game::Save::SelectContainer);
  REQUIRE(Screen().confirmation);
  CHECK_FALSE(Screen().affirmative);
  Buttons(0, kPadLup);
  Tick(VS::Game::Save::SelectContainer);
  Press(kPadRright, VS::Game::Save::SelectContainer);
  Buttons(0);
  REQUIRE(VS::Game::Save::SelectContainer(0) == 1);
  auto* bank = VS::Game::Save::ContainerData();
  REQUIRE(bank != nullptr);
  CHECK(VS::Input::settings.container_empty == 1);
  bank->misc[255].id = 308;
  bank->misc[255].count = 2;
  REQUIRE(VS::Menu::UpdateLiveDataSaveMenu(2) == 0);
  CheckContainerTail(bank);
  VS::Game::Save::ReleaseContainer();
}

TEST_CASE("Native confirmation is vertical explicit consent and cancelable",
          "[save-menu]") {
  MenuFixture fixture;
  BeginPicker(VS::Game::Save::SelectContainer, 1);
  Press(kPadRright, VS::Game::Save::SelectContainer);
  REQUIRE(Screen().confirmation);
  CHECK_FALSE(Screen().affirmative);
  CheckVerticalConsent();
  Press(kPadRright, VS::Game::Save::SelectContainer);
  CheckCanceledConsent(fixture.root);
  Press(kPadRright, VS::Game::Save::SelectContainer);
  CHECK_FALSE(Screen().affirmative);
  Move(kPadLup, VS::Game::Save::SelectContainer);
  CHECK(Screen().affirmative);
  Press(kPadRdown, VS::Game::Save::SelectContainer);
  CheckCanceledConsent(fixture.root);
}

TEST_CASE("Native reentry retains the last file at the end of the scroll",
          "[save-menu]") {
  MenuFixture fixture;
  BeginPicker(VS::Game::Save::SelectContainer, 1);
  Move(kPadLup, VS::Game::Save::SelectContainer);
  CheckSelectedSlot(99);
  Press(kPadRright, VS::Game::Save::SelectContainer);
  Move(kPadLup, VS::Game::Save::SelectContainer);
  Press(kPadRright, VS::Game::Save::SelectContainer);
  Buttons(0);
  REQUIRE(VS::Game::Save::SelectContainer(0) == 1);
  BeginPicker(VS::Menu::UpdateLiveDataSaveMenu, 2);
  CheckSelectedSlot(99);
  CHECK(Screen().files.front().slot == 97);
  CHECK(Screen().files.back().slot == 99);
  CHECK_FALSE(Screen().title);
  CHECK(Screen().save);
  VS::Game::Save::ReleaseContainer();
}

TEST_CASE("Native session reset clears borrowed presentation and footer",
          "[save-menu]") {
  MenuFixture fixture;
  REQUIRE(VS::Menu::UpdateLiveLoadMenu(2) == 0);
  REQUIRE(VS::Menu::UpdateLiveLoadMenu(0) == 0);
  REQUIRE(Save::FileMenuPresentation());
  REQUIRE(VS::Title::memoryCardMessage != nullptr);
  Save::ResetFileMenuSession();
  CHECK_FALSE(Save::FileMenuPresentation());
  CHECK(VS::Title::memoryCardMessage == nullptr);
  VS::Game::Save::DrawFileOverlay(false);
  VS::Game::Save::DrawFileOverlay(true);
}

TEST_CASE("Native clear saves require staged original ending inputs",
          "[save-menu]") {
  MenuFixture fixture;
  REQUIRE(VS::Menu::UpdateLiveSaveMenu(1) == 0);
  CHECK(Text(VS::Title::memoryCardMessage) == "No ending clear save available");
  Buttons(kPadRright);
  CHECK(VS::Menu::UpdateLiveSaveMenu(0) == -1);
  CHECK(fs::is_empty(fixture.root));
}

namespace {
struct ClearInputsFixture {
  decltype(VS::State::stateFlags) flags = VS::State::stateFlags;
  decltype(VS::Items::inventory) inventory = VS::Items::inventory;
  decltype(VS::Items::inventoryIndices) order = VS::Items::inventoryIndices;
  VS::State::AshleySnapshot ashley = VS::State::ashleySnapshot;
  VS::Field::MpdDoorRecord entry = VS::State::savedEntryDoor;
  VS::State::RoomVisitFlags visits = VS::State::roomVisitProgress;
  VS::State::ScoreData score = VS::State::scoreData;
  decltype(VS::State::playTime) time = VS::State::playTime;
  decltype(std::to_array(VS::State::recentRoomEnemyStates)) caches =
      std::to_array(VS::State::recentRoomEnemyStates);
  decltype(std::to_array(VS::State::learnedActionFlags)) learned =
      std::to_array(VS::State::learnedActionFlags);
  int cache_slot = VS::State::currentRoomStateSlot;
  int clear_pending = VS::Boot::isClearSavePending;

  ClearInputsFixture() {
    VS::State::stateFlags = {};
    VS::Items::inventory = {};
    VS::Items::inventoryIndices = {};
    VS::Items::inventory.misc[0].id = 308;
    VS::Items::inventory.misc[0].count = 2;
    VS::Items::inventoryIndices.misc[0] = 1;
    VS::State::ashleySnapshot.stats =
        MenuContentFixture::Document().state.ashley;
    VS::State::savedEntryDoor = {};
    VS::State::savedEntryDoor.entry_tile_x = 4;
    VS::State::savedEntryDoor.entry_tile_z = 2;
    VS::State::roomVisitProgress = {};
    VS::State::scoreData = {};
    VS::State::playTime = {.frames = 0, .seconds = 0, .minutes = 0, .hours = 1};
    VS::State::currentRoomStateSlot = 0;
    std::fill_n(VS::State::recentRoomEnemyStates, caches.size(),
                VS::State::RoomEnemyStates{});
    std::fill_n(VS::State::learnedActionFlags, learned.size(), 0);
    VS::State::learnedActionFlags[0] = static_cast<char>(0x80);
    VS::Input::settings = {};
    VS::Input::settings.current_game_save_count = 7;
    VS::Input::settings.cumulative_save_count = 19;
    VS::Input::settings.save_file_generation = 3;
    VS::Ending::LiveCommitClearProgress();
    VS::Boot::isClearSavePending = 1;
    VS::Game::Save::StageClearGame();
    VS::Boot::isClearSavePending = 0;
  }

  ~ClearInputsFixture() {
    VS::State::stateFlags = flags;
    VS::Items::inventory = inventory;
    VS::Items::inventoryIndices = order;
    VS::State::ashleySnapshot = ashley;
    VS::State::savedEntryDoor = entry;
    VS::State::roomVisitProgress = visits;
    VS::State::scoreData = score;
    VS::State::playTime = time;
    std::copy(caches.begin(), caches.end(), VS::State::recentRoomEnemyStates);
    std::copy(learned.begin(), learned.end(), VS::State::learnedActionFlags);
    VS::State::currentRoomStateSlot = cache_slot;
    VS::Boot::isClearSavePending = clear_pending;
  }
};

std::string FileBytes(const fs::path& path) {
  std::ifstream input(path, std::ios::binary);
  REQUIRE(input.good());
  return {std::istreambuf_iterator<char>(input), {}};
}

void ExecuteClearSave() {
  BeginPicker(VS::Menu::UpdateLiveSaveMenu, 1);
  ChooseYes(VS::Menu::UpdateLiveSaveMenu);
  Buttons(0);
  Tick(VS::Menu::UpdateLiveSaveMenu);
}
}  // namespace

namespace {
void CheckClearSaveProgress(const Save::Document& document) {
  CHECK(document.generation == 1);
  CHECK(document.state.settings.cumulative_save_count == 20);
  CHECK(document.state.settings.current_game_save_count == 0);
  CHECK(document.state.settings.save_file_generation == 4);
  CHECK(document.state.script_flags[0] == 1);
}

void CheckClearSaveCarryover(const Save::Document& document) {
  CHECK(document.state.ashley.current_hp == 250);
  CHECK(document.state.ashley.current_mp == 50);
  CHECK(document.state.inventory.misc[0].id == 0);
  CHECK(document.state.learned_actions[0]);
}

void CheckClearSaveSession() {
  CHECK(VS::Input::settings.cumulative_save_count == 20);
  CHECK(VS::Input::settings.current_game_save_count == 0);
  CHECK(VS::Menu::Files::dataNotSaved == 1);
  CHECK(Text(Screen().summaries[0].room_name.data()) == "CLEAR GAME");
}
}  // namespace

namespace {
// The value of an optional a test has established must be present.
template <class Optional>
auto& Engaged(Optional& optional) {
  if (!optional.has_value()) throw std::runtime_error("expected a value");
  return *optional;
}
}  // namespace

TEST_CASE("Original ending component writes a distinct native clear save",
          "[save-menu][save-clear]") {
  MenuFixture fixture;
  MenuContentFixture content(fixture);
  ClearInputsFixture ending;
  ExecuteClearSave();
  REQUIRE(Text(VS::Title::memoryCardMessage) == "Save complete");
  auto files = Save::Files::Open(fixture.root);
  REQUIRE(files);
  const auto loaded = files->Load(0);
  REQUIRE(loaded);
  REQUIRE(loaded->document.new_game_entry);
  CheckClearSaveProgress(loaded->document);
  CheckClearSaveCarryover(loaded->document);
  CheckClearSaveSession();
  Buttons(kPadRright);
  CHECK(VS::Menu::UpdateLiveSaveMenu(0) == 1);
  Save::ShutdownFileMenus();
  Save::ConfigureDirectory(fixture.root);
  CHECK(VS::Game::Save::InitialTitleSelection() == 1);
  BeginPicker(VS::Menu::UpdateLiveLoadMenu, 1);
  CHECK(Engaged(Screen().files.front().document).new_game_entry.has_value());
}

namespace {
void CheckOverwrittenBank(const fs::path& root) {
  auto files = Save::Files::Open(root);
  REQUIRE(files);
  const auto loaded = files->Load(0);
  REQUIRE(loaded);
  CHECK(loaded->document.generation == 2);
  CHECK(loaded->document.new_game_entry.has_value());
  const auto& bank = loaded->document.state.storage;
  CHECK(bank.misc[0].id == 0);
  CHECK(bank.misc[63].id == 0);
  CHECK(bank.misc[64].count == 2);
  CHECK(bank.misc[255].count == 3);
}

void CheckReplacedRecoveredFile(const fs::path& root) {
  const auto files = Save::Files::Open(root);
  REQUIRE(files);
  const auto loaded = files->Load(0);
  REQUIRE(loaded);
  CHECK_FALSE(loaded->recovered);
  CHECK(loaded->document.new_game_entry.has_value());
  CHECK(loaded->document.generation == 2);
}

void CheckFailedClearSaveChangedNothing(const fs::path& path,
                                        const std::string& bytes) {
  CHECK(Text(VS::Title::memoryCardMessage) == "Save failed - see log");
  CHECK(VS::Input::settings.cumulative_save_count == 19);
  CHECK(VS::Input::settings.current_game_save_count == 7);
  CHECK(VS::Input::settings.save_file_generation == 3);
  CHECK(FileBytes(path) == bytes);
}
}  // namespace

TEST_CASE("Clear overwrite preserves per-file bank and exact original reset",
          "[save-menu][save-clear]") {
  MenuFixture fixture;
  MenuContentFixture content(fixture);
  auto checkpoint = MenuContentFixture::Document();
  checkpoint.state.storage.misc[0] = {308, 1, 0};
  checkpoint.state.storage.misc[63] = {308, 1, 0};
  checkpoint.state.storage.misc[64] = {308, 2, 0};
  checkpoint.state.storage.misc[255] = {308, 3, 0};
  content.Store(checkpoint);
  const auto path = fixture.root / "slot00.vssave";
  const auto original = FileBytes(path);
  ClearInputsFixture ending;
  ExecuteClearSave();
  REQUIRE(Text(VS::Title::memoryCardMessage) == "Save complete");
  CHECK(FileBytes(path.string() + ".bak") == original);
  CheckOverwrittenBank(fixture.root);
}

TEST_CASE("Clear-save consent rejects changed content and reset releases input",
          "[save-menu][save-clear]") {
  MenuFixture fixture;
  MenuContentFixture content(fixture);
  ClearInputsFixture ending;
  BeginPicker(VS::Menu::UpdateLiveSaveMenu, 1);
  ChooseYes(VS::Menu::UpdateLiveSaveMenu);
  content.Store(MenuContentFixture::Document());
  const auto original = FileBytes(fixture.root / "slot00.vssave");
  Buttons(0);
  Tick(VS::Menu::UpdateLiveSaveMenu);
  CHECK(Text(VS::Title::memoryCardMessage) == "File changed - select again");
  CHECK(FileBytes(fixture.root / "slot00.vssave") == original);
  CHECK(VS::Input::settings.cumulative_save_count == 19);
  CHECK(VS::Input::settings.current_game_save_count == 7);
  Save::ResetFileMenuSession();
  REQUIRE(VS::Menu::UpdateLiveSaveMenu(1) == 0);
  CHECK(Text(VS::Title::memoryCardMessage) == "No ending clear save available");
}

TEST_CASE("Clear saves retain explicit recovered-file replacement consent",
          "[save-menu][save-clear]") {
  MenuFixture fixture;
  MenuContentFixture content(fixture);
  const auto checkpoint = MenuContentFixture::Document();
  content.Store(checkpoint, "slot00.vssave.bak");
  const auto path = fixture.root / "slot00.vssave";
  {
    std::ofstream broken(path);
    broken << "corrupt primary";
  }
  const auto original = FileBytes(path);
  const auto backup = FileBytes(path.string() + ".bak");
  ClearInputsFixture ending;
  BeginPicker(VS::Menu::UpdateLiveSaveMenu, 1);
  REQUIRE(Screen().files[0].status == Save::FileStatus::kRecovered);
  Press(kPadRright, VS::Menu::UpdateLiveSaveMenu);
  CHECK(Text(VS::Title::memoryCardMessage) == "Replace invalid original");
  CHECK_FALSE(Screen().affirmative);
  Press(kPadRright, VS::Menu::UpdateLiveSaveMenu);  // Default No.
  CHECK_FALSE(Screen().confirmation);
  CHECK(FileBytes(path) == original);
  CHECK(FileBytes(path.string() + ".bak") == backup);
  ChooseYes(VS::Menu::UpdateLiveSaveMenu);
  Press(0, VS::Menu::UpdateLiveSaveMenu);
  REQUIRE(Text(VS::Title::memoryCardMessage) == "Save complete");
  CHECK(FileBytes(path.string() + ".bak") == backup);
  CheckReplacedRecoveredFile(fixture.root);
}

TEST_CASE("Clear-save failure leaves counters and original bytes unchanged",
          "[save-menu][save-clear]") {
  MenuFixture fixture;
  MenuContentFixture content(fixture);
  content.Store(MenuContentFixture::Document());
  const auto path = fixture.root / "slot00.vssave";
  const auto original = FileBytes(path);
  ClearInputsFixture ending;
  BeginPicker(VS::Menu::UpdateLiveSaveMenu, 1);
  ChooseYes(VS::Menu::UpdateLiveSaveMenu);
  // The target preview remains identical, but replacement cannot reserve its
  // lock. Exercise the same existing persistence failure as ordinary Save.
  REQUIRE(fs::create_directory(path.string() + ".lock"));
  Press(0, VS::Menu::UpdateLiveSaveMenu);
  CheckFailedClearSaveChangedNothing(path, original);
  REQUIRE(fs::remove(path.string() + ".lock"));
  Press(kPadRright, VS::Menu::UpdateLiveSaveMenu);
  Press(0, VS::Menu::UpdateLiveSaveMenu);
  ChooseYes(VS::Menu::UpdateLiveSaveMenu);
  Press(0, VS::Menu::UpdateLiveSaveMenu);
  CHECK(Text(VS::Title::memoryCardMessage) == "Save complete");
  CHECK(VS::Input::settings.cumulative_save_count == 20);
}

TEST_CASE(
    "Native Load dispatches selection sounds at file and affirmative consent",
    "[save-menu]") {
  MenuFixture fixture;
  MenuContentFixture content(fixture);
  content.Store(MenuContentFixture::Document());
  BeginPicker(VS::Menu::UpdateLiveLoadMenu, 2);
  REQUIRE(Screen().files.front().status == Save::FileStatus::kValid);
  CheckSounds(fixture, {});
  Move(kPadLleft, VS::Menu::UpdateLiveLoadMenu);
  Move(kPadLright, VS::Menu::UpdateLiveLoadMenu);
  CheckSounds(fixture, {});
  Press(kPadRright, VS::Menu::UpdateLiveLoadMenu);
  CheckSounds(fixture, {0, 1, 0, 0, 0});
  CHECK_FALSE(Screen().affirmative);
  Move(kPadLleft, VS::Menu::UpdateLiveLoadMenu);
  Move(kPadLright, VS::Menu::UpdateLiveLoadMenu);
  CheckSounds(fixture, {0, 1, 0, 0, 0});
  Move(kPadLup, VS::Menu::UpdateLiveLoadMenu);
  CheckSounds(fixture, {1, 1, 0, 0, 0});
  Press(kPadRright, VS::Menu::UpdateLiveLoadMenu);
  // Both dispatches precede Execute and the C-only gameplay transition.
  CheckSounds(fixture, {1, 2, 0, 0, 0});
  CHECK_FALSE(Save::ResumePending());
}

TEST_CASE(
    "Native confirmation No cancellation and invalid files use original sounds",
    "[save-menu]") {
  MenuFixture fixture;
  BeginPicker(VS::Game::Save::SelectContainer, 1);
  Press(kPadRright, VS::Game::Save::SelectContainer);
  CheckSounds(fixture, {0, 1, 0, 0, 0});
  Press(kPadRright, VS::Game::Save::SelectContainer);
  CHECK_FALSE(Screen().confirmation);
  CheckSounds(fixture, {0, 1, 1, 0, 0});
  Press(kPadRright, VS::Game::Save::SelectContainer);
  Move(kPadLup, VS::Game::Save::SelectContainer);
  Press(kPadRdown, VS::Game::Save::SelectContainer);
  CHECK_FALSE(Screen().confirmation);
  CheckSounds(fixture, {1, 2, 2, 0, 0});
  Buttons(kPadRdown);
  REQUIRE(VS::Game::Save::SelectContainer(0) == -1);
  CheckSounds(fixture, {1, 2, 3, 0, 0});

  BeginPicker(VS::Menu::UpdateLiveDataLoadMenu, 1);
  Press(kPadRright, VS::Menu::UpdateLiveDataLoadMenu);
  CHECK(Text(VS::Menu::Files::memoryCardMessage) == "No usable native save");
  CheckSounds(fixture, {1, 2, 3, 1, 0});
}

TEST_CASE(
    "Native wrap restores cached valid cards immediately and reentry is fresh",
    "[save-menu]") {
  MenuFixture fixture;
  MenuContentFixture content(fixture);
  content.Store(MenuContentFixture::Document());
  BeginPicker(VS::Menu::UpdateLiveLoadMenu, 2);
  REQUIRE(Screen().files.front().status == Save::FileStatus::kValid);
  REQUIRE(fs::remove(fixture.root / "slot00.vssave"));
  Move(kPadLup, VS::Menu::UpdateLiveLoadMenu);
  REQUIRE(SelectedSlot() == 99);
  Buttons(0, kPadLdown);
  Tick(VS::Menu::UpdateLiveLoadMenu);
  // A reread would show Empty. Cached wrap must also avoid a blank read frame.
  REQUIRE(Screen().files.size() == 3);
  CheckSelectedSlot(0);
  CHECK(Screen().files.front().status == Save::FileStatus::kValid);
  CHECK(Text(VS::Title::memoryCardMessage) == "Select a file");
  Buttons(kPadRdown);
  REQUIRE(VS::Menu::UpdateLiveLoadMenu(0) == -1);
  BeginPicker(VS::Menu::UpdateLiveLoadMenu, 2);
  CHECK(Screen().files.front().status == Save::FileStatus::kEmpty);
}

TEST_CASE("Native revisits keep invalid and missing previews until fresh entry",
          "[save-menu]") {
  MenuFixture fixture;
  {
    std::ofstream invalid(fixture.root / "slot00.vssave");
    invalid << "damaged";
  }
  BeginPicker(VS::Menu::UpdateLiveLoadMenu, 2);
  CheckFirstTwoFiles({Save::FileStatus::kInvalid, Save::FileStatus::kEmpty});
  REQUIRE(fs::remove(fixture.root / "slot00.vssave"));
  {
    std::ofstream appeared(fixture.root / "slot01.vssave");
    appeared << "new damaged file";
  }
  Move(kPadLup, VS::Menu::UpdateLiveLoadMenu);
  Buttons(0, kPadLdown);
  Tick(VS::Menu::UpdateLiveLoadMenu);
  CheckFirstTwoFiles({Save::FileStatus::kInvalid, Save::FileStatus::kEmpty});
  Buttons(kPadRdown);
  REQUIRE(VS::Menu::UpdateLiveLoadMenu(0) == -1);
  BeginPicker(VS::Menu::UpdateLiveLoadMenu, 2);
  CheckFirstTwoFiles({Save::FileStatus::kEmpty, Save::FileStatus::kInvalid});
}

TEST_CASE("Native browsing surfaces filesystem errors on uncached slots",
          "[save-menu]") {
  MenuFixture fixture;
  BeginPicker(VS::Menu::UpdateLiveLoadMenu, 2);
  REQUIRE(fs::create_directory(fixture.root / "slot03.vssave"));
  Move(kPadLdown, VS::Menu::UpdateLiveLoadMenu);
  Move(kPadLdown, VS::Menu::UpdateLiveLoadMenu);
  Move(kPadLdown, VS::Menu::UpdateLiveLoadMenu);
  CHECK(Text(VS::Title::memoryCardMessage) == "Cannot read native saves");
  Buttons(kPadRdown);
  REQUIRE(VS::Menu::UpdateLiveLoadMenu(0) == -1);
}

TEST_CASE(
    "Native cached load consent rejects changed content and recovery status",
    "[save-menu]") {
  MenuFixture fixture;
  MenuContentFixture content(fixture);
  auto document = MenuContentFixture::Document();
  bool recovered = false;
  SECTION("A different document with unchanged generation is not consented") {
    content.Store(document);
  }
  SECTION("Recovery consent is not consent to a newly valid primary") {
    content.Store(document, "slot00.vssave.bak");
    std::ofstream invalid(fixture.root / "slot00.vssave");
    invalid << "damaged primary";
    recovered = true;
  }
  BeginPicker(VS::Menu::UpdateLiveLoadMenu, 2);
  REQUIRE(Screen().files[0].status == (recovered ? Save::FileStatus::kRecovered
                                                 : Save::FileStatus::kValid));
  ChooseYes(VS::Menu::UpdateLiveLoadMenu);
  document.state.ashley.current_hp = 249;
  content.Store(document);
  Press(0, VS::Menu::UpdateLiveLoadMenu);
  CHECK(Text(VS::Title::memoryCardMessage) == "File changed - select again");
  CHECK_FALSE(Save::ResumePending());
  CheckSounds(fixture, {1, 2, 0, 0, 0});
  Press(kPadRdown, VS::Menu::UpdateLiveLoadMenu);
  Press(0, VS::Menu::UpdateLiveLoadMenu);
  const auto refreshed = Screen().files[0].document;
  REQUIRE(refreshed);
  CHECK(Screen().files[0].status == Save::FileStatus::kValid);
  if (refreshed) {
    CHECK(refreshed->state.ashley.current_hp == 249);
  }
}

TEST_CASE("Native empty container consent revalidates an appeared save",
          "[save-menu]") {
  MenuFixture fixture;
  MenuContentFixture content(fixture);
  BeginPicker(VS::Game::Save::SelectContainer, 1);
  REQUIRE(Screen().files[0].status == Save::FileStatus::kEmpty);
  ChooseYes(VS::Game::Save::SelectContainer);
  content.Store(MenuContentFixture::Document());
  Press(0, VS::Game::Save::SelectContainer);
  CHECK(Text(VS::Menu::Files::memoryCardMessage) ==
        "File changed - select again");
  CHECK_FALSE(Save::ResumePending());
}

TEST_CASE("Native save retry refreshes previews without losing staged storage",
          "[save-menu]") {
  MenuFixture fixture;
  BeginPicker(VS::Game::Save::SelectContainer, 1);
  ChooseYes(VS::Game::Save::SelectContainer);
  Buttons(0);
  REQUIRE(VS::Game::Save::SelectContainer(0) == 1);
  auto* bank = VS::Game::Save::ContainerData();
  bank->misc[0].id = 323;
  bank->misc[0].count = 2;
  BeginPicker(VS::Menu::UpdateLiveDataSaveMenu, 2);
  ConfirmSave();
  CheckFailedSave(fixture);
  {
    std::ofstream appeared(fixture.root / "slot00.vssave");
    appeared << "changed after failed write";
  }
  Press(kPadRright, VS::Menu::UpdateLiveDataSaveMenu);
  Press(0, VS::Menu::UpdateLiveDataSaveMenu);
  CHECK(Screen().files[0].status == Save::FileStatus::kInvalid);
  REQUIRE(fs::remove(fixture.root / "slot00.vssave"));
  Press(kPadRright, VS::Menu::UpdateLiveDataSaveMenu);
  REQUIRE(Text(VS::Menu::Files::memoryCardMessage) == "No usable native save");
  Press(kPadRright, VS::Menu::UpdateLiveDataSaveMenu);
  Press(0, VS::Menu::UpdateLiveDataSaveMenu);
  REQUIRE(Screen().files[0].status == Save::FileStatus::kEmpty);
  ConfirmSave();
  CheckFailedSave(fixture);
  CheckStagedBank(bank);
  VS::Game::Save::ReleaseContainer();
}

TEST_CASE("Native cached previews cannot conceal missing load content",
          "[save-menu]") {
  MenuFixture fixture;
  MenuContentFixture content(fixture);
  content.Store(MenuContentFixture::Document());
  BeginPicker(VS::Menu::UpdateLiveLoadMenu, 2);
  REQUIRE(Screen().files[0].status == Save::FileStatus::kValid);
  REQUIRE(fs::remove(content.root / "disc" / "MAP" / "MAP009.MPD"));
  ChooseYes(VS::Menu::UpdateLiveLoadMenu);
  Press(0, VS::Menu::UpdateLiveLoadMenu);
  CHECK(Text(VS::Title::memoryCardMessage) == "File changed - select again");
  CHECK_FALSE(Save::ResumePending());
  Press(kPadRdown, VS::Menu::UpdateLiveLoadMenu);
  Press(0, VS::Menu::UpdateLiveLoadMenu);
  CHECK(Screen().files[0].status == Save::FileStatus::kInvalid);
  CHECK_FALSE(Screen().files[0].message.empty());
}

TEST_CASE("Invalid container menu states are diagnosed without mutating state",
          "[save-menu][container-state]") {
  bool diagnosed = false;
  const auto previous =
      VS::Core::SetLogObserver([&diagnosed](const VS::Core::LogRecord& record) {
        diagnosed |= record.level == VS::Core::LogLevel::Error &&
                     record.channel == "save" &&
                     record.message == "Invalid container menu state: 255";
      });
  char state = static_cast<char>(255);
  const auto result = VS::Menu::Files::UpdateLiveContainerSave(state);
  VS::Core::SetLogObserver(previous);
  CHECK(result == 0);
  CHECK(static_cast<unsigned char>(state) == 255);
  CHECK(diagnosed);
}
