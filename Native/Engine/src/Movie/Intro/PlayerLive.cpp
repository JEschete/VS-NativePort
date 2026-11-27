// Hand-maintained live binding for Movie/Intro/Player.cpp.
#include <cstdint>
#include <optional>
#include <utility>

#include "Core/Log.hpp"
#include "Input/Actions.hpp"
#include "Input/Buttons.hpp"
#include "Input/ControllerLive.hpp"
#include "Input/ControllerLiveState.hpp"
#include "Legacy/Psx.hpp"
#include "Movie/Intro/Playback.hpp"
#include "Movie/IntroPlayer.hpp"
#include "Movie/IntroPlayerLive.hpp"
#include "Movie/MoviePlayer.hpp"
#include "Platform/LogicalTicks.hpp"

using VS::Legacy::ClearImage;
using VS::Legacy::DISPENV;
using VS::Legacy::PutDispEnv;
using VS::Legacy::RECT;
using VS::Legacy::SetDefDispEnv;
using VS::Legacy::SetDispMask;
using VS::Legacy::setRECT;

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
    return VS::Platform::ElapsedTicks();
  }
  void WaitTick() override { VS::Platform::WaitTicks(); }
  void UpdateControllers() override { VS::Input::UpdateLiveControllers(); }
  [[nodiscard]] bool SkipRequested() const override {
    return (VS::Input::buttonsPressed &
            (VS::Input::Circle | VS::Input::Start)) != 0;
  }

  void ClearPicturePages() override {
    RECT rect;
    setRECT(&rect, 0, 0, kPageWidth, 2 * kPictureHeight);
    ClearImage(&rect, 0, 0, 0);
  }
  void OpenMovie() override {
    auto opened = VS::Movie::PlaybackSession::Open("MOV/TITLE.STR");
    if (!opened) {
      VS::Core::LogError("movie", "cannot start intro: {}", opened.error());
      session_.reset();
      frame_ = {};
      return;
    }
    session_.emplace(std::move(*opened));
  }
  void CloseMovie() override {
    frame_ = {};
    session_.reset();
  }
  void DrawFrame(std::int32_t elapsed_ticks, PicturePage) override {
    if (!session_) return;
    auto update = session_->Update(static_cast<std::uint32_t>(elapsed_ticks));
    if (!update) {
      VS::Core::LogError("movie", "intro playback failed: {}", update.error());
      frame_ = {};
      session_.reset();
      return;
    }
    frame_ = update->video;
  }
  void PumpAudioTail() {
    if (!session_) return;
    auto update = session_->PumpAudio(Tick() - StartTick());
    if (!update) {
      VS::Core::LogError("movie", "intro soundtrack update failed: {}",
                         update.error());
      frame_ = {};
      session_.reset();
    }
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

  void SetPresenting(bool presenting) { presenting_ = presenting; }
  [[nodiscard]] VS::Movie::VideoFrameView CurrentVideoFrame() const {
    return presenting_ ? frame_ : VS::Movie::VideoFrameView{};
  }

 private:
  std::optional<VS::Movie::PlaybackSession> session_;
  VS::Movie::VideoFrameView frame_;
  bool presenting_ = false;
  PicturePage draw_page_ = PicturePage::kUpper;
};

LiveHost& Instance() {
  static LiveHost host;
  return host;
}

}  // namespace

void StartLive() {
  Instance().SetPresenting(false);
  Start(Instance());
}

void StopLive() {
  Instance().SetPresenting(false);
  Stop(Instance());
}

void StopLiveAfterAudioTail() {
  LiveHost& host = Instance();
  host.PumpAudioTail();
  StopAfterAudioTail(host);
}

int PlayLive() {
  LiveHost& host = Instance();
  host.SetPresenting(true);
  const PlaybackResult result = Play(host);
  host.SetPresenting(false);
  return result == PlaybackResult::kSkipped ? 1 : 0;
}

VideoFrameView CurrentVideoFrame() { return Instance().CurrentVideoFrame(); }

}  // namespace VS::Movie::Intro
