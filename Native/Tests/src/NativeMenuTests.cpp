#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>

#include <catch2/catch_test_macros.hpp>

#include "Core/Log.hpp"
#include "Save/NativeFilePresentation.hpp"
#include "Save/NativeMenu.h"
#include "Save/NativeMenu.hpp"

extern "C" {
#include "Audio/Driver/SoundDriver.h"
#include "Audio/Mixer/VoiceMixer.h"
#include "Audio/Playback/SoundPlayback.h"
#include "Audio/Sfx.h"
#include "Boot/GameLoop.h"
#include "Input/Controller.h"
#include "Menu/Files.h"
#include "Title/SaveFiles.h"
}

namespace {
namespace fs = std::filesystem;
namespace Save = VS::Game::Save;

void IgnoreSound(Audio_CommandParams*) {}

struct MenuFixture {
  fs::path root;
  State_Settings settings = Input_settings;
  int active = Boot_isGameplayActive;
  int pressed = Input_buttonsPressed.allButtons;
  int repeat = Input_buttonRepeat;
  std::array<u_int, 12> sound_offsets{};
  std::array<u_short, 12> sound_selectors{};
  u_int* saved_offsets = Audio_Playback_soundEffectProgramOffsets;
  u_short* saved_selectors = Audio_Playback_soundEffectBankSelectors;
  Audio_CommandHandler sound_handler = Audio_Mixer_kCommandHandlers[0x20];
  Audio_CommandArguments sound_arguments = Audio_Driver_commandArguments;
  Audio_CommandParams sound_params = Audio_Mixer_commandParams;
  int sound_group = Audio_nextSfxChannelGroup7E;

  MenuFixture() {
    static std::atomic<unsigned> counter = 0;
    const auto tick =
        std::chrono::steady_clock::now().time_since_epoch().count();
    root = fs::temp_directory_path() /
           ("vs-native-menu-" + std::to_string(tick) + "-" +
            std::to_string(counter.fetch_add(1)));
    REQUIRE(fs::create_directory(root));
    Save::ConfigureDirectory(root);
    Boot_isGameplayActive = 0;
    Input_buttonsPressed.allButtons = 0;
    Input_buttonRepeat = 0;
    sound_selectors.fill(0x8000);
    Audio_Playback_soundEffectProgramOffsets = sound_offsets.data();
    Audio_Playback_soundEffectBankSelectors = sound_selectors.data();
    Audio_Mixer_kCommandHandlers[0x20] = IgnoreSound;
    Audio_nextSfxChannelGroup7E = 2;
  }

