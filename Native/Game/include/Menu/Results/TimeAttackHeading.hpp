#pragma once
// Hand-maintained (tools/port/owned.csv).
// MENUF.PRG time-attack start screen: the "TIME ATTACK" heading, the course
// number, the boss name, the best time and the start countdown
// (F03018-F03022). The draws turn atlas widths, a reveal timer and the course
// into flash-fade, fade and time draws for a TimeAttackDrawHost, so the cores
// reach no global state. The end screen's palette pulse (F03023) is in
// ResultsPalette.hpp.

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "Menu/Results/ResultsDrawing.hpp"
#include "Render/DisplayControl.hpp"
#include "Render/TextureRegionDraw.hpp"

namespace VS::Menu::Results {
// The heading, course, boss name and best time clamp their timer to at most
// 64 and draw nothing at zero or less.
inline constexpr std::int32_t kTimeAttackRevealSteps = 64;

// Atlas entries (disIndexAttack0*, 3B8.c); entries 1-8 are the course digits.
inline constexpr std::size_t kTimeAttackCourseLabel = 84;
inline constexpr std::size_t kTimeAttackFirstBossName = 85;
inline constexpr std::size_t kTimeAttackDeath = 91;
inline constexpr std::size_t kTimeAttackOgreZombie = 93;
inline constexpr std::size_t kTimeAttackBestTimeLabel = 95;
inline constexpr std::size_t kTimeAttackCountdownThree = 96;
// "TIME" and "ATTACK".
inline constexpr std::size_t kTimeAttackHeading = 100;

// Course 6 fights Death and the Ogre Zombie: its name is two entries.
inline constexpr std::uint8_t kTimeAttackPairedCourse = 6;
// vs_main_scoredata.bossTimeTrialScores has a row per course.
inline constexpr std::size_t kTimeAttackCourses = 8;
// A trial score keeps its time in the low 24 bits.
inline constexpr std::uint32_t kTrialScoreTimeMask = 0xFFFFFF;
// getClut(832, 511): the course digit's palette.
inline constexpr std::int32_t kTimeAttackCourseClut = 0x7FF4;

// The countdown shows each number for this many frames, fading in again from
// brightness zero.
inline constexpr std::int32_t kTimeAttackCountdownFrames = 15;

// The left and right colors of one flash-fade draw; the left color's step
// drives it.
using TimeAttackColors = std::array<Render::RegionColor, 2>;

// Where the time-attack draws go. The texture index is always inside the
// atlas the draw was given.
class TimeAttackDrawHost {
 public:
  TimeAttackDrawHost() = default;
  TimeAttackDrawHost(const TimeAttackDrawHost&) = delete;
  TimeAttackDrawHost& operator=(const TimeAttackDrawHost&) = delete;
  TimeAttackDrawHost(TimeAttackDrawHost&&) = delete;
  TimeAttackDrawHost& operator=(TimeAttackDrawHost&&) = delete;
  virtual ~TimeAttackDrawHost() = default;
  // _renderTexturePopIn.
  virtual void DrawFlashFade(ResultsPoint origin, std::size_t texture_index,
                             const TimeAttackColors& colors) = 0;
  // _renderTextureFadeIn, with a brightness above zero.
  virtual void DrawFade(ResultsPoint origin, std::size_t texture_index,
                        std::int32_t brightness) = 0;
  // _renderTextureFadeInClut.
  virtual void DrawFadeClut(ResultsPoint origin, std::size_t texture_index,
                            std::int32_t brightness, std::int32_t clut) = 0;
  // _renderTime, never in the new-record palette.
  virtual void DrawTime(ResultsPoint origin, std::int32_t timer,
                        std::int32_t time) = 0;
};

// "TIME" and "ATTACK" flashed in side by side, centred on `x`. kHidden at a
// timer of zero or less; kInvalidTexture for an atlas without the heading.
ResultsDrawResult DrawTimeAttackHeading(
    std::span<const Render::TextureRegion> atlas, std::int32_t x,
    std::int32_t y, std::int32_t timer, TimeAttackDrawHost& host);

// "COURSE" and the digit `course` + 1 in the course palette, centred on `x`.
// A digit outside the atlas refuses with kInvalidTexture before drawing.
ResultsDrawResult DrawCourseNumber(std::span<const Render::TextureRegion> atlas,
                                   std::int32_t x, std::int32_t y,
                                   std::int32_t timer, std::uint8_t course,
                                   TimeAttackDrawHost& host);

// The course's boss name centred on `x`; course 6 draws "DEATH" and "OGRE
// ZOMBIE" side by side. A name outside the atlas refuses with
// kInvalidTexture.
ResultsDrawResult DrawBossName(std::span<const Render::TextureRegion> atlas,
                               std::int32_t x, std::int32_t y,
                               std::int32_t timer, std::uint8_t course,
                               TimeAttackDrawHost& host);

// "BEST TIME" and the course's first-place time from `first_place_scores`
// (one packed trial score per course), centred on `x`. A course without a
// score refuses with kInvalidTexture before drawing.
ResultsDrawResult DrawBestTime(
    std::span<const Render::TextureRegion> atlas, std::int32_t x,
    std::int32_t y, std::int32_t timer, std::uint8_t course,
    std::span<const std::uint32_t> first_place_scores,
    TimeAttackDrawHost& host);

// The countdown number for `timer` (a new number every 15 frames from "3")
// centred on (x, y) and faded in by the frames since it appeared. A number
// outside the atlas refuses with kInvalidTexture; kHidden on the number's
// first frame and for a negative timer's non-positive remainder.
ResultsDrawResult DrawStartCountdown(
    std::span<const Render::TextureRegion> atlas, std::int32_t x,
    std::int32_t y, std::int32_t timer, TimeAttackDrawHost& host);
}  // namespace VS::Menu::Results
