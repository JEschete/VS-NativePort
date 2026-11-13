#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <vector>

#include "Battle/HudOrchestrationLive.hpp"
#include "Battle/HudRenderingLiveFixture.hpp"
#include "Battle/HudShortcutLiveFixture.hpp"
#include "Content/ModuleData.h"
extern "C" {
#include "Audio/Seq/Channel.h"
#include "Battle/Feedback.h"
#include "Menu/Framework/Menus.h"
#include "Menu/Framework/TextBoxes.h"
#include "Menu/TextBox.h"
#include "Render/Text/CompactFont.h"
#include "State/SceneTimer.h"
#include "State/Timers.h"
}

namespace hud_orchestration_live_fixture {
namespace b = VS::Battle;

template <auto& Owner>
class SavedArray {
 public:
  SavedArray() : saved_(std::to_array(Owner)) {}
  ~SavedArray() { std::ranges::copy(saved_, std::begin(Owner)); }
  SavedArray(const SavedArray&) = delete;
  SavedArray& operator=(const SavedArray&) = delete;

  void Load(std::span<const unsigned char> image, unsigned offset,
            std::size_t count) {
    const auto target = std::span(Owner).first(count);
    const auto bytes = static_cast<unsigned>(target.size_bytes());
    const Content_ModuleDataEntry entry{"BATTLE/BATTLE.PRG", offset, bytes,
                                        target.data(), bytes};
    if (!VS::Content::CopyModuleDataEntry(entry, image)) {
      throw std::runtime_error("HUD fixture needs its original bounded data");
    }
  }

 private:
  decltype(std::to_array(Owner)) saved_;
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
  SavedArray<Render_compactGlyphLeftTrim> trims_;
  SavedArray<Render_compactGlyphWidths> widths_;
  SavedArray<Render_compactGlyphIndices> indices_;
  SavedArray<Menu_Framework_animationIndices> animation_;
  SavedArray<Menu_Framework_rowAnimationSteps> steps_;
  SavedArray<Battle_secondShortcutMarkerPositions> ability_markers_;
  SavedArray<Battle_secondShortcutIconPositions> ability_icons_;
  SavedArray<Battle_secondShortcutIconSizes> ability_sizes_;
  SavedArray<Battle_secondShortcutIconTexCoords> ability_textures_;
  SavedArray<Battle_firstShortcutMarkerPositions> spell_markers_;
  SavedArray<Battle_firstShortcutIconPositions> spell_icons_;
  SavedArray<Battle_firstShortcutIconSizes> spell_sizes_;
  SavedArray<Battle_firstShortcutIconTexCoords> spell_textures_;
  SavedArray<Battle_firstShortcutDigitOffsets> digits_;
  SavedArray<Battle_limbDisplaySpriteOffsets> limb_positions_;
  SavedArray<Battle_limbDisplaySpriteSizes> limb_sizes_;
  SavedArray<Battle_limbDisplaySpriteTexCoords> limb_textures_;
  SavedArray<Battle_limbStatusColors> limb_colors_;
  SavedArray<Battle_hudGaugePositions> gauge_positions_;
  SavedArray<Battle_hudGaugeLabelPositions> gauge_labels_;
  SavedArray<Battle_hudGaugeLabelWidths> gauge_widths_;
  SavedArray<Battle_hudGaugeLabelTexU> gauge_textures_;
  SavedArray<Battle_statusIconTexOffsets> status_textures_;
  SavedArray<Battle_upperStatusIconFlags> upper_status_;
  SavedArray<Render_kStatGaugeColors> gauge_colors_;

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
    Battle_hudHidden = 0;
    Battle_hudBattleMode = 0;
    Battle_hudStatusIconsHidden = 0;
    Battle_shortcutHudRequested = 0;
    Battle_shortcutHudAnimationStep = 10;
    Battle_battleModeIndicatorStep = 0;
    Battle_battleModeIndicatorWobble = 0;
    Battle_lowerScreenUiState = 0;
    Battle_statusIconCarousel.bytes[0] = 255;
    Battle_statusIconCarousel.bytes[1] = 255;
    Battle_statusIconCarousel.bytes[2] = 255;
    Battle_statusIconCarousel.bytes[3] = 0;
    State_tickSpeed = 2;
    actor.stats = {};
    for (auto& limb : actor.stats.limbs) {
      limb.hp = limb.maxHp = 100;
    }
    auto boxes = std::span(Menu_textBoxes);
    boxes[7] = {};
  }

 private:
  SavedValue<Battle_hudHidden> hidden_;
  SavedValue<Battle_hudBattleMode> mode_;
  SavedValue<Battle_hudStatusIconsHidden> status_hidden_;
  SavedValue<Battle_shortcutHudRequested> shortcut_request_;
  SavedValue<Battle_shortcutHudAnimationStep> shortcut_step_;
  SavedValue<Battle_battleModeIndicatorStep> indicator_step_;
  SavedValue<Battle_battleModeIndicatorWobble> indicator_phase_;
  SavedValue<Battle_lowerScreenUiState> lower_;
  SavedValue<Battle_statusIconCarousel> carousel_;
  SavedValue<Battle_actorMarkersHidden> marker_hidden_;
  SavedValue<Battle_ashleyStatusLabelsSuppressed> labels_;
  SavedValue<Battle_abilityTimingCueFrames> cue_;
  SavedValue<Battle_floatingLabelCount> label_count_;
  SavedValue<Battle_statusLabelStepTimer> label_step_;
  SavedValue<Battle_puzzleTimerTextPointer> timer_alias_;
  SavedValue<Audio_Seq_unusedStateFlags> flags_;
  SavedValue<State_tickSpeed> speed_;
  SavedArray<Battle_actorMarkers> markers_;
  SavedArray<Battle_puzzleTimerText> timer_text_;
  SavedArray<Menu_textBoxes> boxes_;
};
}  // namespace hud_orchestration_live_fixture
