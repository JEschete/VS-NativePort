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

std::string Text(const Menu_Files_FileMenuElement& element) {
  std::string result;
  for (const unsigned char c : element.text) {
    if (c == 0xFF) break;
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
  return result;
}

void Buttons(int pressed, int repeat = 0) {
  Input_buttonsPressed.allButtons = pressed;
  Input_buttonRepeat = repeat;
}

void ConfirmSave() {
  Buttons(PADRright);
  CHECK(Menu_Files_UpdateDataSaveMenu(0) == 0);
  Buttons(0, PADLleft);
  CHECK(Menu_Files_UpdateDataSaveMenu(0) == 0);
  Buttons(PADRright);
  CHECK(Menu_Files_UpdateDataSaveMenu(0) == 0);
  Buttons(0);
  CHECK(Menu_Files_UpdateDataSaveMenu(0) == 0);
}
}  // namespace

TEST_CASE("Native Continue pages all one hundred files and cancels",
          "[save-menu]") {
  MenuFixture fixture;
  CHECK(Save_NativeInitialTitleSelection() == 0);
  REQUIRE(Menu_Files_UpdateLoadMenu(2) == 0);
  REQUIRE(Menu_Files_UpdateLoadMenu(0) == 0);
  CHECK(Text(Title_fileMenuElements[1]) == "File 1 - Empty");
  CHECK(static_cast<unsigned char>(Title_fileMenuElements[1].slotId) == 255);
  REQUIRE(Title_memoryCardMessage != nullptr);
  CHECK(static_cast<unsigned char>(Title_memoryCardMessage[27]) == 0xE8);
  Buttons(0, PADLleft);
  CHECK(Menu_Files_UpdateLoadMenu(0) == 0);
  Buttons(0);
  CHECK(Menu_Files_UpdateLoadMenu(0) == 0);
  CHECK(Text(Title_fileMenuElements[0]) == "LOAD NATIVE - PAGE 20/20");
  CHECK(Text(Title_fileMenuElements[5]) == "File 100 - Empty");
  Buttons(PADRdown);
  CHECK(Menu_Files_UpdateLoadMenu(0) == -1);
}

TEST_CASE("Native Save cannot bypass production save-point eligibility",
          "[save-menu]") {
  MenuFixture fixture;
  const auto before = Input_settings;
  REQUIRE(Menu_Files_UpdateDataSaveMenu(1) == 0);
  REQUIRE(Menu_Files_UpdateDataSaveMenu(0) == 0);
  ConfirmSave();
  CHECK(Text(Menu_Files_fileMenuElements[8]) == "Save failed - see log");
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
  CHECK(Text(Menu_Files_fileMenuElements[1]) == "File 1 - Invalid");
  Buttons(PADRright);
  CHECK(Menu_Files_UpdateDataSaveMenu(0) == 0);
  CHECK(Text(Menu_Files_fileMenuElements[8]) == "No usable native save");
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
  Buttons(0, PADLleft);
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

TEST_CASE("Unmodeled native clear saves report the limit without card access",
          "[save-menu]") {
  MenuFixture fixture;
  REQUIRE(Menu_Files_UpdateSaveMenu(1) == 0);
  CHECK(Text(Title_fileMenuElements[8]) == "Native clear save unavailable");
  Buttons(PADRright);
  CHECK(Menu_Files_UpdateSaveMenu(0) == -1);
  CHECK(fs::is_empty(fixture.root));
}
