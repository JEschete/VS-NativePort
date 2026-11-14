// Hand-maintained (tools/port/owned.csv).
#include "Menu/MagicMenus.hpp"  // NOLINT(build/include): public API uses .hpp.

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <span>

#include "Actors/ActorModelSlot.h"
#include "Actors/ModelInstance.h"
#include "Actors/Skeleton.h"
#include "Audio/Seq/Channel.h"
#include "Battle/AbilitiesLive.hpp"
#include "Battle/AbilityState.h"
#include "Battle/Action.h"
#include "Battle/ActionCostsLive.hpp"
#include "Battle/Actor.h"
#include "Battle/Feedback.h"
#include "Battle/Sphere.h"
#include "Content/StringTables.h"
#include "Core/Math/ScalarMath.hpp"
#include "Ending/DisplayHost.hpp"
#include "Field/ChestInteractionsLive.h"
#include "Field/RoomStateLive.hpp"
#include "Input/ControllerLiveState.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Legacy/Psx.h"
#include "Menu/Framework/Framework.h"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Framework/FrameworkLiveState.h"
#include "Menu/MagicSpellMenu.hpp"
#include "Menu/Sounds.hpp"
#include "Render/GameFontLive.hpp"
#include "Render/LegacyPackets.h"
#include "Render/Palette.h"
#include "Render/Render.h"
#include "Render/SpritesLive.hpp"
#include "Render/UiPrimitives.h"
#include "State/StateFlags.h"

extern "C" {
#include "Battle/ActionTable.h"
#include "Menu/Framework/Menus.h"
#include "Menu/Magic.h"
}

// decomp: local static of _warlockMagicMenu, at file scope so the module reset
// reaches it
static char Menu_Magic_warlockMenuLevelledSpells[] = {
    Battle_WarlockSpell_Explosion,
    Battle_WarlockSpell_Thunderburst,
    Battle_WarlockSpell_FlameSphere,
    Battle_WarlockSpell_GaeaStrike,
    Battle_WarlockSpell_Avalanche,
    Battle_WarlockSpell_RadialSurge,
    static_cast<char>(Battle_WarlockSpell_Meteor)};

static char Menu_Magic_warlockMenuSpellLevel1[] = "LV1";
static char Menu_Magic_warlockMenuSpellLevel2[] = "LV2";
static char Menu_Magic_warlockMenuSpellLevel3[] = "LV3";
static char Menu_Magic_warlockMenuSpellLevel4[] = "LV4";
static char Menu_Magic_warlockMenuSpellLevelLocked[] = "--";

// decomp: local static of _warlockMagicMenu, at file scope so the module reset
// reaches it
static char* Menu_Magic_warlockMenuSpellLevels[] = {
    Menu_Magic_warlockMenuSpellLevel1, Menu_Magic_warlockMenuSpellLevel2,
    Menu_Magic_warlockMenuSpellLevel3, Menu_Magic_warlockMenuSpellLevel4,
    Menu_Magic_warlockMenuSpellLevelLocked};

