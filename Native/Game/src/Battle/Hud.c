// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Actors/ActorModelSlot.h"
#include "Actors/ModelInstance.h"
#include "Actors/Skeleton.h"
#include "Audio/Seq/Channel.h"
#include "Battle/Abilities.h"
#include "Battle/Actor.h"
#include "Battle/Feedback.h"
#include "Battle/Hud.h"
#include "Battle/TextBoxes.h"
#include "Core/Math/FixedPoint.h"
#include "Field/Map.h"
#include "Items/Equipment.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Items/Weapons.h"
#include "Menu/Framework/Framework.h"
#include "Menu/Framework/Menus.h"
#include "Menu/Framework/TextBoxes.h"
#include "Menu/TextBox.h"
#include "Render/Font.h"
#include "Render/Palette.h"
#include "Render/Render.h"
#include "Render/Sprites.h"
#include "Render/UiPrimitives.h"
#include "State/SceneTimer.h"
#include "State/StateFlags.h"
#include "State/Timers.h"

// decomp: vs_battle_limbNames (vs_battle_limbNames@BATTLE/BATTLE.PRG)
char* Battle_limbNames[20]; /* contents: module image, relocated (ModulePointers.c) */

// decomp: D_800EB9AE (D_800EB9AE@BATTLE/BATTLE.PRG)
char Battle_hudStatusIconsHidden; /* contents: module image (ModuleData.c) */

// decomp: D_800EB9AF (D_800EB9AF@BATTLE/BATTLE.PRG)
char Battle_hudHidden; /* contents: module image (ModuleData.c) */

// decomp: D_800EB9B0 (D_800EB9B0@BATTLE/BATTLE.PRG)
int Battle_hudSlideOffset; /* contents: module image (ModuleData.c) */

// decomp: D_800EB9CC (D_800EB9CC@BATTLE/BATTLE.PRG)
char Battle_hudBattleMode; /* contents: module image (ModuleData.c) */

// decomp: D_800EB9CD (D_800EB9CD@BATTLE/BATTLE.PRG)
char Battle_battleModeIndicatorStep; /* contents: module image (ModuleData.c) */

// decomp: D_800EB9D0 (D_800EB9D0@BATTLE/BATTLE.PRG)
union Battle_HudHighlightWord Battle_statusIconCarousel; /* data: contents loaded in Step 5 */

// decomp: D_800EBBDC (D_800EBBDC@BATTLE/BATTLE.PRG)
int Battle_hudGaugePositions[4]; /* contents: module image (ModuleData.c) */

// decomp: D_800EBBEC (D_800EBBEC@BATTLE/BATTLE.PRG)
int Battle_hudGaugeLabelPositions[4]; /* contents: module image (ModuleData.c) */

// decomp: D_800EBBFC (D_800EBBFC@BATTLE/BATTLE.PRG)
char Battle_hudGaugeLabelWidths[4]; /* contents: module image (ModuleData.c) */

// decomp: D_800EBC00 (D_800EBC00@BATTLE/BATTLE.PRG)
char Battle_hudGaugeLabelTexU[20]; /* contents: module image (ModuleData.c) */

// decomp: vs_battle_statusIconTexOffsets (vs_battle_statusIconTexOffsets@BATTLE/BATTLE.PRG)
u_short Battle_statusIconTexOffsets[32]; /* contents: module image (ModuleData.c) */

// decomp: D_800EBC54 (D_800EBC54@BATTLE/BATTLE.PRG)
u_int Battle_limbStatusColors[10]; /* contents: module image (ModuleData.c) */

// decomp: D_800EBC68 (D_800EBC68@BATTLE/BATTLE.PRG)
u_short Battle_upperStatusIconFlags[10]; /* contents: module image (ModuleData.c) */

// decomp: D_800EBC78 (D_800EBC78@BATTLE/BATTLE.PRG)
char Battle_shortcutHudRequested; /* contents: module image (ModuleData.c) */

// decomp: D_800EBC88 (D_800EBC88@BATTLE/BATTLE.PRG)
int Battle_firstShortcutDigitOffsets[58]; /* contents: module image (ModuleData.c) */

// decomp: D_800EBC98 (D_800EBC98@BATTLE/BATTLE.PRG)
u_char Battle_secondShortcutMarkerPositions[216]; /* contents: module image (ModuleData.c) */

// decomp: D_800EBCA0 (D_800EBCA0@BATTLE/BATTLE.PRG)
u_char Battle_secondShortcutIconPositions[208]; /* contents: module image (ModuleData.c) */

