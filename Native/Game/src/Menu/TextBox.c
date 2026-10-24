// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Actors/TextureAnimation.h"
#include "Audio/Seq/Channel.h"
#include "Battle/Action.h"
#include "Battle/ActionTable.h"
#include "Battle/Hud.h"
#include "Battle/Targeting.h"
#include "Battle/TextBoxes.h"
#include "Content/StringTables.h"
#include "Core/Memory/Heap.h"
#include "Core/Random.h"
#include "Effects/ScreenEffects.h"
#include "Event/ScriptVm.h"
#include "Input/Buttons.h"
#include "Input/Controller.h"
#include "Menu/Framework/Framework.h"
#include "Menu/Framework/Menus.h"
#include "Menu/Framework/TextBoxes.h"
#include "Menu/Sounds.h"
#include "Menu/TextBox.h"
#include "Render/Display.h"
#include "Render/Font.h"
#include "Render/LegacyPackets.h"
#include "Render/Render.h"
#include "Render/Sprites.h"
#include "Render/UiPrimitives.h"
#include "Save/MemoryCard/GameFiles.h"
#include "State/SceneTimer.h"
#include "State/StateFlags.h"
#include "State/Timers.h"

// decomp: D_800EB9D8 (D_800EB9D8@BATTLE/BATTLE.PRG)
int* Menu_sceneNotificationBuffer; /* data: contents loaded in Step 5 */

// decomp: D_800EBF58 (D_800EBF58@BATTLE/BATTLE.PRG)
char Menu_kNotificationItemNames[65][12]; /* contents: module image (ModuleData.c) */

// decomp: D_800EC2D8 (D_800EC2D8@BATTLE/BATTLE.PRG)
int Menu_kInformationHeaderSpriteWords[5477]; /* contents: module image (ModuleData.c) */

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

// decomp: BATTLE.PRG vs_battle_setTextBox (src/BATTLE/BATTLE.PRG/5BF94.c:1536)
int Menu_SetTextBoxText(int boxId, char* str)
{
    Menu_TextBoxState* box = &Menu_textBoxes[boxId];

    if (str != NULL) {
        box->string = str;
        box->speed = 1;
        box->opaque4 = 0;
        box->prevChunksPrinted = 0;
        if (!(box->flags.speakerTexture & 0x20)) {
            Actors_SetActorTextureAnimation(box->flags.speakerTexture, 0, box->opaque2E + 1, 1);
            box->opaque2C = 1;
        }
        return 0;
    }

    if ((box->speed != 0xFFFF) && (box->speed != 0)) {
        return -1;
    }

    return 0;
}

