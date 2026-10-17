// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_MOVIE_INTRO_PLAYBACK_H_TYPES
#define VS_MOVIE_INTRO_PLAYBACK_H_TYPES
#include "Legacy/Psx.h"

typedef struct Movie_Intro_MdecState Movie_Intro_MdecState;

// decomp: MovieData_t (src/TITLE/TITLE.PRG/22C.c:3413)
typedef struct Movie_Intro_MdecState {
    u_long* encodedBuffers[2];
    int encodedBufferIndex;
    void* decodedBuffers[2];
    int decodedBufferIndex;
    RECT frameRects[2];
    u_int frameRectIndex;
    RECT sliceRect;
    int frameComplete;
} Movie_Intro_MdecState;

#endif // VS_MOVIE_INTRO_PLAYBACK_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_MOVIE_INTRO_PLAYBACK_H_DECLS)
#define VS_MOVIE_INTRO_PLAYBACK_H_DECLS

extern u_int Movie_Intro_playbackStartTick;;
extern int Movie_Intro_isPlaying;;
extern int Movie_Intro_frameHeight;
extern int Movie_Intro_frameWidth;
void Movie_Intro_InitializeFrameBuffers(Movie_Intro_MdecState* arg0, short x0, short y0, short x1, int y1);
void Movie_Intro_InitializeStream(DslLOC* loc, void (*dctoutCb)(void));
void Movie_Intro_OnDecodedStrip(void);
int Movie_Intro_DecodeNextFrame(Movie_Intro_MdecState* movie);
u_short* Movie_Intro_ReadNextFrame(Movie_Intro_MdecState* movie);
void Movie_Intro_WaitForDecodedFrame(Movie_Intro_MdecState* movie, int arg1 __attribute__((unused)));
void Movie_Intro_StartDiscStream(DslLOC* loc);
void Movie_Intro_Playback_ResetTitleStatics(void);

#endif // VS_MOVIE_INTRO_PLAYBACK_H_DECLS
