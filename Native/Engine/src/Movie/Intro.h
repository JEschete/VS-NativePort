// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_MOVIE_INTRO_H_TYPES
#define VS_MOVIE_INTRO_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_MOVIE_INTRO_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_MOVIE_INTRO_H_DECLS)
#define VS_MOVIE_INTRO_H_DECLS
#include "Movie/Intro/Playback.h"

extern int Movie_legacyStreamInterruptFlag;
extern void* Movie_Intro_legacyDecodedStripZero;;
extern void* Movie_Intro_legacyDecodedStripOne;;
extern int Movie_Intro_legacyDiscStreamingMode;;
extern void* Movie_Intro_legacyEncodedFrameZero;;
extern void* Movie_Intro_legacyEncodedFrameOne;;
extern DslLOC Movie_Intro_legacyDiscStreamLocation;;
extern Movie_Intro_MdecState Movie_Intro_legacyMdecState;;
extern u_long* Movie_Intro_legacyDiscRingBuffer;;
extern DECDCTTAB Movie_Intro_legacyVlcDecodeTable;;

#endif // VS_MOVIE_INTRO_H_DECLS
