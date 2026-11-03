#pragma once
// Hand-maintained (tools/port/owned.csv).

#include "Actors/TextureAnimation.hpp"

struct Actors_ActorRecord;

// Bindings from the actor records, the shared sequence bank's texture page
// and row and the texture slot tables to the typed texture animation
// (TextureAnimation.cpp), in the original argument order for the generated
// callers (tools/port/copy_fixups.py OWNED_CALL_BINDINGS). The moves are
// queued in the frame's overlay bucket.
namespace VS::Actors {
// Returns -1 for an empty actor slot or a channel past 1, else 0. `command`
// is a TextureAnimationCommand; another value changes nothing.
int SetLiveActorTextureAnimation(int actor_id, int channel, int timeline,
                                 int command);
void UpdateLiveActorTextureAnimations(Actors_ActorRecord* actor);
void UploadLiveActorTextureFrame(Actors_ActorRecord* actor, int channel,
                                 int frame);
// Copies frame 0 of both channels for each actor with a row in `bank_index`,
// when that is the shared bank.
void ResetLiveActorTextureFrames(int bank_index);
void UploadLiveActorTextureOverlay(int actor_id, TextureOverlayCopy copy);
// The room pass carries its actor index. The menu preview (MAINMENU
// func_800F9EB8) carried its camera matrix pointer, which no byte width
// matches.
void UpdateLiveActorTextureScrolling(
    Actors_ActorRecord* actor, int carried_height = kNoCarriedScrollHeight);
}  // namespace VS::Actors
