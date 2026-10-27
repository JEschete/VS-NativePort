// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Actors/ActorModelSlot.h"
#include "Actors/ActorRecord.h"
#include "Actors/ActorState.h"
#include "Actors/Animation.h"
#include "Actors/ModelInstance.h"
#include "Actors/Movement.h"
#include "Actors/Shadow.h"
#include "Actors/Skeleton.h"
#include "Actors/TextureAnimation.h"
#include "Battle/Actor.h"
#include "Battle/Damage.h"
#include "Battle/Feedback.h"
#include "Battle/Hud.h"
#include "Battle/Sphere.h"
#include "Battle/Targeting.h"
#include "Content/StringTables.h"
#include "DiscQueue.h"
#include "Field/RoomRendering.h"
#include "Input/Buttons.h"
#include "Input/Controller.h"
#include "Items/Equipment.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Items/Weapons.h"
#include "Menu/Framework/ActorPreview.h"
#include "Menu/Framework/EquipmentDetails.h"
#include "Menu/Framework/Framework.h"
#include "Menu/Framework/ItemRows.h"
#include "Menu/Framework/MenuMemory.h"
#include "Menu/Framework/Menus.h"
#include "Menu/Framework/TextBoxes.h"
#include "Menu/Sounds.h"
#include "Menu/Status/ActorStatus.h"
#include "Menu/Status/EquipmentDetails.h"
#include "Render/Camera.h"
#include "Render/Display.h"
#include "Render/Font.h"
#include "Render/Images.h"
#include "Render/LegacyPackets.h"
#include "Render/Palette.h"
#include "Render/Render.h"
#include "Render/Sprites.h"
#include "Render/UiPrimitives.h"

// decomp: local static of vs_menu4_exec, at file scope so the module reset reaches it
static int Menu_Status_limbCursorAnimation = 0;

// decomp: _ (_@src/MENU/MENU4.PRG/120.c)
char Menu_Status_layoutPadding1[24];

// decomp: _animationStep (_animationStep@src/MENU/MENU4.PRG/120.c)
int Menu_Status_animationStep = 0;

// decomp: _basicStatsAnimDelay (_basicStatsAnimDelay@src/MENU/MENU4.PRG/120.c)
char Menu_Status_basicStatsAnimDelay = 0;

// decomp: _basicStatsAnimState (_basicStatsAnimState@src/MENU/MENU4.PRG/120.c)
char Menu_Status_basicStatsAnimState = 0;

// decomp: _delayScreenUpdate (_delayScreenUpdate@src/MENU/MENU4.PRG/120.c)
char Menu_Status_delayScreenUpdate = 0;

// decomp: _drawBackgroundFirst (_drawBackgroundFirst@src/MENU/MENU4.PRG/120.c)
int Menu_Status_drawBackgroundFirst = 0;

// decomp: _fadeScreen (_fadeScreen@src/MENU/MENU4.PRG/120.c)
int Menu_Status_fadeScreen = 0;

// decomp: _initializedActor (_initializedActor@src/MENU/MENU4.PRG/120.c)
char Menu_Status_initializedActor = 0;

// decomp: _limbStatuses (_limbStatuses@src/MENU/MENU4.PRG/120.c)
char* Menu_Status_limbStatuses[] = { "DYING", "BAD", "AVERAGE", "GOOD", "EXCELLENT", NULL };

// decomp: _previousActor (_previousActor@src/MENU/MENU4.PRG/120.c)
int Menu_Status_previousActor = 0;

// decomp: _renderBasicStatsState (_renderBasicStatsState@src/MENU/MENU4.PRG/120.c)
char Menu_Status_renderBasicStatsState = 0;

// decomp: _selectedActor (_selectedActor@src/MENU/MENU4.PRG/120.c)
int Menu_Status_selectedActor = 0;

// decomp: _selectedElement (_selectedElement@src/MENU/MENU4.PRG/120.c)
u_char Menu_Status_selectedElement;

// decomp: _selectedStatRow (_selectedStatRow@src/MENU/MENU4.PRG/120.c)
char Menu_Status_selectedStatRow;

// decomp: _statusStrings (_statusStrings@src/MENU/MENU4.PRG/120.c)
u_short Menu_Status_statusStrings[1760]; /* contents: disc string table (ModuleData.c) */

// decomp: _xPos (_xPos@src/MENU/MENU4.PRG/120.c)
int Menu_Status_xPos = 0;

// decomp: _yPos (_yPos@src/MENU/MENU4.PRG/120.c)
int Menu_Status_yPos = 0;

// decomp: local static of vs_menu4_exec, at file scope so the module reset reaches it
static int Menu_Status_updateAnimWait = 0;

// decomp: local static of _navigateStatusModifiers, at file scope so the module reset reaches it
static char Menu_Status_navigateStatusEffectsAnimationState = 0;

// decomp: local static of _renderLimbUi, at file scope so the module reset reaches it
static int Menu_Status_limbPanelLimbAnimSteps[6];

// decomp: local static of vs_menu4_exec, at file scope so the module reset reaches it
static int Menu_Status_updateScreenEnabled = 0;

// decomp: local static of _renderLimbUi, at file scope so the module reset reaches it
static char Menu_Status_limbPanelState = 0;

// decomp: local static of _renderLimbUi, at file scope so the module reset reaches it
static u_char Menu_Status_limbPanelActor = 0;

// decomp: MENU4.PRG _initEquipmentScreen (src/MENU/MENU4.PRG/120.c:528)
void Menu_Status_BeginVitalsTransition(int actor)
{
    if (actor == 0) {
        Menu_Status_renderBasicStatsState = 1;
        Menu_Status_basicStatsAnimDelay = 4;
    } else if (Menu_Status_initializedActor == 0) {
        Battle_SetHudHidden(1);
        Menu_Status_renderBasicStatsState = 4;
        Menu_Status_basicStatsAnimDelay = 4;
    } else {
        Menu_Status_renderBasicStatsState = 2;
        Menu_Status_basicStatsAnimDelay = 4;
    }

    Menu_Status_initializedActor = actor;
}

// decomp: MENU4.PRG _getNextValidActor (src/MENU/MENU4.PRG/120.c:548)
int Menu_Status_FindNextInspectableActor(int currentActor, int targetActor)
{
    int candidate_actor_state_flags;

    while (1) {

        currentActor = (currentActor + targetActor) % 15;
        candidate_actor_state_flags = Actors_ActorStateFlags(currentActor);

        if ((Battle_actors[currentActor] != NULL)
            && (Battle_actors[currentActor]->kind < 5)
            && (candidate_actor_state_flags & 0x01000001) == 1) {
            break;
        }
    }

    return currentActor;
}

