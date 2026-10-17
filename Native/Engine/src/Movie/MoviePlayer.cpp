#include "Movie/MoviePlayer.h"

#include "Core/Log.hpp"

// The converted-movie decoder (dav1d + libwebm + FLAC) arrives with the movie milestone; until then the movie
// functions keep their timing and show black.

extern "C" int Movie_PlayerOpen(const char* discPath)
{
    VS_LOG_WARN("movie", "{}: movie playback not available yet", discPath);
    return 0;
}

extern "C" int Movie_PlayerDrawFrame(int ticks, int x, int y)
{
    (void)ticks;
    (void)x;
    (void)y;
    return 0;
}

extern "C" void Movie_PlayerClose(void) {}