// decomp: D_800EBCA8 (D_800EBCA8@BATTLE/BATTLE.PRG)
int Battle_secondShortcutIconSizes[50]; /* contents: module image (ModuleData.c) */

// decomp: D_800EBCB8 (D_800EBCB8@BATTLE/BATTLE.PRG)
u_short Battle_secondShortcutIconTexCoords[92]; /* contents: module image (ModuleData.c) */

// decomp: D_800EBCC0 (D_800EBCC0@BATTLE/BATTLE.PRG)
u_char Battle_firstShortcutMarkerPositions[176]; /* contents: module image (ModuleData.c) */

// decomp: D_800EBCC8 (D_800EBCC8@BATTLE/BATTLE.PRG)
u_char Battle_firstShortcutIconPositions[168]; /* contents: module image (ModuleData.c) */

// decomp: D_800EBCD0 (D_800EBCD0@BATTLE/BATTLE.PRG)
int Battle_firstShortcutIconSizes[40]; /* contents: module image (ModuleData.c) */

// decomp: D_800EBCE0 (D_800EBCE0@BATTLE/BATTLE.PRG)
u_short Battle_firstShortcutIconTexCoords[72]; /* contents: module image (ModuleData.c) */

// decomp: D_800EBCE8 (D_800EBCE8@BATTLE/BATTLE.PRG)
u_char Battle_shortcutHudAnimationStep; /* contents: module image (ModuleData.c) */

// decomp: D_800EBCEC (D_800EBCEC@BATTLE/BATTLE.PRG)
int Battle_limbDisplaySpriteOffsets[33]; /* contents: module image (ModuleData.c) */

// decomp: D_800EBD14 (D_800EBD14@BATTLE/BATTLE.PRG)
int Battle_limbDisplaySpriteSizes[23]; /* contents: module image (ModuleData.c) */

// decomp: D_800EBD3C (D_800EBD3C@BATTLE/BATTLE.PRG)
u_int Battle_limbDisplaySpriteTexCoords[13]; /* contents: module image (ModuleData.c) */

// decomp: D_800EBD64 (D_800EBD64@BATTLE/BATTLE.PRG)
u_short Battle_battleModeIndicatorWobble; /* contents: module image (ModuleData.c) */

// decomp: D_800EC258 (D_800EC258@BATTLE/BATTLE.PRG)
char* Battle_puzzleTimerTextPointer; /* data: contents loaded in Step 5 */

// decomp: vs_battle_lowerScreenUiState (vs_battle_lowerScreenUiState@BATTLE/BATTLE.PRG)
char Battle_lowerScreenUiState;

// decomp: D_80069998 (D_80069998@BATTLE/BATTLE.PRG)
char Battle_puzzleTimerText[9] = "00:00:00";

// decomp: D_8005046C (D_8005046C@SLUS_010.40)
int Battle_puzzleTimer;

// decomp: BATTLE.PRG func_8007BD8C (src/BATTLE/BATTLE.PRG/146C.c:8561)
int Battle_SetGameplayHudHidden(int hidden)
{
    if (hidden != 0) {
        Battle_SetHudHidden(1);
        Field_SetMinimapVisible(0);
    } else {
        Battle_SetHudHidden(0);
        Field_SetMinimapVisible(1);
    }
    return hidden;
}

// decomp: BATTLE.PRG _renderStatBar (src/BATTLE/BATTLE.PRG/5BF94.c:3088)
void Battle_DrawHudStatGauge(int colorIndex, int current, int max)
{
    u_long* prim = Render_frameScratch.orderingTable - 0xC;
    int xy = Battle_hudGaugePositions[colorIndex] - Battle_hudSlideOffset;

    if (max != 0) {
        Render_DrawStatGauge(colorIndex, (((current << 6) + max) - 1) / max, prim, xy);
        if (colorIndex & 2) {
            xy += 54 | (3 << 0x10);
            if (colorIndex == 3) {
                current = (current + 0xFF) >> 8;
            }
        } else {
            xy = Render_DrawIntegerRightAligned(0, xy + (56 | ((u_short)-8 << 0x10)), max, NULL);
            Render_DrawNumericGlyph(2, xy, 0, prim);
            xy += (u_short)-7 | ((u_short)-2 << 0x10);
        }
        Render_DrawIntegerRightAligned(1, xy, current, NULL);
        Render_QueueSpriteWithTexturePage(0x180, Battle_hudGaugeLabelPositions[colorIndex] - Battle_hudSlideOffset,
            Battle_hudGaugeLabelWidths[colorIndex] | 0x90000, prim)[4] =
            Battle_hudGaugeLabelTexU[colorIndex] | 0x37F60000;
    }
}