// decomp: MENU4.PRG _switchCurrentActor (src/MENU/MENU4.PRG/120.c:579)
int Menu_Status_AnimateActorSwitch(int init)
{
    enum Menu_Status_ActorSwitchState {
        Menu_Status_ActorSwitchState_MenuLoaded,
        Menu_Status_ActorSwitchState_MenuLoading,
        Menu_Status_ActorSwitchState_AnimateForward,
        Menu_Status_ActorSwitchState_MenuLoadedReverse,
        Menu_Status_ActorSwitchState_MenuLoadingReverse,
        Menu_Status_ActorSwitchState_AnimateReverse,
        Menu_Status_ActorSwitchState_Complete
    };

    static u_int Menu_Status_animateActorSwitchState = Menu_Status_ActorSwitchState_MenuLoaded;

    if (init != 0) {
        if (Menu_Status_selectedActor == 0) {
            Menu_Status_animateActorSwitchState = Menu_Status_ActorSwitchState_MenuLoading;
        } else {
            Menu_PlayOpenSound();
            Menu_Status_animateActorSwitchState = Menu_Status_ActorSwitchState_MenuLoaded;
        }

        if (init & 0x10) {
            Menu_Status_animateActorSwitchState += 3;
        }

        Menu_Status_selectedActor = init & 0xF;

        Menu_Framework_SetMenuItem(
            4, 180, 18, 140, 8, Battle_actors[Menu_Status_selectedActor - 1]->stats->name)
            ->selected = 1;

        return 0;
    }

    switch (Menu_Status_animateActorSwitchState) {
    case Menu_Status_ActorSwitchState_MenuLoaded:
        if (Menu_Status_animationStep < 160) {
            Menu_Status_drawBackgroundFirst = 1;
            Menu_Status_animationStep += 32;
            break;
        } else {
            Menu_Status_drawBackgroundFirst = 0;
            Menu_Framework_ReleaseActorPreview();
        }
        // Fallthrough

    case Menu_Status_ActorSwitchState_MenuLoading:
        Menu_Status_fadeScreen = 1;

        Menu_Framework_LoadActorPreview(Menu_Status_selectedActor - 1);

        Menu_Status_delayScreenUpdate = 1;
        Menu_Status_animationStep = -160;
        Menu_Status_animateActorSwitchState = Menu_Status_ActorSwitchState_AnimateForward;

        break;

    case Menu_Status_ActorSwitchState_AnimateForward:
        if (Menu_Status_animationStep < 0) {
            Menu_Status_animationStep += 32;
            break;
        }

        Menu_Status_fadeScreen = 0;
        Menu_Status_animateActorSwitchState = Menu_Status_ActorSwitchState_Complete;

        return 1;

    case Menu_Status_ActorSwitchState_MenuLoadedReverse:
        if (Menu_Status_animationStep >= -159) {
            Menu_Status_fadeScreen = 1;
            Menu_Status_animationStep -= 32;
            break;
        }

        Menu_Framework_ReleaseActorPreview();
        // Fallthrough

    case Menu_Status_ActorSwitchState_MenuLoadingReverse:
        Menu_Status_fadeScreen = 0;

        Menu_Framework_LoadActorPreview(Menu_Status_selectedActor - 1);

        Menu_Status_delayScreenUpdate = 1;
        Menu_Status_animationStep = 160;
        Menu_Status_animateActorSwitchState = Menu_Status_ActorSwitchState_AnimateReverse;

        break;

    case Menu_Status_ActorSwitchState_AnimateReverse:
        if (Menu_Status_animationStep > 0) {
            Menu_Status_drawBackgroundFirst = 1;
            Menu_Status_animationStep -= 32;
            break;
        }

        Menu_Status_drawBackgroundFirst = 0;
        Menu_Status_animateActorSwitchState = Menu_Status_ActorSwitchState_Complete;
        return 1;

    case Menu_Status_ActorSwitchState_Complete:
        return 1;
    }

    return 0;
}

// decomp: MENU4.PRG _renderUnknownValue (src/MENU/MENU4.PRG/120.c:692)
void Menu_Status_DrawUnanalyzedValue(int xy, int withMax, u_long* before)
{
    int i;

    if (withMax != 0) {
        for (i = 0; i < 3; ++i) {
            Render_QueueSprite(128, xy, vs_getWH(6, 9), before)[4] =
                vs_getUV0Clut(228, 0, 832, 223);
            xy -= 5;
        }
        Render_DrawIntegerRightAligned(2, xy, 0, before);
        xy -= 7;
    }

    xy += -1 << 16;

    for (i = 0; i < 3; ++i) {
        Render_QueueSprite(128, xy, vs_getWH(7, 10), before)[4] =
            vs_getUV0Clut(234, 0, 832, 223);
        xy -= 6;
    }
}

// decomp: MENU4.PRG _renderStatBar (src/MENU/MENU4.PRG/120.c:721)
void Menu_Status_DrawVitalBar(int colorIndex, int current, int max, int xy)
{
    if (max == 0) {
        current = 0;
        max = 1;
    }

    if ((colorIndex >> 8) == 0) {
        current = max;
    }

    current <<= 6;

    Render_DrawStatGauge(
        colorIndex & 0xFF, ((current + max) - 1) / max, Render_frameScratch.orderingTable - 0xC, xy);
}

// decomp: MENU4.PRG _renderBasicStats (src/MENU/MENU4.PRG/120.c:741)
void Menu_Status_DrawVitals(void)
{
    int i;
    int y;
    Battle_ActorStats* actor = Battle_actors[Menu_Status_selectedActor - 1]->stats;
    u_long* insertBefore = Render_frameScratch.orderingTable - 0xC;

    switch (Menu_Status_renderBasicStatsState) {
    case 0:
        break;
    case 1:
        if (Menu_Status_basicStatsAnimState == 0) {
            if (Menu_Status_basicStatsAnimDelay == 0) {
                Battle_SetHudHidden(0);
                Menu_Status_renderBasicStatsState = 0;
            } else {
                --Menu_Status_basicStatsAnimDelay;
            }
        } else {
            --Menu_Status_basicStatsAnimState;
            actor = Battle_actors[Menu_Status_previousActor - 1]->stats;
        }
        break;

    case 2:
        if (Menu_Status_basicStatsAnimState != 0) {
            --Menu_Status_basicStatsAnimState;
            actor = Battle_actors[Menu_Status_previousActor - 1]->stats;
        } else if (Menu_Status_basicStatsAnimDelay == 0) {
            Menu_Status_renderBasicStatsState = 3;
        } else {
            --Menu_Status_basicStatsAnimDelay;
        }
        break;

    case 3:
        if (Menu_Status_basicStatsAnimState < 4) {
            ++Menu_Status_basicStatsAnimState;
        } else {
            Menu_Status_renderBasicStatsState = 0;
        }
        break;

    case 4:
        if (Battle_hudSlideOffset == 0x200000) {
            if (Menu_Status_basicStatsAnimDelay == 0) {
                Menu_Status_renderBasicStatsState = 3;
            } else {
                --Menu_Status_basicStatsAnimDelay;
            }
        }
        break;
    }

    if (Menu_Status_basicStatsAnimState == 0) {
        return;
    }

    y = ((Menu_Status_basicStatsAnimState * 8) - 22) << 16;

    if (actor->isAnalyzed) {

        i = Render_DrawIntegerRightAligned(0, y | 66, actor->maxHp, insertBefore);
        Render_DrawIntegerRightAligned(2, i, 0, insertBefore);
        Render_DrawIntegerRightAligned(1, i + 0xFFFEFFF9, actor->currentHp, insertBefore);

        i = Render_DrawIntegerRightAligned(0, y | 136, actor->maxMp, insertBefore);
        Render_DrawIntegerRightAligned(2, i, 0, insertBefore);
        Render_DrawIntegerRightAligned(1, i + vs_getXY(-7, -2), actor->currentMp, insertBefore);

        y += 15 << 16;

        Render_DrawIntegerRightAligned(1, y | 64, actor->risk, insertBefore);

        y += 1 << 16;
    } else {
        Menu_Status_DrawUnanalyzedValue(y | 66, 1, insertBefore);
        Menu_Status_DrawUnanalyzedValue(y | 136, 1, insertBefore);

        y += 16 << 16;

        Menu_Status_DrawUnanalyzedValue(y | 64, 0, insertBefore);
    }

    for (i = 0; i < 3; ++i) {
        Render_QueueSpriteWithTexturePage(384,
            Battle_hudGaugeLabelPositions[i] + ((Menu_Status_basicStatsAnimState - 4) << 0x13), Battle_hudGaugeLabelWidths[i] | 0x90000,
            insertBefore)[4] = Battle_hudGaugeLabelTexU[i] | 0x37F60000;
    }

    y += -8 << 16;
    i = actor->isAnalyzed << 8;

    Menu_Status_DrawVitalBar(i, actor->currentHp, actor->maxHp, y | 10);
    Menu_Status_DrawVitalBar(i | 1, actor->currentMp, actor->maxMp, y | 80);

    y += 4 << 16;

    Menu_Status_DrawVitalBar(i | 2, actor->risk, 100, y | 10);
}

// decomp: MENU4.PRG _drawStatusIndicator (src/MENU/MENU4.PRG/120.c:847)
void Menu_Status_DrawLimbConditionIndicator(int colorIndex, int xy, int subdued)
{
    u_int color;
    u_long* prim;
    u_long* limb_indicator_ordering_table;

    limb_indicator_ordering_table = Render_frameScratch.overlayOrderingTable;
    prim = Render_frameScratch.primitiveCursor;
    color = Battle_limbStatusColors[colorIndex] >> subdued;
    prim[0] = (*limb_indicator_ordering_table & 0xFFFFFF) | 0x06000000;
    prim[1] = color | (Render_LegacyPrimitiveCode_Tile << 24);
    prim[2] = xy;
    prim[3] = vs_getWH(7, 7);
    prim[4] = (color * 2) | (Render_LegacyPrimitiveCode_Tile << 24);
    prim[5] = xy + vs_getXY(1, 1);
    prim[6] = vs_getWH(5, 5);
    *limb_indicator_ordering_table = LegacyGpu_Handle(prim);
    Render_frameScratch.primitiveCursor = prim + 7;
}

