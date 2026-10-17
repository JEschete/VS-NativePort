// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_STATE_ROOMMEMORY_H_TYPES
#define VS_STATE_ROOMMEMORY_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_STATE_ROOMMEMORY_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_STATE_ROOMMEMORY_H_DECLS)
#define VS_STATE_ROOMMEMORY_H_DECLS
#include "State/RoomCache.h"

extern int State_currentRoomStateSlot;
extern State_RoomEnemyStates State_recentRoomEnemyStates[4];

#endif // VS_STATE_ROOMMEMORY_H_DECLS