// decomp: BATTLE.PRG vs_battle_selectTextBox (src/BATTLE/BATTLE.PRG/5BF94.c:1559)
int Menu_SelectTextBoxChoice(u_char boxMask)
{
    Menu_TextBoxState tempBox;
    short yPositions[8];
    int i;
    int j;
    int tempOrder;
    int selectedIndex;
    u_int count = 0;

    if (Menu_textBoxSelector.count == 0) {

        for (i = 0; i < 8; ++i) {
            yPositions[i] = 0x7FFF;
            if ((boxMask >> i) & 1) {
                ++count;
                yPositions[i] = Menu_textBoxes[i].y;
            }
        }

        for (i = 0; i < count; ++i) {
            int sortedIndex = 0;
            for (j = 0; j < 8; ++j) {
                if (yPositions[sortedIndex] > yPositions[j]) {
                    sortedIndex = j;
                }
            }
            Menu_textBoxSelector.order[i] = sortedIndex;
            yPositions[sortedIndex] = 0x7FFF;
        }

        selectedIndex = Audio_Seq_unusedStateFlags.opaque11 - 1;

        if (selectedIndex >= count) {
            selectedIndex = 0;
        }

        Menu_textBoxSelector.selectedIndex = 0;
        Menu_textBoxSelector.count = count;
    } else {
        selectedIndex = Menu_textBoxSelector.selectedIndex;
        count = Menu_textBoxSelector.count;

        if (Input_buttonsPressed.allButtons
            & (PADL2 | PADR2 | PADL1 | PADR1 | PADRup | PADRright | PADRdown
                | PADRleft)) {
            Menu_PlaySelectSound();
            Menu_textBoxSelector.count = 0;
            return selectedIndex + 1;
        }

        if (Input_buttonsPressed.allButtons & PADLup) {
            int v0 = selectedIndex - 1;
            selectedIndex = v0 + count;
        }

        if (Input_buttonsPressed.allButtons & PADLdown) {
            ++selectedIndex;
        }

        selectedIndex %= count;
    }

    if (selectedIndex != Menu_textBoxSelector.selectedIndex) {
        Menu_PlayChangeSound();
        Menu_textBoxSelector.selectedIndex = selectedIndex;
    }

    j = selectedIndex;

    for (i = 0; i < count; ++i) {
        if (Menu_textBoxSelector.order[j] < Menu_textBoxSelector.order[i]) {
            j = i;
        }
    }

    tempOrder = Menu_textBoxSelector.order[selectedIndex];
    Menu_textBoxSelector.order[selectedIndex] = Menu_textBoxSelector.order[j];
    Menu_textBoxSelector.order[j] = tempOrder;

    tempBox = Menu_textBoxes[tempOrder];
    Menu_textBoxes[tempOrder] =
        Menu_textBoxes[Menu_textBoxSelector.order[selectedIndex]];
    Menu_textBoxes[Menu_textBoxSelector.order[selectedIndex]] = tempBox;

    for (i = 0; i < count; ++i) {
        Menu_textBoxes[Menu_textBoxSelector.order[i]].brightness =
            i == selectedIndex ? 0x80 : 0x40;
    }

    return 0;
}

// decomp: BATTLE.PRG vs_battle_initInformationTextBox (src/BATTLE/BATTLE.PRG/5BF94.c:2653)
void Menu_InitInformationTextBox(enum Menu_InformationBoxHeader header)
{
    Menu_InitTextBox(7, (header << 8) | 4, 0, 1, 0, 4, 0, 0);
}

// decomp: BATTLE.PRG _dismissAllTextBoxes (src/BATTLE/BATTLE.PRG/5BF94.c:3757)
int Menu_DismissAllTextBoxes(void)
{
    int i;
    int var_s1 = 1;

    for (i = 0; i < 8; ++i) {
        if (Menu_DismissTextBox(i) != 0) {
            var_s1 = 0;
        }
    }

    return var_s1;
}

// decomp: BATTLE.PRG vs_battle_displaySceneMessage (src/BATTLE/BATTLE.PRG/5BF94.c:3771)
void Menu_ShowSceneNotification(int arg0, int arg1, int arg2)
{
    Menu_sceneNotificationBuffer = Core_Memory_AllocateFromHighEnd(112);
    Menu_sceneNotificationTimed = arg2;
    if (Menu_sceneNotificationTimed) {
        Menu_sceneNotificationTimer = 0x78;
    }

    Menu_InitTextBox(7, 0x304, 0, 1, 0, 3, 0, 0);

    Menu_sceneNotificationBuffer[0] = 0x00F8 | (0x04FB << 16);
    Battle_stringContext.strings[0] = Battle_actionDefinitions[arg1].name;

    if (arg0 == 12) {
        Render_FormatEncodedText((char*)&Menu_sceneNotificationBuffer[1],
            (char*)&Menu_menuStrings[Text_Battle_menuStrings_Offset_spellMastered]);
    } else if (arg0 == 11) {
        Menu_textBoxes[7].flags.frameShape = 0;
        Render_FormatEncodedText((char*)&Menu_sceneNotificationBuffer[1],
            (char*)&Menu_menuStrings[Menu_menuStrings[Battle_actionDefinitions[arg1].type
                                                                + 22]]);
    } else {
        if (arg1 != 0) {
            Battle_stringContext.strings[0] = Menu_kNotificationItemNames[(arg1 - 0x1CA) * 2];
        }
        Render_FormatEncodedText((char*)Menu_sceneNotificationBuffer, (char*)&Battle_doorStrings[Battle_doorStrings[arg0]]);
    }

    Menu_SetTextBoxText(7, (char*)Menu_sceneNotificationBuffer);
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
                return -Menu_SetTextBoxText(7, NULL);
            }

            return 0;
        }
        return -Menu_SetTextBoxText(7, NULL);
    }
    return 0;
}

