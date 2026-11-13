// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Battle/HudRenderingLive.hpp"
#include "Battle/HudStateLive.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Render/GameFontLive.hpp"
#include "Render/SpritesLive.hpp"
extern "C" {
#include "Legacy/Psx.h"
#include "Actors/ActorModelSlot.h"
#include "Actors/ModelInstance.h"
#include "Actors/Skeleton.h"
#include "Audio/Seq/Channel.h"
#include "Battle/AbilityState.h"
#include "Battle/Actor.h"
#include "Battle/Feedback.h"
#include "Battle/Hud.h"
#include "Battle/TextBoxes.h"
#include "Items/Equipment.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Items/Weapons.h"
#include "Menu/Framework/Menus.h"
#include "Menu/Framework/TextBoxes.h"
#include "Menu/TextBox.h"
#include "Render/Palette.h"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"
#include "State/SceneTimer.h"
#include "State/StateFlags.h"
#include "State/Timers.h"

// decomp: vs_battle_limbNames (vs_battle_limbNames@BATTLE/BATTLE.PRG)
char* Battle_limbNames[20]; /* contents: module image, relocated (ModulePointers.cpp) */

// decomp: D_800EB9AE (D_800EB9AE@BATTLE/BATTLE.PRG)
char Battle_hudStatusIconsHidden; /* contents: module image (ModuleData.cpp) */

// decomp: D_800EB9AF (D_800EB9AF@BATTLE/BATTLE.PRG)
char Battle_hudHidden; /* contents: module image (ModuleData.cpp) */

// decomp: D_800EB9B0 (D_800EB9B0@BATTLE/BATTLE.PRG)
int Battle_hudSlideOffset; /* contents: module image (ModuleData.cpp) */

// decomp: D_800EB9CC (D_800EB9CC@BATTLE/BATTLE.PRG)
char Battle_hudBattleMode; /* contents: module image (ModuleData.cpp) */

// decomp: D_800EB9CD (D_800EB9CD@BATTLE/BATTLE.PRG)
char Battle_battleModeIndicatorStep; /* contents: module image (ModuleData.cpp) */

// decomp: D_800EB9D0 (D_800EB9D0@BATTLE/BATTLE.PRG)
union Battle_HudHighlightWord Battle_statusIconCarousel; /* data: contents loaded in Step 5 */

// decomp: D_800EBBDC (D_800EBBDC@BATTLE/BATTLE.PRG)
int Battle_hudGaugePositions[4]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800EBBEC (D_800EBBEC@BATTLE/BATTLE.PRG)
int Battle_hudGaugeLabelPositions[4]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800EBBFC (D_800EBBFC@BATTLE/BATTLE.PRG)
char Battle_hudGaugeLabelWidths[4]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800EBC00 (D_800EBC00@BATTLE/BATTLE.PRG)
char Battle_hudGaugeLabelTexU[20]; /* contents: module image (ModuleData.cpp) */

// decomp: vs_battle_statusIconTexOffsets (vs_battle_statusIconTexOffsets@BATTLE/BATTLE.PRG)
u_short Battle_statusIconTexOffsets[32]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800EBC54 (D_800EBC54@BATTLE/BATTLE.PRG)
u_int Battle_limbStatusColors[10]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800EBC68 (D_800EBC68@BATTLE/BATTLE.PRG)
u_short Battle_upperStatusIconFlags[10]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800EBC78 (D_800EBC78@BATTLE/BATTLE.PRG)
char Battle_shortcutHudRequested; /* contents: module image (ModuleData.cpp) */

// decomp: D_800EBC88 (D_800EBC88@BATTLE/BATTLE.PRG)
int Battle_firstShortcutDigitOffsets[58]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800EBC98 (D_800EBC98@BATTLE/BATTLE.PRG)
u_char Battle_secondShortcutMarkerPositions[216]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800EBCA0 (D_800EBCA0@BATTLE/BATTLE.PRG)
u_char Battle_secondShortcutIconPositions[208]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800EBCA8 (D_800EBCA8@BATTLE/BATTLE.PRG)
int Battle_secondShortcutIconSizes[50]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800EBCB8 (D_800EBCB8@BATTLE/BATTLE.PRG)
u_short Battle_secondShortcutIconTexCoords[92]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800EBCC0 (D_800EBCC0@BATTLE/BATTLE.PRG)
u_char Battle_firstShortcutMarkerPositions[176]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800EBCC8 (D_800EBCC8@BATTLE/BATTLE.PRG)
u_char Battle_firstShortcutIconPositions[168]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800EBCD0 (D_800EBCD0@BATTLE/BATTLE.PRG)
int Battle_firstShortcutIconSizes[40]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800EBCE0 (D_800EBCE0@BATTLE/BATTLE.PRG)
u_short Battle_firstShortcutIconTexCoords[72]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800EBCE8 (D_800EBCE8@BATTLE/BATTLE.PRG)
u_char Battle_shortcutHudAnimationStep; /* contents: module image (ModuleData.cpp) */

// decomp: D_800EBCEC (D_800EBCEC@BATTLE/BATTLE.PRG)
int Battle_limbDisplaySpriteOffsets[33]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800EBD14 (D_800EBD14@BATTLE/BATTLE.PRG)
int Battle_limbDisplaySpriteSizes[23]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800EBD3C (D_800EBD3C@BATTLE/BATTLE.PRG)
u_int Battle_limbDisplaySpriteTexCoords[13]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800EBD64 (D_800EBD64@BATTLE/BATTLE.PRG)
u_short Battle_battleModeIndicatorWobble; /* contents: module image (ModuleData.cpp) */

// decomp: D_800EC258 (D_800EC258@BATTLE/BATTLE.PRG)
char* Battle_puzzleTimerTextPointer; /* data: contents loaded in Step 5 */

// decomp: vs_battle_lowerScreenUiState (vs_battle_lowerScreenUiState@BATTLE/BATTLE.PRG)
char Battle_lowerScreenUiState;

// decomp: D_80069998 (D_80069998@BATTLE/BATTLE.PRG)
char Battle_puzzleTimerText[9] = "00:00:00";

// decomp: D_8005046C (D_8005046C@SLUS_010.40)
int Battle_puzzleTimer;

// decomp: BATTLE.PRG func_800C9FE8 (src/BATTLE/BATTLE.PRG/5BF94.c:3249)
void Battle_DrawShortcutHud(void)
{
    int panel_x_offset;
    int icon_brightness_transparency;
    int i;
    u_long* sprite_packet;

    if (Battle_shortcutHudRequested != 0) {
        if (Battle_shortcutHudAnimationStep != 0) {
            Battle_shortcutHudAnimationStep -= 1;
        }
    } else {
        Battle_shortcutHudAnimationStep = Menu_Framework_animationIndices[Battle_shortcutHudAnimationStep];
    }

    if (Battle_shortcutHudAnimationStep != 10) {

        panel_x_offset = -Menu_Framework_rowAnimationSteps[Battle_shortcutHudAnimationStep];

        for (i = 0; i < 4; ++i) {
            icon_brightness_transparency = VS::Battle::LiveShortcutIconBrightness(i + 4);
            if (icon_brightness_transparency != 0) {
                VS::Battle::DrawLiveShortcutButtonMarker(
                    ((panel_x_offset + Battle_secondShortcutMarkerPositions[i]) & 0xFFFF) | (Battle_secondShortcutMarkerPositions[i + 4] << 0x10),
                    icon_brightness_transparency);
                sprite_packet = VS::Render::QueueLiveSpriteWithTexturePage(icon_brightness_transparency,
                    ((panel_x_offset + Battle_secondShortcutIconPositions[i]) & 0xFFFF) | (Battle_secondShortcutIconPositions[i + 4] << 0x10),
                    Battle_secondShortcutIconSizes[i], (u_long *)(Render_frameScratch.overlayOrderingTable));
                sprite_packet[1] = 0xE1000017;
                sprite_packet[4] = (*((i) + Battle_secondShortcutIconTexCoords) | 0x373D0000);
            }
        }

        VS::Render::QueueLiveSpriteWithTexturePage(0x180, ((panel_x_offset + 0x48) & 0xFFFF) | 0x660000,
            0x1E001E, (u_long *)(Render_frameScratch.overlayOrderingTable))[4] = 0x37FBA0B8;
        panel_x_offset = (0xA0 - panel_x_offset);

        for (i = 0; i < 4; ++i) {
            icon_brightness_transparency = VS::Battle::LiveShortcutIconBrightness(i);
            if (icon_brightness_transparency != 0) {
                VS::Battle::DrawLiveShortcutButtonMarker(
                    ((panel_x_offset + Battle_firstShortcutMarkerPositions[i]) & 0xFFFF) | (Battle_firstShortcutMarkerPositions[i + 4] << 0x10),
                    icon_brightness_transparency);
                sprite_packet = VS::Render::QueueLiveSpriteWithTexturePage(icon_brightness_transparency,
                    (((panel_x_offset + Battle_firstShortcutIconPositions[i]) - 2) & 0xFFFF)
                        | (Battle_firstShortcutIconPositions[i + 4] << 0x10),
                    Battle_firstShortcutIconSizes[i], (u_long *)(Render_frameScratch.overlayOrderingTable));
                sprite_packet[1] = 0xE1000017;
                sprite_packet[4] = Battle_firstShortcutIconTexCoords[i] | 0x373D0000;
            }
            VS::Render::QueueLiveSpriteWithTexturePage(0x80, panel_x_offset + Battle_firstShortcutDigitOffsets[3 - i], 0x100010,
                (u_long *)(Render_frameScratch.overlayOrderingTable))[4] = (((3 - i) * 0x10) | 0x37FB8000);
        }
    }
    Battle_shortcutHudRequested = 0;
}

// decomp: BATTLE.PRG func_800CA2DC (src/BATTLE/BATTLE.PRG/5BF94.c:3307)
void Battle_DrawBattleHud(void)
{
    u_long* prim;
    Battle_ActorStats* actor;
    void* ot;
    int i;
    int j;
    int k;
    int rightX;
    u_int flags;
    u_int count;
    int selected;

    actor = Battle_characterState->stats;
    ot = ((u_char*)(Render_frameScratch.orderingTable)) - 4;
    Battle_DrawShortcutHud();

    if (Battle_hudBattleMode == 1) {
        if ((u_char)Battle_battleModeIndicatorStep < 4) {
            Battle_battleModeIndicatorWobble = 0;
            ++Battle_battleModeIndicatorStep;
        }
    } else if (Battle_battleModeIndicatorStep != 0) {
        --Battle_battleModeIndicatorStep;
    }

    if (Battle_battleModeIndicatorStep != 0) {
        prim = VS::Render::QueueLiveSprite(0x140,
            ((((u_char)Battle_battleModeIndicatorStep * 3 - 4) << 16) - Battle_hudSlideOffset) | 0xEC, 0xA0049,
            (u_long *)(((u_char*)(Render_frameScratch.orderingTable)) - 0xC));
        prim[1] = 0xE1000017;
        k = VS::Core::Math::Sine(Battle_battleModeIndicatorWobble & 0x7FF);
        prim[4] = 0x373D0040;
        prim[2] += k >> 5;
        Battle_battleModeIndicatorWobble += State_tickSpeed * 64;
    }

    if (Battle_hudHidden != 0) {
        if (Battle_hudSlideOffset == 0x200000) {
            return;
        }
        Battle_hudSlideOffset += 0x80000;
    } else {
        k = Menu_textBoxes[7].state;
        if (!(Battle_lowerScreenUiState & 2)
            && ((k == 0) || (k != Menu_textBoxes[7].openDuration + 1))) {
            k = 0xB80008;
            for (i = 0; i < 5; ++i) {
                j = i + (Battle_hudBattleMode & 1) * 5;
                prim = VS::Render::QueueLiveSpriteWithTexturePage(
                    0, k + Battle_limbDisplaySpriteOffsets[j], Battle_limbDisplaySpriteSizes[j], (u_long *)(ot));
                prim[2] = Battle_limbStatusColors[VS::Battle::LiveLimbCondition(
                              &Battle_characterState->stats->limbs[i])]
                        | 0x64000000;
                prim[4] = Battle_limbDisplaySpriteTexCoords[j];
            }
        }
        if (Battle_hudSlideOffset != 0) {
            Battle_hudSlideOffset -= 0x80000;
        }
    }

    VS::Battle::DrawLiveHudStatGauge(0, actor->currentHp, actor->maxHp);
    VS::Battle::DrawLiveHudStatGauge(1, actor->currentMp, actor->maxMp);
    if ((k = actor->risk) != 0) {
        VS::Battle::DrawLiveHudStatGauge(2, k, 100);
    }
    if (k == 0) {
        VS::Render::DrawLiveCompactText(
            "RISK  ZERO", 0x16000C - Battle_hudSlideOffset, ((u_char*)(Render_frameScratch.orderingTable)) - 0xC);
    }

    if (Battle_hudStatusIconsHidden != 0) {
        return;
    }

    flags = VS::Battle::LiveStatusIconFlags(actor);
    count = 0;
    selected = 0xFF;
    for (j = 0; j < 32; ++j) {
        if ((flags >> j) & 1) {
            if (j == Battle_statusIconCarousel.bytes[1]) {
                selected = count;
            }
            ++count;
        }
    }
    for (j = 0; j < 3; ++j) {
        if (!((flags >> Battle_statusIconCarousel.bytes[j]) & 1)) {
            Battle_statusIconCarousel.bytes[j] = 0xFF;
        }
    }

    if (count == 0) {
        return;
    }

    j = Battle_statusIconCarousel.bytes[3];
    if (j == 12) {
        Battle_statusIconCarousel.bytes[0] = Battle_statusIconCarousel.bytes[1];
        Battle_statusIconCarousel.bytes[1] = Battle_statusIconCarousel.bytes[2];
        Battle_statusIconCarousel.bytes[2] = 0xFF;
        i = (Battle_statusIconCarousel.bytes[1] + 1) & 0xFF;
        for (k = 0; k < 32; ++k, ++i) {
            i %= 32;
            if (((flags >> i) & 1) && (i != Battle_statusIconCarousel.bytes[1])) {
                Battle_statusIconCarousel.bytes[2] = i;
                break;
            }
        }
    }

    k = 1; // dead store kept for codegen: count == 1 below is compared against this
           // register
    if ((Battle_statusIconCarousel.bytes[0] == Battle_statusIconCarousel.bytes[2]) && (Battle_statusIconCarousel.bytes[1] != 0xFF)) {
        if ((count == 1) && (j < 3)) {
            j = 0;
        }
        if ((count == 2) && (Battle_statusIconCarousel.bytes[0] != 0xFF) && ((u_int)(j - 12) < 3)) {
            j = 12;
        }
    }

    k = -8;
    if (j >= 9) {
        if (j < 12) {
            k = 8 - j * 2;
        } else {
            k = 24 - j * 2;
        }
    }

    if ((++j != Battle_statusIconCarousel.bytes[3]) && (State_tickSpeed == 4)) {
        j = (j + 1) & 0xE;
    }
    rightX = k + 16;
    Battle_statusIconCarousel.bytes[3] = j & 0xF;
    VS::Battle::DrawLiveSlidingStatusIcon(Battle_statusIconCarousel.bytes[2], rightX, 1);
    ot = (u_char*)(ot) - (4);
    VS::Battle::DrawLiveSlidingStatusIcon(Battle_statusIconCarousel.bytes[1], k, 1);
    VS::Battle::DrawLiveSlidingStatusIcon(Battle_statusIconCarousel.bytes[1], rightX, 0);
    VS::Battle::DrawLiveSlidingStatusIcon(Battle_statusIconCarousel.bytes[0], k, 0);

    k = 0x80000 - Battle_hudSlideOffset;
    if (count < 3) {
        for (i = 0; i < count; ++i) {
            prim =
                VS::Render::QueueLiveSpriteWithTexturePage(0x80, (0x94 + i * 3) | k, 0x40004, (u_long *)(ot));
            prim[4] = 0x37F40C1A;
        }
        return;
    }

    j = k + 0x30000;
    if (selected != 0xFF) {
        prim = VS::Render::QueueLiveSprite(
            0x80, ((selected & 7) * 3 + 0x94) | (!(selected & 8) ? k : j), 0x40004, (u_long *)(ot));
        prim[4] = 0x37F40C1A;
    }
    i = 25;
    if (count < 9) {
        i = count * 3 + 1;
    } else {
        prim = VS::Render::QueueLiveSprite(0x80, j | 0x94, ((count - 8) * 3 + 1) | 0x40000, (u_long *)(ot));
        prim[4] = 0x37F40C00;
    }
    prim = VS::Render::QueueLiveSpriteWithTexturePage(0x80, k | 0x94, i | 0x40000, (u_long *)(ot));
    prim[4] = 0x37F40C00;
}

// decomp: BATTLE.PRG _renderTimer (src/BATTLE/BATTLE.PRG/5BF94.c:3844)
void Battle_DrawPuzzleTimer(const State_SceneTimer* timer)
{
    char timer_components[4] = {0, (char)(timer->seconds), (char)(timer->minutes), (char)(timer->hours)};
    int i;

    timer_components[0] = Audio_Seq_unusedStateFlags.puzzleTimerHundredths;

    if (timer_components[3] != 0) {
        timer_components[2] = 59;
        timer_components[1] = 59;
        timer_components[0] = 99;
    }

    for (i = 0; i < 3; ++i) {
        int bcd = VS::Core::Math::SplitDecimalDigit(timer_components[i]);
        Battle_puzzleTimerText[6 - i * 3] = (bcd >> 4) + '0';
        Battle_puzzleTimerText[7 - i * 3] = (bcd & 0xF) + '0';
    }

    Core_Debugf("hud.timer", "text=%s seconds=%d minutes=%d hours=%d frames=%d",
        Battle_puzzleTimerText, timer->seconds, timer->minutes, timer->hours, timer->frames);
    VS::Render::DrawLiveCompactText(Battle_puzzleTimerText, vs_getXY(128, 200), ((u_char*)(Render_frameScratch.orderingTable)) - 4);
}

}  // extern "C"
