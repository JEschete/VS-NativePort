// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Battle/HudStateLive.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Field/RoomRenderingLive.hpp"
#include "Input/Controller.hpp"
#include "Menu/Framework/ActorPreviewLive.hpp"
#include "Menu/Framework/EquipmentDetailsLive.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Framework/ItemRowsLive.hpp"
#include "Menu/Sounds.hpp"
#include "Menu/Status/ActorSwitchingLive.hpp"
#include "Menu/Status/LimbControllersLive.hpp"
#include "Render/CameraViewLive.hpp"
#include "Render/GameFontLive.hpp"
extern "C" {
#include "Legacy/Psx.h"
#include "Actors/ActorModelSlot.h"
#include "Actors/ActorRecord.h"
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
#include "Content/StringTables.h"
#include "Input/ControllerLiveState.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Menu/Framework/EquipmentDetailsLiveState.h"
#include "Menu/Framework/Framework.h"
#include "Menu/Framework/FrameworkLiveState.h"
#include "Menu/Framework/ItemRowsLiveState.h"
#include "Menu/Framework/Menus.h"
#include "Menu/Score/Encyclopedia.h"
#include "Menu/Score/Records.h"
#include "Menu/Status/ActorStatus.h"
#include "Menu/Status/EquipmentDetails.h"
#include "Render/Camera.h"
#include "Render/Display.h"
#include "Render/LegacyPackets.h"
#include "Render/Palette.h"
#include "Render/Render.h"
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
u_short Menu_Status_statusStrings[1760]; /* contents: disc string table (ModuleData.cpp) */

// decomp: _xPos (_xPos@src/MENU/MENU4.PRG/120.c)
int Menu_Status_xPos = 0;

// decomp: _yPos (_yPos@src/MENU/MENU4.PRG/120.c)
int Menu_Status_yPos = 0;

// decomp: local static of vs_menu4_exec, at file scope so the module reset reaches it
static int Menu_Status_updateAnimWait = 0;

// decomp: local static of vs_menu4_exec, at file scope so the module reset reaches it
static int Menu_Status_updateScreenEnabled = 0;

// decomp: MENU4.PRG _fadeScreenLeft (src/MENU/MENU4.PRG/120.c:2081)
void Menu_Status_DrawModelSlideFade(void)
{
    int uv1Tpage;
    u_long* prim = (u_long *)(Render_frameScratch.primitiveCursor);
    u_long* model_fade_ordering_table = (u_long *)(Render_frameScratch.orderingTable);

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

    u_long* prim = (u_long *)(Render_frameScratch.primitiveCursor);
    void* ordering_table_base = Render_frameScratch.orderingTable;
    u_int* background_ordering_table_entry = (u_int *)(((u_char*)(ordering_table_base)) + 0x1FF4);

    if (Menu_Status_drawBackgroundFirst != 0) {
        background_ordering_table_entry = (u_int *)(((u_char*)(ordering_table_base)) - 4);
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

    camera->lookAt.vx = ((-VS::Core::Math::Sine(Menu_Status_xPos) * xOffset) >> 8) * z;
    camera->lookAt.vy = -(x * ONE / 2);
    camera->lookAt.vz = ((VS::Core::Math::Cosine(Menu_Status_xPos) * xOffset) >> 8) * z;
    camera->position.vx = camera->lookAt.vx + ((VS::Core::Math::Cosine(Menu_Status_xPos) * VS::Core::Math::Cosine(Menu_Status_yPos)) >> 0xA) * z;
    camera->position.vy = camera->lookAt.vy - VS::Core::Math::Sine(Menu_Status_yPos) * z * 4;

    camera_orbit_z_offset = (VS::Core::Math::Sine(Menu_Status_xPos) * VS::Core::Math::Cosine(Menu_Status_yPos) >> 0xA) * z;
    camera->angles.vx = 0;
    camera->angles.vy = 0;
    camera->angles.vz = 0;
    camera->farClip = ONE;
    camera->position.vz = camera->lookAt.vz + camera_orbit_z_offset;

    VS::Render::BuildLiveCameraViewMatrix();

    if (Menu_Status_delayScreenUpdate) {
        --Menu_Status_delayScreenUpdate;
    } else {
        VS::Menu::Framework::DrawLiveActorPreview(&Render_frameScratch.viewMatrix);
    }

    Menu_Status_DrawBackground();
    Menu_Status_DrawModelSlideFade();
    VS::Menu::Status::LiveUpdateLimbPanel(0);
    VS::Menu::Status::LiveDrawVitals();
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
            VS::Menu::Framework::LoadLiveItemText(1);
        }

        if (VS::Menu::Framework::AreLiveMenuRowsIdle() == 0) {
            break;
        }

        Battle_hudStatusIconsHidden = 1;

        VS::Field::LiveSetFieldRenderingEnabled(0);
        VS::Menu::Framework::SetLiveBackgroundPriority(2046, 128);

        Menu_Status_drawBackgroundFirst = 0;
        Menu_Status_fadeScreen = 0;
        Menu_Status_selectedActor = 0;
        Menu_Status_previousActor = 0;
        Menu_Status_renderBasicStatsState = 0;
        Menu_Status_basicStatsAnimState = 0;
        Menu_Status_initializedActor = 0;

        VS::Menu::Status::LiveAnimateActorSwitch(1);

        Menu_Status_updateAnimWait = 1;
        Menu_Status_savedProjectionDistance = Render_projectionDistance;

        VS::Render::SetLiveCameraProjectionDistance(0x200);

        {

            Menu_Status_savedViewMatrix = Render_frameScratch.viewMatrix;
            Menu_Status_savedCamera = Render_frameScratch.camera;
            Render_frameScratch.camera.farClip = ONE;
            Render_frameScratch.camera.angles.vx = 0;
            Render_frameScratch.camera.angles.vy = 0;
            Render_frameScratch.camera.angles.vz = 0;
            Menu_Status_xPos = ONE / 16 * 11;
            Menu_Status_yPos = 0;

            VS::Menu::Framework::InitializeLiveInformationBox();

            Menu_Status_selectedElement = 0;
            Menu_Framework_enabledStatPages = 0;

            VS::Menu::Status::LiveShowSelectedLimbCondition();

            *state = 5;
        }
        break;

    case Menu_Status_StatusMenuState_WaitForMenu:
        if (Menu_Status_updateAnimWait != 0) {

            --Menu_Status_updateAnimWait;

            VS::Menu::Status::LiveUpdateLimbPanel(-1);
            VS::Menu::Framework::UpdateLivePrimaryStatPanel(1);

        } else {

            VS::Menu::Status::LiveShowSelectedLimbCondition();

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
            int preview_ready = VS::Menu::Status::LiveAnimateActorSwitch(0);
            Menu_Status_updateScreenEnabled = 1;
            if (preview_ready) {

                Menu_Status_updateAnimWait = 0;

                VS::Menu::Status::LiveUpdateLimbPanel(-1);
                VS::Menu::Framework::UpdateLivePrimaryStatPanel(1);

                userInput = Menu_Status_selectedElement;

                if (userInput < Text_Menu4_status_Index_statDesc) {
                    limbs = VS::Menu::Status::LiveLimbCount(Menu_Status_selectedActor - 1) - 1;
                    if (limbs < userInput) {
                        Menu_Status_selectedElement = limbs;
                    }

                    VS::Menu::Status::LiveShowSelectedLimbCondition();
                } else {
                    VS::Menu::Framework::SetLiveInformationMessage((char*)&Menu_Status_statusStrings
                            [Menu_Status_statusStrings[userInput - Text_Menu4_status_Index_statDesc]]);
                }
            }
        } else if (VS::Menu::Framework::LoadLiveItemText(0) != 0) {
            if (VS::Menu::Status::LiveUpdateLimbPanel(-3) == 2) {
                if (Input_buttonsPressed.allButtons & PADRup) {

                    VS::Menu::PlayLeaveSound();

                    *state = Menu_Status_StatusMenuState_WaitQuitToBattle;
                    break;

                } else if (Input_buttonsPressed.allButtons & PADRdown) {

                    VS::Menu::PlayLeaveSound();

                    *state = Menu_Status_StatusMenuState_WaitQuitToMenu;
                    break;

                } else if ((Input_buttonsPressed.allButtons & PADRright)
                           && ((Menu_Status_selectedElement < VS::Menu::Status::LiveLimbCount(Menu_Status_selectedActor - 1))
                               != 0)) {

                    VS::Menu::PlaySelectSound();
                    VS::Menu::Framework::UpdateLivePrimaryStatPanel(2);
                    VS::Menu::Status::LiveUpdateLimbPanel(-2);

                    Menu_Status_updateAnimWait = 8;
                    *state = Menu_Status_StatusMenuState_ViewEquipment;
                    break;
                }

                limb_status_available = 0;

                if (actor_details_visible != 0) {
                    int selected_element_index = Menu_Status_selectedElement;
                    limb_status_available = selected_element_index < VS::Menu::Status::LiveLimbCount(Menu_Status_selectedActor - 1);
                }

                if (Input_buttonsPressed.allButtons & PADRleft) {
                    if (limb_status_available != 0) {

                        VS::Menu::PlaySelectSound();
                        VS::Menu::Status::LivePopulateSelectedLimbDefense();
                        VS::Menu::Framework::UpdateLiveEquipmentStatPages(6);

                        Menu_Status_selectedStatRow = 0;
                        Menu_Status_updateAnimWait = 10;
                        *state = 9;
                        break;
                    }
                    VS::Menu::PlayInvalidSound();
                }

                if (Input_buttonsState & PADL2) {

                    userInput = VS::Input::MapStickDeadZone(Input_stickPosBuf.rStickX);
                    limbs = VS::Input::MapStickDeadZone(Input_stickPosBuf.rStickY);

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

                    VS::Menu::Status::LiveUpdateLimbPanel(7);

                    if (userInput == 13) {
                        if (VS::Menu::Status::LiveNavigateStatusEffects(
                                Battle_actors[Menu_Status_selectedActor - 1]->stats, 1)
                            == 0) {
                            VS::Menu::PlayChangeSound();
                            *state = 6;
                            break;
                        }
                        userInput = Menu_Status_selectedElement;
                    }

                    if (userInput < 6) {

                        limbs = VS::Menu::Status::LiveLimbCount(Menu_Status_selectedActor - 1);

                        if (userInput < limbs) {
                            VS::Menu::Status::LiveUpdateLimbPanel(userInput + 1);
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
                        (userInput < VS::Menu::Status::LiveLimbCount(Menu_Status_selectedActor - 1)) ^ 1;
                    Menu_Status_limbCursorAnimation =
                        VS::Menu::Framework::DrawLiveCursor(Menu_Status_limbCursorAnimation, Menu_Status_kLimbCursorPositions[userInput]);

                    if (userInput != Menu_Status_selectedElement) {

                        VS::Menu::PlayChangeSound();

                        if (userInput != 13) {
                            Menu_Status_selectedElement = userInput;
                        }
                    }

                    if (userInput < 6) {
                        VS::Menu::Status::LiveShowSelectedLimbCondition();
                    } else {
                        VS::Menu::Framework::SetLiveInformationMessage((char*)&Menu_Status_statusStrings
                                [Menu_Status_statusStrings[userInput - Text_Menu4_status_Index_statDesc]]);
                    }
                }
            }
            userInput = Menu_Status_selectedActor - 1;

            if (Input_buttonsPressed.allButtons & PADR1) {
                userInput = VS::Menu::Status::LiveFindNextInspectableActor(userInput, 1);
                limbs = 17;
            }

            if (Input_buttonsPressed.allButtons & PADL1) {
                userInput = VS::Menu::Status::LiveFindNextInspectableActor(userInput, 14);
                limbs = 1;
            }

            if (userInput != (Menu_Status_selectedActor - 1)) {

                Menu_Status_previousActor = Menu_Status_selectedActor;

                VS::Menu::Status::LiveUpdateLimbPanel(-2);
                VS::Menu::Framework::UpdateLivePrimaryStatPanel(2);
                VS::Menu::Status::LiveBeginVitalsTransition(userInput);
                VS::Menu::Status::LiveAnimateActorSwitch(userInput + limbs);

                Menu_Status_updateAnimWait = 1;
            }
        }

        Menu_Framework_cursorColor =
            (Menu_Status_selectedElement < VS::Menu::Status::LiveLimbCount(Menu_Status_selectedActor - 1)) ^ 1;
        break;

    case 6:
        actor_details_visible = 0;

        if ((Menu_Status_selectedActor == 1)
            || Battle_actors[Menu_Status_selectedActor - 1]->stats->isAnalyzed) {
            actor_details_visible = 1;
        }

        if (Menu_Status_updateAnimWait != 0) {
            int preview_ready = VS::Menu::Status::LiveAnimateActorSwitch(0);
            Menu_Status_updateScreenEnabled = 1;
            if (preview_ready) {

                VS::Menu::Status::LiveNavigateStatusEffects(Battle_actors[Menu_Status_selectedActor - 1]->stats, 0);

                Menu_Status_updateAnimWait = 0;

                VS::Menu::Status::LiveUpdateLimbPanel(-1);
                VS::Menu::Framework::UpdateLivePrimaryStatPanel(1);
            }
        } else {
            if (VS::Menu::Status::LiveUpdateLimbPanel(-3) == 2) {

                userInput = VS::Menu::Status::LiveNavigateStatusEffects(0, 1);

                if (userInput != 0) {
                    switch (userInput) {
                    case 1:
                        *state = 5;
                        break;
                    case 2:
                        VS::Menu::PlayLeaveSound();
                        *state = 13;
                        break;
                    case 3:
                        VS::Menu::PlayLeaveSound();
                        *state = 11;
                        break;
                    }
                }
            }

            userInput = Menu_Status_selectedActor - 1;

            if (Input_buttonsPressed.allButtons & PADR1) {
                userInput = VS::Menu::Status::LiveFindNextInspectableActor(userInput, 1);
                limbs = 17;
            }

            if (Input_buttonsPressed.allButtons & PADL1) {
                userInput = VS::Menu::Status::LiveFindNextInspectableActor(userInput, 14);
                limbs = 1;
            }

            if (userInput != (Menu_Status_selectedActor - 1)) {

                Menu_Status_previousActor = Menu_Status_selectedActor;

                VS::Menu::Status::LiveUpdateLimbPanel(-2);
                VS::Menu::Framework::UpdateLivePrimaryStatPanel(2);
                VS::Menu::Status::LiveBeginVitalsTransition(userInput);
                VS::Menu::Status::LiveAnimateActorSwitch(userInput + limbs);

                Menu_Status_updateAnimWait = 1;

                if (VS::Battle::LiveStatusIconFlags(Battle_actors[userInput]->stats) == 0) {
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

            VS::Menu::PlayLeaveSound();
            VS::Menu::Framework::UpdateLiveEquipmentStatPages(-1);

            if (Input_buttonsPressed.allButtons & PADRup) {
                *state = Menu_Status_StatusMenuState_WaitQuitToBattle;
                break;
            }

            Menu_Status_updateAnimWait = 10;
            *state = 10;
        } else {
            if ((Input_buttonsState & (PADL1 | PADR1)) != (PADL1 | PADR1)) {

                userInput = Menu_Status_selectedElement;
                limbs = VS::Menu::Status::LiveLimbCount(Menu_Status_selectedActor - 1);

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

                    VS::Menu::PlayChangeSound();

                    Menu_Status_selectedElement = userInput;

                    VS::Menu::Status::LivePopulateSelectedLimbDefense();
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
                VS::Menu::PlayChangeSound();
                Menu_Status_selectedStatRow = userInput;
            }

            VS::Menu::Framework::SetLiveInformationMessage((char*)&Menu_Status_statusStrings
                    [Menu_Status_statusStrings[userInput
                                    + (limbs != 3 ? Text_Menu4_status_Index_physicalDefense
                                                  : Text_Menu4_status_Index_bluntDefense)]]);

            Menu_Status_limbCursorAnimation =
                VS::Menu::Framework::DrawLiveCursor(Menu_Status_limbCursorAnimation, ((userInput * 16) + 50) << 16);
        }
        break;

    case 10:
        actor_details_visible = 1;

        if (Menu_Status_updateAnimWait != 0) {
            Menu_Status_animationStep = Menu_Framework_rowAnimationSteps[--Menu_Status_updateAnimWait];
        } else {
            VS::Menu::Status::LiveUpdateLimbPanel(Menu_Status_selectedElement + 1);
            *state = 5;
        }
        break;

    case Menu_Status_StatusMenuState_WaitQuitToMenu:
    case Menu_Status_StatusMenuState_WaitQuitToBattle:
        VS::Menu::Framework::UpdateLivePrimaryStatPanel(2);
        VS::Menu::Status::LiveUpdateLimbPanel(-2);
        VS::Menu::Status::LiveUpdateLimbPanel(0);
        VS::Menu::Status::LiveBeginVitalsTransition(0);
        VS::Menu::Status::LiveDrawVitals();
        VS::Render::SetLiveCameraProjectionDistance(Menu_Status_savedProjectionDistance);

        Render_frameScratch.viewMatrix = Menu_Status_savedViewMatrix;
        Render_frameScratch.camera = Menu_Status_savedCamera;

        VS::Field::LiveSetFieldRenderingEnabled(1);
        VS::Menu::Framework::SetLiveCommandLabel(Menu_Framework_MenuCommand_None);
        VS::Menu::Framework::ClearLiveMenuExcept(Menu_Framework_MenuItemId_None);
        VS::Menu::Framework::ReleaseLiveActorPreview();
        VS::Menu::Framework::SetLiveBackgroundPriority(-2, 128);

        Battle_hudStatusIconsHidden = 0;
        Menu_Status_updateAnimWait = 8;
        Menu_Status_updateScreenEnabled = 0;
        ++*state;
        break;

    case Menu_Status_StatusMenuState_QuitToMenu:
        VS::Menu::Status::LiveUpdateLimbPanel(0);
        VS::Menu::Status::LiveDrawVitals();
        VS::Menu::Framework::SetLiveBackgroundFade(0);
        VS::Menu::Framework::DismissLiveInformationBox();

        if (Menu_Status_updateAnimWait <= 0) {
            if (VS::Menu::Framework::UnloadLiveItemTextWhenReady() != 0) {
                *state = Menu_Status_StatusMenuState_None;
                return 1;
            }
        }

        --Menu_Status_updateAnimWait;
        break;

    case Menu_Status_StatusMenuState_QuitToBattle:
        VS::Menu::Status::LiveUpdateLimbPanel(0);
        VS::Menu::Status::LiveDrawVitals();
        VS::Menu::Framework::SetLiveBackgroundFade(0);
        VS::Menu::Framework::DismissLiveInformationBox();

        if (Menu_Status_updateAnimWait <= 0) {
            if (VS::Menu::Framework::UnloadLiveItemTextWhenReady() != 0) {
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

        VS::Menu::Framework::DrawLiveButtonPromptBackground(16 - userInput, 38, 88, 10);
        VS::Menu::Framework::DrawLiveButtonIcon(Menu_Framework_ButtonId_Square, 8 - userInput, 36, 0);
        VS::Render::DrawLiveCompactTextColor(
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

}  // extern "C"
