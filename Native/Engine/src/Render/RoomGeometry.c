// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Field/Room.h"
#include "Render/Render.h"
#include "Render/RoomGeometry.h"

// decomp: BATTLE.PRG func_8009723C (src/BATTLE/BATTLE.PRG/2EA3C.s:0) [asm-file: not yet ported]
void Render_DrawRoomGeometry(void*, int)
{
    VS_UNPORTED("Render::DrawRoomGeometry");
}

// decomp: BATTLE.PRG func_80097388 (src/BATTLE/BATTLE.PRG/2EA3C.s:0) [asm-file: not yet ported]
int Render_DrawRoomGeometryPolygons()
{
    VS_UNPORTED("Render::DrawRoomGeometryPolygons");
    return 0;
}

// decomp: BATTLE.PRG func_8009797C (src/BATTLE/BATTLE.PRG/2EA3C.s:0) [asm-file: not yet ported]
int Render_DecodeRoomPolygonVertices()
{
    VS_UNPORTED("Render::DecodeRoomPolygonVertices");
    return 0;
}

// decomp: BATTLE.PRG func_80097A10 (src/BATTLE/BATTLE.PRG/2EA3C.s:0) [asm-file: not yet ported]
int Render_ProjectAndDecodeRoomPolygon()
{
    VS_UNPORTED("Render::ProjectAndDecodeRoomPolygon");
    return 0;
}

// decomp: BATTLE.PRG func_80097AEC (src/BATTLE/BATTLE.PRG/2EA3C.s:0) [asm-file: not yet ported]
int Render_SubdivideRoomTriangle()
{
    VS_UNPORTED("Render::SubdivideRoomTriangle");
    return 0;
}

// decomp: BATTLE.PRG func_80097CF0 (src/BATTLE/BATTLE.PRG/2EA3C.s:0) [asm-file: not yet ported]
int Render_SubdivideRoomQuad()
{
    VS_UNPORTED("Render::SubdivideRoomQuad");
    return 0;
}

// decomp: BATTLE.PRG func_80097F70 (src/BATTLE/BATTLE.PRG/2EA3C.s:0) [asm-file: not yet ported]
int Render_InterpolateRoomVertexMidpoint()
{
    VS_UNPORTED("Render::InterpolateRoomVertexMidpoint");
    return 0;
}

// decomp: BATTLE.PRG func_80098014 (src/BATTLE/BATTLE.PRG/2EA3C.s:0) [asm-file: not yet ported]
int Render_IsRoomPolygonOnScreen()
{
    VS_UNPORTED("Render::IsRoomPolygonOnScreen");
    return 0;
}

// decomp: BATTLE.PRG func_800980F8 (src/BATTLE/BATTLE.PRG/2EA3C.s:0) [asm-file: not yet ported]
int Render_LinkRoomPolygonByDepth()
{
    VS_UNPORTED("Render::LinkRoomPolygonByDepth");
    return 0;
}

// decomp: BATTLE.PRG func_80098160 (src/BATTLE/BATTLE.PRG/2EA3C.s:0) [asm-file: not yet ported]
void Render_SetRoomPolygonNearClip(int)
{
    VS_UNPORTED("Render::SetRoomPolygonNearClip");
}

// decomp: BATTLE.PRG func_8009817C (src/BATTLE/BATTLE.PRG/2EA3C.s:0) [asm-file: not yet ported]
int Render_SetRoomPolygonFarClip()
{
    VS_UNPORTED("Render::SetRoomPolygonFarClip");
    return 0;
}

// decomp: BATTLE.PRG func_80098194 (src/BATTLE/BATTLE.PRG/2EA3C.s:0) [asm-file: not yet ported]
void Render_SetRoomTextureLodDepth(int)
{
    VS_UNPORTED("Render::SetRoomTextureLodDepth");
}

// decomp: BATTLE.PRG func_800981B8 (src/BATTLE/BATTLE.PRG/2EA3C.s:0) [asm-file: not yet ported]
int Render_SetRoomPolygonScreenBounds()
{
    VS_UNPORTED("Render::SetRoomPolygonScreenBounds");
    return 0;
}

// decomp: BATTLE.PRG func_8009820C (src/BATTLE/BATTLE.PRG/2EA3C.s:0) [asm-file: not yet ported]
void Render_DrawSkyDome(void*)
{
    VS_UNPORTED("Render::DrawSkyDome");
}

// decomp: BATTLE.PRG func_800983F8 (src/BATTLE/BATTLE.PRG/2EA3C.s:0) [asm-file: not yet ported]
void Render_UpdateRoomGroupTransforms(void*)
{
    VS_UNPORTED("Render::UpdateRoomGroupTransforms");
}

// decomp: BATTLE.PRG func_800985AC (src/BATTLE/BATTLE.PRG/2EA3C.s:0) [asm-file: not yet ported]
void Render_BuildRoomGeometryViewMatrix(VECTOR*, VECTOR*, MATRIX*)
{
    VS_UNPORTED("Render::BuildRoomGeometryViewMatrix");
}

// decomp: BATTLE.PRG func_80098648 (src/BATTLE/BATTLE.PRG/2EA3C.s:0) [asm-file: not yet ported]
void Render_BuildScaleCompensatedRoomView(VECTOR*, VECTOR*, MATRIX*)
{
    VS_UNPORTED("Render::BuildScaleCompensatedRoomView");
}

// decomp: BATTLE.PRG func_80098B38 (src/BATTLE/BATTLE.PRG/2EA3C.s:0) [asm-file: not yet ported]
void Render_SampleRoomLighting(int, int)
{
    VS_UNPORTED("Render::SampleRoomLighting");
}

// decomp: BATTLE.PRG func_80098C3C (src/BATTLE/BATTLE.PRG/2EA3C.s:0) [asm-file: not yet ported]
int Render_InterpolateRoomLightTriangle()
{
    VS_UNPORTED("Render::InterpolateRoomLightTriangle");
    return 0;
}

// decomp: BATTLE.PRG func_80098D6C (src/BATTLE/BATTLE.PRG/2EA3C.s:0) [asm-file: not yet ported]
void Render_TintRoomGeometryPrimitives(void*, void*, Field_RoomTint*, int)
{
    VS_UNPORTED("Render::TintRoomGeometryPrimitives");
}

// decomp: BATTLE.PRG func_80098E50 (src/BATTLE/BATTLE.PRG/2EA3C.s:0) [asm-file: not yet ported]
int Render_TintRoomPrimitiveColor()
{
    VS_UNPORTED("Render::TintRoomPrimitiveColor");
    return 0;
}

// decomp: BATTLE.PRG vs_battle_getGeomOffset (src/BATTLE/BATTLE.PRG/2EA3C.s:0) [asm-file: not yet ported]
void Render_GetProjectionScreenOffset(Render_ScreenOffset*)
{
    VS_UNPORTED("Render::GetProjectionScreenOffset");
}

// decomp: BATTLE.PRG vs_battle_setGeomOffset (src/BATTLE/BATTLE.PRG/2EA3C.s:0) [asm-file: not yet ported]
void Render_SetProjectionScreenOffset(Render_ScreenOffset*)
{
    VS_UNPORTED("Render::SetProjectionScreenOffset");
}

