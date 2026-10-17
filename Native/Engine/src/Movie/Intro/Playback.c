// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Content/DiscLayout.h"
#include "Core/Memory/Heap.h"
#include "Input/Buttons.h"
#include "Input/Controller.h"
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
void Movie_Intro_InitializeFrameBuffers(Movie_Intro_MdecState* arg0, short x0, short y0, short x1, int y1)
{
    arg0->frameRects[0].x = x0;
    arg0->sliceRect.x = x0;
    arg0->frameRects[0].y = y0;
    arg0->sliceRect.y = y0;
    arg0->encodedBufferIndex = 0;
    arg0->decodedBufferIndex = 0;
    arg0->frameRects[1].x = x1;
    arg0->frameRectIndex = 0;
    arg0->sliceRect.w = 24;
    arg0->frameComplete = 0;
    arg0->encodedBuffers[0] = Movie_Intro_legacyEncodedFrameZero;
    arg0->encodedBuffers[1] = Movie_Intro_legacyEncodedFrameOne;
    arg0->decodedBuffers[0] = Movie_Intro_legacyDecodedStripZero;
    arg0->decodedBuffers[1] = Movie_Intro_legacyDecodedStripOne;
    arg0->frameRects[1].y = y1;
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
    int i;

    i = 2000;

    while ((frameData = (u_long*)Movie_Intro_ReadNextFrame(movie)) == 0) {
        if (--i == 0) {
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
    int i;
    u_int temp_v1;

    for (i = 2000; StGetNext((u_long**)&addr, (u_long**)&header) != 0;) {
        if (--i == 0) {
            return 0;
        }
    }

    if ((Movie_Intro_frameWidth != header->width) || (Movie_Intro_frameHeight != header->height)) {
        setRECT(&rect, 0, 0, 480, 448);
        ClearImage(&rect, 0, 0, 0);
        Movie_Intro_frameWidth = header->width;
        Movie_Intro_frameHeight = header->height;
    }
    temp_v1 = Movie_Intro_frameWidth * 3;
    movie->frameRects[1].w = (int)(temp_v1 + (temp_v1 >> 31)) >> 1;
    movie->frameRects[0].w = (int)(temp_v1 + (temp_v1 >> 31)) >> 1;
    movie->frameRects[1].h = Movie_Intro_frameHeight;
    movie->frameRects[0].h = Movie_Intro_frameHeight;
    movie->sliceRect.h = Movie_Intro_frameHeight;
    return addr;
}

// decomp: TITLE.PRG _waitForFrame (src/TITLE/TITLE.PRG/22C.c:3542)
void Movie_Intro_WaitForDecodedFrame(Movie_Intro_MdecState* movie, int arg1 __attribute__((unused)))
{
    volatile int sp0[2];
    int* new_var;
    int i = movie->frameComplete;
    sp0[0] = 0x800000;

    while ((*(new_var = &i)) == 0) {
        if ((--sp0[0]) == 0) {
            movie->frameComplete = 1;
            movie->frameRectIndex = movie->frameRectIndex == 0;
            movie->sliceRect.x = movie->frameRects[movie->frameRectIndex].x;
            movie->sliceRect.y = movie->frameRects[movie->frameRectIndex].y;
        }
        i = movie->frameComplete;
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

// decomp: TITLE.PRG _initIntroMovie (src/TITLE/TITLE.PRG/22C.c:3649)
void Movie_Intro_Start(void)
{
    Movie_Intro_legacyDiscStreamingMode = DslModeStream2 | DslModeSpeed | DslModeRT;
    DsIntToPos(VS_TITLE_STR_LBA, &Movie_Intro_legacyDiscStreamLocation);
    Movie_Intro_legacyDiscRingBuffer = Core_Memory_AllocateFromLowEnd(0x20000);
    Movie_Intro_legacyEncodedFrameZero = Core_Memory_AllocateFromLowEnd(0x23000);
    Movie_Intro_legacyEncodedFrameOne = Core_Memory_AllocateFromLowEnd(0x23000);
    Movie_Intro_legacyDecodedStripZero = Core_Memory_AllocateFromLowEnd(0x2A00);
    Movie_Intro_legacyDecodedStripOne = Core_Memory_AllocateFromLowEnd(0x2A00);
    Movie_Intro_InitializeFrameBuffers(&Movie_Intro_legacyMdecState, 0, 0, 0, 224);
    Movie_Intro_InitializeStream(&Movie_Intro_legacyDiscStreamLocation, Movie_Intro_OnDecodedStrip);
    DecDCTvlcBuild(Movie_Intro_legacyVlcDecodeTable);

    while (Movie_Intro_DecodeNextFrame(&Movie_Intro_legacyMdecState) == -1) {
        DslLOC loc = Movie_Intro_legacyDiscStreamLocation;
        Movie_Intro_StartDiscStream(&loc);
    }
    Movie_Intro_isPlaying = 1;
    Movie_Intro_playbackStartTick = VSync(-1);
}

// decomp: TITLE.PRG _stopIntroMovie (src/TITLE/TITLE.PRG/22C.c:3670)
void Movie_Intro_Stop(void)
{
    SpuSetCommonMasterVolume(0, 0);
    Movie_Intro_isPlaying = 0;
    DecDCToutCallback(0);
    StUnSetRing();
    while (DsControlB(DslPause, 0, 0) == 0)
        ;
    Core_Memory_FreeFromLowEnd(Movie_Intro_legacyDecodedStripOne);
    Core_Memory_FreeFromLowEnd(Movie_Intro_legacyDecodedStripZero);
    Core_Memory_FreeFromLowEnd(Movie_Intro_legacyEncodedFrameOne);
    Core_Memory_FreeFromLowEnd(Movie_Intro_legacyEncodedFrameZero);
    Core_Memory_FreeFromLowEnd(Movie_Intro_legacyDiscRingBuffer);
    SpuSetCommonMasterVolume(0x3FFF, 0x3FFF);
}

// decomp: TITLE.PRG _stopMovieIfComplete (src/TITLE/TITLE.PRG/22C.c:3686)
void Movie_Intro_StopAfterAudioTail(void)
{
    if ((Movie_Intro_isPlaying != 0) && ((VSync(-1) - Movie_Intro_playbackStartTick) >= 4401)) {
        Movie_Intro_Stop();
    }
}

// decomp: TITLE.PRG _playIntroMovie (src/TITLE/TITLE.PRG/22C.c:3693)
int Movie_Intro_Play(void)
{
    DslLOC loc;
    DISPENV disp;
    int var_s2;
    u_int dataSize;

    var_s2 = 0;

    while (1) {
        DecDCTin(Movie_Intro_legacyMdecState.encodedBuffers[Movie_Intro_legacyMdecState.encodedBufferIndex], 3);
        dataSize = Movie_Intro_legacyMdecState.sliceRect.w * Movie_Intro_legacyMdecState.sliceRect.h;
        DecDCTout(Movie_Intro_legacyMdecState.decodedBuffers[Movie_Intro_legacyMdecState.decodedBufferIndex],
            (int)(dataSize + (dataSize >> 0x1F)) >> 1);

        while (Movie_Intro_DecodeNextFrame(&Movie_Intro_legacyMdecState) == -1) {
            if ((u_int)(StGetBackloc(&loc) - 1) >= 0x44CU) {
                loc = Movie_Intro_legacyDiscStreamLocation;
            }
            Movie_Intro_StartDiscStream(&loc);
        }

        Movie_Intro_WaitForDecodedFrame(&Movie_Intro_legacyMdecState, 0);
        VSync(0);
        Input_UpdateControllers();

        if ((VSync(-1) - Movie_Intro_playbackStartTick) >= 0x8F5) {
            return 0;
        }

        SetDefDispEnv(&disp, 0, Movie_Intro_legacyMdecState.frameRectIndex == 0 ? 224 : 0, 480, 224);
        disp.screen.h = 224;
        disp.screen.y = 8;
        disp.isrgb24 = 1;
        disp.disp.w = (disp.disp.w * 2) / 3;
        PutDispEnv(&disp);

        if (++var_s2 != 1) {
            SetDispMask(1);
        }

        if ((Input_buttonsPressed.allButtons & (PADRright | PADstart))) {
            return 1;
        }
    }
    return 0;
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

