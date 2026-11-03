// Hand-maintained live binding for Movie/Intro/Player.cpp.
#include <cstdint>

#include "Input/ControllerLive.hpp"
#include "Movie/IntroPlayer.hpp"
#include "Movie/IntroPlayerLive.hpp"
extern "C" {
#include "Input/Buttons.h"
#include "Input/ControllerLiveState.h"
#include "Legacy/Psx.h"
#include "Movie/Intro.h"
#include "Movie/Intro/Playback.h"
#include "Movie/MoviePlayer.h"
}

namespace VS::Movie::Intro {
namespace {

constexpr int kPictureWidth = 320;
constexpr int kPictureHeight = 224;
constexpr int kScreenHeight = 240;
constexpr int kRgbChannels = 3;
constexpr int kPageUnitBytes = 2;
// 24-bit pixels in 16-bit page units.
constexpr int kPageWidth = kPictureWidth * kRgbChannels / kPageUnitBytes;

int PageY(PicturePage page) {
  return page == PicturePage::kUpper ? 0 : kPictureHeight;
}

class LiveHost final : public Host {
 public:
  [[nodiscard]] std::uint32_t Tick() const override {
    return static_cast<std::uint32_t>(VSync(-1));
  }
  void WaitForVerticalBlank() override { VSync(0); }
  void UpdateControllers() override { VS::Input::UpdateLiveControllers(); }
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
  void DrawFrame(std::int32_t elapsed_ticks, PicturePage page) override {
    Movie_PlayerDrawFrame(elapsed_ticks, 0, PageY(page));
  }
  void ShowPage(PicturePage page) override {
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
  [[nodiscard]] PicturePage DrawPage() const override {
    return Movie_Intro_legacyMdecState.frameRectIndex == 0
               ? PicturePage::kUpper
               : PicturePage::kLower;
  }
  void SetDrawPage(PicturePage page) override {
    Movie_Intro_legacyMdecState.frameRectIndex =
        page == PicturePage::kUpper ? 0U : 1U;
  }
};

LiveHost& Instance() {
  static LiveHost host;
  return host;
}

}  // namespace

void StartLive() { Start(Instance()); }

void StopLive() { Stop(Instance()); }

void StopLiveAfterAudioTail() { StopAfterAudioTail(Instance()); }

int PlayLive() { return Play(Instance()) == PlaybackResult::kSkipped ? 1 : 0; }

}  // namespace VS::Movie::Intro
