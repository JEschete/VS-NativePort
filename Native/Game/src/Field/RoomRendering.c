// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Actors/ActorModelSlot.h"
#include "Actors/ModelInstance.h"
#include "Actors/Skeleton.h"
#include "Battle/Abilities.h"
#include "Battle/Actor.h"
#include "Battle/Feedback.h"
#include "Core/Memory/WordCopy.h"
#include "Effects/Weather.h"
#include "Field/Camera.h"
#include "Field/Doors.h"
#include "Field/Environment.h"
#include "Field/Gimmicks.h"
#include "Field/Map.h"
#include "Field/Minimap.h"
#include "Field/MinimapScene.h"
#include "Field/Mpd.h"
#include "Field/Room.h"
#include "Field/RoomEffects.h"
#include "Field/RoomRendering.h"
#include "Field/RoomTriggerMarkers.h"
#include "Field/Zone.h"
#include "Input/Controller.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Items/Loot.h"
#include "Items/TreasureChests.h"
#include "Render/Display.h"
#include "Render/Palette.h"
#include "Render/Render.h"
#include "Render/RoomDraw.h"
#include "Render/RoomLighting.h"
#include "Render/UiPrimitives.h"
#include "State/Settings.h"
#include "State/Timers.h"

// decomp: D_800F190C (D_800F190C@BATTLE/BATTLE.PRG)
int Field_isFieldRenderingEnabled;

// decomp: D_800F1BA4 (D_800F1BA4@BATTLE/BATTLE.PRG)
u_short Field_areRoomUpdatesPaused;

// decomp: D_800F1BA8 (D_800F1BA8@BATTLE/BATTLE.PRG)
u_short* Field_roomPalettes;

// decomp: D_800F1BB0 (D_800F1BB0@BATTLE/BATTLE.PRG)
Field_RoomTint Field_roomTint;

// decomp: BATTLE.PRG func_8008A4DC (src/BATTLE/BATTLE.PRG/146C.c:15521)
void Field_SetFieldRenderingEnabled(int enabled)
{
    if (enabled != 0) {
        Field_isFieldRenderingEnabled = 1;
    } else {
        Field_isFieldRenderingEnabled = 0;
    }
}

// decomp: BATTLE.PRG func_8008AB80 (src/BATTLE/BATTLE.PRG/146C.c:15756)
int Field_EnableOwnedRoomRendering(int owner_room_index)
{
    if ((Field_roomData.isLoaded == 1) && (Field_roomData.roomIndex == owner_room_index)) {
        Field_roomData.displayFlag = Field_roomData.isLoaded;
        return 1;
    }
    return 0;
}

// decomp: BATTLE.PRG func_8008ABB8 (src/BATTLE/BATTLE.PRG/146C.c:15765)
int Field_DisableOwnedRoomRendering(int owner_room_index)
{
    if ((Field_roomData.isLoaded == 1) && (Field_roomData.roomIndex == owner_room_index)) {
        Field_roomData.displayFlag = 0;
        return 1;
    }
    return 0;
}