// decomp: MENU4.PRG _drawLimbLeaderLine (src/MENU/MENU4.PRG/120.c:870)
void Menu_Status_DrawLimbLeaderLine(
    int limb, int xy, int arg2 __attribute__((unused)), int opaque)
{
    int limb_row_y;
    int packed_leader_elbow;
    u_long* prim;
    u_long* leader_line_ordering_table;
    int x;

    opaque = (0x687018 << opaque) | ((33 - opaque) << 25);
    limb_row_y = limb * 16 + 42;
    limb = ((xy >> 16) - limb_row_y);

    prim = Render_frameScratch.primitiveCursor;
    leader_line_ordering_table = Render_frameScratch.orderingTable;

    x = (short)xy;

    if (limb < 0) {
        limb = -limb;
    }

    limb = (limb * 3) >> 1;

    prim[0] = (leader_line_ordering_table[-1] & 0xFFFFFF) | 0x08000000;
    prim[1] = 0xE1000000;
    prim[2] = opaque;
    prim[3] = xy;
    packed_leader_elbow = ((x + ((limb * 2) / 3)) & 0xFFFF) | (limb_row_y << 0x10);
    prim[4] = packed_leader_elbow;
    prim[5] = (leader_line_ordering_table[-1] & 0xFFFFFF) | 0x03000000;
    prim[6] = opaque;
    prim[7] = packed_leader_elbow;
    prim[8] = (limb_row_y << 0x10) | 216;
    leader_line_ordering_table[-1] = (LegacyGpu_Handle(prim));
    prim += 9;
    Render_frameScratch.primitiveCursor = prim;
}

// decomp: MENU4.PRG _renderStatusIcons (src/MENU/MENU4.PRG/120.c:913)
void Menu_Status_DrawStatusEffectIcons(Battle_ActorStats* actor, int animationStep)
{
    int i;

    u_int flags = Battle_StatusIconFlags(actor);
    int statusCount = 0;

    for (i = 0; i < 32; ++i) {

        int column = statusCount & 7;

        if ((flags >> i) & 1) {
            int x;
            int y;
            int new_var = 16;
            int row = statusCount >> 3;
            ++statusCount;
            column *= 16;
            x = column - (animationStep - new_var);
            y = ((row * new_var) + 144) << new_var;

            if (i >= new_var) {
                // Element indicator
                Render_QueueSpriteWithTexturePage(
                    128, ((x + 8) & 0xFFFF) | y, vs_getWH(8, 8), Render_frameScratch.orderingTable - 8)[4] =
                    ((((i & 3) * 8) + 0x3068) | (getClut(1008, 223) << 16));
            }

            // Nasty match hack
            actor = (Battle_ActorStats*)Render_QueueSpriteWithTexturePage(
                128, (x & 0xFFFF) | y, vs_getWH(16, 16), Render_frameScratch.orderingTable - 8);

            // 0x0F0F906A = Flags for toggling the icon palette
            ((u_long*)actor)[4] =
                (Battle_statusIconTexOffsets[i]
                    | (((0x0F0F906A >> i) & 1) ? (getClut(912, 223) << 16)
                                               : (getClut(896, 223) << 16)));
            // 0x37F90000 : 0x37F80000
        }
    }
}

// decomp: MENU4.PRG _navigateStatusModifiers (src/MENU/MENU4.PRG/120.c:961)
int Menu_Status_NavigateStatusEffects(Battle_ActorStats* actor, int selection_active)
{
    static char Menu_Status_navigateStatusEffectsActiveStatusModifiers[32];
    static u_char Menu_Status_navigateStatusEffectsSelectedStatusModifier;
    static u_char Menu_Status_navigateStatusEffectsStatusModifierCount;
    static char Menu_Status_navigateStatusEffectsLayoutPadding[2] __attribute__((unused));
    static Battle_ActorStats* Menu_Status_navigateStatusEffectsStatusModifersCurrentTarget;

    int yPos;
    int xPos;

    if (actor != NULL) {

        xPos = Battle_StatusIconFlags(actor);

        if (xPos == 0) {
            return 1;
        }

        Menu_Status_navigateStatusEffectsStatusModifersCurrentTarget = actor;
        Menu_Status_navigateStatusEffectsStatusModifierCount = 0;

        for (yPos = 0; yPos < 32; ++yPos) {
            if ((xPos >> yPos) & 1) {
                Menu_Status_navigateStatusEffectsActiveStatusModifiers[Menu_Status_navigateStatusEffectsStatusModifierCount++] = yPos;
            }
        }

        if (selection_active == 0) {
            if (Menu_Status_navigateStatusEffectsSelectedStatusModifier >= Menu_Status_navigateStatusEffectsStatusModifierCount) {
                Menu_Status_navigateStatusEffectsSelectedStatusModifier = Menu_Status_navigateStatusEffectsStatusModifierCount - 1;
            }
        } else if (Menu_Status_selectedElement == 11) {
            Menu_Status_navigateStatusEffectsSelectedStatusModifier = 0;
        } else {
            if (Menu_Status_navigateStatusEffectsStatusModifierCount < 8) {
                Menu_Status_navigateStatusEffectsSelectedStatusModifier = Menu_Status_navigateStatusEffectsStatusModifierCount - 1;
            } else {
                Menu_Status_navigateStatusEffectsSelectedStatusModifier = 7;
            }
        }

    } else {

        selection_active = 1;

        if (Input_buttonsPressed.allButtons & PADRup) {
            return 2;
        }

        if (Input_buttonsPressed.allButtons & PADRdown) {
            return 3;
        }

        if (Input_buttonsState & PADRleft) {

            xPos = Input_MapStickDeadZone(Input_stickPosBuf.rStickX);
            yPos = Input_MapStickDeadZone(Input_stickPosBuf.rStickY);

            if (xPos == 0) {
                if (yPos == 0) {
                    if (Input_buttonsState & PADLup) {
                        yPos = -64;
                    }
                    if (Input_buttonsState & PADLdown) {
                        yPos = 64;
                    }
                    if (Input_buttonsState & PADLleft) {
                        xPos = -64;
                    }
                    if (Input_buttonsState & PADLright) {
                        xPos = 64;
                    }
                }
            }

            Menu_Status_xPos += xPos;
            Menu_Status_yPos -= yPos;

            if (Menu_Status_yPos < 0) {
                Menu_Status_yPos = 0;
            }

            if (Menu_Status_yPos > 896) {
                Menu_Status_yPos = 896;
            }

            return 0;
        }

        yPos = Menu_Status_navigateStatusEffectsSelectedStatusModifier;

        if (Input_buttonRepeat & PADLup) {
            if (yPos < 8) {
                Menu_PlayChangeSound();
                Menu_Status_selectedElement = 11;
                return 1;
            }
            yPos -= 8;
        }

        if (Input_buttonRepeat & PADLdown) {
            if ((Menu_Status_navigateStatusEffectsStatusModifierCount >= 8) && (yPos < 8)) {
                yPos += 8;
                if (yPos >= Menu_Status_navigateStatusEffectsStatusModifierCount) {
                    yPos = Menu_Status_navigateStatusEffectsStatusModifierCount - 1;
                }
            }
        }

        if (Input_buttonRepeat & PADLleft) {
            if (yPos & 7) {
                --yPos;
            }
        }

        if (Input_buttonRepeat & PADLright) {

            if (((yPos & 7) == 7) || (yPos == Menu_Status_navigateStatusEffectsStatusModifierCount - 1)) {
                Menu_PlayChangeSound();
                Menu_Status_selectedElement = 6;
                return 1;
            }

            if (++yPos >= Menu_Status_navigateStatusEffectsStatusModifierCount) {
                yPos = Menu_Status_navigateStatusEffectsStatusModifierCount - 1;
            }
        }

        if (yPos != Menu_Status_navigateStatusEffectsSelectedStatusModifier) {
            Menu_PlayChangeSound();
            Menu_Status_navigateStatusEffectsSelectedStatusModifier = yPos;
        }
    }

    Menu_Framework_SetInformationMessage((
        char*)&Menu_Status_statusStrings[Menu_Status_statusStrings[Menu_Status_navigateStatusEffectsActiveStatusModifiers[Menu_Status_navigateStatusEffectsSelectedStatusModifier]
                                             + Text_Menu4_status_Index_strDown]]);

    if (selection_active != 0) {
        Menu_Status_navigateStatusEffectsAnimationState = Menu_Framework_DrawCursor(
            Menu_Status_navigateStatusEffectsAnimationState, (((Menu_Status_navigateStatusEffectsSelectedStatusModifier & 7) * 16) + 2)
                                | ((((Menu_Status_navigateStatusEffectsSelectedStatusModifier >> 3) * 16) + 136) << 0x10));
        Menu_Framework_cursorColor = 1;
    }

    return 0;
}

