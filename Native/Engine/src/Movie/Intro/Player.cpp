// Hand-maintained (tools/port/owned.csv).
// The intro movie (TITLE.STR), modernized. The original streamed the STR from
// the disc and decoded it with the MDEC into the page space; natively the
// converted movie (VS.Extract) is decoded by Engine\Movie. These keep the
// original's control flow and timing (22C.c:3649-3730): Play shows the picture
// until 0x8F5 ticks after the start or until Start/Circle skips it; the
// soundtrack runs on under the title screen until StopAfterAudioTail, 4401
// ticks after the start.
extern "C" {
#include "Movie/Intro/Player.h"

#include <stdint.h>
#include <string.h>

#include "Input/Buttons.h"
#include "Input/Controller.h"
#include "Legacy/Psx.h"
#include "Movie/Intro.h"
#include "Movie/Intro/Playback.h"
#include "Movie/MoviePlayer.h"

#define VS_INTRO_PICTURE_TICKS 0x8F5
#define VS_INTRO_SOUNDTRACK_TICKS 4401

enum {
  kPictureWidth = 320,
  kPictureHeight = 224,
  kScreenHeight = 240,
  kRgbChannels = 3,
  kPageWidth = kPictureWidth * kRgbChannels / static_cast<int>(sizeof(uint16_t))
};

// decomp: TITLE.PRG _initIntroMovie (src/TITLE/TITLE.PRG/22C.c:3649)
void Movie_Intro_Start(void) {
  RECT rect;

  // the original cleared both picture buffers when the first frame's size
  // became known
  setRECT(&rect, 0, 0, kPageWidth, 2 * kPictureHeight);
  ClearImage(&rect, 0, 0, 0);
  Movie_Intro_legacyMdecState.frameRectIndex = 0;
  Movie_PlayerOpen("MOV/TITLE.STR");
  Movie_Intro_isPlaying = 1;
  Movie_Intro_playbackStartTick = VSync(-1);
}

// decomp: TITLE.PRG _stopIntroMovie (src/TITLE/TITLE.PRG/22C.c:3670)
void Movie_Intro_Stop(void) {
  Movie_Intro_isPlaying = 0;
  Movie_PlayerClose();
}

// decomp: TITLE.PRG _stopMovieIfComplete (src/TITLE/TITLE.PRG/22C.c:3686)
void Movie_Intro_StopAfterAudioTail(void) {
  if ((Movie_Intro_isPlaying != 0) &&
      ((VSync(-1) - Movie_Intro_playbackStartTick) >=
       VS_INTRO_SOUNDTRACK_TICKS)) {
    Movie_Intro_Stop();
  }
}

// decomp: TITLE.PRG _playIntroMovie (src/TITLE/TITLE.PRG/22C.c:3693)
int Movie_Intro_Play(void) {
  DISPENV disp;
  int frames = 0;

  while (1) {
    // the picture alternates between two 480x224 halves of the page space,
    // 24-bit
    const u_int target = Movie_Intro_legacyMdecState.frameRectIndex;
    const uint32_t elapsed_bits = VSync(-1) - Movie_Intro_playbackStartTick;
    int32_t elapsed;
    memcpy(&elapsed, &elapsed_bits, sizeof elapsed);
    Movie_PlayerDrawFrame(elapsed, 0, target == 0 ? 0 : kPictureHeight);
    Movie_Intro_legacyMdecState.frameRectIndex = target == 0;
    VSync(0);
    Input_UpdateControllers();

    if ((VSync(-1) - Movie_Intro_playbackStartTick) >= VS_INTRO_PICTURE_TICKS) {
      return 0;
    }

    SetDefDispEnv(&disp, 0, target == 0 ? 0 : kPictureHeight, kPageWidth,
                  kPictureHeight);
    disp.screen.h = kPictureHeight;
    disp.screen.y = (kScreenHeight - kPictureHeight) / 2;
    disp.isrgb24 = 1;
    disp.disp.w = static_cast<int16_t>((disp.disp.w * 2) / kRgbChannels);
    PutDispEnv(&disp);

    if (++frames != 1) {
      SetDispMask(1);
    }

    if ((Input_buttonsPressed.allButtons & (PADRright | PADstart))) {
      return 1;
    }
  }
  return 0;
}
}  // extern "C"
