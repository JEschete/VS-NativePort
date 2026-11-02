#pragma once

#include <cstdint>

namespace VS::Movie {

// TITLE.PRG _playIntroMovie shows the picture for 0x8F5 ticks after the start;
// the soundtrack runs on under the title screen until 4401 ticks.
inline constexpr std::uint32_t kIntroPictureTicks = 0x8F5;
inline constexpr std::uint32_t kIntroSoundtrackTicks = 4401;

// The picture alternates between the upper and lower 224-line page halves.
enum class IntroPicturePage : std::uint8_t { kUpper, kLower };

enum class IntroMovieResult : std::uint8_t { kFinished, kSkipped };

// Platform and state services the intro movie control flow needs. The live
// binding keeps the title module's playing flag, start tick and page index.
class IntroMovieHost {
 public:
  IntroMovieHost() = default;
  IntroMovieHost(const IntroMovieHost&) = delete;
  IntroMovieHost& operator=(const IntroMovieHost&) = delete;
  IntroMovieHost(IntroMovieHost&&) = delete;
  IntroMovieHost& operator=(IntroMovieHost&&) = delete;
  virtual ~IntroMovieHost() = default;

  [[nodiscard]] virtual std::uint32_t Tick() const = 0;
  virtual void WaitForVerticalBlank() = 0;
  virtual void UpdateControllers() = 0;
  [[nodiscard]] virtual bool SkipRequested() const = 0;

  virtual void ClearPicturePages() = 0;
  virtual void OpenMovie() = 0;
  virtual void CloseMovie() = 0;
  virtual void DrawFrame(std::int32_t elapsed_ticks, IntroPicturePage page) = 0;
  virtual void ShowPage(IntroPicturePage page) = 0;
  virtual void EnableDisplay() = 0;

  [[nodiscard]] virtual bool IsPlaying() const = 0;
  virtual void SetPlaying(bool playing) = 0;
  [[nodiscard]] virtual std::uint32_t StartTick() const = 0;
  virtual void SetStartTick(std::uint32_t tick) = 0;
  [[nodiscard]] virtual IntroPicturePage DrawPage() const = 0;
  virtual void SetDrawPage(IntroPicturePage page) = 0;
};

void StartIntroMovie(IntroMovieHost& host);
void StopIntroMovie(IntroMovieHost& host);
void StopIntroMovieAfterAudioTail(IntroMovieHost& host);
IntroMovieResult PlayIntroMovie(IntroMovieHost& host);

}  // namespace VS::Movie
