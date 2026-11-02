#pragma once

namespace VS::Movie {

// Live bindings of the intro movie core to the title module's playing flag,
// start tick and page index, the native movie player and the display.
void StartLiveIntroMovie();
void StopLiveIntroMovie();
void StopLiveIntroMovieAfterAudioTail();
int PlayLiveIntroMovie();

}  // namespace VS::Movie