// decomp: BATTLE.PRG func_800CB50C (src/BATTLE/BATTLE.PRG/5BF94.c:3829)
void Menu_ReleaseSceneNotification(void)
{
    if (Menu_sceneNotificationBuffer != NULL) {
        Menu_DismissTextBox(7);
        Core_Memory_FreeFromHighEnd(Menu_sceneNotificationBuffer);
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

        Render_activeTextBoxIndex = id;

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
                scale = Core_Gte_Sin(scale * (ONE / 4) / duration) * 16;
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
                Render_BlendTextBoxIntoFramebuffer(box, scale);
                break;
            }

            goto text;
        case 4:
            Menu_DrawInformationBoxBackdrop(id);

            if (state < 0) {
                break;
            }

            goto text;
        case 6:
            Render_BlendTextBoxIntoFramebuffer(box, scale);
            break;
        case 2:
        case 7:
            Menu_DrawTextBoxFrame(id);

        default:
        text:
            if (state == duration + 1) {
                Render_DrawProgressiveTextBoxText(box);
            } else if (state < 0) {
                Render_DrawScaledTextBoxText(box, scale);
            }
            break;
        case 3:
            break;
        }
    }

    AddPrims(Render_frameScratch.orderingTable - 24, Menu_activeTextBoxOrderingTable, Menu_activeTextBoxOrderingTable + 33);
    Menu_textBoxBufferIndex = Menu_textBoxBufferIndex == 2 ? 0 : Menu_textBoxBufferIndex + 1;
    Menu_activeTextBoxOrderingTable = Menu_textBoxOrderingTables + Menu_textBoxBufferIndex * 34;
    ClearOTag(Menu_activeTextBoxOrderingTable, 34);

    if (Battle_lowerScreenUiState == 1) {
        Battle_DrawPuzzleTimer(&State_sceneTimer);
    }

    Effects_DrawScreenEffectOverlay();
    Core_Random_NextInterfaceRandomBits(0);
}

// decomp: BATTLE.PRG vs_battle_getTextBox (src/BATTLE/BATTLE.PRG/5BF94.c:4544)
Menu_TextBoxState* Menu_TextBox(int id) { return &Menu_textBoxes[id]; }

// decomp: BATTLE.PRG func_800CCE10 (src/BATTLE/BATTLE.PRG/5BF94.c:4546)
Menu_TextAnchorWord Menu_TextBoxAnchor(int arg0, int arg1, int arg2, int arg3)
{
    Menu_TextAnchorWord ret;
    Menu_TextAnchorWord* p =
        &Menu_textBoxAnchors[arg0][((arg1 >> 2) & 3) == 1 || ((arg1 >> 2) & 3) == 2];
    int offsetX = p->x;
    int offsetY = p->y;
    int unk24 = p->alignment;

    if ((arg1 >> 2) & 1) {
        offsetX = -offsetX;
        unk24 ^= 2;
    }

    ret.x = offsetX - ((arg2 * 12 + 10) >> 1);
    ret.y = offsetY - ((arg3 * 13 + 4) >> 1);
    ret.alignment = unk24;
    ret.opaque3Bit2 = p->opaque3Bit2;
    return ret;
}

