// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_FIELD_INTERACTIONS_H_TYPES
#define VS_FIELD_INTERACTIONS_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_FIELD_INTERACTIONS_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_FIELD_INTERACTIONS_H_DECLS)
#define VS_FIELD_INTERACTIONS_H_DECLS
#include "Field/InteractionFocusLive.h"

extern Field_InteractionFocus Field_interactionFocus;
extern u_int Field_controlMode;
void Field_ApplyUnlockEffectToRoomObject(SVECTOR* target_position);
int Field_CheckAshleyTileTriggers(int actorId, u_char* inputPosition);

#endif // VS_FIELD_INTERACTIONS_H_DECLS
