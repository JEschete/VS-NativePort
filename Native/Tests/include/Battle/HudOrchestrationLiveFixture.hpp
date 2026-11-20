#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <ranges>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <vector>

#include "Battle/Feedback.hpp"
#include "Battle/HudOrchestrationLive.hpp"
#include "Battle/HudRenderingLiveFixture.hpp"
#include "Battle/HudShortcutLiveFixture.hpp"
#include "Content/ModuleData.hpp"
#include "Menu/Framework/Menus.hpp"
#include "Menu/Framework/TextBoxes.hpp"
#include "Menu/TextBox.hpp"
#include "Render/Text/CompactFont.hpp"
#include "State/StateFlags.hpp"
#include "State/Timers.hpp"

namespace hud_orchestration_live_fixture {
namespace b = VS::Battle;

template <auto& Owner>
class SavedArray {
 public:
  SavedArray() { std::ranges::copy(Owner, std::begin(saved_)); }
  ~SavedArray() { std::ranges::copy(saved_, std::begin(Owner)); }
  SavedArray(const SavedArray&) = delete;
  SavedArray& operator=(const SavedArray&) = delete;

  void Load(std::span<const unsigned char> image, unsigned offset,
            std::size_t count) {
    const auto target = std::span(Owner).first(count);
    const auto bytes = static_cast<unsigned>(target.size_bytes());
    const VS::Content::ModuleDataEntry entry{"BATTLE/BATTLE.PRG", offset, bytes,
                                             target.data(), bytes};
    if (!VS::Content::CopyModuleDataEntry(entry, image)) {
      throw std::runtime_error("HUD fixture needs its original bounded data");
    }
  }

 private:
  std::array<std::ranges::range_value_t<decltype(Owner)>, std::size(Owner)>
      saved_{};
};

template <auto& Owner>
class SavedValue {
 public:
  SavedValue() : saved_(Owner) {}
  ~SavedValue() { Owner = saved_; }
  SavedValue(const SavedValue&) = delete;
  SavedValue& operator=(const SavedValue&) = delete;

 private:
  std::remove_reference_t<decltype(Owner)> saved_;
};

class ProductionTables {
 public:
  ProductionTables() {
    const char* path = std::getenv("VS_HUD_ORCHESTRATION_DISC");
    std::ifstream stream(
        path != nullptr ? path : R"(Native\data\disc\BATTLE\BATTLE.PRG)",
        std::ios::binary);
    if (!stream) {
      throw std::runtime_error(
          "HUD integration needs the extracted original BATTLE.PRG");
    }
    const std::vector<unsigned char> image{
        std::istreambuf_iterator<char>(stream), {}};
    LoadFontsAndAnimation(image);
    LoadShortcutRows(image);
    LoadStatusAndVitals(image);
  }

 private:
  SavedArray<VS::Render::compactGlyphLeftTrim> trims_;
  SavedArray<VS::Render::compactGlyphWidths> widths_;
  SavedArray<VS::Render::compactGlyphIndices> indices_;
  SavedArray<VS::Menu::Framework::animationIndices> animation_;
  SavedArray<VS::Menu::Framework::rowAnimationSteps> steps_;
  SavedArray<VS::Battle::secondShortcutMarkerPositions> ability_markers_;
  SavedArray<VS::Battle::secondShortcutIconPositions> ability_icons_;
  SavedArray<VS::Battle::secondShortcutIconSizes> ability_sizes_;
  SavedArray<VS::Battle::secondShortcutIconTexCoords> ability_textures_;
  SavedArray<VS::Battle::firstShortcutMarkerPositions> spell_markers_;
  SavedArray<VS::Battle::firstShortcutIconPositions> spell_icons_;
  SavedArray<VS::Battle::firstShortcutIconSizes> spell_sizes_;
  SavedArray<VS::Battle::firstShortcutIconTexCoords> spell_textures_;
  SavedArray<VS::Battle::firstShortcutDigitOffsets> digits_;
  SavedArray<VS::Battle::limbDisplaySpriteOffsets> limb_positions_;
  SavedArray<VS::Battle::limbDisplaySpriteSizes> limb_sizes_;
  SavedArray<VS::Battle::limbDisplaySpriteTexCoords> limb_textures_;
  SavedArray<VS::Battle::limbStatusColors> limb_colors_;
  SavedArray<VS::Battle::hudGaugePositions> gauge_positions_;
  SavedArray<VS::Battle::hudGaugeLabelPositions> gauge_labels_;
  SavedArray<VS::Battle::hudGaugeLabelWidths> gauge_widths_;
  SavedArray<VS::Battle::hudGaugeLabelTexU> gauge_textures_;
  SavedArray<VS::Battle::statusIconTexOffsets> status_textures_;
  SavedArray<VS::Battle::upperStatusIconFlags> upper_status_;
  SavedArray<VS::Render::kStatGaugeColors> gauge_colors_;