// decomp: BATTLE.PRG vs_battle_initTextBox (src/BATTLE/BATTLE.PRG/5BF94.c:4567)
void Menu_InitTextBox(
    int id, int flags, int x, int y, int charsPerLine, int h, int centerX, int centerY)
{
    Menu_TextBoxState* box = &Menu_textBoxes[id];

    *(int*)&box->flags = flags & 0xFFFF;
    box->flags.speakerTexture = 32;
    box->x = x;
    box->y = y;
    box->opaque4 = 0;
    box->opaque6 = 0;
    box->speed = 0;
    box->xIndent = x + 5;
    box->yIndent = y + 3;
    box->opaque2E = 0;
    box->brightness = 0x80;
    box->charsPerLine = charsPerLine;
    box->lineCount = h;
    box->centerX = centerX;
    box->centerY = centerY;
    // looks up { 10, 20, 10, 8 };
    box->openDuration = ((0x45A5 >> ((box->flags.openStyle * 4))) & 0xF) * 2;

    if (box->state <= 0) {
        box->state = 1;
    }

    switch (box->flags.kind) {
    case 1:
        box->flags.kind = 7;
        box->flags.frameShape = 0;
        return;
    case 4:
        box->charsPerLine = 0x18;
        box->centerX = 0xA0;
        box->centerY = 0xF0;
        box->xIndent = 0x10;
        box->yIndent = 0xF2 - (h * 0xD);
        return;
    case 5:
        box->flags.kind = 7;
        box->flags.frameShape = 2;
        return;
    }
}

// decomp: BATTLE.PRG vs_battle_dismissTextBox (src/BATTLE/BATTLE.PRG/5BF94.c:4613)
int Menu_DismissTextBox(int id)
{
    Menu_TextBoxState* box = &Menu_textBoxes[id];

    if (!(box->flags.speakerTexture & 0x20)) {
        Actors_SetActorTextureAnimation(box->flags.speakerTexture, 0, 0, 0);
    }

    if (box->state != 0) {
        if (box->state > 0) {
            box->openDuration = 8;
            box->state = -1;
            box->flags.openStyle = 0;
        }
        return -1;
    }

    return 0;
}

// decomp: BATTLE.PRG func_800CD0FC (src/BATTLE/BATTLE.PRG/5BF94.c:4633)
void Menu_DrawInformationBoxHeader(int arg0, u_int arg1)
{
    if (arg1 < 3) {
        Render_QueueDefaultSprite(Menu_informationHeaderPositions[arg1], (arg0 << 0x10) | 0xA)[4] =
            Menu_kInformationHeaderSpriteWords[arg1];
    }
}

// decomp: BATTLE.PRG func_800CD158 (src/BATTLE/BATTLE.PRG/5BF94.c:4642)
void Menu_DrawInformationBoxBackdrop(int id)
{
    Menu_TextBoxState* box = &Menu_textBoxes[id];
    int y = box->y;
    u_long* ot = Menu_activeTextBoxOrderingTable + id * 4 + 1;
    int edge = box->frameHeight;
    int offset = Render_frameBuf ? 0 : 320;
    u_int i;
    u_int count;

    if (y == 0) {
        y = edge;
    } else {
        y = 240 - edge;
    }
    count = box->lineCount * 13 + 4 - edge;
    if (count > 16) {
        count = 16;
    } else if (count < 4) {
        count = 4;
    }
    for (i = 0; i < count; ++i) {
        u_int brightness = ((count - i) * 128) / (count + 1);
        u_long* prim =
            Render_QueueSprite(brightness, vs_getXY(256, y), vs_getWH(64, 1), ot);
        u_int page = ((u_int)(offset + 256) >> 6) | 0x100;
        prim[1] = _get_mode(0, 0, page);
        prim[4] = y << 8;
        prim = Render_QueueSprite(brightness, vs_getXY(0, y), vs_getWH(256, 1), ot);
        brightness = ((u_int)offset >> 6) | 0x100;
        prim[1] = _get_mode(0, 0, brightness);
        prim[4] = y << 8;
        if (y < 128) {
            if (--y < 0) {
                break;
            }
        } else if (++y >= 240) {
            break;
        }
    }
    if (y < 128) {
        if (y >= 0) {
            Render_InsertThreeWordPrimitive(ot, vs_getRGB0(Render_LegacyPrimitiveCode_Tile, 0, 0, 0), 0, vs_getWH(320, y + 1));
        }
        return;
    }
    if (y < 240) {
        Render_InsertThreeWordPrimitive(ot, vs_getRGB0(Render_LegacyPrimitiveCode_Tile, 0, 0, 0), vs_getXY(0, y),
            (0xF00000 - (y << 16)) | 320);
    }
    if (box->state == box->openDuration + 1) {
        Menu_DrawInformationBoxHeader(y - 7, box->flags.frameShape - 1);
    }
}

