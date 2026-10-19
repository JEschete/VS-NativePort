#pragma once
// Hand-maintained (tools/port/owned.csv).
#ifndef VS_OWNED_MOVIE_INTRO_PLAYER_H
#define VS_OWNED_MOVIE_INTRO_PLAYER_H
#include "Legacy/Psx.h"

void Movie_Intro_Start(void);

void Movie_Intro_Stop(void);

void Movie_Intro_StopAfterAudioTail(void);

int Movie_Intro_Play(void);

#endif  // VS_OWNED_MOVIE_INTRO_PLAYER_H
