#pragma once

#include <cstdint>

namespace VS::Movie::Intro {

// TITLE.PRG _playIntroMovie shows the picture for 0x8F5 ticks after the start;
// the soundtrack runs on under the title screen until 4401 ticks.
inline constexpr std::uint32_t kPictureTicks = 0x8F5;
inline constexpr std::uint32_t kSoundtrackTicks = 4401;

// The picture alternates between the upper and lower 224-line page halves.
enum class PicturePage : std::uint8_t { kUpper, kLower };

enum class PlaybackResult : std::uint8_t { kFinished, kSkipped };

// Platform and state services the intro movie control flow needs. The live
// binding keeps the title module's playing flag, start tick and page index.
class Host {
 public:
  Host() = default;
  Host(const Host&) = delete;
  Host& operator=(const Host&) = delete;
  Host(Host&&) = delete;
  Host& operator=(Host&&) = delete;
  virtual ~Host() = default;

  [[nodiscard]] virtual std::uint32_t Tick() const = 0;
  virtual void WaitForVerticalBlank() = 0;
  virtual void UpdateControllers() = 0;
  [[nodiscard]] virtual bool SkipRequested() const = 0;

  virtual void ClearPicturePages() = 0;
  virtual void OpenMovie() = 0;
  virtual void CloseMovie() = 0;
  virtual void DrawFrame(std::int32_t elapsed_ticks, PicturePage page) = 0;
  virtual void ShowPage(PicturePage page) = 0;
  virtual void EnableDisplay() = 0;

  [[nodiscard]] virtual bool IsPlaying() const = 0;
  virtual void SetPlaying(bool playing) = 0;
  [[nodiscard]] virtual std::uint32_t StartTick() const = 0;
  virtual void SetStartTick(std::uint32_t tick) = 0;
  [[nodiscard]] virtual PicturePage DrawPage() const = 0;
  virtual void SetDrawPage(PicturePage page) = 0;
};

void Start(Host& host);
void Stop(Host& host);
void StopAfterAudioTail(Host& host);
PlaybackResult Play(Host& host);

}  // namespace VS::Movie::Intro
