#pragma once
// The opening movie's playback state, shared by the intro player
// (PlayerLive.cpp) and the title menu that waits on it (TitleMenuLive.cpp).
#include <cstdint>

namespace VS::Movie::Intro {
// decomp: _introMovieDisplayedAt (src/TITLE/TITLE.PRG/22C.c)
extern std::uint32_t playbackStartTick;
// decomp: _introMoviePlaying (src/TITLE/TITLE.PRG/22C.c)
extern std::int32_t isPlaying;
}  // namespace VS::Movie::Intro
