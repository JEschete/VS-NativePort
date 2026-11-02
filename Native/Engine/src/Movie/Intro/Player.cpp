// Hand-maintained (tools/port/owned.csv).
// The intro movie (TITLE.STR), modernized. The original streamed the STR from
// the disc and decoded it with the MDEC into the page space; natively the
// converted movie (VS.Extract) is decoded by Engine\Movie behind the
// Host. These keep the original's control flow and timing
// (22C.c:3649-3730): Play shows the picture until 0x8F5 ticks after the start
// or until Start/Circle skips it; the soundtrack runs on under the title
// screen until StopAfterAudioTail, 4401 ticks after the start.
#include <bit>
#include <cstdint>

#include "Movie/IntroPlayer.hpp"

namespace VS::Movie::Intro {
namespace {

std::uint32_t ElapsedTicks(const Host& host) {
  return host.Tick() - host.StartTick();
}

PicturePage OtherPage(PicturePage page) {
  return page == PicturePage::kUpper ? PicturePage::kLower
                                     : PicturePage::kUpper;
}

}  // namespace

// decomp: TITLE.PRG _initIntroMovie (src/TITLE/TITLE.PRG/22C.c:3649)
// The original cleared both picture pages when the first frame's size became
// known.
void Start(Host& host) {
  host.ClearPicturePages();
  host.SetDrawPage(PicturePage::kUpper);
  host.OpenMovie();
  host.SetPlaying(true);
  host.SetStartTick(host.Tick());
}

// decomp: TITLE.PRG _stopIntroMovie (src/TITLE/TITLE.PRG/22C.c:3670)
void Stop(Host& host) {
  host.SetPlaying(false);
  host.CloseMovie();
}

// decomp: TITLE.PRG _stopMovieIfComplete (src/TITLE/TITLE.PRG/22C.c:3686)
void StopAfterAudioTail(Host& host) {
  if (host.IsPlaying() && ElapsedTicks(host) >= kSoundtrackTicks) {
    Stop(host);
  }
}

// decomp: TITLE.PRG _playIntroMovie (src/TITLE/TITLE.PRG/22C.c:3693)
// Each pass draws into the current page, flips the draw page, waits for the
// vertical blank and polls the pads; the drawn page is shown and the display
// enabled from the second shown frame. Elapsed ticks wrap like the original's
// unsigned VSync difference.
PlaybackResult Play(Host& host) {
  bool shown_first = false;
  while (true) {
    const PicturePage page = host.DrawPage();
    host.DrawFrame(std::bit_cast<std::int32_t>(ElapsedTicks(host)), page);
    host.SetDrawPage(OtherPage(page));
    host.WaitForVerticalBlank();
    host.UpdateControllers();

    if (ElapsedTicks(host) >= kPictureTicks) {
      return PlaybackResult::kFinished;
    }

    host.ShowPage(page);
    if (shown_first) {
      host.EnableDisplay();
    }
    shown_first = true;

    if (host.SkipRequested()) {
      return PlaybackResult::kSkipped;
    }
  }
}

}  // namespace VS::Movie::Intro