// decomp: BATTLE.PRG func_800CD3A0 (src/BATTLE/BATTLE.PRG/5BF94.c:4697)
int Menu_FlipTextBoxShapePoint(int arg0, int arg1)
{
    int var_v1 = arg0;

    if (arg1 & 1) {
        var_v1 = (var_v1 & 0xFF) | ((0xFF - (var_v1 >> 8)) << 8);
    }

    if (arg1 & 2) {
        var_v1 = (~var_v1 & 0xFF) | (var_v1 & 0xFF00);
    }

    return var_v1;
}

// decomp: src/BATTLE/BATTLE.PRG/5BF94.c:4712
#define POINT_X(p) (((short*)&points[p])[0])
// decomp: src/BATTLE/BATTLE.PRG/5BF94.c:4713
#define POINT_Y(p) (((short*)&points[p])[1])
// decomp: BATTLE.PRG func_800CD3E4 (src/BATTLE/BATTLE.PRG/5BF94.c:4715)
void Menu_DrawTextBoxFrame(int index)
{
    u_long* ot = Menu_activeTextBoxOrderingTable + index * 4;
    u_int part;
    Menu_TextBoxState* box = &Menu_textBoxes[index];
    u_short* verts = (u_short*)Event_textBoxFrameStyles[box->flags.frameShape];
    u_char* shape;
    int header;
    int* points = (int*)((uintptr_t)Core_ScratchBase() + 0x088);
    short* out;
    u_long* prim;
    int x = box->frameX;
    int y = box->frameY;
    int w = box->frameWidth;
    int h = box->frameHeight;
    int flip;
    int i;
    int n;
    u_char count;
    int k;
    int v;

    if (verts == NULL) {
        return;
    }

    out = (short*)((uintptr_t)Core_ScratchBase() + 0x088);
    flip = box->flags.alignment;
    shape = (u_char*)verts;
    part = ((int*)shape)[box->flags.framePart];
    verts = (u_short*)(shape + (shape[1] + 1) * 4);
    count = shape[2];
    header = *(int*)shape;

    for (i = 0; i < count; ++i) {
        v = Menu_FlipTextBoxShapePoint(verts[i], flip);
        *out++ = x + (((v & 0xFF) * w) >> 8);
        *out++ = y + (((v >> 8) * h) >> 8);
    }
    if ((u_int)(box->flags.framePart - 1) < ((header >> 8) & 0xFF)) {
        v = Menu_FlipTextBoxShapePoint(verts[part & 0xFF], flip);

        for (; i < (header & 0xFF); ++i) {
            n = Menu_FlipTextBoxShapePoint(verts[i], flip);
            k = n >> 8;
            n &= 0xFF;

            if (n >= 128) {
                n -= 256;
            }
            if (k >= 128) {
                k -= 256;
            }

            *out++ = x + (((n + (v & 0xFF)) * w) >> 8);
            *out++ = y + (((k + (v >> 8)) * h) >> 8);
        }
    } else {
        part = 0xFFFF;
    }

    verts += header & 0xFF;

    i = (header >> 16) & 0xFF;

    for (n = 0; n < i; ++n) {
        if (n == (part & 0xFF)) {
            v = 0;

            for (k = 0; k < box->flags.framePart; ++k) {
                v += shape[k * 4 + 2];
            }
            for (k = 0; k < shape[box->flags.framePart * 4 + 2]; ++k) {
                Render_InsertThreeWordPrimitive(
                    ot + 2, vs_getRGB0(Render_LegacyPrimitiveCode_LineF2, 48, 48, 48), points[n], points[v + k]);
                n = v + k;
            }

            v = n;
            n = (part >> 8) & 0xFF;
            Render_InsertThreeWordPrimitive(
                ot + 2, vs_getRGB0(Render_LegacyPrimitiveCode_LineF2, 48, 48, 48), points[n], points[v]);
        }

        v = (n + 1) % i;
        Render_InsertThreeWordPrimitive(
            ot + 2, vs_getRGB0(Render_LegacyPrimitiveCode_LineF2, 48, 48, 48), points[n], points[v]);
    }

    Render_InsertDrawModeCommand(0xE2000000, ot + 2);

    prim = Render_frameScratch.primitiveCursor;
    flip = box->brightness;
    flip = flip | (flip << 8) | (flip << 16);

    for (n = 0; n <= shape[1]; ++n) {
        for (k = 0; k < shape[n * 4 + 3]; ++k) {
            int quad;

            if ((n != 0) && (n != box->flags.framePart)) {
                verts += 2;
                continue;
            }

            v = ((u_char*)verts)[0];
            i = ((u_char*)verts)[1];
            w = ((u_char*)verts)[2];
            h = ((u_char*)verts)[3];
            verts += 2;

            x = POINT_X(v);

            if (POINT_X(i) < x) {
                x = POINT_X(i);
            }
            if (POINT_X(w) < x) {
                x = POINT_X(w);
            }

            y = POINT_Y(v);

            if (POINT_Y(i) < y) {
                y = POINT_Y(i);
            }
            if (POINT_Y(w) < y) {
                y = POINT_Y(w);
            }

            quad = (h != 0xFF) * 2;

            if (quad) {
                if (POINT_X(h) < x) {
                    x = POINT_X(h);
                }
                if (POINT_Y(h) < y) {
                    y = POINT_Y(h);
                }
            } else {
                h = 0;
            }

            prim[0] = (ot[2] & 0xFFFFFF) | ((quad + 7) << 24);
            prim[1] = flip | ((quad + 9) << 26);
            x = (x >> 6) << 6;
            y = (y >> 6) << 6;
            prim[2] = points[v];
            prim[3] = ((POINT_X(v) - x) & 0xFF) | (((POINT_Y(v) - y) << 8) & 0xFF00)
                    | (getClut(992, 220) << 16);
            prim[4] = points[i];
            prim[5] = ((POINT_X(i) - x) & 0xFF) | (((POINT_Y(i) - y) << 8) & 0xFF00)
                    | (getTPage(0, 0, 448, 256) << 16);
            prim[6] = points[w];
            prim[7] = ((POINT_X(w) - x) & 0xFF) | (((POINT_Y(w) - y) << 8) & 0xFF00);
            prim[8] = points[h];
            prim[9] = ((POINT_X(h) - x) & 0xFF) | (((POINT_Y(h) - y) << 8) & 0xFF00);
            ot[2] = LegacyGpu_Handle(prim);
            prim += quad + 8;

            prim[0] = (ot[1] & 0xFFFFFF) | 0x0C000000;
            prim[1] = (quad + 8) << 26;
            prim[2] = ((POINT_X(v) + 2) & 0xFFFF) | ((POINT_Y(v) + 2) << 16);
            prim[3] = ((POINT_X(i) + 2) & 0xFFFF) | ((POINT_Y(i) + 2) << 16);
            prim[4] = ((POINT_X(w) + 2) & 0xFFFF) | ((POINT_Y(w) + 2) << 16);
            prim[5] = quad ? ((POINT_X(h) + 2) & 0xFFFF) | ((POINT_Y(h) + 2) << 16) : 0;
            prim[6] = 0x48000000;
            prim[7] = prim[2];
            prim[8] = prim[3];
            prim[9] = prim[quad + 3];
            prim[10] = prim[4];
            prim[11] = prim[2];
            prim[12] = 0x55555555;
            ot[1] = LegacyGpu_Handle(prim);
            prim += 13;
        }
    }

    Render_frameScratch.primitiveCursor = prim;
    Render_InsertThreeWordPrimitive(ot + 2, _get_mode(0, 0, getTPage(0, 0, 448, 256)), 0xE2000318, 0);
}
#undef POINT_Y
#undef POINT_X

