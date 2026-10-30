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
#include <string>
#include <system_error>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Content/DataStore.hpp"
#include "Content/ModuleData.h"
#include "Core/Log.hpp"
#include "Save/NativeFilePresentation.hpp"
#include "Save/NativeMenu.h"
#include "Save/NativeMenu.hpp"
#include "Save/RuntimeSnapshot.hpp"

extern "C" {
#include "Audio/Driver/SoundDriver.h"
#include "Audio/Mixer/VoiceMixer.h"
#include "Audio/Playback/SoundPlayback.h"
#include "Audio/Seq/Channel.h"
#include "Audio/Sfx.h"
#include "Boot/GameLoop.h"
#include "Ending/ClearResults.h"
#include "Field/Map.h"
#include "Field/Mpd.h"
#include "Field/Znd.h"
#include "Input/Controller.h"
#include "Items/Categories.h"
#include "Menu/Files.h"
#include "Save/NativeContainer.h"
#include "State/Ashley.h"
#include "State/Location.h"
#include "State/PlayTime.h"
#include "State/RoomMemory.h"
#include "Title/SaveFiles.h"
}

namespace {
namespace fs = std::filesystem;
namespace Save = VS::Game::Save;

constexpr std::size_t kSoundCount = 12;
std::array<unsigned, kSoundCount>* sound_calls = nullptr;

void CountSound(Audio_CommandParams* parameters) {
  if (sound_calls != nullptr && parameters->param1 < sound_calls->size()) {
    ++(*sound_calls)[parameters->param1];
  }
}

struct MenuFixture {
  fs::path root;
  State_Settings settings = Input_settings;
  int active = Boot_isGameplayActive;
  decltype(Input_buttonsPressed.allButtons) pressed =
      Input_buttonsPressed.allButtons;
  int repeat = Input_buttonRepeat;
  std::array<u_int, 12> sound_offsets{};
  std::array<u_short, 12> sound_selectors{};
  u_int* saved_offsets = Audio_Playback_soundEffectProgramOffsets;
  u_short* saved_selectors = Audio_Playback_soundEffectBankSelectors;
  Audio_CommandHandler sound_handler = Audio_Mixer_kCommandHandlers[0x20];
  Audio_CommandArguments sound_arguments = Audio_Driver_commandArguments;
  Audio_CommandParams sound_params = Audio_Mixer_commandParams;
  int sound_group = Audio_nextSfxChannelGroup7E;
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
    Boot_isGameplayActive = 0;
    Input_buttonsPressed.allButtons = 0;
    Input_buttonRepeat = 0;
    sound_selectors.fill(0x8000);
    Audio_Playback_soundEffectProgramOffsets = sound_offsets.data();
    Audio_Playback_soundEffectBankSelectors = sound_selectors.data();
    Audio_Mixer_kCommandHandlers[0x20] = CountSound;
    Audio_nextSfxChannelGroup7E = 2;
    sound_calls = &sounds;
  }

