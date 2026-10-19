// Hand-maintained (tools/port/owned.csv).
#include "Actors/MovementCommands.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>

#include "Actors/ActorRecord.h"
#include "Actors/Animation.h"
#include "Legacy/Psx.h"

// decomp: BATTLE.PRG func_800B64A8 (src/BATTLE/BATTLE.PRG/4A0A8.c:415).
void Actors_SetDirectMovement(int actor_id, int direction, int speed) {
  Actors_QueueNavigationTileTarget(actor_id, direction, speed, direction);
  Actors_actorRecords[actor_id]->turnState.mode = 0;
}

// decomp: BATTLE.PRG func_800B64CC (src/BATTLE/BATTLE.PRG/4A0A8.c:428).
void Actors_QueueNavigationTileTarget(int actor_id, int direction, int distance,
                                      int facing) {
  if ((unsigned)actor_id >=
          sizeof Actors_actorRecords / sizeof Actors_actorRecords[0] ||
      Actors_actorRecords[actor_id] == NULL) {
    Core_Debugf("actors", "Movement command references missing actor %d",
                actor_id);
    abort();
  }
  Actors_MovementRequest* request = &Actors_actorRecords[actor_id]->turnState;
  enum { kOrientationBits = sizeof(int16_t) * CHAR_BIT };
  const u_int orientation =
      (u_int)direction | ((u_int)facing << kOrientationBits);
  request->mode = 1;
  request->reserved1 = 0;
  request->reserved2 = 0;
  request->reserved3 = 0;
  request->direction = (int16_t)orientation;
  request->facing = (int16_t)(orientation >> kOrientationBits);
  request->distance = (u_int)distance;
}