  void LoadFontsAndAnimation(std::span<const unsigned char> image) {
    trims_.Load(image, 0x82F08, 172);
    widths_.Load(image, 0x82FB4, 32);
    indices_.Load(image, 0x82FD4, 60);
    animation_.Load(image, 0x8347C, 12);
    steps_.Load(image, 0x833C8, 20);
  }
  void LoadShortcutRows(std::span<const unsigned char> image) {
    ability_markers_.Load(image, 0x83498, 8);
    ability_icons_.Load(image, 0x834A0, 8);
    ability_sizes_.Load(image, 0x834A8, 4);
    ability_textures_.Load(image, 0x834B8, 4);
    spell_markers_.Load(image, 0x834C0, 8);
    spell_icons_.Load(image, 0x834C8, 8);
    spell_sizes_.Load(image, 0x834D0, 4);
    spell_textures_.Load(image, 0x834E0, 4);
    digits_.Load(image, 0x83488, 4);
  }
  void LoadStatusAndVitals(std::span<const unsigned char> image) {
    limb_positions_.Load(image, 0x834EC, 10);
    limb_sizes_.Load(image, 0x83514, 10);
    limb_textures_.Load(image, 0x8353C, 10);
    limb_colors_.Load(image, 0x83454, 5);
    gauge_positions_.Load(image, 0x833DC, 4);
    gauge_labels_.Load(image, 0x833EC, 4);
    gauge_widths_.Load(image, 0x833FC, 4);
    gauge_textures_.Load(image, 0x83400, 4);
    status_textures_.Load(image, 0x83414, 32);
    upper_status_.Load(image, 0x83468, 8);
    gauge_colors_.Load(image, 0x83404, 4);
  }
};

class LiveState {
 public:
  hud_rendering_fixture::LiveFrame frame;
  hud_rendering_fixture::LiveShortcut actor;
  ProductionTables production;

  LiveState() {
    VS::Battle::hudHidden = 0;
    VS::Battle::hudBattleMode = 0;
    VS::Battle::hudStatusIconsHidden = 0;
    VS::Battle::shortcutHudRequested = 0;
    VS::Battle::shortcutHudAnimationStep = 10;
    VS::Battle::battleModeIndicatorStep = 0;
    VS::Battle::battleModeIndicatorWobble = 0;
    VS::Battle::lowerScreenUiState = 0;
    VS::Battle::statusIconCarousel.bytes[0] = 255;
    VS::Battle::statusIconCarousel.bytes[1] = 255;
    VS::Battle::statusIconCarousel.bytes[2] = 255;
    VS::Battle::statusIconCarousel.bytes[3] = 0;
    VS::State::tickSpeed = 2;
    actor.stats = {};
    for (auto& limb : actor.stats.limbs) {
      limb.hp = limb.maxHp = 100;
    }
    auto boxes = std::span(VS::Menu::textBoxes);
    boxes[7] = {};
  }

 private:
  SavedValue<VS::Battle::hudHidden> hidden_;
  SavedValue<VS::Battle::hudBattleMode> mode_;
  SavedValue<VS::Battle::hudStatusIconsHidden> status_hidden_;
  SavedValue<VS::Battle::shortcutHudRequested> shortcut_request_;
  SavedValue<VS::Battle::shortcutHudAnimationStep> shortcut_step_;
  SavedValue<VS::Battle::battleModeIndicatorStep> indicator_step_;
  SavedValue<VS::Battle::battleModeIndicatorWobble> indicator_phase_;
  SavedValue<VS::Battle::lowerScreenUiState> lower_;
  SavedValue<VS::Battle::statusIconCarousel> carousel_;
  SavedValue<VS::Battle::actorMarkersHidden> marker_hidden_;
  SavedValue<VS::Battle::ashleyStatusLabelsSuppressed> labels_;
  SavedValue<VS::Battle::abilityTimingCueFrames> cue_;
  SavedValue<VS::Battle::floatingLabelCount> label_count_;
  SavedValue<VS::Battle::statusLabelStepTimer> label_step_;
  SavedValue<VS::Battle::puzzleTimerTextPointer> timer_alias_;
  SavedValue<VS::State::stateFlags> flags_;
  SavedValue<VS::State::tickSpeed> speed_;
  SavedArray<VS::Battle::actorMarkers> markers_;
  SavedArray<VS::Battle::puzzleTimerText> timer_text_;
  SavedArray<VS::Menu::textBoxes> boxes_;
};
}  // namespace hud_orchestration_live_fixture