// decomp: MENU4.PRG _getLimbCount (src/MENU/MENU4.PRG/120.c:1114)
int Menu_Status_LimbCount(int actor)
{
    int i;

    for (i = 0; i < 6; ++i) {
        if (Battle_actors[actor]->stats->limbs[i].nameIndex == 0) {
            break;
        }
    }

    return i;
}

// decomp: MENU4.PRG _renderLimbUi (src/MENU/MENU4.PRG/120.c:1146)
int Menu_Status_UpdateLimbPanel(int init)
{
    static char Menu_Status_limbPanelLayoutPadding[8] __attribute__((unused));

    int lineXy[2];
    Battle_ActorStats* actor;
    Battle_LimbStats* limb;
    int limbCount;
    int highlighted_limb_index;
    int highlight_dimming_shift;
    int panel_dimming_shift;
    int limbStatus;
    int i;
    int step;

    actor = Battle_actors[Menu_Status_limbPanelActor]->stats;
    limbCount = Menu_Status_LimbCount(Menu_Status_limbPanelActor);

    if (init != 0) {
        if (init > 0) {
            Menu_Framework_menuMemory.opaque51[49] = init - 1;
            return 0;
        }

        if (init == -3) {
            return Menu_Status_limbPanelState;
        }

        if (init == -2) {
            Menu_Status_limbPanelState = 3;
        } else {
            Menu_Status_limbPanelActor = Menu_Status_selectedActor - 1;
            Menu_Framework_SetActorComparisonStats(Menu_Status_limbPanelActor);
            for (i = 0; i < 6; ++i) {
                Menu_Status_limbPanelLimbAnimSteps[i] = 12 + i;
            }
            Menu_Status_limbPanelState = 1;
        }

        return 0;
    }

    switch (Menu_Status_limbPanelState) {
    case Menu_Status_LimbUiMode_None:
        break;

    case Menu_Status_LimbUiMode_Init:
        Menu_Status_DrawStatusEffectIcons(actor, Menu_Framework_rowAnimationSteps[Menu_Status_limbPanelLimbAnimSteps[0]]);

        for (i = 0; i < limbCount; ++i) {

            limb = &actor->limbs[i];
            step = Menu_Status_limbPanelLimbAnimSteps[i];

            Render_DrawCompactText(Battle_limbNames[limb->nameIndex],
                (Menu_Framework_rowAnimationSteps[step] + 216) | ((34 + i * 16) << 16), NULL);

            limbStatus = Battle_LimbCondition(limb);

            Menu_Status_DrawLimbConditionIndicator(limbStatus,
                (Menu_Framework_rowAnimationSteps[step] + 216) | ((36 + i * 16) << 16), 0);
            Render_DrawCompactText(Menu_Status_limbStatuses[limbStatus],
                (Menu_Framework_rowAnimationSteps[step] + 224) | ((34 + i * 16) << 16), 0);

            if (step < 8) {
                Actors_ProjectActorBone(1, limb->anchorBone, lineXy, 0);
                Menu_Status_DrawLimbLeaderLine(i, lineXy[0], 8 - step, 0);
            }

            limbStatus = 0;

            if (step != 0) {
                Menu_Status_limbPanelLimbAnimSteps[i] = step - 1;
            } else {
                limbStatus = 1;
            }
        }

        if (limbStatus != 0) {
            Menu_Status_limbPanelState = 2;
        }

        break;

    case 2:
        Menu_Status_DrawStatusEffectIcons(actor, 0);

        step = Menu_Framework_menuMemory.opaque51[49];

        for (i = 0; i < limbCount; ++i) {

            limb = &actor->limbs[i];
            panel_dimming_shift = step >> 7;
            highlighted_limb_index = step - 128;
            highlight_dimming_shift = panel_dimming_shift - 1;

            Render_DrawCompactTextColor(Battle_limbNames[limb->nameIndex],
                216 | ((34 + i * 16) << 16),
                i == highlighted_limb_index ? vs_getRGB888(128, 128, 128) >> highlight_dimming_shift
                          : vs_getRGB888(128, 128, 128) >> panel_dimming_shift,
                0);

            limbStatus = Battle_LimbCondition(limb);

            Menu_Status_DrawLimbConditionIndicator(
                limbStatus, (0x24 + i * 0x10) * 0x10000 | 0xD8, panel_dimming_shift - (i == highlighted_limb_index));
            Render_DrawCompactTextColor(Menu_Status_limbStatuses[limbStatus],
                224 | ((34 + i * 16) << 16),
                i == highlighted_limb_index ? vs_getRGB888(128, 128, 128) >> highlight_dimming_shift
                          : vs_getRGB888(128, 128, 128) >> panel_dimming_shift,
                0);

            Actors_ProjectActorBone(1, limb->anchorBone, lineXy, 0);

            if (!(step & 0x80) && ((Menu_Status_fadeScreen | Menu_Status_drawBackgroundFirst) == 0)) {
                Menu_Status_DrawLimbLeaderLine(i, lineXy[0], 8, i == step);
            }
        }
        break;

    case 3:
        Menu_Status_DrawStatusEffectIcons(actor, Menu_Status_limbPanelLimbAnimSteps[0] << 5);

        for (i = 0; i < limbCount; ++i) {
            if (Menu_Status_limbPanelLimbAnimSteps[i] < 8) {

                limb = &actor->limbs[i];
                ++Menu_Status_limbPanelLimbAnimSteps[i];

                Render_DrawCompactText(Battle_limbNames[limb->nameIndex],
                    ((Menu_Status_limbPanelLimbAnimSteps[i] << 5) + 216) | ((34 + i * 16) << 16), 0);

                limbStatus = Battle_LimbCondition(limb);

                Menu_Status_DrawLimbConditionIndicator(limbStatus,
                    ((Menu_Status_limbPanelLimbAnimSteps[i] << 5) + 216) | ((36 + i * 16) << 16), 0);
                Render_DrawCompactText(Menu_Status_limbStatuses[limbStatus],
                    ((Menu_Status_limbPanelLimbAnimSteps[i] << 5) + 224) | ((34 + i * 16) << 16), 0);
            } else {
                Menu_Status_limbPanelState = 0;
            }
        }
        break;
    }
    return 0;
}