// decomp: BATTLE.PRG func_800C9CB4 (src/BATTLE/BATTLE.PRG/5BF94.c:3112)
void Battle_DrawSlidingStatusIcon(int status_icon_id, int slide_offset, int arg2)
{
    int clipped_width;
    int texture_offset;
    u_long* sprite_packet;

    if ((status_icon_id != 0xFF) && (Battle_hudSlideOffset == 0)) {

        texture_offset = Battle_statusIconTexOffsets[status_icon_id];
        arg2 = (arg2 * 0x10) + 0xA0;

        if (slide_offset < 0) {
            texture_offset -= slide_offset;
            clipped_width = slide_offset + 0x10;
        } else {
            arg2 += slide_offset;
            clipped_width = 0x10 - slide_offset;
        }

        sprite_packet = Render_QueueSpriteWithTexturePage(
            0x80, arg2 | 0x80000, clipped_width | 0x100000, Render_frameScratch.orderingTable - 4);

        sprite_packet[4] = (texture_offset | (((0x0F0F906A >> status_icon_id) & 1) ? 0x37F90000 : 0x37F80000));

        if ((status_icon_id & 0x10) && !(arg2 & 8)) {
            texture_offset = (status_icon_id & 3) * 8 + 0x3068;
            if (clipped_width >= 9) {
                if (arg2 & 7) {
                    arg2 += 8;
                    clipped_width -= 8;
                } else {
                    int clipped_icon_left = arg2 - 8;
                    arg2 = clipped_icon_left + clipped_width;
                    clipped_width = 8;
                }
            } else {
                int overlay_texture_end = (status_icon_id & 3) * 8 + 0x3070;
                texture_offset = overlay_texture_end - clipped_width;
            }

            Render_QueueSpriteWithTexturePage(0x80, arg2 | 0x80000, clipped_width | 0x80000,
                Render_frameScratch.orderingTable - 4)[4] = texture_offset | 0x37FF0000;
        }
    }
}

// decomp: BATTLE.PRG vs_battle_getStatusFlags (src/BATTLE/BATTLE.PRG/5BF94.c:3158)
int Battle_StatusIconFlags(Battle_ActorStats* actor_stats)
{
    int i;
    int remaining_status_bits = (u_int)actor_stats->statuses >> 5;
    int icon_flags = remaining_status_bits & 0xFFFF;
    remaining_status_bits >>= 16;

    for (i = 0; i < 8; ++i) {
        if ((remaining_status_bits >> i) & 1) {
            icon_flags |= Battle_upperStatusIconFlags[i] << 16;
        }
    }

    return icon_flags;
}

// decomp: BATTLE.PRG vs_battle_getLimbStatus (src/BATTLE/BATTLE.PRG/5BF94.c:3174)
int Battle_LimbCondition(Battle_LimbStats* limb)
{
    int maxHp = limb->maxHp;

    if (limb->hp < 2) {
        return Battle_LimbConditionLevel_Critical;
    }

    if ((limb->hp * 4) < maxHp) {
        return Battle_LimbConditionLevel_Damaged;
    }

    if ((limb->hp * 4) < (maxHp * 3)) {
        return Battle_LimbConditionLevel_Wounded;
    }

    if (limb->hp < maxHp) {
        return Battle_LimbConditionLevel_Good;
    }

    return Battle_LimbConditionLevel_Excellent;
}

// decomp: BATTLE.PRG func_800C9EB8 (src/BATTLE/BATTLE.PRG/5BF94.c:3197)
int Battle_ShortcutIconBrightness(int shortcut_index)
{
    int restriction_flags = Menu_Framework_ActionRestrictionFlags(0);
    int brightness_transparency = 0x180;

    switch (shortcut_index) {
    case 0:
    case 1:
    case 2:
    case 3:
        if (Battle_HasUnlockedSpellClass((0x3021 >> (shortcut_index * 4)) & 0xF) == 0) {
            return 0;
        }
        if (restriction_flags & 0xB7) {
            brightness_transparency = 0x140;
        }
        break;
    case 4:
        if (restriction_flags & 0x7) {
            brightness_transparency = 0x140;
        }
        break;
    case 5:
    case 6:
        if (Battle_HasUnlockedBattleAbility(6 - shortcut_index) == 0) {
            return 0;
        }
        break;
    case 7:
        if (Battle_HasUnlockedWeaponBreakArt() == 0) {
            return 0;
        }
        if (restriction_flags & 0x15F) {
            brightness_transparency = 0x140;
        }
        break;
    default:
        break;
    }
    return brightness_transparency;
}