  ~MenuFixture() {
    try {
      Save::ShutdownFileMenus();
      Input_settings = settings;
      Boot_isGameplayActive = active;
      Input_buttonsPressed.allButtons = pressed;
      Input_buttonRepeat = repeat;
      Audio_Playback_soundEffectProgramOffsets = saved_offsets;
      Audio_Playback_soundEffectBankSelectors = saved_selectors;
      Audio_Mixer_kCommandHandlers[0x20] = sound_handler;
      Audio_Driver_commandArguments = sound_arguments;
      Audio_Mixer_commandParams = sound_params;
      Audio_nextSfxChannelGroup7E = sound_group;
      sound_calls = previous_calls;
      std::error_code error;
      fs::remove_all(root, error);
      if (error) {
        VS_LOG_ERROR("test", "Cannot clean native menu fixture: {}",
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
    std::copy_n(Items_itemCategoryUpperIds, previous_bounds.size(),
                previous_bounds.begin());
    std::copy(kBounds.begin(), kBounds.end(), Items_itemCategoryUpperIds);
    std::vector<std::uint8_t> room;
    Field_MpdHeader header{};
    header.roomSectionOffset = sizeof(header);
    std::array<std::uint32_t, 24> lengths{};
    lengths[1] = sizeof(Field_MpdCollisionHeader) + 4 + 16;
    lengths[10] = sizeof(Field_MpdEnvironment);
    Put(room, sizeof(header), lengths);
    const std::size_t collision = sizeof(header) + sizeof(lengths);
    Put(room, collision, Field_MpdCollisionHeader{1, 1, 0, 1});
    room.resize(collision + lengths[1]);
    Field_MpdEnvironment environment{};
    environment.sceneId = 1;
    Put(room, room.size(), environment);
    header.enemySectionOffset = static_cast<int>(room.size());
    Put(room, 0, header);
    Add("MAP/MAP009.MPD", room, 200);

    std::vector<std::uint8_t> zone;
    Put(zone, 0, Field_ZndHeader{28, 8, 0, 0, 0, 0, 0});
    Put(zone, 28, std::uint32_t{200});
    Put(zone, 32, static_cast<std::uint32_t>(room.size()));
    Add("MAP/ZONE009.ZND", zone, 100);
    std::vector<std::uint8_t> start;
    lengths[1] = sizeof(Field_MpdCollisionHeader) + 8 * 8 * 4 + 16;
    Put(start, sizeof(header), lengths);
    Put(start, collision, Field_MpdCollisionHeader{8, 8, 0, 1});
    start.resize(collision + lengths[1]);
    environment.sceneId = 0;
    Put(start, start.size(), environment);
    header.enemySectionOffset = static_cast<int>(start.size());
    Put(start, 0, header);
    Add("MAP/MAP001.MPD", start, 201);
    Put(zone, 28, std::uint32_t{201});
    Put(zone, 32, static_cast<std::uint32_t>(start.size()));
    Add("MAP/ZONE001.ZND", zone, 101);

    std::vector<std::uint8_t> arm;
    Put(arm, 0, std::uint32_t{1});
    Put(arm, 4, Field_MapRoom{0, 24, 9, 1});
    arm.resize(4 + sizeof(Field_MapRoom) + 24);
    Field_MapRoomName name{};
    name.name[0] = 10;
    name.name[1] = static_cast<char>(0xE7);
    Put(arm, arm.size(), name);
    Add("SMALL/SCEN001.ARM", arm, 300);

    bool found = false;
    for (unsigned index = 0; index < Content_moduleDataCount; ++index) {
      const auto& slice = Content_moduleData[index];
      if (slice.target != &Items_itemCategoryUpperIds) continue;
      std::vector<std::uint8_t> battle;
      Put(battle, slice.offset, kBounds);
      Add(slice.module, battle, 10000);
      found = true;
      break;
    }
    REQUIRE(found);
    {
      std::ofstream output(root / "files.tsv");
      output << manifest;
      REQUIRE(output.good());
    }
    const auto opened = VS::Content::DataStore::Open(root);
    INFO((opened ? "content open" : opened.error()));
    REQUIRE(opened.has_value());
  }

  ~MenuContentFixture() {
    service.Restore();
    std::copy(previous_bounds.begin(), previous_bounds.end(),
              Items_itemCategoryUpperIds);
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
    state.ashley.currentHp = 250;
    state.ashley.maxHp = 250;
    state.ashley.currentMp = 50;
    state.ashley.maxMp = 50;
    state.ashley.cameraDistance = 1536;
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
  Input_buttonsPressed.allButtons = pressed;
  Input_buttonRepeat = repeat;
}

void ConfirmSave() {
  Buttons(PADRright);
  CHECK(Menu_Files_UpdateDataSaveMenu(0) == 0);
  Buttons(0, PADLup);
  CHECK(Menu_Files_UpdateDataSaveMenu(0) == 0);
  Buttons(PADRright);
  CHECK(Menu_Files_UpdateDataSaveMenu(0) == 0);
  Buttons(0);
  CHECK(Menu_Files_UpdateDataSaveMenu(0) == 0);
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
  Buttons(PADRright);
  REQUIRE(update(0) == 0);
  REQUIRE(Screen().confirmation);
  CHECK_FALSE(Screen().affirmative);
  Move(PADLup, update);
  REQUIRE(Screen().affirmative);
  Buttons(PADRright);
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

void CheckStagedBank(const Items_ContainerData* bank) {
  CHECK(Save_NativeContainerData() == bank);
  CHECK(bank->misc[0].id == 323);
  CHECK(bank->misc[0].count == 2);
}

void CheckSelectedSlot(int slot) { CHECK(SelectedSlot() == slot); }

void CheckTitlePickerWrapAndCancel() {
  Move(PADLdown, Menu_Files_UpdateLoadMenu);
  CHECK(SelectedSlot() == 0);
  Move(PADLup, Menu_Files_UpdateLoadMenu);
  CHECK(SelectedSlot() == 99);
  CHECK(Screen().files.front().slot == 97);
  CHECK(Screen().files.back().slot == 99);
  Buttons(PADRdown);
  CHECK(Menu_Files_UpdateLoadMenu(0) == -1);
  CHECK_FALSE(Save::FileMenuPresentation());
}

void CheckContainerTail(const Items_ContainerData* bank) {
  CHECK(Save_NativeContainerData() == bank);
  CHECK(Save_NativeContainerData()->misc[255].id == 308);
  CHECK(Save_NativeContainerData()->misc[255].count == 2);
}

void CheckVerticalConsent() {
  Move(PADLleft, Save_NativeSelectContainer);
  CHECK_FALSE(Screen().affirmative);
  Move(PADLright, Save_NativeSelectContainer);
  CHECK_FALSE(Screen().affirmative);
  Move(PADLup, Save_NativeSelectContainer);
  CHECK(Screen().affirmative);
  Move(PADLdown, Save_NativeSelectContainer);
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
  REQUIRE(Text(Menu_Files_memoryCardMessage) == "Save failed - see log");
  CHECK(fixture.sounds[8] == 0);
  CHECK(Input_settings.cumulativeSaveCount ==
        fixture.settings.cumulativeSaveCount);
  CHECK(Input_settings.currentGameSaveCount ==
        fixture.settings.currentGameSaveCount);
  CHECK(Input_settings.saveFileGeneration ==
        fixture.settings.saveFileGeneration);
  CHECK_FALSE(fs::exists(fixture.root / "slot00.vssave"));
}
}  // namespace

TEST_CASE("Native Continue scrolls all one hundred files vertically",
          "[save-menu]") {
  MenuFixture fixture;
  CHECK(Save_NativeInitialTitleSelection() == 0);
  BeginPicker(Menu_Files_UpdateLoadMenu, 2);
  CHECK(SelectedSlot() == 0);
  CHECK(Screen().summaries.size() == 3);
  CHECK(Text(Title_memoryCardMessage) == "Select a file");
  REQUIRE(Title_memoryCardMessage != nullptr);
  for (int slot = 1; slot < 100; ++slot) {
    Move(PADLdown, Menu_Files_UpdateLoadMenu);
    CHECK(SelectedSlot() == slot);
    CHECK(Screen().files.front().slot == std::max(0, slot - 2));
  }
  CheckTitlePickerWrapAndCancel();
}

TEST_CASE("Native file screens have three cards without generic row panels",
          "[save-menu]") {
  MenuFixture fixture;
  BeginPicker(Menu_Files_UpdateLoadMenu, 2);
  REQUIRE(Screen().files.size() == 3);
  CHECK(Screen().title);
  CHECK_FALSE(Screen().save);
  for (int i = 0; i < 10; ++i) {
    CHECK(Title_fileMenuElements[i].state == Menu_Files_ElementState_Inactive);
  }
  for (const auto& file : Screen().files) {
    CHECK(file.status == Save::FileStatus::kEmpty);
    CHECK_FALSE(file.document);
  }
  Move(PADLleft, Menu_Files_UpdateLoadMenu);
  CheckSelectedSlot(0);
  Move(PADLright, Menu_Files_UpdateLoadMenu);
  CheckSelectedSlot(0);
}

TEST_CASE("Native Save cannot bypass production save-point eligibility",
          "[save-menu]") {
  MenuFixture fixture;
  const auto before = Input_settings;
  REQUIRE(Menu_Files_UpdateDataSaveMenu(1) == 0);
  REQUIRE(Menu_Files_UpdateDataSaveMenu(0) == 0);
  ConfirmSave();
  CHECK(Text(Menu_Files_memoryCardMessage) == "Save failed - see log");
  CHECK(!fs::exists(fixture.root / "slot00.vssave"));
  CHECK(Input_settings.cumulativeSaveCount == before.cumulativeSaveCount);
  CHECK(Input_settings.currentGameSaveCount == before.currentGameSaveCount);
  CHECK(Input_settings.saveFileGeneration == before.saveFileGeneration);
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
  CHECK(Save_NativeInitialTitleSelection() == 1);
  REQUIRE(Menu_Files_UpdateDataSaveMenu(1) == 0);
  REQUIRE(Menu_Files_UpdateDataSaveMenu(0) == 0);
  CHECK(Screen().files.front().status == Save::FileStatus::kInvalid);
  CHECK(Text(Menu_Files_memoryCardMessage) == "Invalid data - not overwritten");
  Buttons(PADRright);
  CHECK(Menu_Files_UpdateDataSaveMenu(0) == 0);
  CHECK(Text(Menu_Files_memoryCardMessage) == "No usable native save");
  std::ifstream input(path, std::ios::binary);
  CHECK(std::string(std::istreambuf_iterator<char>(input), {}) ==
        "unsupported or damaged save");
}

TEST_CASE("Container selection owns typed storage beyond picker reentry",
          "[save-menu]") {
  MenuFixture fixture;
  BeginPicker(Save_NativeSelectContainer, 1);
  Press(PADRright, Save_NativeSelectContainer);
  REQUIRE(Screen().confirmation);
  CHECK_FALSE(Screen().affirmative);
  Buttons(0, PADLup);
  Tick(Save_NativeSelectContainer);
  Press(PADRright, Save_NativeSelectContainer);
  Buttons(0);
  REQUIRE(Save_NativeSelectContainer(0) == 1);
  auto* bank = Save_NativeContainerData();
  REQUIRE(bank != nullptr);
  CHECK(Input_settings.containerEmpty == 1);
  bank->misc[255].id = 308;
  bank->misc[255].count = 2;
  REQUIRE(Menu_Files_UpdateDataSaveMenu(2) == 0);
  CheckContainerTail(bank);
  Save_NativeReleaseContainer();
}

TEST_CASE("Native confirmation is vertical explicit consent and cancelable",
          "[save-menu]") {
  MenuFixture fixture;
  BeginPicker(Save_NativeSelectContainer, 1);
  Press(PADRright, Save_NativeSelectContainer);
  REQUIRE(Screen().confirmation);
  CHECK_FALSE(Screen().affirmative);
  CheckVerticalConsent();
  Press(PADRright, Save_NativeSelectContainer);
  CheckCanceledConsent(fixture.root);
  Press(PADRright, Save_NativeSelectContainer);
  CHECK_FALSE(Screen().affirmative);
  Move(PADLup, Save_NativeSelectContainer);
  CHECK(Screen().affirmative);
  Press(PADRdown, Save_NativeSelectContainer);
  CheckCanceledConsent(fixture.root);
}

TEST_CASE("Native reentry retains the last file at the end of the scroll",
          "[save-menu]") {
  MenuFixture fixture;
  BeginPicker(Save_NativeSelectContainer, 1);
  Move(PADLup, Save_NativeSelectContainer);
  CheckSelectedSlot(99);
  Press(PADRright, Save_NativeSelectContainer);
  Move(PADLup, Save_NativeSelectContainer);
  Press(PADRright, Save_NativeSelectContainer);
  Buttons(0);
  REQUIRE(Save_NativeSelectContainer(0) == 1);
  BeginPicker(Menu_Files_UpdateDataSaveMenu, 2);
  CheckSelectedSlot(99);
  CHECK(Screen().files.front().slot == 97);
  CHECK(Screen().files.back().slot == 99);
  CHECK_FALSE(Screen().title);
  CHECK(Screen().save);
  Save_NativeReleaseContainer();
}

TEST_CASE("Native session reset clears borrowed presentation and footer",
          "[save-menu]") {
  MenuFixture fixture;
  REQUIRE(Menu_Files_UpdateLoadMenu(2) == 0);
  REQUIRE(Menu_Files_UpdateLoadMenu(0) == 0);
  REQUIRE(Save::FileMenuPresentation());
  REQUIRE(Title_memoryCardMessage != nullptr);
  Save::ResetFileMenuSession();
  CHECK_FALSE(Save::FileMenuPresentation());
  CHECK(Title_memoryCardMessage == nullptr);
  Save_NativeDrawFileOverlay(0);
  Save_NativeDrawFileOverlay(1);
}

TEST_CASE("Native clear saves require staged original ending inputs",
          "[save-menu]") {
  MenuFixture fixture;
  REQUIRE(Menu_Files_UpdateSaveMenu(1) == 0);
  CHECK(Text(Title_memoryCardMessage) == "No ending clear save available");
  Buttons(PADRright);
  CHECK(Menu_Files_UpdateSaveMenu(0) == -1);
  CHECK(fs::is_empty(fixture.root));
}

namespace {
struct ClearInputsFixture {
  decltype(Audio_Seq_unusedStateFlags) flags = Audio_Seq_unusedStateFlags;
  decltype(Audio_Seq_unusedInventory) inventory = Audio_Seq_unusedInventory;
  decltype(Audio_Seq_unusedInventoryIndices) order =
      Audio_Seq_unusedInventoryIndices;
  State_AshleySnapshot ashley = State_ashleySnapshot;
  Field_MpdDoorRecord entry = State_savedEntryDoor;
  State_RoomVisitFlags visits = State_roomVisitProgress;
  State_ScoreData score = Audio_Seq_unusedScoredata;
  decltype(State_playTime) time = State_playTime;
  decltype(std::to_array(State_recentRoomEnemyStates)) caches =
      std::to_array(State_recentRoomEnemyStates);
  decltype(std::to_array(State_learnedActionFlags)) learned =
      std::to_array(State_learnedActionFlags);
  int cache_slot = State_currentRoomStateSlot;
  int clear_pending = Boot_isClearSavePending;

  ClearInputsFixture() {
    Audio_Seq_unusedStateFlags = {};
    Audio_Seq_unusedInventory = {};
    Audio_Seq_unusedInventoryIndices = {};
    Audio_Seq_unusedInventory.misc[0].id = 308;
    Audio_Seq_unusedInventory.misc[0].count = 2;
    Audio_Seq_unusedInventoryIndices.misc[0] = 1;
    State_ashleySnapshot.stats = MenuContentFixture::Document().state.ashley;
    State_savedEntryDoor = {};
    State_savedEntryDoor.entryTileX = 4;
    State_savedEntryDoor.entryTileZ = 2;
    State_roomVisitProgress = {};
    Audio_Seq_unusedScoredata = {};
    State_playTime.all = 0;
    State_playTime.t.hours = 1;
    State_currentRoomStateSlot = 0;
    std::fill_n(State_recentRoomEnemyStates, caches.size(),
                State_RoomEnemyStates{});
    std::fill_n(State_learnedActionFlags, learned.size(), 0);
    State_learnedActionFlags[0] = static_cast<char>(0x80);
    Input_settings = {};
    Input_settings.currentGameSaveCount = 7;
    Input_settings.cumulativeSaveCount = 19;
    Input_settings.saveFileGeneration = 3;
    Ending_CommitClearProgress();
    Boot_isClearSavePending = 1;
    Save_NativeStageClearGame();
    Boot_isClearSavePending = 0;
  }

  ~ClearInputsFixture() {
    Audio_Seq_unusedStateFlags = flags;
    Audio_Seq_unusedInventory = inventory;
    Audio_Seq_unusedInventoryIndices = order;
    State_ashleySnapshot = ashley;
    State_savedEntryDoor = entry;
    State_roomVisitProgress = visits;
    Audio_Seq_unusedScoredata = score;
    State_playTime = time;
    std::copy(caches.begin(), caches.end(), State_recentRoomEnemyStates);
    std::copy(learned.begin(), learned.end(), State_learnedActionFlags);
    State_currentRoomStateSlot = cache_slot;
    Boot_isClearSavePending = clear_pending;
  }
};

std::string FileBytes(const fs::path& path) {
  std::ifstream input(path, std::ios::binary);
  REQUIRE(input.good());
  return {std::istreambuf_iterator<char>(input), {}};
}

void ExecuteClearSave() {
  BeginPicker(Menu_Files_UpdateSaveMenu, 1);
  ChooseYes(Menu_Files_UpdateSaveMenu);
  Buttons(0);
  Tick(Menu_Files_UpdateSaveMenu);
}
}  // namespace

TEST_CASE("Original ending component writes a distinct native clear save",
          "[save-menu][save-clear]") {
  MenuFixture fixture;
  MenuContentFixture content(fixture);
  ClearInputsFixture ending;
  ExecuteClearSave();
  REQUIRE(Text(Title_memoryCardMessage) == "Save complete");
  auto files = Save::Files::Open(fixture.root);
  REQUIRE(files);
  const auto loaded = files->Load(0);
  REQUIRE(loaded);
  REQUIRE(loaded->document.new_game_entry);
  CHECK(loaded->document.generation == 1);
  CHECK(loaded->document.state.settings.cumulativeSaveCount == 20);
  CHECK(loaded->document.state.settings.currentGameSaveCount == 0);
  CHECK(loaded->document.state.settings.saveFileGeneration == 4);
  CHECK(loaded->document.state.script_flags[0] == 1);
  CHECK(loaded->document.state.ashley.currentHp == 250);
  CHECK(loaded->document.state.ashley.currentMp == 50);
  CHECK(loaded->document.state.inventory.misc[0].id == 0);
  CHECK(loaded->document.state.learned_actions[0]);
  CHECK(Input_settings.cumulativeSaveCount == 20);
  CHECK(Input_settings.currentGameSaveCount == 0);
  CHECK(Menu_Files_dataNotSaved == 1);
  CHECK(Text(Screen().summaries[0].room_name.data()) == "CLEAR GAME");
  Buttons(PADRright);
  CHECK(Menu_Files_UpdateSaveMenu(0) == 1);
  Save::ShutdownFileMenus();
  Save::ConfigureDirectory(fixture.root);
  CHECK(Save_NativeInitialTitleSelection() == 1);
  BeginPicker(Menu_Files_UpdateLoadMenu, 1);
  CHECK(Screen().files.front().document->new_game_entry.has_value());
}

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
  REQUIRE(Text(Title_memoryCardMessage) == "Save complete");
  CHECK(FileBytes(path.string() + ".bak") == original);
  auto files = Save::Files::Open(fixture.root);
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

TEST_CASE("Clear-save consent rejects changed content and reset releases input",
          "[save-menu][save-clear]") {
  MenuFixture fixture;
  MenuContentFixture content(fixture);
  ClearInputsFixture ending;
  BeginPicker(Menu_Files_UpdateSaveMenu, 1);
  ChooseYes(Menu_Files_UpdateSaveMenu);
  content.Store(MenuContentFixture::Document());
  const auto original = FileBytes(fixture.root / "slot00.vssave");
  Buttons(0);
  Tick(Menu_Files_UpdateSaveMenu);
  CHECK(Text(Title_memoryCardMessage) == "File changed - select again");
  CHECK(FileBytes(fixture.root / "slot00.vssave") == original);
  CHECK(Input_settings.cumulativeSaveCount == 19);
  CHECK(Input_settings.currentGameSaveCount == 7);
  Save::ResetFileMenuSession();
  REQUIRE(Menu_Files_UpdateSaveMenu(1) == 0);
  CHECK(Text(Title_memoryCardMessage) == "No ending clear save available");
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
  BeginPicker(Menu_Files_UpdateSaveMenu, 1);
  REQUIRE(Screen().files[0].status == Save::FileStatus::kRecovered);
  Press(PADRright, Menu_Files_UpdateSaveMenu);
  CHECK(Text(Title_memoryCardMessage) == "Replace invalid original");
  CHECK_FALSE(Screen().affirmative);
  Press(PADRright, Menu_Files_UpdateSaveMenu);  // Default No.
  CHECK_FALSE(Screen().confirmation);
  CHECK(FileBytes(path) == original);
  CHECK(FileBytes(path.string() + ".bak") == backup);
  ChooseYes(Menu_Files_UpdateSaveMenu);
  Press(0, Menu_Files_UpdateSaveMenu);
  REQUIRE(Text(Title_memoryCardMessage) == "Save complete");
  CHECK(FileBytes(path.string() + ".bak") == backup);
  const auto files = Save::Files::Open(fixture.root);
  REQUIRE(files);
  const auto loaded = files->Load(0);
  REQUIRE(loaded);
  CHECK_FALSE(loaded->recovered);
  CHECK(loaded->document.new_game_entry.has_value());
  CHECK(loaded->document.generation == 2);
}

TEST_CASE("Clear-save failure leaves counters and original bytes unchanged",
          "[save-menu][save-clear]") {
  MenuFixture fixture;
  MenuContentFixture content(fixture);
  content.Store(MenuContentFixture::Document());
  const auto path = fixture.root / "slot00.vssave";
  const auto original = FileBytes(path);
  ClearInputsFixture ending;
  BeginPicker(Menu_Files_UpdateSaveMenu, 1);
  ChooseYes(Menu_Files_UpdateSaveMenu);
  // The target preview remains identical, but replacement cannot reserve its
  // lock. Exercise the same existing persistence failure as ordinary Save.
  REQUIRE(fs::create_directory(path.string() + ".lock"));
  Press(0, Menu_Files_UpdateSaveMenu);
  CHECK(Text(Title_memoryCardMessage) == "Save failed - see log");
  CHECK(Input_settings.cumulativeSaveCount == 19);
  CHECK(Input_settings.currentGameSaveCount == 7);
  CHECK(Input_settings.saveFileGeneration == 3);
  CHECK(FileBytes(path) == original);
  REQUIRE(fs::remove(path.string() + ".lock"));
  Press(PADRright, Menu_Files_UpdateSaveMenu);
  Press(0, Menu_Files_UpdateSaveMenu);
  ChooseYes(Menu_Files_UpdateSaveMenu);
  Press(0, Menu_Files_UpdateSaveMenu);
  CHECK(Text(Title_memoryCardMessage) == "Save complete");
  CHECK(Input_settings.cumulativeSaveCount == 20);
}

TEST_CASE(
    "Native Load dispatches selection sounds at file and affirmative consent",
    "[save-menu]") {
  MenuFixture fixture;
  MenuContentFixture content(fixture);
  content.Store(MenuContentFixture::Document());
  BeginPicker(Menu_Files_UpdateLoadMenu, 2);
  REQUIRE(Screen().files.front().status == Save::FileStatus::kValid);
  CheckSounds(fixture, {});
  Move(PADLleft, Menu_Files_UpdateLoadMenu);
  Move(PADLright, Menu_Files_UpdateLoadMenu);
  CheckSounds(fixture, {});
  Press(PADRright, Menu_Files_UpdateLoadMenu);
  CheckSounds(fixture, {0, 1, 0, 0, 0});
  CHECK_FALSE(Screen().affirmative);
  Move(PADLleft, Menu_Files_UpdateLoadMenu);
  Move(PADLright, Menu_Files_UpdateLoadMenu);
  CheckSounds(fixture, {0, 1, 0, 0, 0});
  Move(PADLup, Menu_Files_UpdateLoadMenu);
  CheckSounds(fixture, {1, 1, 0, 0, 0});
  Press(PADRright, Menu_Files_UpdateLoadMenu);
  // Both dispatches precede Execute and the C-only gameplay transition.
  CheckSounds(fixture, {1, 2, 0, 0, 0});
  CHECK_FALSE(Save::ResumePending());
}

TEST_CASE(
    "Native confirmation No cancellation and invalid files use original sounds",
    "[save-menu]") {
  MenuFixture fixture;
  BeginPicker(Save_NativeSelectContainer, 1);
  Press(PADRright, Save_NativeSelectContainer);
  CheckSounds(fixture, {0, 1, 0, 0, 0});
  Press(PADRright, Save_NativeSelectContainer);
  CHECK_FALSE(Screen().confirmation);
  CheckSounds(fixture, {0, 1, 1, 0, 0});
  Press(PADRright, Save_NativeSelectContainer);
  Move(PADLup, Save_NativeSelectContainer);
  Press(PADRdown, Save_NativeSelectContainer);
  CHECK_FALSE(Screen().confirmation);
  CheckSounds(fixture, {1, 2, 2, 0, 0});
  Buttons(PADRdown);
  REQUIRE(Save_NativeSelectContainer(0) == -1);
  CheckSounds(fixture, {1, 2, 3, 0, 0});

  BeginPicker(Menu_Files_UpdateDataLoadMenu, 1);
  Press(PADRright, Menu_Files_UpdateDataLoadMenu);
  CHECK(Text(Menu_Files_memoryCardMessage) == "No usable native save");
  CheckSounds(fixture, {1, 2, 3, 1, 0});
}

TEST_CASE(
    "Native wrap restores cached valid cards immediately and reentry is fresh",
    "[save-menu]") {
  MenuFixture fixture;
  MenuContentFixture content(fixture);
  content.Store(MenuContentFixture::Document());
  BeginPicker(Menu_Files_UpdateLoadMenu, 2);
  REQUIRE(Screen().files.front().status == Save::FileStatus::kValid);
  REQUIRE(fs::remove(fixture.root / "slot00.vssave"));
  Move(PADLup, Menu_Files_UpdateLoadMenu);
  REQUIRE(SelectedSlot() == 99);
  Buttons(0, PADLdown);
  Tick(Menu_Files_UpdateLoadMenu);
  // A reread would show Empty. Cached wrap must also avoid a blank read frame.
  REQUIRE(Screen().files.size() == 3);
  CheckSelectedSlot(0);
  CHECK(Screen().files.front().status == Save::FileStatus::kValid);
  CHECK(Text(Title_memoryCardMessage) == "Select a file");
  Buttons(PADRdown);
  REQUIRE(Menu_Files_UpdateLoadMenu(0) == -1);
  BeginPicker(Menu_Files_UpdateLoadMenu, 2);
  CHECK(Screen().files.front().status == Save::FileStatus::kEmpty);
}

TEST_CASE("Native revisits keep invalid and missing previews until fresh entry",
          "[save-menu]") {
  MenuFixture fixture;
  {
    std::ofstream invalid(fixture.root / "slot00.vssave");
    invalid << "damaged";
  }
  BeginPicker(Menu_Files_UpdateLoadMenu, 2);
  CheckFirstTwoFiles({Save::FileStatus::kInvalid, Save::FileStatus::kEmpty});
  REQUIRE(fs::remove(fixture.root / "slot00.vssave"));
  {
    std::ofstream appeared(fixture.root / "slot01.vssave");
    appeared << "new damaged file";
  }
  Move(PADLup, Menu_Files_UpdateLoadMenu);
  Buttons(0, PADLdown);
  Tick(Menu_Files_UpdateLoadMenu);
  CheckFirstTwoFiles({Save::FileStatus::kInvalid, Save::FileStatus::kEmpty});
  Buttons(PADRdown);
  REQUIRE(Menu_Files_UpdateLoadMenu(0) == -1);
  BeginPicker(Menu_Files_UpdateLoadMenu, 2);
  CheckFirstTwoFiles({Save::FileStatus::kEmpty, Save::FileStatus::kInvalid});
}

TEST_CASE("Native browsing surfaces filesystem errors on uncached slots",
          "[save-menu]") {
  MenuFixture fixture;
  BeginPicker(Menu_Files_UpdateLoadMenu, 2);
  REQUIRE(fs::create_directory(fixture.root / "slot03.vssave"));
  Move(PADLdown, Menu_Files_UpdateLoadMenu);
  Move(PADLdown, Menu_Files_UpdateLoadMenu);
  Move(PADLdown, Menu_Files_UpdateLoadMenu);
  CHECK(Text(Title_memoryCardMessage) == "Cannot read native saves");
  Buttons(PADRdown);
  REQUIRE(Menu_Files_UpdateLoadMenu(0) == -1);
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
  BeginPicker(Menu_Files_UpdateLoadMenu, 2);
  REQUIRE(Screen().files[0].status == (recovered ? Save::FileStatus::kRecovered
                                                 : Save::FileStatus::kValid));
  ChooseYes(Menu_Files_UpdateLoadMenu);
  document.state.ashley.currentHp = 249;
  content.Store(document);
  Press(0, Menu_Files_UpdateLoadMenu);
  CHECK(Text(Title_memoryCardMessage) == "File changed - select again");
  CHECK_FALSE(Save::ResumePending());
  CheckSounds(fixture, {1, 2, 0, 0, 0});
  Press(PADRdown, Menu_Files_UpdateLoadMenu);
  Press(0, Menu_Files_UpdateLoadMenu);
  const auto refreshed = Screen().files[0].document;
  REQUIRE(refreshed);
  CHECK(Screen().files[0].status == Save::FileStatus::kValid);
  if (refreshed) {
    CHECK(refreshed->state.ashley.currentHp == 249);
  }
}

TEST_CASE("Native empty container consent revalidates an appeared save",
          "[save-menu]") {
  MenuFixture fixture;
  MenuContentFixture content(fixture);
  BeginPicker(Save_NativeSelectContainer, 1);
  REQUIRE(Screen().files[0].status == Save::FileStatus::kEmpty);
  ChooseYes(Save_NativeSelectContainer);
  content.Store(MenuContentFixture::Document());
  Press(0, Save_NativeSelectContainer);
  CHECK(Text(Menu_Files_memoryCardMessage) == "File changed - select again");
  CHECK_FALSE(Save::ResumePending());
}

TEST_CASE("Native save retry refreshes previews without losing staged storage",
          "[save-menu]") {
  MenuFixture fixture;
  BeginPicker(Save_NativeSelectContainer, 1);
  ChooseYes(Save_NativeSelectContainer);
  Buttons(0);
  REQUIRE(Save_NativeSelectContainer(0) == 1);
  auto* bank = Save_NativeContainerData();
  bank->misc[0].id = 323;
  bank->misc[0].count = 2;
  BeginPicker(Menu_Files_UpdateDataSaveMenu, 2);
  ConfirmSave();
  CheckFailedSave(fixture);
  {
    std::ofstream appeared(fixture.root / "slot00.vssave");
    appeared << "changed after failed write";
  }
  Press(PADRright, Menu_Files_UpdateDataSaveMenu);
  Press(0, Menu_Files_UpdateDataSaveMenu);
  CHECK(Screen().files[0].status == Save::FileStatus::kInvalid);
  REQUIRE(fs::remove(fixture.root / "slot00.vssave"));
  Press(PADRright, Menu_Files_UpdateDataSaveMenu);
  REQUIRE(Text(Menu_Files_memoryCardMessage) == "No usable native save");
  Press(PADRright, Menu_Files_UpdateDataSaveMenu);
  Press(0, Menu_Files_UpdateDataSaveMenu);
  REQUIRE(Screen().files[0].status == Save::FileStatus::kEmpty);
  ConfirmSave();
  CheckFailedSave(fixture);
  CheckStagedBank(bank);
  Save_NativeReleaseContainer();
}

TEST_CASE("Native cached previews cannot conceal missing load content",
          "[save-menu]") {
  MenuFixture fixture;
  MenuContentFixture content(fixture);
  content.Store(MenuContentFixture::Document());
  BeginPicker(Menu_Files_UpdateLoadMenu, 2);
  REQUIRE(Screen().files[0].status == Save::FileStatus::kValid);
  REQUIRE(fs::remove(content.root / "disc" / "MAP" / "MAP009.MPD"));
  ChooseYes(Menu_Files_UpdateLoadMenu);
  Press(0, Menu_Files_UpdateLoadMenu);
  CHECK(Text(Title_memoryCardMessage) == "File changed - select again");
  CHECK_FALSE(Save::ResumePending());
  Press(PADRdown, Menu_Files_UpdateLoadMenu);
  Press(0, Menu_Files_UpdateLoadMenu);
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
  u_char state = 255;
  const auto result = Menu_Files_UpdateContainerSave(&state);
  VS::Core::SetLogObserver(previous);
  CHECK(result == 0);
  CHECK(state == 255);
  CHECK(diagnosed);
}