// decomp: MENU4.PRG _fadeScreenLeft (src/MENU/MENU4.PRG/120.c:2081)
void Menu_Status_DrawModelSlideFade(void)
{
    int uv1Tpage;
    u_long* prim = Render_frameScratch.primitiveCursor;
    u_long* model_fade_ordering_table = Render_frameScratch.orderingTable;

    if (Menu_Status_fadeScreen == 0) {
        return;
    }

    prim[0] = (model_fade_ordering_table[-1] & 0xFFFFFF) | (13 << 24);
    prim[1] = vs_getTpage(0, 0, Render_TextureFormat_Palette4Bit, Render_BlendMode_Average, Render_Dithering_Off);
    prim[2] = vs_getRGB0(Render_LegacyPrimitiveCode_PolyGt4, 0, 0, 0);
    prim[3] = vs_getXY(0, 0);
    prim[4] = vs_getUV0Clut(0, 0, 0, 0);
    prim[5] = vs_getRGB888(128, 128, 128);
    prim[6] = vs_getXY(96, 0);

    uv1Tpage = vs_getUV1Tpage(96, 0, 0, 0, Render_TextureFormat_Direct16Bit, Render_BlendMode_Average);

    if (Render_frameBuf == 0) {
        uv1Tpage = vs_getUV1Tpage(96, 0, 320, 0, Render_TextureFormat_Direct16Bit, Render_BlendMode_Average);
    }

    prim[7] = uv1Tpage;
    prim[8] = vs_getRGB888(0, 0, 0);
    prim[9] = vs_getXY(0, 240);
    prim[10] = vs_getUV(0, 240);
    prim[11] = vs_getRGB888(128, 128, 128);
    prim[12] = vs_getXY(96, 240);
    prim[13] = vs_getUV(96, 240);

    model_fade_ordering_table[-1] = (LegacyGpu_Handle(prim));

    Render_frameScratch.primitiveCursor = prim + 14;
}

// decomp: MENU4.PRG _drawMenuBackground (src/MENU/MENU4.PRG/120.c:2121)
void Menu_Status_DrawBackground(void)
{
    int uv1Tpage;

    u_long* prim = Render_frameScratch.primitiveCursor;
    void* ordering_table_base = Render_frameScratch.orderingTable;
    u_int* background_ordering_table_entry = ordering_table_base + 0x1FF4;

    if (Menu_Status_drawBackgroundFirst != 0) {
        background_ordering_table_entry = ordering_table_base - 4;
    }

    prim[0] = (*background_ordering_table_entry & 0xFFFFFF) | (13 << 24);
    prim[1] = vs_getRGB0(Render_LegacyPrimitiveCode_PolyGt4SemiTrans, 16, 16, 16);
    prim[2] = vs_getXY(64, 0);
    prim[3] = vs_getUV0Clut(64, 0, 768, 227);
    prim[4] = vs_getRGB888(144, 144, 144);
    prim[5] = vs_getXY(160, 0);
    prim[6] = vs_getUV1Tpage(160, 0, 640, 256, Render_TextureFormat_Palette8Bit, Render_BlendMode_Add);
    prim[7] = vs_getRGB888(16, 16, 16);
    prim[8] = vs_getXY(64, 240);
    prim[9] = vs_getUV(64, 240);
    prim[10] = vs_getRGB888(144, 144, 144);
    prim[11] = vs_getXY(160, 240);
    prim[12] = vs_getUV(160, 240);
    prim[13] = vs_getTpage(0, 0, 0, 0, Render_Dithering_On);
    *background_ordering_table_entry = LegacyGpu_Handle(prim);

    prim += 14;
    prim[0] = (int)((*background_ordering_table_entry & 0xFFFFFF) | (8 << 24));
    prim[1] = vs_getTpage(704, 256, 1, 1, 0);
    prim[2] = vs_getRGB0(Render_LegacyPrimitiveCode_Sprite, 128, 128, 128);
    prim[3] = vs_getXY(160, 0);
    prim[4] = vs_getUV0Clut(32, 0, 768, 227);
    prim[5] = vs_getWH(96, 240);
    prim[6] = vs_getRGB0(Render_LegacyPrimitiveCode_Tile, 0, 0, 0);
    prim[7] = vs_getXY(256, 0);
    prim[8] = vs_getWH(64, 240);
    *background_ordering_table_entry = LegacyGpu_Handle(prim);

    prim += 9;
    prim[0] = (int)((*background_ordering_table_entry & 0xFFFFFF) | (13 << 24));
    prim[1] = vs_getTpage(0, 0, Render_TextureFormat_Palette4Bit, Render_BlendMode_Average, Render_Dithering_Off);
    prim[2] = vs_getRGB0(Render_LegacyPrimitiveCode_PolyGt4, 128, 128, 128);
    prim[3] = vs_getXY(64, 0);
    prim[4] = vs_getUV0Clut(64, 0, 0, 0);
    prim[5] = vs_getRGB888(0, 0, 0);
    prim[6] = vs_getXY(160, 0);
    uv1Tpage = vs_getUV1Tpage(160, 0, 0, 0, Render_TextureFormat_Direct16Bit, Render_BlendMode_Average);

    if (Render_frameBuf == 0) {
        uv1Tpage = vs_getUV1Tpage(160, 0, 320, 0, Render_TextureFormat_Direct16Bit, Render_BlendMode_Average);
    }

    prim[7] = uv1Tpage;
    prim[8] = vs_getRGB888(128, 128, 128);
    prim[9] = vs_getXY(64, 240);
    prim[10] = vs_getUV(64, 240);
    prim[11] = vs_getRGB888(0, 0, 0);
    prim[12] = vs_getXY(160, 240);
    prim[13] = vs_getUV(160, 240);
    *background_ordering_table_entry = LegacyGpu_Handle(prim);

    Render_frameScratch.primitiveCursor = prim + 14;
}

// decomp: MENU4.PRG _drawScreen (src/MENU/MENU4.PRG/120.c:2190)
void Menu_Status_DrawFrame(void)
{
    int camera_orbit_z_offset;
    int xOffset = (128 - Menu_Status_animationStep);
    int z = Actors_actorRecords[1]->menuCameraDistance;
    int x = Actors_actorRecords[1]->collisionHeight;
    Render_Camera* camera = &Render_frameScratch.camera;

    camera->lookAt.vx = ((-Core_Gte_Sin(Menu_Status_xPos) * xOffset) >> 8) * z;
    camera->lookAt.vy = -(x * ONE / 2);
    camera->lookAt.vz = ((Core_Gte_Cos(Menu_Status_xPos) * xOffset) >> 8) * z;
    camera->position.vx = camera->lookAt.vx + ((Core_Gte_Cos(Menu_Status_xPos) * Core_Gte_Cos(Menu_Status_yPos)) >> 0xA) * z;
    camera->position.vy = camera->lookAt.vy - Core_Gte_Sin(Menu_Status_yPos) * z * 4;

    camera_orbit_z_offset = (Core_Gte_Sin(Menu_Status_xPos) * Core_Gte_Cos(Menu_Status_yPos) >> 0xA) * z;
    camera->angles.vx = 0;
    camera->angles.vy = 0;
    camera->angles.vz = 0;
    camera->farClip = ONE;
    camera->position.vz = camera->lookAt.vz + camera_orbit_z_offset;

    Render_BuildCameraViewMatrix();

    if (Menu_Status_delayScreenUpdate) {
        --Menu_Status_delayScreenUpdate;
    } else {
        Menu_Framework_DrawActorPreview(&Render_frameScratch.viewMatrix);
    }

    Menu_Status_DrawBackground();
    Menu_Status_DrawModelSlideFade();
    Menu_Status_UpdateLimbPanel(0);
    Menu_Status_DrawVitals();
}

// decomp: MENU4.PRG _renderSelectedLimbCondition (src/MENU/MENU4.PRG/120.c:2228)
void Menu_Status_ShowSelectedLimbCondition(void)
{
    Battle_LimbStats* limb =
        &Battle_actors[Menu_Status_selectedActor - 1]->stats->limbs[Menu_Status_selectedElement];
    Battle_stringContext.strings[0] =
        (char*)&Battle_statusStrings[Battle_statusStrings[limb->nameIndex]];
    Battle_stringContext.strings[1] =
        (char*)&Menu_Status_statusStrings[Menu_Status_statusStrings[Battle_LimbCondition(limb)
                                              + Text_Menu4_status_Index_critical]];
    Render_FormatEncodedText(
        Menu_Framework_stringBuf, (char*)&Menu_Status_statusStrings[Text_Menu4_status_Offset_values]);
    Menu_Framework_SetInformationMessage(Menu_Framework_stringBuf);
    Menu_Status_UpdateLimbPanel(Menu_Status_selectedElement + 1);
}

