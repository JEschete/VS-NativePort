// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Battle/HudOrchestrationLive.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Core/RandomLive.hpp"
#include "Effects/ScreenDistortion/ScreenOverlaysLive.hpp"
#include "Menu/TextBoxFramesLive.hpp"
#include "Menu/TextBoxStateLive.hpp"
#include "Render/EncodedTextLive.hpp"
#include "Render/GameFontLive.hpp"
extern "C" {
#include "Legacy/Psx.h"
#include "Battle/Action.h"
#include "Battle/ActionTable.h"
#include "Battle/Hud.h"
#include "Battle/TargetingState.h"
#include "Battle/TextBoxes.h"
#include "Content/StringTables.h"
#include "Menu/Framework/FrameworkLiveState.h"
#include "Menu/Framework/Menus.h"
#include "Menu/Framework/TextBoxes.h"
#include "Menu/TextBox.h"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"
#include "State/Timers.h"

// decomp: D_800EB9D8 (D_800EB9D8@BATTLE/BATTLE.PRG)
int* Menu_sceneNotificationBuffer; /* data: contents loaded in Step 5 */

// decomp: D_800EBF58 (D_800EBF58@BATTLE/BATTLE.PRG)
char Menu_kNotificationItemNames[65][12]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800EC2D8 (D_800EC2D8@BATTLE/BATTLE.PRG)
int Menu_kInformationHeaderSpriteWords[5477]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800F4E68 (D_800F4E68@BATTLE/BATTLE.PRG)
char Menu_sceneNotificationTimer;

// decomp: D_800F4E69 (D_800F4E69@BATTLE/BATTLE.PRG)
char Menu_sceneNotificationTimed;

// decomp: textBoxSelector (textBoxSelector@BATTLE/BATTLE.PRG)
Menu_Framework_TextBoxSelector Menu_textBoxSelector;

// decomp: vs_battle_textBoxes (vs_battle_textBoxes@BATTLE/BATTLE.PRG)
Menu_TextBoxState Menu_textBoxes[8];

// decomp: D_800F51B8 (D_800F51B8@BATTLE/BATTLE.PRG)
u_long* Menu_activeTextBoxOrderingTable;

// decomp: BATTLE.PRG vs_battle_displaySceneMessage (src/BATTLE/BATTLE.PRG/5BF94.c:3771)
void Menu_ShowSceneNotification(int notification_kind, int notification_subject_id, int timed_notification)
{
    Menu_sceneNotificationBuffer = (int *)(VS::Core::Memory::AllocateLiveFromHighEnd(112));
    Menu_sceneNotificationTimed = timed_notification;
    if (Menu_sceneNotificationTimed) {
        Menu_sceneNotificationTimer = 0x78;
    }

    VS::Menu::InitLiveTextBox(7, 0x304, 0, 1, 0, 3, 0, 0);

    Menu_sceneNotificationBuffer[0] = 0x00F8 | (0x04FB << 16);
    Battle_stringContext.strings[0] = Battle_actionDefinitions[notification_subject_id].name;

    if (notification_kind == 12) {
        VS::Render::FormatLiveEncodedText((char*)&Menu_sceneNotificationBuffer[1],
            (char*)&Menu_menuStrings[Text_Battle_menuStrings_Offset_spellMastered]);
    } else if (notification_kind == 11) {
        Menu_textBoxes[7].flags.frameShape = 0;
        VS::Render::FormatLiveEncodedText((char*)&Menu_sceneNotificationBuffer[1],
            (char*)&Menu_menuStrings[Menu_menuStrings[Battle_actionDefinitions[notification_subject_id].type
                                                                + 22]]);
    } else {
        if (notification_subject_id != 0) {
            Battle_stringContext.strings[0] = Menu_kNotificationItemNames[(notification_subject_id - 0x1CA) * 2];
        }
        VS::Render::FormatLiveEncodedText((char*)Menu_sceneNotificationBuffer, (char*)&Battle_doorStrings[Battle_doorStrings[notification_kind]]);
    }

    VS::Menu::SetLiveTextBoxText(7, (char*)Menu_sceneNotificationBuffer);
}

// decomp: BATTLE.PRG func_800CB45C (src/BATTLE/BATTLE.PRG/5BF94.c:3802)
int Menu_IsSceneNotificationPending(void)
{
    if (Menu_sceneNotificationBuffer != NULL) {

        if (!Menu_textBoxes[7].flags.done) {
            return 1;
        }

        if (Menu_sceneNotificationTimed != 0) {

            if (Menu_sceneNotificationTimer < 4) {
                Menu_sceneNotificationTimer = 0;
            } else {
                Menu_sceneNotificationTimer -= State_tickSpeed;
            }

            if (Menu_sceneNotificationTimer != 0) {
                return -VS::Menu::SetLiveTextBoxText(7, NULL);
            }

            return 0;
        }
        return -VS::Menu::SetLiveTextBoxText(7, NULL);
    }
    return 0;
}

// decomp: BATTLE.PRG func_800CB50C (src/BATTLE/BATTLE.PRG/5BF94.c:3829)
void Menu_ReleaseSceneNotification(void)
{
    if (Menu_sceneNotificationBuffer != NULL) {
        VS::Menu::DismissLiveTextBox(7);
        VS::Core::Memory::FreeLiveFromHighEnd(Menu_sceneNotificationBuffer);
        Menu_sceneNotificationBuffer = NULL;
    }
}

// decomp: BATTLE.PRG func_800CB83C (src/BATTLE/BATTLE.PRG/5BF94.c:3936)
void Menu_UpdateTextBoxes(void)
{
    int id;

    for (id = 7; id != -1; --id) {
        Menu_TextBoxState* box = &Menu_textBoxes[id];
        int state = box->state;
        int duration;
        int scale;

        if (state == 0) {
            continue;
        }

        if (box->flags.opaque3Bit7) {
            continue;
        }

        duration = box->openDuration;
        scale = State_tickSpeed >> 1;

        if (state < 0) {
            state -= scale;

            if (state < -duration) {
                box->state = 0;
                continue;
            }

            scale = ((duration + state + 1) << 16) / duration;
        } else {
            state += scale;

            if (duration < state) {
                state = duration + 1;
            }

            scale = state - 1;

            switch (box->flags.openStyle) {
            case 0:
            case 1:
                scale = VS::Core::Math::Sine(scale * (ONE / 4) / duration) * 16;
                break;
            case 2:
                if (scale < duration - 1) {
                    scale = (scale << 16) / (duration - 2);
                } else {
                    scale = scale == duration ? 0x10000 : 0x11000;
                }
                break;
            case 3:
                if (duration >= 3) {
                    if (scale < duration - 1) {
                        scale = (scale * 0xC000) / (duration - 2) + 0x6000;
                    } else {
                        scale = scale == duration ? 0x10000 : 0x11000;
                    }
                }
                break;
            }
        }

        box->state = state;
        box->frameX = (box->x * scale + box->centerX * (0x10000 - scale)) >> 16;
        box->frameY = (box->y * scale + box->centerY * (0x10000 - scale)) >> 16;
        box->frameWidth = ((box->charsPerLine * 12 + 10) * scale) >> 16;
        box->frameHeight = ((box->lineCount * 13 + 4) * scale) >> 16;

        switch (box->flags.kind) {
        case 0:
            if (state < 0) {
                VS::Render::BlendLiveTextBoxIntoFramebuffer(box, scale);
                break;
            }

            goto text;
        case 4:
            VS::Menu::DrawLiveInformationBoxBackdrop(id);

            if (state < 0) {
                break;
            }

            goto text;
        case 6:
            VS::Render::BlendLiveTextBoxIntoFramebuffer(box, scale);
            break;
        case 2:
        case 7:
            VS::Menu::DrawLiveTextBoxFrame(id);

        default:
        text:
            if (state == duration + 1) {
                VS::Render::DrawLiveProgressiveTextBoxText(box);
            } else if (state < 0) {
                VS::Render::DrawLiveScaledTextBoxText(box, scale);
            }
            break;
        case 3:
            break;
        }
    }

    AddPrims(((u_char*)(Render_frameScratch.orderingTable)) - 24, Menu_activeTextBoxOrderingTable, Menu_activeTextBoxOrderingTable + 33);
    Menu_textBoxBufferIndex = Menu_textBoxBufferIndex == 2 ? 0 : Menu_textBoxBufferIndex + 1;
    Menu_activeTextBoxOrderingTable = Menu_textBoxOrderingTables + Menu_textBoxBufferIndex * 34;
    ClearOTag(Menu_activeTextBoxOrderingTable, 34);

    if (Battle_lowerScreenUiState == 1) {
        VS::Battle::DrawLivePuzzleTimer(&State_sceneTimer);
    }

    VS::Effects::ScreenDistortion::DrawLiveScreenOverlay();
    VS::Core::NextLiveInterfaceRandomBits(0);
}

}  // extern "C"