// decomp: BATTLE.PRG func_800C9F88 (src/BATTLE/BATTLE.PRG/5BF94.c:3239)
void Battle_ShowShortcutHud(void) { Battle_shortcutHudRequested = 1; }

// decomp: BATTLE.PRG func_800C9F98 (src/BATTLE/BATTLE.PRG/5BF94.c:3241)
void Battle_DrawShortcutButtonMarker(int packed_screen_position, int brightness_transparency)
{
    u_long* sprite_packet =
        Render_QueueSpriteWithTexturePage(brightness_transparency, packed_screen_position, 0x60006, Render_frameScratch.overlayOrderingTable);
    sprite_packet[1] = 0xE1000017;
    sprite_packet[4] = 0x373D80C0;
}

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
            icon_brightness_transparency = Battle_ShortcutIconBrightness(i + 4);
            if (icon_brightness_transparency != 0) {
                Battle_DrawShortcutButtonMarker(
                    ((panel_x_offset + Battle_secondShortcutMarkerPositions[i]) & 0xFFFF) | (Battle_secondShortcutMarkerPositions[i + 4] << 0x10),
                    icon_brightness_transparency);
                sprite_packet = Render_QueueSpriteWithTexturePage(icon_brightness_transparency,
                    ((panel_x_offset + Battle_secondShortcutIconPositions[i]) & 0xFFFF) | (Battle_secondShortcutIconPositions[i + 4] << 0x10),
                    Battle_secondShortcutIconSizes[i], Render_frameScratch.overlayOrderingTable);
                sprite_packet[1] = 0xE1000017;
                sprite_packet[4] = (*((i) + Battle_secondShortcutIconTexCoords) | 0x373D0000);
            }
        }

        Render_QueueSpriteWithTexturePage(0x180, ((panel_x_offset + 0x48) & 0xFFFF) | 0x660000,
            0x1E001E, Render_frameScratch.overlayOrderingTable)[4] = 0x37FBA0B8;
        panel_x_offset = (0xA0 - panel_x_offset);

        for (i = 0; i < 4; ++i) {
            icon_brightness_transparency = Battle_ShortcutIconBrightness(i);
            if (icon_brightness_transparency != 0) {
                Battle_DrawShortcutButtonMarker(
                    ((panel_x_offset + Battle_firstShortcutMarkerPositions[i]) & 0xFFFF) | (Battle_firstShortcutMarkerPositions[i + 4] << 0x10),
                    icon_brightness_transparency);
                sprite_packet = Render_QueueSpriteWithTexturePage(icon_brightness_transparency,
                    (((panel_x_offset + Battle_firstShortcutIconPositions[i]) - 2) & 0xFFFF)
                        | (Battle_firstShortcutIconPositions[i + 4] << 0x10),
                    Battle_firstShortcutIconSizes[i], Render_frameScratch.overlayOrderingTable);
                sprite_packet[1] = 0xE1000017;
                sprite_packet[4] = Battle_firstShortcutIconTexCoords[i] | 0x373D0000;
            }
            Render_QueueSpriteWithTexturePage(0x80, panel_x_offset + Battle_firstShortcutDigitOffsets[3 - i], 0x100010,
                Render_frameScratch.overlayOrderingTable)[4] = (((3 - i) * 0x10) | 0x37FB8000);
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
    ot = Render_frameScratch.orderingTable - 4;
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
        prim = Render_QueueSprite(0x140,
            ((((u_char)Battle_battleModeIndicatorStep * 3 - 4) << 16) - Battle_hudSlideOffset) | 0xEC, 0xA0049,
            Render_frameScratch.orderingTable - 0xC);
        prim[1] = 0xE1000017;
        k = Core_Gte_Sin(Battle_battleModeIndicatorWobble & 0x7FF);
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
                prim = Render_QueueSpriteWithTexturePage(
                    0, k + Battle_limbDisplaySpriteOffsets[j], Battle_limbDisplaySpriteSizes[j], ot);
                prim[2] = Battle_limbStatusColors[Battle_LimbCondition(
                              &Battle_characterState->stats->limbs[i])]
                        | 0x64000000;
                prim[4] = Battle_limbDisplaySpriteTexCoords[j];
            }
        }
        if (Battle_hudSlideOffset != 0) {
            Battle_hudSlideOffset -= 0x80000;
        }
    }

    Battle_DrawHudStatGauge(0, actor->currentHp, actor->maxHp);
    Battle_DrawHudStatGauge(1, actor->currentMp, actor->maxMp);
    if ((k = actor->risk) != 0) {
        Battle_DrawHudStatGauge(2, k, 100);
    }
    if (k == 0) {
        Render_DrawCompactText(
            "RISK  ZERO", 0x16000C - Battle_hudSlideOffset, Render_frameScratch.orderingTable - 0xC);
    }

    if (Battle_hudStatusIconsHidden != 0) {
        return;
    }

    flags = Battle_StatusIconFlags(actor);
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
    Battle_DrawSlidingStatusIcon(Battle_statusIconCarousel.bytes[2], rightX, 1);
    ot -= 4;
    Battle_DrawSlidingStatusIcon(Battle_statusIconCarousel.bytes[1], k, 1);
    Battle_DrawSlidingStatusIcon(Battle_statusIconCarousel.bytes[1], rightX, 0);
    Battle_DrawSlidingStatusIcon(Battle_statusIconCarousel.bytes[0], k, 0);

    k = 0x80000 - Battle_hudSlideOffset;
    if (count < 3) {
        for (i = 0; i < count; ++i) {
            prim =
                Render_QueueSpriteWithTexturePage(0x80, (0x94 + i * 3) | k, 0x40004, ot);
            prim[4] = 0x37F40C1A;
        }
        return;
    }

    j = k + 0x30000;
    if (selected != 0xFF) {
        prim = Render_QueueSprite(
            0x80, ((selected & 7) * 3 + 0x94) | (!(selected & 8) ? k : j), 0x40004, ot);
        prim[4] = 0x37F40C1A;
    }
    i = 25;
    if (count < 9) {
        i = count * 3 + 1;
    } else {
        prim = Render_QueueSprite(0x80, j | 0x94, ((count - 8) * 3 + 1) | 0x40000, ot);
        prim[4] = 0x37F40C00;
    }
    prim = Render_QueueSpriteWithTexturePage(0x80, k | 0x94, i | 0x40000, ot);
    prim[4] = 0x37F40C00;
}