// decomp: BATTLE.PRG func_8008AC78 (src/BATTLE/BATTLE.PRG/146C.c:15791)
void Field_DrawRoomScene(void)
{
    Render_ScreenOffset originalOffset;
    Render_ScreenOffset mapOffset;
    RECT mapArea;
    MATRIX mapMatrix;
    DR_AREA* drawArea;
    LINE_G4* border;
    LINE_G2* edge;
    LINE_G2* nextLine;

    Core_Gte_SetRotMatrix(&((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->viewMatrix);
    Core_Gte_SetTransMatrix(&((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->viewMatrix);
    Field_DrawRoomTriggerMarkers();
    Battle_DrawActorMarkers();
    Field_UpdateGimmickAnimationQueue();

    if (Field_areRoomUpdatesPaused == 0) {
        int i;

        for (i = 0; i < (State_tickSpeed >> 1); ++i) {
            Field_UpdateRoomEffects();
        }
    }

    Field_DrawMovingPlatformShadows();
    Field_UpdateRoomWeather();

    if (*(u_short*)&Field_roomData != 0x101) {
        return;
    }

    if (Field_roomData.geometry != NULL) {
        int facing = ((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->camera.angles.vy;
        int facing0 = 1 << (((0x9C0 - facing) & 0xFFF) >> 9);
        int facing1 = 1 << (((0x840 - facing) & 0xFFF) >> 9);
        int facing2 = 1 << (((0x900 - facing) & 0xFFF) >> 9);
        ((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->firstPersonViewFacing =
            (((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->firstPersonViewFacing & ~0xFF) | facing0
            | facing1 | facing2;
        Field_DrawRoomGeometry(Field_roomData.geometry);
    }

    if (*(u_short*)&Field_roomData != 0x101 || !(Input_settings.simpleMap & 1)
        || Field_roomData.minimapScene == NULL) {
        return;
    }

    if (Field_isMinimapVisible != 0) {
        if (++Field_minimapSlideFrame > 8) {
            Field_minimapSlideFrame = 8;
        }
    } else {
        Field_minimapSlideFrame -= 2;

        if (Field_minimapSlideFrame < 0) {
            Field_minimapSlideFrame = 0;
        }
    }

    if (Field_minimapSlideFrame <= 0) {
        return;
    }

    Field_BuildMinimapRotation(&mapMatrix, 0x800 - ((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->camera.angles.vy);
    Core_Gte_SetRotMatrix(&mapMatrix);
    drawArea = ((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->primitiveCursor;
    SetDrawArea(drawArea, &Render_legacyDrawEnvironments[(Render_frameBuf + 1) & 1].clip);
    AddPrim((u_long*)((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->orderingTable + 5, drawArea);
    ++drawArea;
    ((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->primitiveCursor = drawArea;

    if (Render_legacyDrawEnvironments[(Render_frameBuf + 1) & 1].clip.x < 320) {
        mapArea.x = 216;
    } else {
        mapArea.x = 536;
    }

    mapArea.y = 156;
    mapArea.w = 96;
    mapArea.h = 64;
    mapOffset.y = 196;
    mapOffset.x = 392 - (Field_minimapSlideFrame << 4);
    Render_GetProjectionScreenOffset(&originalOffset);
    Render_SetProjectionScreenOffset(&mapOffset);
    Field_DrawMinimapScene(Field_roomData.minimapScene, Field_roomData.header.minimapSceneLength);
    Render_SetProjectionScreenOffset(&originalOffset);
    Core_Gte_SetRotMatrix(&((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->viewMatrix);
    Core_Gte_SetTransMatrix(&((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->viewMatrix);
    drawArea = ((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->primitiveCursor;
    SetDrawArea(drawArea, &mapArea);
    AddPrim((u_long*)((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->orderingTable + 5, drawArea);
    ++drawArea;
    ((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->primitiveCursor = drawArea;

    if (!(Input_settings.simpleMap & 2)) {
        return;
    }

    border = (LINE_G4*)drawArea;
    setLineG4(border);
    setXY4(border, 215, 155, 313, 155, 313, 220, 215, 220);
    setSemiTrans(border, 1);
    AddPrim((u_long*)((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->orderingTable + 5, border++);
    // Retain the frame pointer for the color update below.
    nextLine = (LINE_G2*)border--;
    setLineG2(nextLine);
    setSemiTrans(nextLine, 1);
    setXY2(nextLine, 215, 220, 215, 155);
    edge = nextLine++;
    AddPrim((u_long*)((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->orderingTable + 5, edge);
    ((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->primitiveCursor = nextLine;
    _insertTPage(5, getTPage(0, 3, 0, 0));

    if (Battle_characterState->weaponDrawn == 1) {
        int pulse = Core_Gte_Sin(Field_minimapBorderPulsePhase << 8) / 64;
        int shade;
        Field_minimapBorderPulsePhase += State_tickSpeed / 2;
        shade = 128 - pulse;
        setRGB0(border, shade, 0, 0);
        setRGB1(border, shade + 32, 0, 0);
        setRGB2(border, shade + 63, 0, 0);
        setRGB3(border, shade + 32, 0, 0);
        setRGB0(edge, shade + 32, 0, 0);
        setRGB1(edge, shade, 0, 0);
    } else {
        Field_minimapBorderPulsePhase = 0;
        setRGB0(border, 0, 128, 0);
        setRGB1(border, 0, 192, 0);
        setRGB2(border, 0, 255, 0);
        setRGB3(border, 0, 192, 0);
        setRGB0(edge, 0, 192, 0);
        setRGB1(edge, 0, 128, 0);
    }
}

// decomp: BATTLE.PRG func_8008B1FC (src/BATTLE/BATTLE.PRG/146C.c:15926)
void Field_DrawRoomGeometry(void* room_geometry)
{
    if (!Render_ValidateRoomGeometry(room_geometry, Field_roomData.header.geometryLength)) {
        Core_Debugf("render", "Invalid room geometry extent");
        return;
    }
    Core_Gte_SetRotMatrix(&Render_frameScratch.viewMatrix); // _camera.viewMatrix
    Core_Gte_SetTransMatrix(&Render_frameScratch.viewMatrix); // _camera.viewMatrix
    Render_UpdateRoomGroupTransforms(room_geometry);
    Field_roomPrimitiveRangeStart = Render_frameScratch.primitiveCursor; // _camera.unk0
    Render_DrawRoomGeometry(room_geometry, Field_roomGeometryDrawMode);
    Field_roomPrimitiveRangeEnd = Render_frameScratch.primitiveCursor; // _camera.unk0
    if (Field_roomTint.isActive != 0) {
        Render_TintRoomGeometryPrimitives(Field_roomPrimitiveRangeStart, Field_roomPrimitiveRangeEnd, &Field_roomTint, 0);
    }
}

// decomp: BATTLE.PRG func_8008B28C (src/BATTLE/BATTLE.PRG/146C.c:15939)
void Field_AdvanceRoomAnimationClock(void)
{
    if (Field_areRoomUpdatesPaused == 0) {
        Field_roomAnimationTicks += State_tickSpeed;
    }
}

// decomp: BATTLE.PRG func_8008B2C0 (src/BATTLE/BATTLE.PRG/146C.c:15946)
void Field_SetRoomRenderingEnabled(int enabled)
{
    if (Field_roomData.isLoaded != 0) {
        Field_roomData.displayFlag = enabled;
    }
}

// decomp: BATTLE.PRG func_8008B2E0 (src/BATTLE/BATTLE.PRG/146C.c:15953)
void Field_CopyZonePaletteRow(void* source_colors, int palette_row, int first_color, int color_count)
{
    Core_Memory_CopyAlignedWords(Field_roomPalettes + (((palette_row * 256) + first_color)), source_colors, color_count * 2);
}

// decomp: BATTLE.PRG func_8008B320 (src/BATTLE/BATTLE.PRG/146C.c:15958)
void Field_UploadRoomPalettes(void)
{
    Render_LoadClutColors(Field_roomPalettes, 0, 0, 0x100);
    Render_LoadClutColors(Field_roomPalettes + 0x100, 1, 0, 0x100);
    Render_LoadClutColors(Field_roomPalettes + 0x200, 2, 0, 0x100);
    ((void (*)(u_short*))Field_SetGimmickPaletteTable)(Field_roomPalettes + 0x300);
}

// decomp: BATTLE.PRG func_8008B390 (src/BATTLE/BATTLE.PRG/146C.c:15966)
void Field_OffsetRoomTextureIds(int texture_id_offset, int* geometry_words)
{
    int i;

    int* group_word_cursor = geometry_words;
    int group_count = *geometry_words;
    group_word_cursor = geometry_words + 1;

    for (i = 0; i < group_count; ++i) {
        void* polygon_block_cursor = geometry_words + ((group_word_cursor[7] >> 2));
        int triangle_count = *(int*)polygon_block_cursor;
        int quad_count;
        int j;

        polygon_block_cursor += 4;
        quad_count = *(int*)polygon_block_cursor;
        polygon_block_cursor += 4;

        for (j = 0; j < triangle_count; ++j, polygon_block_cursor += 0x20) {
            *((short*)(polygon_block_cursor + 0x1E)) += texture_id_offset;
        }

        for (j = 0; j < quad_count; ++j, polygon_block_cursor += 0x28) {
            *((short*)(polygon_block_cursor + 0x1E)) += texture_id_offset;
        }
        group_word_cursor += 0x10;
    }
}

// decomp: BATTLE.PRG func_8008B430 (src/BATTLE/BATTLE.PRG/146C.c:15995)
void Field_SetRoomTint(Render_ColorCode* tint_color, int tint_strength)
{
    if ((tint_color->red == 0) && (tint_color->green == 0) && (tint_color->blue == 0) && (tint_strength == 0)) {
        Field_isRoomTintActive = 0;
        return;
    }
    Field_roomTint.isActive = 1;
    Field_roomTint.strength = tint_strength;
    Field_roomTint.red = tint_color->red;
    Field_roomTint.green = tint_color->green;
    Field_roomTint.blue = tint_color->blue;
}

// decomp: BATTLE.PRG func_8008B4B0 (src/BATTLE/BATTLE.PRG/146C.c:16008)
void Field_ClearRoomTint(void) { Field_isRoomTintActive = 0; }

// decomp: BATTLE.PRG func_8008B4BC (src/BATTLE/BATTLE.PRG/146C.c:16010)
void Field_SetRoomAnimationsPaused(int paused) { Field_areRoomUpdatesPaused = paused; }

// decomp: BATTLE.PRG func_8008C2C0 (src/BATTLE/BATTLE.PRG/146C.c:16412)
int Field_ApplyCameraGeometryOcclusion(int query_tile_x, int query_tile_z, int query_floor_layer, int camera_distance_mode)
{
    Field_MpdViewTrigger* view_trigger_cursor;
    Field_MpdViewTrigger* view_trigger;
    int i;
    int j;
    int len;
    int v;
    char* effect_id_cursor;

    if (Field_roomData.viewTriggers != 0) {
        Field_ClearGeometryOcclusion();
        v = ((Render_frameScratch.camera.angles.vy + 0x900) & 0xFFF) >> 9;
        view_trigger_cursor = Field_roomData.viewTriggers;
        len = Field_roomData.header.viewTriggersLength;
        for (i = 0; i < len;) {
            view_trigger = view_trigger_cursor;
            if (view_trigger->tileX == query_tile_x) {
                if ((((view_trigger->tileZ)) == query_tile_z) && (((view_trigger->floorLayer)) == query_floor_layer)
                    && (((view_trigger->mode)) == camera_distance_mode)
                    && (((view_trigger->viewDirection)) == (((v))))) {

                    effect_id_cursor = view_trigger->effectIds;
                    for (j = 0; j < view_trigger->effectCount; ++j, ++effect_id_cursor) {
                        ((void (*)(int))Field_DisableRoomGeometryGroup)(*effect_id_cursor);
                    }
                    return 1;
                }
            }
            j = view_trigger->effectCount + ((4 - (view_trigger->effectCount & 3)) & 3) + 4;
            i += j;
            view_trigger_cursor = (Field_MpdViewTrigger*)((char*)view_trigger_cursor + j);
        }
    }
    return 0;
}

// decomp: BATTLE.PRG func_8008C40C (src/BATTLE/BATTLE.PRG/146C.c:16449)
void Field_ClearGeometryOcclusion(void)
{
    int geometry_group_count;
    int i;
    int* geometry_group_cursor;

    geometry_group_cursor = Field_roomData.geometry + 1;
    if (Field_roomData.geometry != 0) {
        geometry_group_count = Field_roomData.geometry[0];
        for (i = 0; i < geometry_group_count; ++i) {
            *geometry_group_cursor &= ~0x1000;
            geometry_group_cursor += 16;
        }
    }
}

// decomp: BATTLE.PRG func_8008C458 (src/BATTLE/BATTLE.PRG/146C.c:16465)
void Field_SampleRoomGroundColor(int packed_world_position, int layer, Render_ColorCode* color)
{
    (void)layer;
    Render_SampleRoomLighting(packed_world_position, 0, color);
}

