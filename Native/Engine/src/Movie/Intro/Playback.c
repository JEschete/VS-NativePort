// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Movie/Intro.h"
#include "Movie/Intro/Playback.h"

// decomp: _introMovieDisplayedAt (_introMovieDisplayedAt@src/TITLE/TITLE.PRG/22C.c)
u_int Movie_Intro_playbackStartTick;

// decomp: _introMoviePlaying (_introMoviePlaying@src/TITLE/TITLE.PRG/22C.c)
int Movie_Intro_isPlaying;

// decomp: _movieHeight (_movieHeight@TITLE/TITLE.PRG)
int Movie_Intro_frameHeight = 0;

// decomp: _movieWidth (_movieWidth@TITLE/TITLE.PRG)
int Movie_Intro_frameWidth = 0;

// decomp: TITLE.PRG _initMovieData (src/TITLE/TITLE.PRG/22C.c:3433)
void Movie_Intro_InitializeFrameBuffers(Movie_Intro_MdecState* movie_state, short x0, short y0, short x1, int y1)
{
    movie_state->frameRects[0].x = x0;
    movie_state->sliceRect.x = x0;
    movie_state->frameRects[0].y = y0;
    movie_state->sliceRect.y = y0;
    movie_state->encodedBufferIndex = 0;
    movie_state->decodedBufferIndex = 0;
    movie_state->frameRects[1].x = x1;
    movie_state->frameRectIndex = 0;
    movie_state->sliceRect.w = 24;
    movie_state->frameComplete = 0;
    movie_state->encodedBuffers[0] = Movie_Intro_legacyEncodedFrameZero;
    movie_state->encodedBuffers[1] = Movie_Intro_legacyEncodedFrameOne;
    movie_state->decodedBuffers[0] = Movie_Intro_legacyDecodedStripZero;
    movie_state->decodedBuffers[1] = Movie_Intro_legacyDecodedStripOne;
    movie_state->frameRects[1].y = y1;
}

// decomp: TITLE.PRG _initMoviePlayback (src/TITLE/TITLE.PRG/22C.c:3452)
void Movie_Intro_InitializeStream(DslLOC* loc, void (*dctoutCb)(void))
{
    DecDCToutCallback(dctoutCb);
    StSetRing(Movie_Intro_legacyDiscRingBuffer, 64);
    StSetStream(1, 1, -1, NULL, NULL);
    Movie_Intro_StartDiscStream(loc);
}

// decomp: TITLE.PRG _decDCToutCallback (src/TITLE/TITLE.PRG/22C.c:3461)
void Movie_Intro_OnDecodedStrip(void)
{
    RECT rect;
    short rightEdge;
    int index;
    u_int dataSize;

    extern int Movie_legacyStreamInterruptFlag;
    if (Movie_legacyStreamInterruptFlag != 0) {
        Movie_legacyStreamInterruptFlag = 0;
    }

    index = Movie_Intro_legacyMdecState.decodedBufferIndex;
    rect = Movie_Intro_legacyMdecState.sliceRect;
    rightEdge = Movie_Intro_legacyMdecState.sliceRect.x + Movie_Intro_legacyMdecState.sliceRect.w;
    Movie_Intro_legacyMdecState.decodedBufferIndex = Movie_Intro_legacyMdecState.decodedBufferIndex == 0;
    Movie_Intro_legacyMdecState.sliceRect.x = rightEdge;

    if (rightEdge < (Movie_Intro_legacyMdecState.frameRects[Movie_Intro_legacyMdecState.frameRectIndex].x
                     + Movie_Intro_legacyMdecState.frameRects[Movie_Intro_legacyMdecState.frameRectIndex].w)) {
        dataSize = Movie_Intro_legacyMdecState.sliceRect.w * Movie_Intro_legacyMdecState.sliceRect.h;
        DecDCTout(Movie_Intro_legacyMdecState.decodedBuffers[Movie_Intro_legacyMdecState.decodedBufferIndex],
            ((int)(dataSize + (dataSize >> 31))) >> 1);
    } else {
        Movie_Intro_legacyMdecState.frameComplete = 1;
        Movie_Intro_legacyMdecState.frameRectIndex = Movie_Intro_legacyMdecState.frameRectIndex < 1;
        Movie_Intro_legacyMdecState.sliceRect.x = Movie_Intro_legacyMdecState.frameRects[Movie_Intro_legacyMdecState.frameRectIndex].x;
        Movie_Intro_legacyMdecState.sliceRect.y = Movie_Intro_legacyMdecState.frameRects[Movie_Intro_legacyMdecState.frameRectIndex].y;
    }
    LoadImage(&rect, Movie_Intro_legacyMdecState.decodedBuffers[index]);
}