// decomp: MENU4.PRG _renderSelectedLimbStats (src/MENU/MENU4.PRG/120.c:2246)
void Menu_Status_PopulateSelectedLimbDefense(void)
{
    int i;
    Battle_LimbStats* limb =
        &Battle_actors[Menu_Status_selectedActor - 1]->stats->limbs[Menu_Status_selectedElement];

    for (i = 0; i < 16; ++i) {
        Menu_Framework_equipmentStats[i + 16] = limb->affinities[i & 7];
    }
    for (i = 0; i < 4; ++i) {
        Menu_Framework_equipmentStats[i + 32] = limb->types[i];
    }

    Menu_Status_UpdateLimbPanel(Menu_Status_selectedElement + 0x81);
}

// decomp: MENU4.PRG vs_menu4_exec (src/MENU/MENU4.PRG/120.c:2270)
int Menu_Status_Update(u_char* state)
{
    enum Menu_Status_StatusMenuState {
        Menu_Status_StatusMenuState_None,
        Menu_Status_StatusMenuState_Init = 3,
        Menu_Status_StatusMenuState_WaitForMenu,
        Menu_Status_StatusMenuState_ViewEquipment = 7,
        Menu_Status_StatusMenuState_WaitQuitToMenu = 11,
        Menu_Status_StatusMenuState_QuitToMenu,
        Menu_Status_StatusMenuState_WaitQuitToBattle,
        Menu_Status_StatusMenuState_QuitToBattle
    };

    static int Menu_Status_kLimbCursorPositions[] = { 0x001400A0, 0x002400A0, 0x003400A0, 0x004400A0,
        0x005400A0, 0x006400A0, 0x00920098, 0x009C0098, 0x00A60098, 0x0004FFFC,
        0x00040042, 0x000CFFFC, 0x000C0042 };

    static int Menu_Status_kLimbNavigation[] = { 0x000A010A, 0x010A0200, 0x020A0301, 0x030A0402,
        0x040A0503, 0x050A0604, 0x060D0705, 0x070D0806, 0x080D0807, 0x0A090B09,
        0x0009000A, 0x000B0D09 };

    static int Menu_Status_savedProjectionDistance;
    static char Menu_Status_updateLayoutPadding[12] __attribute__((unused));
    static MATRIX Menu_Status_savedViewMatrix;
    static Render_Camera Menu_Status_savedCamera;
    static u_char Menu_Status_updateAnimationIndex;

    int limbs;
    int actor_details_visible;
    int limb_status_available;
    int userInput;

    actor_details_visible = 0;
    limb_status_available = 0;

    switch (*state) {
    case Menu_Status_StatusMenuState_Init:
        Menu_Status_updateScreenEnabled = 0;
        Menu_Status_updateAnimationIndex = 10;

        if (Menu_Framework_itemNames == NULL) {
            Menu_Framework_LoadItemText(1);
        }

        if (Menu_Framework_AreMenuRowsIdle() == 0) {
            break;
        }

        Battle_hudStatusIconsHidden = 1;

        Field_SetFieldRenderingEnabled(0);
        Menu_Framework_SetBackgroundPriority(2046, 128);

        Menu_Status_drawBackgroundFirst = 0;
        Menu_Status_fadeScreen = 0;
        Menu_Status_selectedActor = 0;
        Menu_Status_previousActor = 0;
        Menu_Status_renderBasicStatsState = 0;
        Menu_Status_basicStatsAnimState = 0;
        Menu_Status_initializedActor = 0;

        Menu_Status_AnimateActorSwitch(1);

        Menu_Status_updateAnimWait = 1;
        Menu_Status_savedProjectionDistance = Render_projectionDistance;

        Render_SetCameraProjectionDistance(0x200);

        {

            Menu_Status_savedViewMatrix = Render_frameScratch.viewMatrix;
            Menu_Status_savedCamera = Render_frameScratch.camera;
            Render_frameScratch.camera.farClip = ONE;
            Render_frameScratch.camera.angles.vx = 0;
            Render_frameScratch.camera.angles.vy = 0;
            Render_frameScratch.camera.angles.vz = 0;
            Menu_Status_xPos = ONE / 16 * 11;
            Menu_Status_yPos = 0;

            Menu_Framework_InitializeInformationBox();

            Menu_Status_selectedElement = 0;
            Menu_Framework_enabledStatPages = 0;

            Menu_Status_ShowSelectedLimbCondition();

            *state = 5;
        }
        break;

    case Menu_Status_StatusMenuState_WaitForMenu:
        if (Menu_Status_updateAnimWait != 0) {

            --Menu_Status_updateAnimWait;

            Menu_Status_UpdateLimbPanel(-1);
            Menu_Framework_UpdatePrimaryStatPanel(1);

        } else {

            Menu_Status_ShowSelectedLimbCondition();

            *state = 5;
        }
        break;

    case 5:
        actor_details_visible = 0;

        if ((Menu_Status_selectedActor == 1)
            || Battle_actors[Menu_Status_selectedActor - 1]->stats->isAnalyzed) {
            actor_details_visible = 1;
        }

        if (Menu_Status_updateAnimWait != 0) {
            int preview_ready = Menu_Status_AnimateActorSwitch(0);
            Menu_Status_updateScreenEnabled = 1;
            if (preview_ready) {

                Menu_Status_updateAnimWait = 0;

                Menu_Status_UpdateLimbPanel(-1);
                Menu_Framework_UpdatePrimaryStatPanel(1);

                userInput = Menu_Status_selectedElement;

                if (userInput < Text_Menu4_status_Index_statDesc) {
                    limbs = Menu_Status_LimbCount(Menu_Status_selectedActor - 1) - 1;
                    if (limbs < userInput) {
                        Menu_Status_selectedElement = limbs;
                    }

                    Menu_Status_ShowSelectedLimbCondition();
                } else {
                    Menu_Framework_SetInformationMessage((char*)&Menu_Status_statusStrings
                            [Menu_Status_statusStrings[userInput - Text_Menu4_status_Index_statDesc]]);
                }
            }
        } else if (Menu_Framework_LoadItemText(0) != 0) {
            if (Menu_Status_UpdateLimbPanel(-3) == 2) {
                if (Input_buttonsPressed.allButtons & PADRup) {

                    Menu_PlayLeaveSound();

                    *state = Menu_Status_StatusMenuState_WaitQuitToBattle;
                    break;

                } else if (Input_buttonsPressed.allButtons & PADRdown) {

                    Menu_PlayLeaveSound();

                    *state = Menu_Status_StatusMenuState_WaitQuitToMenu;
                    break;

                } else if ((Input_buttonsPressed.allButtons & PADRright)
                           && ((Menu_Status_selectedElement < Menu_Status_LimbCount(Menu_Status_selectedActor - 1))
                               != 0)) {

                    Menu_PlaySelectSound();
                    Menu_Framework_UpdatePrimaryStatPanel(2);
                    Menu_Status_UpdateLimbPanel(-2);

                    Menu_Status_updateAnimWait = 8;
                    *state = Menu_Status_StatusMenuState_ViewEquipment;
                    break;
                }

                limb_status_available = 0;

                if (actor_details_visible != 0) {
                    int selected_element_index = Menu_Status_selectedElement;
                    limb_status_available = selected_element_index < Menu_Status_LimbCount(Menu_Status_selectedActor - 1);
                }

                if (Input_buttonsPressed.allButtons & PADRleft) {
                    if (limb_status_available != 0) {

                        Menu_PlaySelectSound();
                        Menu_Status_PopulateSelectedLimbDefense();
                        Menu_Framework_UpdateEquipmentStatPages(6);

                        Menu_Status_selectedStatRow = 0;
                        Menu_Status_updateAnimWait = 10;
                        *state = 9;
                        break;
                    }
                    Menu_PlayInvalidSound();
                }

                if (Input_buttonsState & PADL2) {

                    userInput = Input_MapStickDeadZone(Input_stickPosBuf.rStickX);
                    limbs = Input_MapStickDeadZone(Input_stickPosBuf.rStickY);

                    if (userInput == 0) {
                        if (limbs == 0) {
                            if (Input_buttonsState & PADLup) {
                                limbs = -64;
                            }
                            if (Input_buttonsState & PADLdown) {
                                limbs = 64;
                            }
                            if (Input_buttonsState & PADLleft) {
                                userInput = -64;
                            }
                            if (Input_buttonsState & PADLright) {
                                userInput = 64;
                            }
                        }
                    }

                    Menu_Status_xPos += userInput;
                    Menu_Status_yPos -= limbs;

                    if (Menu_Status_yPos < 0) {
                        Menu_Status_yPos = 0;
                    }
                    if (Menu_Status_yPos > 896) {
                        Menu_Status_yPos = 896;
                    }
                } else {

                    userInput = Menu_Status_selectedElement;
                    limbs = Menu_Status_kLimbNavigation[userInput];

                    if (Input_buttonRepeat & PADLup) {
                        userInput = limbs;
                    } else if (Input_buttonRepeat & PADLdown) {
                        userInput = limbs >> 8;
                    } else if (Input_buttonRepeat & PADLleft) {
                        userInput = limbs >> 16;
                    } else {
                        userInput = Menu_Status_selectedElement;
                        if (Input_buttonRepeat & PADLright) {
                            userInput = limbs >> 24;
                        }
                    }

                    userInput &= 0xFF;

                    Menu_Status_UpdateLimbPanel(7);

                    if (userInput == 13) {
                        if (Menu_Status_NavigateStatusEffects(
                                Battle_actors[Menu_Status_selectedActor - 1]->stats, 1)
                            == 0) {
                            Menu_PlayChangeSound();
                            *state = 6;
                            break;
                        }
                        userInput = Menu_Status_selectedElement;
                    }

                    if (userInput < 6) {

                        limbs = Menu_Status_LimbCount(Menu_Status_selectedActor - 1);

                        if (userInput < limbs) {
                            Menu_Status_UpdateLimbPanel(userInput + 1);
                        } else {
                            userInput = 6;
                            if (!(Input_buttonRepeat & PADLdown)) {
                                userInput = limbs - 1;
                                if (!(Input_buttonRepeat & PADLup)) {
                                    Menu_Status_selectedElement = userInput;
                                }
                            }
                        }
                    }

                    Menu_Framework_cursorColor =
                        (userInput < Menu_Status_LimbCount(Menu_Status_selectedActor - 1)) ^ 1;
                    Menu_Status_limbCursorAnimation =
                        Menu_Framework_DrawCursor(Menu_Status_limbCursorAnimation, Menu_Status_kLimbCursorPositions[userInput]);

                    if (userInput != Menu_Status_selectedElement) {

                        Menu_PlayChangeSound();

                        if (userInput != 13) {
                            Menu_Status_selectedElement = userInput;
                        }
                    }

                    if (userInput < 6) {
                        Menu_Status_ShowSelectedLimbCondition();
                    } else {
                        Menu_Framework_SetInformationMessage((char*)&Menu_Status_statusStrings
                                [Menu_Status_statusStrings[userInput - Text_Menu4_status_Index_statDesc]]);
                    }
                }
            }
            userInput = Menu_Status_selectedActor - 1;

            if (Input_buttonsPressed.allButtons & PADR1) {
                userInput = Menu_Status_FindNextInspectableActor(userInput, 1);
                limbs = 17;
            }

            if (Input_buttonsPressed.allButtons & PADL1) {
                userInput = Menu_Status_FindNextInspectableActor(userInput, 14);
                limbs = 1;
            }

            if (userInput != (Menu_Status_selectedActor - 1)) {

                Menu_Status_previousActor = Menu_Status_selectedActor;

                Menu_Status_UpdateLimbPanel(-2);
                Menu_Framework_UpdatePrimaryStatPanel(2);
                Menu_Status_BeginVitalsTransition(userInput);
                Menu_Status_AnimateActorSwitch(userInput + limbs);

                Menu_Status_updateAnimWait = 1;
            }
        }

        Menu_Framework_cursorColor =
            (Menu_Status_selectedElement < Menu_Status_LimbCount(Menu_Status_selectedActor - 1)) ^ 1;
        break;

    case 6:
        actor_details_visible = 0;

        if ((Menu_Status_selectedActor == 1)
            || Battle_actors[Menu_Status_selectedActor - 1]->stats->isAnalyzed) {
            actor_details_visible = 1;
        }

        if (Menu_Status_updateAnimWait != 0) {
            int preview_ready = Menu_Status_AnimateActorSwitch(0);
            Menu_Status_updateScreenEnabled = 1;
            if (preview_ready) {

                Menu_Status_NavigateStatusEffects(Battle_actors[Menu_Status_selectedActor - 1]->stats, 0);

                Menu_Status_updateAnimWait = 0;

                Menu_Status_UpdateLimbPanel(-1);
                Menu_Framework_UpdatePrimaryStatPanel(1);
            }
        } else {
            if (Menu_Status_UpdateLimbPanel(-3) == 2) {

                userInput = Menu_Status_NavigateStatusEffects(0, 1);

                if (userInput != 0) {
                    switch (userInput) {
                    case 1:
                        *state = 5;
                        break;
                    case 2:
                        Menu_PlayLeaveSound();
                        *state = 13;
                        break;
                    case 3:
                        Menu_PlayLeaveSound();
                        *state = 11;
                        break;
                    }
                }
            }

            userInput = Menu_Status_selectedActor - 1;

            if (Input_buttonsPressed.allButtons & PADR1) {
                userInput = Menu_Status_FindNextInspectableActor(userInput, 1);
                limbs = 17;
            }

            if (Input_buttonsPressed.allButtons & PADL1) {
                userInput = Menu_Status_FindNextInspectableActor(userInput, 14);
                limbs = 1;
            }

            if (userInput != (Menu_Status_selectedActor - 1)) {

                Menu_Status_previousActor = Menu_Status_selectedActor;

                Menu_Status_UpdateLimbPanel(-2);
                Menu_Framework_UpdatePrimaryStatPanel(2);
                Menu_Status_BeginVitalsTransition(userInput);
                Menu_Status_AnimateActorSwitch(userInput + limbs);

                Menu_Status_updateAnimWait = 1;

                if (Battle_StatusIconFlags(Battle_actors[userInput]->stats) == 0) {
                    *state = 5;
                }
            }
        }
        break;

    case Menu_Status_StatusMenuState_ViewEquipment:
        if (Menu_Status_updateAnimWait == 0) {
            Menu_Status_UpdateEquipmentList(Menu_Status_selectedElement + 1);
            *state = 8;
        } else {
            --Menu_Status_updateAnimWait;
        }
        break;

    case 8:
        userInput = Menu_Status_UpdateEquipmentList(0);

        if (userInput != 0) {
            if (userInput != -2) {
                Menu_Status_updateAnimWait = 8;
                *state = Menu_Status_StatusMenuState_WaitForMenu;
            } else {
                *state = 13;
            }
        }
        break;

    case 9:
        if (Menu_Status_updateAnimWait != 0) {
            Menu_Status_animationStep = 128 - Menu_Framework_rowAnimationSteps[--Menu_Status_updateAnimWait];
        } else if (Input_buttonsPressed.allButtons & (PADRup | PADRdown)) {

            Menu_PlayLeaveSound();
            Menu_Framework_UpdateEquipmentStatPages(-1);

            if (Input_buttonsPressed.allButtons & PADRup) {
                *state = Menu_Status_StatusMenuState_WaitQuitToBattle;
                break;
            }

            Menu_Status_updateAnimWait = 10;
            *state = 10;
        } else {
            if ((Input_buttonsState & (PADL1 | PADR1)) != (PADL1 | PADR1)) {

                userInput = Menu_Status_selectedElement;
                limbs = Menu_Status_LimbCount(Menu_Status_selectedActor - 1);

                if (Input_buttonRepeat & PADL1) {
                    char new_var = 1;
                    userInput = (userInput - new_var) + limbs;
                }

                if (Input_buttonRepeat & PADR1) {
                    ++userInput;
                }

                if (userInput >= limbs) {
                    userInput -= limbs;
                }

                if (userInput != Menu_Status_selectedElement) {

                    Menu_PlayChangeSound();

                    Menu_Status_selectedElement = userInput;

                    Menu_Status_PopulateSelectedLimbDefense();
                }
            }

            userInput = Menu_Status_selectedStatRow;
            limbs = 11 - Menu_Framework_itemStatPage * 4;

            if (userInput >= limbs) {
                userInput = limbs - 1;
            }

            if (Input_buttonRepeat & PADLup) {
                char new_var = 1;
                userInput = (userInput - new_var) + limbs;
            }

            if (Input_buttonRepeat & PADLdown) {
                ++userInput;
            }

            if (userInput >= limbs) {
                userInput -= limbs;
            }

            if (userInput != Menu_Status_selectedStatRow) {
                Menu_PlayChangeSound();
                Menu_Status_selectedStatRow = userInput;
            }

            Menu_Framework_SetInformationMessage((char*)&Menu_Status_statusStrings
                    [Menu_Status_statusStrings[userInput
                                    + (limbs != 3 ? Text_Menu4_status_Index_physicalDefense
                                                  : Text_Menu4_status_Index_bluntDefense)]]);

            Menu_Status_limbCursorAnimation =
                Menu_Framework_DrawCursor(Menu_Status_limbCursorAnimation, ((userInput * 16) + 50) << 16);
        }
        break;

    case 10:
        actor_details_visible = 1;

        if (Menu_Status_updateAnimWait != 0) {
            Menu_Status_animationStep = Menu_Framework_rowAnimationSteps[--Menu_Status_updateAnimWait];
        } else {
            Menu_Status_UpdateLimbPanel(Menu_Status_selectedElement + 1);
            *state = 5;
        }
        break;

    case Menu_Status_StatusMenuState_WaitQuitToMenu:
    case Menu_Status_StatusMenuState_WaitQuitToBattle:
        Menu_Framework_UpdatePrimaryStatPanel(2);
        Menu_Status_UpdateLimbPanel(-2);
        Menu_Status_UpdateLimbPanel(0);
        Menu_Status_BeginVitalsTransition(0);
        Menu_Status_DrawVitals();
        Render_SetCameraProjectionDistance(Menu_Status_savedProjectionDistance);

        Render_frameScratch.viewMatrix = Menu_Status_savedViewMatrix;
        Render_frameScratch.camera = Menu_Status_savedCamera;

        Field_SetFieldRenderingEnabled(1);
        Menu_Framework_SetCommandLabel(Menu_Framework_MenuCommand_None);
        Menu_Framework_ClearMenuExcept(Menu_Framework_MenuItemId_None);
        Menu_Framework_ReleaseActorPreview();
        Menu_Framework_SetBackgroundPriority(-2, 128);

        Battle_hudStatusIconsHidden = 0;
        Menu_Status_updateAnimWait = 8;
        Menu_Status_updateScreenEnabled = 0;
        ++*state;
        break;

    case Menu_Status_StatusMenuState_QuitToMenu:
        Menu_Status_UpdateLimbPanel(0);
        Menu_Status_DrawVitals();
        Menu_Framework_SetBackgroundFade(0);
        Menu_Framework_DismissInformationBox();

        if (Menu_Status_updateAnimWait <= 0) {
            if (Menu_Framework_UnloadItemTextWhenReady() != 0) {
                *state = Menu_Status_StatusMenuState_None;
                return 1;
            }
        }

        --Menu_Status_updateAnimWait;
        break;

    case Menu_Status_StatusMenuState_QuitToBattle:
        Menu_Status_UpdateLimbPanel(0);
        Menu_Status_DrawVitals();
        Menu_Framework_SetBackgroundFade(0);
        Menu_Framework_DismissInformationBox();

        if (Menu_Status_updateAnimWait <= 0) {
            if (Menu_Framework_UnloadItemTextWhenReady() != 0) {
                Menu_Framework_menuState.currentMenu = 5;
                *state = Menu_Status_StatusMenuState_None;
                return 1;
            }
        }

        --Menu_Status_updateAnimWait;
        break;
    }

    if (Menu_Status_updateScreenEnabled != 0) {
        Menu_Status_DrawFrame();
    }

    if (actor_details_visible != 0) {
        if (Menu_Status_updateAnimationIndex != 0) {
            --Menu_Status_updateAnimationIndex;
        }
    } else {
        Menu_Status_updateAnimationIndex = Menu_Framework_animationIndices[Menu_Status_updateAnimationIndex];
    }

    if (Menu_Status_updateAnimationIndex < 10) {

        userInput = Menu_Framework_rowAnimationSteps[Menu_Status_updateAnimationIndex];

        Menu_Framework_DrawButtonPromptBackground(16 - userInput, 38, 88, 10);
        Menu_Framework_DrawButtonIcon(Menu_Framework_ButtonId_Square, 8 - userInput, 36, 0);
        Render_DrawCompactTextColor(
            "STATUS", ((0x1C - userInput) & 0xFFFF) | 0x260000, 0x404040 << limb_status_available, 0);
    }

    return 0;
}

