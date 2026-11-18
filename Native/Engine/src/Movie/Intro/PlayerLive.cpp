// Hand-maintained live binding for Movie/Intro/Player.cpp.
#include <cstdint>

#include "Input/Buttons.hpp"
#include "Input/ControllerLive.hpp"
#include "Input/ControllerLiveState.hpp"
#include "Legacy/Psx.h"
#include "Movie/Intro/Playback.hpp"
#include "Movie/IntroPlayer.hpp"
#include "Movie/IntroPlayerLive.hpp"
#include "Movie/MoviePlayer.hpp"

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
    return (VS::Input::buttonsPressed & (PADRright | PADstart)) != 0;
  }

  void ClearPicturePages() override {
    RECT rect;
    setRECT(&rect, 0, 0, kPageWidth, 2 * kPictureHeight);
    ClearImage(&rect, 0, 0, 0);
  }
  void OpenMovie() override { VS::Movie::PlayerOpen("MOV/TITLE.STR"); }
  void CloseMovie() override { VS::Movie::PlayerClose(); }
  void DrawFrame(std::int32_t elapsed_ticks, PicturePage page) override {
    VS::Movie::PlayerDrawFrame(elapsed_ticks, 0, PageY(page));
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
    return VS::Movie::Intro::isPlaying != 0;
  }
  void SetPlaying(bool playing) override {
    VS::Movie::Intro::isPlaying = playing ? 1 : 0;
  }
  [[nodiscard]] std::uint32_t StartTick() const override {
    return VS::Movie::Intro::playbackStartTick;
  }
  void SetStartTick(std::uint32_t tick) override {
    VS::Movie::Intro::playbackStartTick = tick;
  }
  // The original kept the page in its MDEC state's frame index; Start resets it
  // to the upper page each time the intro opens.
  [[nodiscard]] PicturePage DrawPage() const override { return draw_page_; }
  void SetDrawPage(PicturePage page) override { draw_page_ = page; }

 private:
  PicturePage draw_page_ = PicturePage::kUpper;
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
