#pragma once

namespace VS::Movie::Intro {

// Live bindings of the intro movie core to the title module's playing flag,
// start tick and page index, the native movie player and the display.
void StartLive();
void StopLive();
void StopLiveAfterAudioTail();
int PlayLive();

}  // namespace VS::Movie::Intro