namespace VS::Menu::Magic {
namespace {

constexpr std::size_t kWarlockMenuStringCapacity = 36;
constexpr std::size_t kWarlockSpellCapacity = 18;
constexpr std::size_t kShamanMenuStringCapacity = 12;
constexpr std::size_t kShamanSpellCapacity = 6;
constexpr std::size_t kSorcererMenuStringCapacity = 36;
constexpr std::size_t kSorcererSpellCapacity = 18;
constexpr std::size_t kEnchanterMenuStringCapacity = 16;
constexpr std::size_t kEnchanterSpellCapacity = 8;
constexpr std::size_t kMenuStringSlotsPerRow = 2;
constexpr int kLevelledSpellCount = 7;
constexpr int kWarlockMenuInfo = 7;
constexpr int kShamanMenuInfo = 8;
constexpr int kSorcererMenuInfo = 9;
constexpr int kEnchanterMenuInfo = 10;
constexpr int kMenuStyleShift = 8;
constexpr int kWarlockLevelSelectionMemoryIndex = 12;
constexpr int kWarlockLevelCountMemoryIndex = 13;
constexpr int kWarlockLevelOffsetMemoryIndex = 14;
constexpr int kWarlockCursorMemoryIndex = 0xC;
constexpr int kWarlockLevelledRowsBegin = 20;
constexpr int kWarlockLevelledRowsEnd = 30;
constexpr int kSecondSpellLevel = 2;
constexpr int kThirdSpellLevel = 3;
constexpr int kFourthSpellLevel = 4;
constexpr int kVisibleSpellLevelCount = 4;
constexpr int kSpellLevelColumnSpacing = 32;
constexpr int kSpellLevelTextBaseX = 99;
constexpr int kSpellLevelIconBaseX = 96;
constexpr int kLockedSpellLevelTextBaseX = 102;
constexpr int kSpellLevelTextY = 103;
constexpr int kSpellLevelIconY = 100;
constexpr int kSpellLevelSpriteWidth = 32;
constexpr int kSpellLevelSpriteHeight = 16;
constexpr int kSpellLevelBlinkMask = 0xF;
constexpr int kLockedSpellLevelBrightness = 64;
constexpr int kSpellLevelArrowBrightness = 128;
constexpr int kSpellLevelArrowX = 80;
constexpr int kSpellLevelArrowY = 100;
constexpr int kSpellLevelArrowSize = 16;
constexpr int kSpellLevelClutX = 848;
constexpr int kSpellLevelClutY = 223;
constexpr int kSpellLevelPageX = 768;
constexpr int kSpellLevelUvX = 192;
constexpr int kSpellLevelUvY = 112;
constexpr int kSpellLevelTexturePageU = 976;
constexpr int kSpellLevelTexturePageV = 223;
constexpr int kSpellLevelLockedTexturePageU = 992;
constexpr int kSpellLevelLockedDrawMode = 0x64602000;
constexpr int kPrimitiveSemitransparencyFlag = 0x02000000;
constexpr int kSpellLevelArrowPrimitiveWords = 6;
constexpr int kSpellLevelArrowTotalWords = 10;
constexpr int kSpellLevelOrderingTableOffset = 4;
constexpr int kClutWordShift = 16;
constexpr int kEnchanterMenuRow = 3;
constexpr int kArrowTagWord = 0;
constexpr int kArrowColorWord = 1;
constexpr int kArrowFirstPositionWord = 2;
constexpr int kArrowFirstUvWord = 3;
constexpr int kArrowSecondPositionWord = 4;
constexpr int kArrowTpageWord = 5;
constexpr int kArrowThirdPositionWord = 6;
constexpr int kArrowSecondUvWord = 7;
constexpr int kArrowFourthPositionWord = 8;
constexpr int kArrowThirdUvWord = 9;
constexpr int kSpriteUvClutWord = 4;
constexpr int kPrimitiveClutOffset = -3;
constexpr int kPrimitiveSemiTransparencyOffset = -5;
constexpr int kPrimitiveSecondClutOffset = -8;
constexpr int kPrimitiveSecondSemiTransparencyOffset = -10;
constexpr int kPrimitiveLockedPageOffset = -12;
constexpr int kPrimitivePageOffset = -17;
constexpr int kPrimitiveArrowClutOffset = -13;
constexpr int kPrimitiveArrowSemiTransparencyOffset = -15;

enum class WarlockMenuState : u_int {
  kInit,
  kHandleInput,
  kReturnIfReady,
  kLevelledSpellInit,
  kLevelledSpell
};

int ReadSpellId(char action_id) {
  return static_cast<unsigned char>(action_id);
}

void CreateLiveSpellMenuRows(int row_count, int style, int shortcut_invoked,
                             char** menu_strings, int* row_types) {
  const char previous_cursor_memory = Input_settings.cursorMemory;
  if (shortcut_invoked != 0) {
    Input_settings.cursorMemory = 1;
  }
  VS::Menu::Framework::CreateLiveMenuRows(row_count, style, menu_strings,
                                          row_types);
  Input_settings.cursorMemory = previous_cursor_memory;
}

int PopulateAvailableSpellRows(const AvailableSpellList& available,
                               char** menu_strings, int* row_types,
                               char* available_spell_ids, int name_text_offset,
                               int description_text_offset,
                               bool use_action_definition_names) {
  for (std::size_t row = 0; row < available.count; ++row) {
    const auto& spell = available.spells[row];
    const int action_id = spell.action_id;
    const std::size_t description_index =
        Menu_Magic_magicStrings[spell.source_index + description_text_offset];

    if (use_action_definition_names) {
      menu_strings[row * kMenuStringSlotsPerRow] =
          Battle_actionDefinitions[action_id].name;
    } else {
      const std::size_t name_index =
          Menu_Magic_magicStrings[spell.source_index + name_text_offset];
      menu_strings[row * kMenuStringSlotsPerRow] =
          reinterpret_cast<char*>(&Menu_Magic_magicStrings[name_index]);
    }

    menu_strings[row * kMenuStringSlotsPerRow + 1] =
        reinterpret_cast<char*>(&Menu_Magic_magicStrings[description_index]);
    row_types[row] = 0;
    if (VS::Battle::LiveActorActionRestrictionFlags(0, action_id) != 0) {
      row_types[row] |= 1;
    }
    available_spell_ids[row] =
        std::bit_cast<char>(static_cast<std::uint8_t>(action_id));
  }
  return static_cast<int>(available.count);
}

template <std::size_t N>
std::optional<AvailableSpellList> BuildLiveAvailableSpellList(
    const char (&spell_ids)[N], std::size_t count) {
  if (count > N || count > AvailableSpellList::kCapacity) {
    return std::nullopt;
  }

  std::array<SpellCandidate, AvailableSpellList::kCapacity> candidates{};
  for (std::size_t index = 0; index < count; ++index) {
    const auto action_id =
        static_cast<std::uint8_t>(static_cast<unsigned char>(spell_ids[index]));
    if (action_id >= std::size(Battle_actionDefinitions)) {
      return std::nullopt;
    }
    candidates[index] = {
        .source_index = static_cast<std::uint8_t>(index),
        .action_id = action_id,
        .unlocked = Battle_actionDefinitions[action_id].unlocked != 0,
    };
  }

  return BuildAvailableSpellList(
      std::span<const SpellCandidate>(candidates.data(), count));
}

void InitializeWarlockSpellRows(WarlockMenuState& menu_state,
                                int menu_shortcut_invoked) {
  char* menu_strings[kWarlockMenuStringCapacity];
  int row_types[kWarlockSpellCapacity];

  if (!Menu_Framework_shortcutInvoked &&
      !VS::Menu::Framework::AreLiveMenuRowsIdle()) {
    return;
  }

  const auto available = BuildLiveAvailableSpellList(Menu_Magic_warlockSpellIds,
                                                     kWarlockSpellCapacity);
  if (!available) {
    return;
  }
  const int row_count = PopulateAvailableSpellRows(
      *available, menu_strings, row_types, Menu_Magic_availableWarlockSpells,
      Text_Menu0_magic_Index_warlockSpells,
      Text_Menu0_magic_Index_warlockSpellDescs, false);

  ApplyGlobalSpellRestrictions(*available,
                               std::span<int>(row_types, kWarlockSpellCapacity),
                               Menu_Framework_actionRestrictions);
  CreateLiveSpellMenuRows(row_count, (2 << kMenuStyleShift) | kWarlockMenuInfo,
                          menu_shortcut_invoked, menu_strings, row_types);
  menu_state = WarlockMenuState::kHandleInput;
}

void HandleWarlockMenuInput(WarlockMenuState& menu_state, int& selected_row) {
  int row_count;

  selected_row = VS::Menu::Framework::LiveSelectedRow() + 1;
  if (selected_row != 0) {
    Menu_Framework_displayActionCost = 0;
    if (Menu_Framework_shortcutInvoked && selected_row == -1) {
      selected_row = -2;
    }

    if (selected_row == -1) {
      VS::Menu::Framework::ClearLiveMenuExcept(0);
      menu_state = WarlockMenuState::kReturnIfReady;
      return;
    }

    if (selected_row > 0) {
      selected_row =
          ReadSpellId(Menu_Magic_availableWarlockSpells[selected_row - 1]);
      for (row_count = 0; row_count < kLevelledSpellCount; ++row_count) {
        if (selected_row == Menu_Magic_warlockMenuLevelledSpells[row_count] &&
            Battle_actionDefinitions[selected_row + 1].unlocked) {
          VS::Menu::Framework::FlyOutLiveRowsAndHoistSelection(
              static_cast<unsigned char>(
                  Menu_Framework_menuMemory
                      .cursorMemories[kWarlockLevelOffsetMemoryIndex]),
              kSecondSpellLevel);
          Menu_Framework_displayActionCost = 1;
          menu_state = WarlockMenuState::kLevelledSpellInit;
        }
      }

      if (menu_state == WarlockMenuState::kLevelledSpellInit) {
        return;
      }
    }

    VS::Menu::Framework::ClearLiveMenuExcept(Menu_Framework_MenuItemId_None);
    VS::Menu::Framework::DismissLiveInformationBox();
    VS::Menu::Framework::SetLiveCommandLabel(Menu_Framework_MenuCommand_None);
    menu_state = WarlockMenuState::kReturnIfReady;
    return;
  }

  int action_id = ReadSpellId(Menu_Magic_availableWarlockSpells
                                  [VS::Menu::Framework::LiveHighlightedRow()]);
  for (row_count = 0; row_count < kLevelledSpellCount; ++row_count) {
    if (action_id == Menu_Magic_warlockMenuLevelledSpells[row_count]) {
      if (Battle_actionDefinitions[action_id + 1].unlocked) {
        action_id = -action_id;
      }
      break;
    }
  }
  Menu_Magic_SetSpellMpCost(action_id);
}

void InitializeWarlockLevelMenu(WarlockMenuState& menu_state,
                                int selected_row) {
  if (VS::Menu::Framework::AreLiveMenuRowsIdle() == 0) {
    return;
  }

  int level_count = kSecondSpellLevel;
  if (Battle_actionDefinitions[selected_row + kSecondSpellLevel].unlocked) {
    level_count = kThirdSpellLevel;
    if (Battle_actionDefinitions[selected_row + kThirdSpellLevel].unlocked) {
      level_count = kFourthSpellLevel;
    }
  }

  Menu_Framework_menuMemory.cursorMemories[kWarlockLevelCountMemoryIndex] =
      static_cast<char>(level_count);
  if (static_cast<unsigned char>(
          Menu_Framework_menuMemory
              .cursorMemories[kWarlockLevelSelectionMemoryIndex]) >=
      level_count) {
    Menu_Framework_menuMemory
        .cursorMemories[kWarlockLevelSelectionMemoryIndex] =
        static_cast<char>(level_count - 1);
  }

  Menu_Magic_warlockLevelCursorBlink = 0;
  menu_state = WarlockMenuState::kLevelledSpell;
}

void DrawWarlockLevelRows(int selected_level, int available_level_count) {
  u_long* spell_level_ordering_table =
      static_cast<u_long*>(Render_frameScratch.orderingTable) -
      kSpellLevelOrderingTableOffset;

  for (int level = 0; level < kVisibleSpellLevelCount; ++level) {
    bool level_locked = false;
    u_long* prim = nullptr;

    if (selected_level >= level) {
      VS::Render::DrawLiveCompactText(
          Menu_Magic_warlockMenuSpellLevels[level],
          (kSpellLevelTextBaseX + level * kSpellLevelColumnSpacing) |
              vs_getXY(0, kSpellLevelTextY),
          nullptr);

      prim = VS::Render::QueueLiveSpriteWithTexturePage(
          Menu_Framework_cursorBrightnessAnimation
              [Menu_Magic_warlockLevelCursorBlink],
          (kSpellLevelIconBaseX + level * kSpellLevelColumnSpacing) |
              vs_getXY(0, kSpellLevelIconY),
          vs_getWH(kSpellLevelSpriteWidth, kSpellLevelSpriteHeight),
          spell_level_ordering_table);
      prim[kSpriteUvClutWord] =
          vs_getUV0Clut(kSpellLevelUvX, kSpellLevelUvY, kSpellLevelTexturePageU,
                        kSpellLevelTexturePageV);

      const auto blink = Menu_Magic_warlockLevelCursorBlink;
      if (level == selected_level) {
        ++Menu_Magic_warlockLevelCursorBlink;
        Menu_Magic_warlockLevelCursorBlink &= kSpellLevelBlinkMask;
      } else {
        Menu_Magic_warlockLevelCursorBlink = blink & kSpellLevelBlinkMask;
      }
    } else if (level < available_level_count) {
      VS::Render::DrawLiveCompactText(
          Menu_Magic_warlockMenuSpellLevels[level],
          (kSpellLevelTextBaseX + level * kSpellLevelColumnSpacing) |
              vs_getXY(0, kSpellLevelTextY),
          nullptr);

      prim = VS::Render::QueueLiveSpriteWithTexturePage(
          kLockedSpellLevelBrightness,
          (kSpellLevelIconBaseX + level * kSpellLevelColumnSpacing) |
              vs_getXY(0, kSpellLevelIconY),
          vs_getWH(kSpellLevelSpriteWidth, kSpellLevelSpriteHeight),
          spell_level_ordering_table);
      prim[kSpriteUvClutWord] =
          vs_getUV0Clut(kSpellLevelUvX, kSpellLevelUvY,
                        kSpellLevelLockedTexturePageU, kSpellLevelTexturePageV);
    } else {
      level_locked = true;
      VS::Render::DrawLiveCompactText(
          Menu_Magic_warlockMenuSpellLevels[kFourthSpellLevel],
          (kLockedSpellLevelTextBaseX + level * kSpellLevelColumnSpacing) |
              vs_getXY(0, kSpellLevelTextY),
          nullptr);

      prim = VS::Render::QueueLiveSpriteWithTexturePage(
          0,
          (kSpellLevelIconBaseX + level * kSpellLevelColumnSpacing) |
              vs_getXY(0, kSpellLevelIconY),
          vs_getWH(kSpellLevelSpriteWidth, kSpellLevelSpriteHeight),
          spell_level_ordering_table);
      prim[kArrowColorWord] = kSpellLevelLockedDrawMode;
      prim[kSpriteUvClutWord] =
          vs_getUV0Clut(kSpellLevelUvX, kSpellLevelUvY,
                        kSpellLevelLockedTexturePageU, kSpellLevelTexturePageV);
    }

    prim[kPrimitiveClutOffset] =
        static_cast<u_short>(prim[kPrimitiveClutOffset]) |
        (getClut(kSpellLevelClutX, kSpellLevelClutY) << kClutWordShift);
    prim[kPrimitiveSemiTransparencyOffset] =
        prim[kPrimitiveSemiTransparencyOffset] | kPrimitiveSemitransparencyFlag;
    prim[kPrimitiveSecondClutOffset] =
        static_cast<u_short>(prim[kPrimitiveSecondClutOffset]) |
        (getClut(kSpellLevelClutX, kSpellLevelClutY) << kClutWordShift);
    prim[kPrimitiveSecondSemiTransparencyOffset] |=
        kPrimitiveSemitransparencyFlag;

    if (level_locked) {
      prim[kPrimitiveLockedPageOffset] = VS::Ending::TexturePageBits(
          VS::Ending::TextureDepth::k4Bit, VS::Ending::BlendMode::kAdd,
          kSpellLevelPageX, 0);
    } else {
      prim[kPrimitivePageOffset] = VS::Ending::TexturePageBits(
          VS::Ending::TextureDepth::k4Bit, VS::Ending::BlendMode::kAdd,
          kSpellLevelPageX, 0);
      prim[kPrimitiveArrowClutOffset] =
          static_cast<u_short>(prim[kPrimitiveArrowClutOffset]) |
          (getClut(kSpellLevelClutX, kSpellLevelClutY) << kClutWordShift);
      prim[kPrimitiveArrowSemiTransparencyOffset] |=
          kPrimitiveSemitransparencyFlag;
    }
  }
}

void DrawWarlockLevelArrow(u_long* spell_level_ordering_table) {
  u_long* prim = VS::Render::QueueLiveSpriteWithTexturePage(
      kSpellLevelArrowBrightness,
      vs_getXY(kSpellLevelArrowX, kSpellLevelArrowY),
      vs_getWH(kSpellLevelArrowSize, kSpellLevelArrowSize),
      spell_level_ordering_table);
  prim[kSpriteUvClutWord] = vs_getUV0Clut(
      kSpellLevelArrowX, 48, kSpellLevelTexturePageU, kSpellLevelTexturePageV);
  prim += kSpellLevelArrowPrimitiveWords;

  prim[kArrowTagWord] = vs_getTag(u_long[9], *spell_level_ordering_table);
  prim[kArrowColorWord] =
      vs_getRGB0(Render_LegacyPrimitiveCode_PolyFt4, 128, 128, 128);
  prim[kArrowFirstPositionWord] = vs_getXY(224, 100);
  prim[kArrowFirstUvWord] =
      vs_getUV0Clut(95, 48, kSpellLevelTexturePageU, kSpellLevelTexturePageV);
  prim[kArrowSecondPositionWord] = vs_getXY(240, 100);
  prim[kArrowTpageWord] =
      vs_getUV1Tpage(79, 48, kSpellLevelPageX, Render_TextureFormat_Palette4Bit,
                     Render_BlendMode_Average, Render_Dithering_Off);
  prim[kArrowThirdPositionWord] = vs_getXY(224, 116);
  prim[kArrowSecondUvWord] = vs_getUV(95, 64);
  prim[kArrowFourthPositionWord] = vs_getXY(240, 116);
  prim[kArrowThirdUvWord] = vs_getUV(79, 64);

  *spell_level_ordering_table = LegacyGpu_Handle(prim);
  Render_frameScratch.primitiveCursor = prim + kSpellLevelArrowTotalWords;
}

void DrawWarlockLevelMenu(int selected_level, int available_level_count) {
  DrawWarlockLevelRows(selected_level, available_level_count);
  auto* spell_level_ordering_table =
      static_cast<u_long*>(Render_frameScratch.orderingTable) -
      kSpellLevelOrderingTableOffset;
  DrawWarlockLevelArrow(spell_level_ordering_table);
}

void UpdateWarlockLevelMenu(WarlockMenuState& menu_state, int& selected_row) {
  int selected_level = static_cast<unsigned char>(
      Menu_Framework_menuMemory
          .cursorMemories[kWarlockLevelSelectionMemoryIndex]);
  const int available_level_count = static_cast<unsigned char>(
      Menu_Framework_menuMemory.cursorMemories[kWarlockLevelCountMemoryIndex]);

  if (Input_buttonsPressed.allButtons & PADRup) {
    VS::Menu::PlayLeaveSound();
    Menu_Framework_displayActionCost = 0;
    VS::Menu::Framework::ClearLiveMenuExcept(Menu_Framework_MenuItemId_None);
    VS::Menu::Framework::DismissLiveInformationBox();
    VS::Menu::Framework::SetLiveCommandLabel(Menu_Framework_MenuCommand_None);
    selected_row = -2;
    menu_state = WarlockMenuState::kReturnIfReady;
    return;
  }

  if (Input_buttonsPressed.allButtons & PADRdown) {
    VS::Menu::PlayLeaveSound();
    for (int row = kWarlockLevelledRowsBegin; row < kWarlockLevelledRowsEnd;
         ++row) {
      VS::Menu::Framework::FlyOutLiveRowRight(row);
    }
    menu_state = WarlockMenuState::kInit;
    return;
  }

  if (Input_buttonsPressed.allButtons & PADRright) {
    if (VS::Battle::LiveActorActionRestrictionFlags(
            0, selected_row + selected_level) == 0) {
      VS::Menu::PlaySelectSound();
      Menu_Framework_displayActionCost = 0;
      VS::Menu::Framework::ClearLiveMenuExcept(Menu_Framework_MenuItemId_None);
      VS::Menu::Framework::DismissLiveInformationBox();
      VS::Menu::Framework::SetLiveCommandLabel(Menu_Framework_MenuCommand_None);
      menu_state = WarlockMenuState::kReturnIfReady;
      selected_row += selected_level;
      return;
    }
    VS::Menu::PlayInvalidSound();
  }

  if (Input_buttonRepeat & PADLright) {
    if (selected_level < available_level_count - 1) {
      ++selected_level;
    }
  }
  if (Input_buttonRepeat & PADLleft && selected_level > 0) {
    --selected_level;
  }

  if (selected_level != static_cast<unsigned char>(
                            Menu_Framework_menuMemory
                                .cursorMemories[kWarlockCursorMemoryIndex])) {
    VS::Menu::PlayChangeSound();
    Menu_Framework_menuMemory.cursorMemories[kWarlockCursorMemoryIndex] =
        static_cast<char>(selected_level);
  }

  Menu_Magic_SetSpellMpCost(selected_row + selected_level);
  DrawWarlockLevelMenu(selected_level, available_level_count);
}

}  // namespace

// decomp: MENU0.PRG _warlockMagicMenu (src/MENU/MENU0.PRG/84.c:164)
int UpdateWarlockMenu(unsigned int init_shortcut_invoked) {
  static WarlockMenuState menu_state;
  static int selected_row;
  static int menu_shortcut_invoked;

  if (init_shortcut_invoked != 0) {
    menu_shortcut_invoked = (init_shortcut_invoked ^ 2) < 1;
    VS::Menu::Framework::FlyOutLiveRowsAndHoistSelection(0, 1);
    menu_state = WarlockMenuState::kInit;
    return 0;
  }

  switch (menu_state) {
    case WarlockMenuState::kInit:
      InitializeWarlockSpellRows(menu_state, menu_shortcut_invoked);
      break;
    case WarlockMenuState::kHandleInput:
      HandleWarlockMenuInput(menu_state, selected_row);
      break;
    case WarlockMenuState::kReturnIfReady:
      if (VS::Menu::Framework::AreLiveMenuRowsIdle() != 0) {
        return selected_row;
      }
      break;
    case WarlockMenuState::kLevelledSpellInit:
      InitializeWarlockLevelMenu(menu_state, selected_row);
      break;
    case WarlockMenuState::kLevelledSpell:
      UpdateWarlockLevelMenu(menu_state, selected_row);
      break;
    default:
      break;
  }
  return 0;
}

// decomp: MENU0.PRG _shamanMagicMenu (src/MENU/MENU0.PRG/84.c:512)
int UpdateShamanMenu(unsigned int init_shortcut_invoked) {
  enum ShamanMenuState { kInit, kHandleInput, kReturnIfReady };

  static int shaman_menu_state;
  static int shaman_menu_selected_row;
  static int shaman_menu_shortcut_invoked;

  char* menu_strings[kShamanMenuStringCapacity];
  int row_types[kShamanSpellCapacity];
  int row_count;

  if (init_shortcut_invoked != 0) {
    shaman_menu_shortcut_invoked = (init_shortcut_invoked ^ 2) < 1;
    VS::Menu::Framework::FlyOutLiveRowsAndHoistSelection(1, 1);
    shaman_menu_state = kInit;
    return 0;
  }

  switch (shaman_menu_state) {
    case kInit: {
      if ((Menu_Framework_shortcutInvoked == 0) &&
          (VS::Menu::Framework::AreLiveMenuRowsIdle() == 0)) {
        break;
      }

      const auto available = BuildLiveAvailableSpellList(
          Menu_Magic_shamanSpellIds, kShamanSpellCapacity);
      if (!available) {
        return 0;
      }
      row_count = PopulateAvailableSpellRows(
          *available, menu_strings, row_types, Menu_Magic_availableShamanSpells,
          0, Text_Menu0_magic_Index_shamanSpellDescs, true);

      ApplyGlobalSpellRestrictions(
          *available, std::span<int>(row_types, kShamanSpellCapacity),
          Menu_Framework_actionRestrictions);
      CreateLiveSpellMenuRows(
          row_count, (2 << kMenuStyleShift) | kShamanMenuInfo,
          shaman_menu_shortcut_invoked, menu_strings, row_types);
      shaman_menu_state = kHandleInput;
      break;
    }
    case kHandleInput:
      shaman_menu_selected_row = VS::Menu::Framework::LiveSelectedRow() + 1;
      if (shaman_menu_selected_row != 0) {
        Menu_Framework_displayActionCost = 0;
        if ((Menu_Framework_shortcutInvoked != 0) &&
            (shaman_menu_selected_row == -1)) {
          shaman_menu_selected_row = -2;
        }
        if (shaman_menu_selected_row == -1) {
          VS::Menu::Framework::ClearLiveMenuExcept(0);
        } else {
          if (shaman_menu_selected_row > 0) {
            shaman_menu_selected_row = ReadSpellId(
                Menu_Magic_availableShamanSpells[shaman_menu_selected_row - 1]);
          }
          VS::Menu::Framework::ClearLiveMenuExcept(
              Menu_Framework_MenuItemId_None);
          VS::Menu::Framework::DismissLiveInformationBox();
          VS::Menu::Framework::SetLiveCommandLabel(
              Menu_Framework_MenuCommand_None);
        }
        shaman_menu_state = kReturnIfReady;
      } else {
        Menu_Magic_SetSpellMpCost(
            ReadSpellId(Menu_Magic_availableShamanSpells
                            [VS::Menu::Framework::LiveHighlightedRow()]));
      }
      break;
    case kReturnIfReady:
      if (VS::Menu::Framework::AreLiveMenuRowsIdle() != 0) {
        return shaman_menu_selected_row;
      }
      break;
    default:
      break;
  }
  return 0;
}

// decomp: MENU0.PRG _sorcererMagicMenu (src/MENU/MENU0.PRG/84.c:613)
int UpdateSorcererMenu(unsigned int init_shortcut_invoked) {
  enum SpellListMenuState { kInit, kHandleInput, kReturnIfReady };

  static int sorcerer_menu_state;
  static int sorcerer_menu_selected_row;
  static int sorcerer_menu_shortcut_invoked;

  char* menu_strings[kSorcererMenuStringCapacity];
  int row_types[kSorcererSpellCapacity];
  int row_count;

  if (init_shortcut_invoked != 0) {
    sorcerer_menu_shortcut_invoked = (init_shortcut_invoked ^ 2) < 1;
    VS::Menu::Framework::FlyOutLiveRowsAndHoistSelection(2, 1);
    sorcerer_menu_state = kInit;
    return 0;
  }

  switch (sorcerer_menu_state) {
    case kInit: {
      if (!Menu_Framework_shortcutInvoked &&
          !VS::Menu::Framework::AreLiveMenuRowsIdle()) {
        break;
      }
      const auto available = BuildLiveAvailableSpellList(
          Menu_Magic_sorcererSpellIds, kSorcererSpellCapacity);
      if (!available) {
        return 0;
      }
      row_count = PopulateAvailableSpellRows(
          *available, menu_strings, row_types,
          Menu_Magic_availableSorcererSpells, 0,
          Text_Menu0_magic_Index_sorcererSpellDescs, true);

      ApplyGlobalSpellRestrictions(
          *available, std::span<int>(row_types, kSorcererSpellCapacity),
          Menu_Framework_actionRestrictions);
      CreateLiveSpellMenuRows(
          row_count, (2 << kMenuStyleShift) | kSorcererMenuInfo,
          sorcerer_menu_shortcut_invoked, menu_strings, row_types);
      sorcerer_menu_state = kHandleInput;
      break;
    }
    case kHandleInput:
      sorcerer_menu_selected_row = VS::Menu::Framework::LiveSelectedRow() + 1;
      if (sorcerer_menu_selected_row != 0) {
        Menu_Framework_displayActionCost = 0;
        if (Menu_Framework_shortcutInvoked &&
            (sorcerer_menu_selected_row == -1)) {
          sorcerer_menu_selected_row = -2;
        }
        if (sorcerer_menu_selected_row == -1) {
          VS::Menu::Framework::ClearLiveMenuExcept(0);
        } else {
          if (sorcerer_menu_selected_row > 0) {
            sorcerer_menu_selected_row = ReadSpellId(
                Menu_Magic_availableSorcererSpells[sorcerer_menu_selected_row -
                                                   1]);
          }
          VS::Menu::Framework::ClearLiveMenuExcept(
              Menu_Framework_MenuItemId_None);
          VS::Menu::Framework::DismissLiveInformationBox();
          VS::Menu::Framework::SetLiveCommandLabel(
              Menu_Framework_MenuCommand_None);
        }
        sorcerer_menu_state = kReturnIfReady;
      } else {
        Menu_Magic_SetSpellMpCost(
            ReadSpellId(Menu_Magic_availableSorcererSpells
                            [VS::Menu::Framework::LiveHighlightedRow()]));
      }
      break;
    case kReturnIfReady:
      if (VS::Menu::Framework::AreLiveMenuRowsIdle() != 0) {
        return sorcerer_menu_selected_row;
      }
      break;
    default:
      break;
  }
  return 0;
}

// decomp: MENU0.PRG _enchanterMagicMenu (src/MENU/MENU0.PRG/84.c:713)
int UpdateEnchanterMenu(unsigned int init_shortcut_invoked) {
  enum SpellListMenuState { kInit, kHandleInput, kReturnIfReady };
  static int enchanter_menu_state;
  static int enchanter_menu_selected_row;
  static int enchanter_menu_shortcut_invoked;

  char* menu_strings[kEnchanterMenuStringCapacity];
  int row_types[kEnchanterSpellCapacity];
  int row_count;

  if (init_shortcut_invoked != 0) {
    enchanter_menu_shortcut_invoked = (init_shortcut_invoked ^ 2) < 1;
    VS::Menu::Framework::FlyOutLiveRowsAndHoistSelection(kEnchanterMenuRow, 1);
    enchanter_menu_state = kInit;
    return 0;
  }

  switch (enchanter_menu_state) {
    case kInit: {
      if ((Menu_Framework_shortcutInvoked == 0) &&
          (VS::Menu::Framework::AreLiveMenuRowsIdle() == 0)) {
        break;
      }
      const auto available = BuildLiveAvailableSpellList(
          Menu_Magic_enchanterSpellIds, kEnchanterSpellCapacity);
      if (!available) {
        return 0;
      }
      row_count = PopulateAvailableSpellRows(
          *available, menu_strings, row_types,
          Menu_Magic_availableEnchanterSpells, 0,
          Text_Menu0_magic_Index_enchanterSpellDescs, true);

      ApplyGlobalSpellRestrictions(
          *available, std::span<int>(row_types, kEnchanterSpellCapacity),
          Menu_Framework_actionRestrictions);
      CreateLiveSpellMenuRows(
          row_count, (2 << kMenuStyleShift) | kEnchanterMenuInfo,
          enchanter_menu_shortcut_invoked, menu_strings, row_types);
      enchanter_menu_state = kHandleInput;
      break;
    }
    case kHandleInput:
      enchanter_menu_selected_row = VS::Menu::Framework::LiveSelectedRow() + 1;
      if (enchanter_menu_selected_row != 0) {
        Menu_Framework_displayActionCost = 0;
        if ((Menu_Framework_shortcutInvoked != 0) &&
            (enchanter_menu_selected_row == -1)) {
          enchanter_menu_selected_row = -2;
        }
        if (enchanter_menu_selected_row == -1) {
          VS::Menu::Framework::ClearLiveMenuExcept(0);
        } else {
          if (enchanter_menu_selected_row > 0) {
            enchanter_menu_selected_row =
                ReadSpellId(Menu_Magic_availableEnchanterSpells
                                [enchanter_menu_selected_row - 1]);
          }
          VS::Menu::Framework::ClearLiveMenuExcept(
              Menu_Framework_MenuItemId_None);
          VS::Menu::Framework::DismissLiveInformationBox();
          VS::Menu::Framework::SetLiveCommandLabel(
              Menu_Framework_MenuCommand_None);
        }
        enchanter_menu_state = kReturnIfReady;
      } else {
        Menu_Magic_SetSpellMpCost(
            ReadSpellId(Menu_Magic_availableEnchanterSpells
                            [VS::Menu::Framework::LiveHighlightedRow()]));
      }
      break;
    case kReturnIfReady:
      if (VS::Menu::Framework::AreLiveMenuRowsIdle() != 0) {
        return enchanter_menu_selected_row;
      }
      break;
    default:
      break;
  }
  return 0;
}

}  // namespace VS::Menu::Magic

extern "C" void Menu_Magic_ResetMenu0Statics(void) {
  static const char initial_spells[] = {
      Battle_WarlockSpell_Explosion,
      Battle_WarlockSpell_Thunderburst,
      Battle_WarlockSpell_FlameSphere,
      Battle_WarlockSpell_GaeaStrike,
      Battle_WarlockSpell_Avalanche,
      Battle_WarlockSpell_RadialSurge,
      static_cast<char>(Battle_WarlockSpell_Meteor)};
  static char* initial_levels[] = {
      Menu_Magic_warlockMenuSpellLevel1, Menu_Magic_warlockMenuSpellLevel2,
      Menu_Magic_warlockMenuSpellLevel3, Menu_Magic_warlockMenuSpellLevel4,
      Menu_Magic_warlockMenuSpellLevelLocked};

  std::copy(std::begin(initial_spells), std::end(initial_spells),
            std::begin(Menu_Magic_warlockMenuLevelledSpells));
  std::copy(std::begin(initial_levels), std::end(initial_levels),
            std::begin(Menu_Magic_warlockMenuSpellLevels));
}
