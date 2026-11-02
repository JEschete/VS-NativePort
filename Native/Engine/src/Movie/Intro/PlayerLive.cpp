// Hand-maintained live binding for Movie/Intro/Player.cpp.
#include <cstdint>

#include "Movie/IntroPlayer.hpp"
#include "Movie/IntroPlayerLive.hpp"
extern "C" {
#include "Input/Buttons.h"
#include "Input/Controller.h"
#include "Legacy/Psx.h"
#include "Movie/Intro.h"
#include "Movie/Intro/Playback.h"
#include "Movie/MoviePlayer.h"
}

namespace VS::Movie {
namespace {

constexpr int kPictureWidth = 320;
constexpr int kPictureHeight = 224;
constexpr int kScreenHeight = 240;
constexpr int kRgbChannels = 3;
constexpr int kPageUnitBytes = 2;
// 24-bit pixels in 16-bit page units.
constexpr int kPageWidth = kPictureWidth * kRgbChannels / kPageUnitBytes;

int PageY(IntroPicturePage page) {
  return page == IntroPicturePage::kUpper ? 0 : kPictureHeight;
}

class LiveIntroMovieHost final : public IntroMovieHost {
 public:
  [[nodiscard]] std::uint32_t Tick() const override {
    return static_cast<std::uint32_t>(VSync(-1));
  }
  void WaitForVerticalBlank() override { VSync(0); }
  void UpdateControllers() override { Input_UpdateControllers(); }
  [[nodiscard]] bool SkipRequested() const override {
    return (Input_buttonsPressed.allButtons & (PADRright | PADstart)) != 0;
  }

  void ClearPicturePages() override {
    RECT rect;
    setRECT(&rect, 0, 0, kPageWidth, 2 * kPictureHeight);
    ClearImage(&rect, 0, 0, 0);
  }
  void OpenMovie() override { Movie_PlayerOpen("MOV/TITLE.STR"); }
  void CloseMovie() override { Movie_PlayerClose(); }
  void DrawFrame(std::int32_t elapsed_ticks, IntroPicturePage page) override {
    Movie_PlayerDrawFrame(elapsed_ticks, 0, PageY(page));
  }
  void ShowPage(IntroPicturePage page) override {
    DISPENV disp;
    SetDefDispEnv(&disp, 0, static_cast<std::int16_t>(PageY(page)), kPageWidth,
                  kPictureHeight);
    disp.screen.h = kPictureHeight;
    disp.screen.y = (kScreenHeight - kPictureHeight) / 2;
    disp.isrgb24 = 1;
    disp.disp.w = static_cast<std::int16_t>((disp.disp.w * 2) / kRgbChannels);
    PutDispEnv(&disp);
  }
  void EnableDisplay() override { SetDispMask(1); }

  [[nodiscard]] bool IsPlaying() const override {
    return Movie_Intro_isPlaying != 0;
  }
  void SetPlaying(bool playing) override {
    Movie_Intro_isPlaying = playing ? 1 : 0;
  }
  [[nodiscard]] std::uint32_t StartTick() const override {
    return Movie_Intro_playbackStartTick;
  }
  void SetStartTick(std::uint32_t tick) override {
    Movie_Intro_playbackStartTick = tick;
  }
  // The original tests the index against zero, so any nonzero index is the
  // lower page.
  [[nodiscard]] IntroPicturePage DrawPage() const override {
    return Movie_Intro_legacyMdecState.frameRectIndex == 0
               ? IntroPicturePage::kUpper
               : IntroPicturePage::kLower;
  }
  void SetDrawPage(IntroPicturePage page) override {
    Movie_Intro_legacyMdecState.frameRectIndex =
        page == IntroPicturePage::kUpper ? 0U : 1U;
  }
};

LiveIntroMovieHost& Host() {
  static LiveIntroMovieHost host;
  return host;
}

}  // namespace

void StartLiveIntroMovie() { StartIntroMovie(Host()); }

void StopLiveIntroMovie() { StopIntroMovie(Host()); }

void StopLiveIntroMovieAfterAudioTail() {
  StopIntroMovieAfterAudioTail(Host());
}

int PlayLiveIntroMovie() {
  return PlayIntroMovie(Host()) == IntroMovieResult::kSkipped ? 1 : 0;
}

}  // namespace VS::Movie