// decomp: BATTLE.PRG func_800CB550 (src/BATTLE/BATTLE.PRG/5BF94.c:3838)
void Battle_ShowPuzzleTimer(void) { Battle_lowerScreenUiState = 1; }

// decomp: BATTLE.PRG func_800CB560 (src/BATTLE/BATTLE.PRG/5BF94.c:3840)
void Battle_HidePuzzleTimer(void) { Battle_lowerScreenUiState = 0; }

// decomp: BATTLE.PRG _renderTimer (src/BATTLE/BATTLE.PRG/5BF94.c:3844)
void Battle_DrawPuzzleTimer(const State_SceneTimer* timer)
{
    char timer_components[4] = {0, timer->seconds, timer->minutes, timer->hours};
    int i;

    timer_components[0] = Audio_Seq_unusedStateFlags.puzzleTimerHundredths;

    if (timer_components[3] != 0) {
        timer_components[2] = 59;
        timer_components[1] = 59;
        timer_components[0] = 99;
    }

    for (i = 0; i < 3; ++i) {
        int bcd = Core_Math_SplitDecimalDigit(timer_components[i]);
        Battle_puzzleTimerText[6 - i * 3] = (bcd >> 4) + '0';
        Battle_puzzleTimerText[7 - i * 3] = (bcd & 0xF) + '0';
    }

    Core_Debugf("hud.timer", "text=%s seconds=%d minutes=%d hours=%d frames=%d",
        Battle_puzzleTimerText, timer->seconds, timer->minutes, timer->hours, timer->frames);
    Render_DrawCompactText(Battle_puzzleTimerText, vs_getXY(128, 200), Render_frameScratch.orderingTable - 4);
}

// decomp: BATTLE.PRG func_800CB654 (src/BATTLE/BATTLE.PRG/5BF94.c:3867)
void Battle_SetHudHidden(int hidden) { Battle_hudHidden = hidden; }

// decomp: BATTLE.PRG func_800CB660 (src/BATTLE/BATTLE.PRG/5BF94.c:3869)
void Battle_SetHudBattleMode(int battle_mode) { Battle_hudBattleMode = battle_mode; }