  ~MenuFixture() {
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
    std::error_code error;
    fs::remove_all(root, error);
    if (error) {
      VS_LOG_ERROR("test", "Cannot clean native menu fixture: {}",
                   error.message());
      std::abort();
    }
  }
};

std::string Text(const char* text) {
  REQUIRE(text != nullptr);
  std::string result;
  for (int i = 0; i < 49; ++i) {
    const auto c = static_cast<unsigned char>(text[i]);
    if (c == 0xE7) return result;
    if (c < 10)
      result += static_cast<char>('0' + c);
    else if (c < 36)
      result += static_cast<char>('A' + c - 10);
    else if (c < 62)
      result += static_cast<char>('a' + c - 36);
    else if (c == 0x8F)
      result += ' ';
    else if (c == 0x9E)
      result += ':';
    else if (c == 0xA1)
      result += '/';
    else if (c == 0xA7)
      result += '-';
    else
      FAIL("Unexpected game glyph in native menu");
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
}  // namespace

TEST_CASE("Native Continue scrolls all one hundred files vertically",
          "[save-menu]") {
  MenuFixture fixture;
  CHECK(Save_NativeInitialTitleSelection() == 0);
  REQUIRE(Menu_Files_UpdateLoadMenu(2) == 0);
  REQUIRE(Menu_Files_UpdateLoadMenu(0) == 0);
  CHECK(SelectedSlot() == 0);
  CHECK(Screen().summaries.size() == 3);
  CHECK(Text(Title_memoryCardMessage) == "Select a file");
  REQUIRE(Title_memoryCardMessage != nullptr);
  for (int slot = 1; slot < 100; ++slot) {
    Move(PADLdown, Menu_Files_UpdateLoadMenu);
    CHECK(SelectedSlot() == slot);
    CHECK(Screen().files.front().slot == std::max(0, slot - 2));
  }
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

TEST_CASE("Native file screens have three cards without generic row panels",
          "[save-menu]") {
  MenuFixture fixture;
  REQUIRE(Menu_Files_UpdateLoadMenu(2) == 0);
  REQUIRE(Menu_Files_UpdateLoadMenu(0) == 0);
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
  CHECK(SelectedSlot() == 0);
  Move(PADLright, Menu_Files_UpdateLoadMenu);
  CHECK(SelectedSlot() == 0);
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
  REQUIRE(Save_NativeSelectContainer(1) == 0);
  REQUIRE(Save_NativeSelectContainer(0) == 0);
  Buttons(PADRright);
  REQUIRE(Save_NativeSelectContainer(0) == 0);
  REQUIRE(Screen().confirmation);
  CHECK_FALSE(Screen().affirmative);
  Buttons(0, PADLup);
  REQUIRE(Save_NativeSelectContainer(0) == 0);
  Buttons(PADRright);
  REQUIRE(Save_NativeSelectContainer(0) == 0);
  Buttons(0);
  REQUIRE(Save_NativeSelectContainer(0) == 1);
  auto* bank = Save_NativeContainerData();
  REQUIRE(bank != nullptr);
  CHECK(Input_settings.containerEmpty == 1);
  bank->misc[255].id = 308;
  bank->misc[255].count = 2;
  REQUIRE(Menu_Files_UpdateDataSaveMenu(2) == 0);
  CHECK(Save_NativeContainerData() == bank);
  CHECK(Save_NativeContainerData()->misc[255].id == 308);
  CHECK(Save_NativeContainerData()->misc[255].count == 2);
  Save_NativeReleaseContainer();
}

TEST_CASE("Native confirmation is vertical explicit consent and cancelable",
          "[save-menu]") {
  MenuFixture fixture;
  REQUIRE(Save_NativeSelectContainer(1) == 0);
  REQUIRE(Save_NativeSelectContainer(0) == 0);
  Buttons(PADRright);
  REQUIRE(Save_NativeSelectContainer(0) == 0);
  REQUIRE(Screen().confirmation);
  CHECK_FALSE(Screen().affirmative);
  Move(PADLleft, Save_NativeSelectContainer);
  CHECK_FALSE(Screen().affirmative);
  Move(PADLright, Save_NativeSelectContainer);
  CHECK_FALSE(Screen().affirmative);
  Move(PADLup, Save_NativeSelectContainer);
  CHECK(Screen().affirmative);
  Move(PADLdown, Save_NativeSelectContainer);
  CHECK_FALSE(Screen().affirmative);
  Buttons(PADRright);
  REQUIRE(Save_NativeSelectContainer(0) == 0);
  CHECK_FALSE(Screen().confirmation);
  CHECK(fs::is_empty(fixture.root));
  Buttons(PADRright);
  REQUIRE(Save_NativeSelectContainer(0) == 0);
  CHECK_FALSE(Screen().affirmative);
  Move(PADLup, Save_NativeSelectContainer);
  CHECK(Screen().affirmative);
  Buttons(PADRdown);
  REQUIRE(Save_NativeSelectContainer(0) == 0);
  CHECK_FALSE(Screen().confirmation);
  CHECK(fs::is_empty(fixture.root));
}

TEST_CASE("Native reentry retains the last file at the end of the scroll",
          "[save-menu]") {
  MenuFixture fixture;
  REQUIRE(Save_NativeSelectContainer(1) == 0);
  REQUIRE(Save_NativeSelectContainer(0) == 0);
  Move(PADLup, Save_NativeSelectContainer);
  CHECK(SelectedSlot() == 99);
  Buttons(PADRright);
  REQUIRE(Save_NativeSelectContainer(0) == 0);
  Move(PADLup, Save_NativeSelectContainer);
  Buttons(PADRright);
  REQUIRE(Save_NativeSelectContainer(0) == 0);
  Buttons(0);
  REQUIRE(Save_NativeSelectContainer(0) == 1);
  REQUIRE(Menu_Files_UpdateDataSaveMenu(2) == 0);
  REQUIRE(Menu_Files_UpdateDataSaveMenu(0) == 0);
  CHECK(SelectedSlot() == 99);
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

TEST_CASE("Unmodeled native clear saves report the limit without card access",
          "[save-menu]") {
  MenuFixture fixture;
  REQUIRE(Menu_Files_UpdateSaveMenu(1) == 0);
  CHECK(Text(Title_memoryCardMessage) == "Native clear save unavailable");
  Buttons(PADRright);
  CHECK(Menu_Files_UpdateSaveMenu(0) == -1);
  CHECK(fs::is_empty(fixture.root));
}
