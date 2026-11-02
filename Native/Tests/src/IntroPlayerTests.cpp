#include <cstdint>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Movie/IntroPlayer.hpp"

namespace {
using VS::Movie::Intro::PicturePage;
using VS::Movie::Intro::PlaybackResult;

std::string PageName(PicturePage page) {
  return page == PicturePage::kUpper ? "upper" : "lower";
}

// Each vertical-blank wait advances the clock by one tick.
class FakeIntroHost final : public VS::Movie::Intro::Host {
 public:
  [[nodiscard]] std::uint32_t Tick() const override { return tick; }
  void WaitForVerticalBlank() override { ++tick; }
  void UpdateControllers() override {
    ++polls;
    skip = skip_at_poll != 0 && polls == skip_at_poll;
  }
  [[nodiscard]] bool SkipRequested() const override { return skip; }

  void ClearPicturePages() override { events.emplace_back("clear"); }
  void OpenMovie() override { events.emplace_back("open"); }
  void CloseMovie() override { events.emplace_back("close"); }
  void DrawFrame(std::int32_t elapsed_ticks, PicturePage page) override {
    events.push_back("draw " + std::to_string(elapsed_ticks) + " " +
                     PageName(page));
  }
  void ShowPage(PicturePage page) override {
    events.push_back("show " + PageName(page));
  }
  void EnableDisplay() override { events.emplace_back("enable"); }

  [[nodiscard]] bool IsPlaying() const override { return playing; }
  void SetPlaying(bool value) override {
    playing = value;
    events.emplace_back(value ? "playing" : "stopped");
  }
  [[nodiscard]] std::uint32_t StartTick() const override { return start; }
  void SetStartTick(std::uint32_t value) override { start = value; }
  [[nodiscard]] PicturePage DrawPage() const override { return page; }
  void SetDrawPage(PicturePage value) override { page = value; }

  std::uint32_t tick = 0;
  std::uint32_t start = 0;
  bool playing = false;
  PicturePage page = PicturePage::kLower;
  int polls = 0;
  int skip_at_poll = 0;
  bool skip = false;
  std::vector<std::string> events;
};
}  // namespace

TEST_CASE("Intro movie start clears both pages before opening and timing",
          "[movie][intro][core]") {
  FakeIntroHost host;
  host.tick = 1234;
  VS::Movie::Intro::Start(host);
  CHECK(host.events == std::vector<std::string>{"clear", "open", "playing"});
  CHECK(host.page == PicturePage::kUpper);
  CHECK(host.start == 1234);
  CHECK(host.playing);
}

TEST_CASE("Intro soundtrack stops only after its 4401-tick tail",
          "[movie][intro][core]") {
  FakeIntroHost host;
  host.playing = true;
  host.start = 100;
  host.tick = 100 + VS::Movie::Intro::kSoundtrackTicks - 1;
  VS::Movie::Intro::StopAfterAudioTail(host);
  CHECK(host.events.empty());

  host.tick = 100 + VS::Movie::Intro::kSoundtrackTicks;
  VS::Movie::Intro::StopAfterAudioTail(host);
  CHECK(host.events == std::vector<std::string>{"stopped", "close"});

  host.events.clear();
  host.tick += 1000;
  VS::Movie::Intro::StopAfterAudioTail(host);
  CHECK(host.events.empty());
}

TEST_CASE("Intro picture alternates pages and enables the display late",
          "[movie][intro][core]") {
  FakeIntroHost host;
  host.page = PicturePage::kUpper;
  host.skip_at_poll = 3;
  CHECK(VS::Movie::Intro::Play(host) == PlaybackResult::kSkipped);
  CHECK(host.events == std::vector<std::string>{"draw 0 upper", "show upper",
                                                "draw 1 lower", "show lower",
                                                "enable", "draw 2 upper",
                                                "show upper", "enable"});
  CHECK(host.page == PicturePage::kLower);
}

TEST_CASE("Intro picture finishes at 0x8F5 ticks before showing that frame",
          "[movie][intro][core]") {
  FakeIntroHost host;
  host.page = PicturePage::kUpper;
  host.start = 10;
  host.tick = 10 + VS::Movie::Intro::kPictureTicks - 2;
  CHECK(VS::Movie::Intro::Play(host) == PlaybackResult::kFinished);
  CHECK(host.events == std::vector<std::string>{"draw 2291 upper", "show upper",
                                                "draw 2292 lower"});
  CHECK(host.tick == 10 + VS::Movie::Intro::kPictureTicks);
}

TEST_CASE("Intro picture treats a start tick ahead of the clock as expired",
          "[movie][intro][core]") {
  // The draw sees the wrapped delta as a negative tick count; the unsigned
  // comparison then ends the picture at once.
  FakeIntroHost host;
  host.page = PicturePage::kUpper;
  host.start = 5;
  host.tick = 3;
  CHECK(VS::Movie::Intro::Play(host) == PlaybackResult::kFinished);
  CHECK(host.events == std::vector<std::string>{"draw -2 upper"});
}
