// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Movie/Intro/Playback.h"

// decomp: _introMovieDisplayedAt (_introMovieDisplayedAt@src/TITLE/TITLE.PRG/22C.c)
u_int Movie_Intro_playbackStartTick;

// decomp: _introMoviePlaying (_introMoviePlaying@src/TITLE/TITLE.PRG/22C.c)
int Movie_Intro_isPlaying;

}  // extern "C"