// decomp: TITLE.PRG _decodeNextMovieFrame (src/TITLE/TITLE.PRG/22C.c:3494)
int Movie_Intro_DecodeNextFrame(Movie_Intro_MdecState* movie)
{
    u_long* frameData;
    int poll_attempts_left;

    poll_attempts_left = 2000;

    while ((frameData = (u_long*)Movie_Intro_ReadNextFrame(movie)) == 0) {
        if (--poll_attempts_left == 0) {
            return -1;
        }
    }

    movie->encodedBufferIndex = movie->encodedBufferIndex == 0;
    DecDCTvlc2(frameData, movie->encodedBuffers[movie->encodedBufferIndex], Movie_Intro_legacyVlcDecodeTable);
    StFreeRing(frameData);
    return 0;
}

// decomp: TITLE.PRG _getNextMovieFrame (src/TITLE/TITLE.PRG/22C.c:3513)
u_short* Movie_Intro_ReadNextFrame(Movie_Intro_MdecState* movie)
{
    RECT rect;
    void* addr;
    StHEADER* header;
    int poll_attempts_left;
    u_int rgb_row_bytes;

    for (poll_attempts_left = 2000; StGetNext((u_long**)&addr, (u_long**)&header) != 0;) {
        if (--poll_attempts_left == 0) {
            return 0;
        }
    }

    if ((Movie_Intro_frameWidth != header->width) || (Movie_Intro_frameHeight != header->height)) {
        setRECT(&rect, 0, 0, 480, 448);
        ClearImage(&rect, 0, 0, 0);
        Movie_Intro_frameWidth = header->width;
        Movie_Intro_frameHeight = header->height;
    }
    rgb_row_bytes = Movie_Intro_frameWidth * 3;
    movie->frameRects[1].w = (int)(rgb_row_bytes + (rgb_row_bytes >> 31)) >> 1;
    movie->frameRects[0].w = (int)(rgb_row_bytes + (rgb_row_bytes >> 31)) >> 1;
    movie->frameRects[1].h = Movie_Intro_frameHeight;
    movie->frameRects[0].h = Movie_Intro_frameHeight;
    movie->sliceRect.h = Movie_Intro_frameHeight;
    return addr;
}

// decomp: TITLE.PRG _waitForFrame (src/TITLE/TITLE.PRG/22C.c:3542)
void Movie_Intro_WaitForDecodedFrame(Movie_Intro_MdecState* movie, int arg1 __attribute__((unused)))
{
    volatile int decode_spin_countdown[2];
    int* frame_complete_alias;
    int frame_complete = movie->frameComplete;
    decode_spin_countdown[0] = 0x800000;

    while ((*(frame_complete_alias = &frame_complete)) == 0) {
        if ((--decode_spin_countdown[0]) == 0) {
            movie->frameComplete = 1;
            movie->frameRectIndex = movie->frameRectIndex == 0;
            movie->sliceRect.x = movie->frameRects[movie->frameRectIndex].x;
            movie->sliceRect.y = movie->frameRects[movie->frameRectIndex].y;
        }
        frame_complete = movie->frameComplete;
    }

    movie->frameComplete = 0;
}

// decomp: TITLE.PRG _playMovie (src/TITLE/TITLE.PRG/22C.c:3562)
void Movie_Intro_StartDiscStream(DslLOC* loc)
{
    u_char params[4];

    params[0] = DslModeSpeed;
    params[1] = 0;
    params[2] = 0;
    params[3] = 0;
    do {
        while (DsControl(DslSetloc, (u_char*)loc, NULL) == 0)
            ;
        while (DsControl(DslSetmode, params, NULL) == 0)
            ;
        VSync(3);
    } while (DsRead2(loc, Movie_Intro_legacyDiscStreamingMode) == 0);
}

// TITLE.PRG reloads restore these statics' initial values (PORTING_WATCHLIST.md §1).
void Movie_Intro_Playback_ResetTitleStatics(void)
{
    {
        static const int initial = 0;
        memcpy((void*)&Movie_Intro_frameHeight, &initial, sizeof Movie_Intro_frameHeight);
    }
    {
        static const int initial = 0;
        memcpy((void*)&Movie_Intro_frameWidth, &initial, sizeof Movie_Intro_frameWidth);
    }
}

