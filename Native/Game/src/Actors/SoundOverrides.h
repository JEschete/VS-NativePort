#pragma once
#ifndef VS_OWNED_ACTORS_SOUNDOVERRIDES_H
#define VS_OWNED_ACTORS_SOUNDOVERRIDES_H

typedef struct Actors_ActorRecord Actors_ActorRecord;

#ifdef __cplusplus
extern "C" {
#endif

void Audio_UpdateActorSoundTransition(Actors_ActorRecord* actor);

#ifdef __cplusplus
}
#endif
#endif  // VS_OWNED_ACTORS_SOUNDOVERRIDES_H