// MENU4.PRG reloads restore these statics' initial values (PORTING_WATCHLIST.md §1).
void Menu_Status_ActorStatus_ResetMenu4Statics(void)
{
    {
        static const int initial = 0;
        memcpy((void*)&Menu_Status_animationStep, &initial, sizeof Menu_Status_animationStep);
    }
    {
        static const char initial = 0;
        memcpy((void*)&Menu_Status_basicStatsAnimDelay, &initial, sizeof Menu_Status_basicStatsAnimDelay);
    }
    {
        static const char initial = 0;
        memcpy((void*)&Menu_Status_basicStatsAnimState, &initial, sizeof Menu_Status_basicStatsAnimState);
    }
    {
        static const char initial = 0;
        memcpy((void*)&Menu_Status_delayScreenUpdate, &initial, sizeof Menu_Status_delayScreenUpdate);
    }
    {
        static const int initial = 0;
        memcpy((void*)&Menu_Status_drawBackgroundFirst, &initial, sizeof Menu_Status_drawBackgroundFirst);
    }
    {
        static const int initial = 0;
        memcpy((void*)&Menu_Status_fadeScreen, &initial, sizeof Menu_Status_fadeScreen);
    }
    {
        static const char initial = 0;
        memcpy((void*)&Menu_Status_initializedActor, &initial, sizeof Menu_Status_initializedActor);
    }
    {
        static const char* initial[] = { "DYING", "BAD", "AVERAGE", "GOOD", "EXCELLENT", NULL };
        memcpy((void*)&Menu_Status_limbStatuses, &initial, sizeof Menu_Status_limbStatuses);
    }
    {
        static const int initial = 0;
        memcpy((void*)&Menu_Status_previousActor, &initial, sizeof Menu_Status_previousActor);
    }
    {
        static const char initial = 0;
        memcpy((void*)&Menu_Status_renderBasicStatsState, &initial, sizeof Menu_Status_renderBasicStatsState);
    }
    {
        static const int initial = 0;
        memcpy((void*)&Menu_Status_selectedActor, &initial, sizeof Menu_Status_selectedActor);
    }
    {
        static const int initial = 0;
        memcpy((void*)&Menu_Status_xPos, &initial, sizeof Menu_Status_xPos);
    }
    {
        static const int initial = 0;
        memcpy((void*)&Menu_Status_yPos, &initial, sizeof Menu_Status_yPos);
    }
    {
        static const char initial = 0;
        memcpy((void*)&Menu_Status_navigateStatusEffectsAnimationState, &initial, sizeof Menu_Status_navigateStatusEffectsAnimationState);
    }
    {
        static const int initial[6];
        memcpy((void*)&Menu_Status_limbPanelLimbAnimSteps, &initial, sizeof Menu_Status_limbPanelLimbAnimSteps);
    }
    {
        static const char initial = 0;
        memcpy((void*)&Menu_Status_limbPanelState, &initial, sizeof Menu_Status_limbPanelState);
    }
    {
        static const u_char initial = 0;
        memcpy((void*)&Menu_Status_limbPanelActor, &initial, sizeof Menu_Status_limbPanelActor);
    }
    {
        static const int initial = 0;
        memcpy((void*)&Menu_Status_limbCursorAnimation, &initial, sizeof Menu_Status_limbCursorAnimation);
    }
    {
        static const int initial = 0;
        memcpy((void*)&Menu_Status_updateAnimWait, &initial, sizeof Menu_Status_updateAnimWait);
    }
    {
        static const int initial = 0;
        memcpy((void*)&Menu_Status_updateScreenEnabled, &initial, sizeof Menu_Status_updateScreenEnabled);
    }
}

