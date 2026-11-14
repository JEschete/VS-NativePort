#include <algorithm>
#include <array>
#include <climits>
#include <cstdint>
#include <iterator>
#include <optional>
#include <span>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Effects/PrimaryEffect.hpp"
#include "Effects/PrimaryEffectLive.hpp"
extern "C" {
#include "Effects/Runtime.h"
}

namespace {
namespace fx = VS::Effects;
using fx::EffectTokenPool;
using fx::PrimaryEffectPhase;
using fx::PrimaryEffectState;

constexpr std::uint32_t kFullWord = 0xFFFFFFFE;

PrimaryEffectState Playing(std::int32_t count, std::int32_t started,
                           std::uint32_t pending) {
  return {.phase = PrimaryEffectPhase::kPlayingTracks,
          .track_count = count,
          .started_tracks = started,
          .pending_tracks = pending};
}

// Saves and restores the primary effect globals and the token words the Live
// entries read.
class PrimaryEffectGlobals {
 public:
  PrimaryEffectGlobals() {
    std::ranges::copy(Tokens(), tokens_.begin());
    std::ranges::fill(Tokens(), 0);
  }
  PrimaryEffectGlobals(const PrimaryEffectGlobals&) = delete;
  PrimaryEffectGlobals& operator=(const PrimaryEffectGlobals&) = delete;
  ~PrimaryEffectGlobals() {
    Effects_primaryPhase = phase_;
    Effects_trackCount = count_;
    Effects_startedTrackCount = started_;
    Effects_pendingTracks = pending_;
    Effects_primaryLoaded = loaded_;
    std::ranges::copy(tokens_, Tokens().begin());
  }

  static std::span<int, fx::kEffectTokenWords> Tokens() {
    return std::span(Effects_tokenBitmaps).first<fx::kEffectTokenWords>();
  }

 private:
  u_int phase_ = Effects_primaryPhase;
  int count_ = Effects_trackCount;
  int started_ = Effects_startedTrackCount;
  u_int pending_ = Effects_pendingTracks;
  char loaded_ = Effects_primaryLoaded;
  std::array<int, fx::kEffectTokenWords> tokens_{};
};
}  // namespace

TEST_CASE("F01427 a track starts while the effect plays a pending track",
          "[primary-effect][F01427]") {
  PrimaryEffectState state = Playing(3, 1, 0x4);
  CHECK(fx::CanStartTrack(state));
  state.phase = PrimaryEffectPhase::kAwaitingTrack;
  CHECK(fx::CanStartTrack(state));
  CHECK(fx::CanStartTrack(Playing(0, -1, 0x80000000)));
  CHECK_FALSE(fx::CanStartTrack(Playing(3, 3, 0x4)));
  CHECK_FALSE(fx::CanStartTrack(Playing(3, 4, 0x4)));
  CHECK_FALSE(fx::CanStartTrack(Playing(3, 1, 0)));
}

TEST_CASE("F01427 no track starts outside the track phases",
          "[primary-effect][F01427]") {
  const auto raw = GENERATE(0U, 2U, 3U, 6U, 7U, 8U, 9U, 0xFFFFFFFFU);
  PrimaryEffectState state = Playing(3, 1, 0x4);
  state.phase = static_cast<PrimaryEffectPhase>(raw);
  CHECK_FALSE(fx::CanStartTrack(state));
  CHECK(fx::PlayableTrackCount(state) == 0);
}

TEST_CASE("F01428 the playable track count is the package's while playing",
          "[primary-effect][F01428]") {
  PrimaryEffectState state = Playing(5, 5, 0);
  CHECK(fx::PlayableTrackCount(state) == 5);
  state.phase = PrimaryEffectPhase::kAwaitingTrack;
  state.track_count = -3;
  CHECK(fx::PlayableTrackCount(state) == -3);
  state.phase = PrimaryEffectPhase::kLoadingPackage;
  CHECK(fx::PlayableTrackCount(state) == 0);
}

TEST_CASE("F01429 a requeued track returns to the pending mask",
          "[primary-effect][F01429]") {
  PrimaryEffectState state = Playing(4, 2, 0x1);
  REQUIRE(fx::RequeueTrack(state, 31));
  CHECK(state.started_tracks == 1);
  CHECK(state.pending_tracks == 0x80000001);
  REQUIRE(fx::RequeueTrack(state, 0));
  CHECK(state.started_tracks == 0);
  CHECK(state.pending_tracks == 0x80000001);

  state.started_tracks = INT32_MIN;
  REQUIRE(fx::RequeueTrack(state, 4));
  CHECK(state.started_tracks == INT32_MAX);
}

TEST_CASE("F01429 a track outside the pending mask is refused",
          "[primary-effect][F01429]") {
  const int track = GENERATE(-1, 32, 33, 255, INT_MIN);
  PrimaryEffectState state = Playing(4, 2, 0x1);
  CHECK_FALSE(fx::RequeueTrack(state, track));
  CHECK(state.started_tracks == 2);
  CHECK(state.pending_tracks == 0x1);
}

TEST_CASE("F01427 F01428 F01429 the Live entries read and write the globals",
          "[primary-effect][F01427][F01428][F01429]") {
  const PrimaryEffectGlobals globals;
  Effects_primaryPhase = 5;
  Effects_trackCount = 3;
  Effects_startedTrackCount = 3;
  Effects_pendingTracks = 0;
  CHECK(fx::LiveCanStartEffectTrack() == 0);
  CHECK(fx::LivePrimaryEffectTrackCount() == 3);

  fx::LiveRequeueEffectTrack(2);
  CHECK(Effects_startedTrackCount == 2);
  CHECK(Effects_pendingTracks == 0x4);
  CHECK(fx::LiveCanStartEffectTrack() == 1);

  fx::LiveRequeueEffectTrack(32);
  CHECK(Effects_startedTrackCount == 2);
  CHECK(Effects_pendingTracks == 0x4);
}

TEST_CASE("F01430 any nonzero loaded byte reports the package loaded",
          "[primary-effect][F01430]") {
  const PrimaryEffectGlobals globals;
  Effects_primaryLoaded = 0;
  CHECK(fx::LiveIsPrimaryEffectLoaded() == 0);
  Effects_primaryLoaded = 1;
  CHECK(fx::LiveIsPrimaryEffectLoaded() == 1);
  Effects_primaryLoaded = static_cast<char>(0x80);
  CHECK(fx::LiveIsPrimaryEffectLoaded() == 1);
  CHECK_FALSE(fx::IsPrimaryEffectLoaded(PrimaryEffectState{}));
}

TEST_CASE("F01438 only the runtime's phases are stored",
          "[primary-effect][F01438]") {
  CHECK(fx::ToPrimaryEffectPhase(4) == PrimaryEffectPhase::kPlayingTracks);
  CHECK(fx::ToPrimaryEffectPhase(9) == PrimaryEffectPhase::kReloadingMenu);
  CHECK_FALSE(fx::ToPrimaryEffectPhase(1).has_value());
  CHECK_FALSE(fx::ToPrimaryEffectPhase(7).has_value());
  CHECK_FALSE(fx::ToPrimaryEffectPhase(10).has_value());

  const PrimaryEffectGlobals globals;
  Effects_primaryPhase = 0;
  fx::LiveSetPrimaryEffectPhase(6);
  CHECK(Effects_primaryPhase == 6);
  fx::LiveSetPrimaryEffectPhase(7);
  fx::LiveSetPrimaryEffectPhase(-1);
  CHECK(Effects_primaryPhase == 6);
}

TEST_CASE("F01440 the phase read returns the stored number",
          "[primary-effect][F01440]") {
  PrimaryEffectState state;
  fx::SetPrimaryEffectPhase(state, PrimaryEffectPhase::kLoadingPlugins);
  CHECK(fx::PrimaryEffectPhaseOf(state) == PrimaryEffectPhase::kLoadingPlugins);

  const PrimaryEffectGlobals globals;
  Effects_primaryPhase = 3;
  CHECK(fx::LivePrimaryEffectPhase() == 3);
  // A number stored around the setter reads back unchanged.
  Effects_primaryPhase = 7;
  CHECK(fx::LivePrimaryEffectPhase() == 7);
}

TEST_CASE("F01441 tokens are the lowest free bit past bit 0 of each word",
          "[primary-effect][F01441]") {
  EffectTokenPool pool{};
  CHECK(fx::AllocateEffectToken(pool) == 1);
  CHECK(pool[0] == 0x2);
  pool[0] = 0x7;
  CHECK(fx::AllocateEffectToken(pool) == 3);
  pool = {kFullWord, 0xFFFFFFFF, 0x1, 0};
  CHECK(fx::AllocateEffectToken(pool) == 65);
  CHECK(pool == EffectTokenPool{kFullWord, 0xFFFFFFFF, 0x3, 0});
  pool = {kFullWord, kFullWord, kFullWord, 0x7FFFFFFE};
  CHECK(fx::AllocateEffectToken(pool) == 127);
}

TEST_CASE("F01441 a full pool hands out no token", "[primary-effect][F01441]") {
  EffectTokenPool pool{kFullWord, 0xFFFFFFFF, kFullWord, kFullWord};
  CHECK_FALSE(fx::AllocateEffectToken(pool).has_value());
  CHECK(pool == EffectTokenPool{kFullWord, 0xFFFFFFFF, kFullWord, kFullWord});

  const PrimaryEffectGlobals globals;
  std::ranges::fill(PrimaryEffectGlobals::Tokens(), -2);
  CHECK(fx::LiveAllocateEffectToken() == 0);
  CHECK(std::ranges::all_of(PrimaryEffectGlobals::Tokens(),
                            [](int word) { return word == -2; }));
}

TEST_CASE("F01442 a released token clears its bit",
          "[primary-effect][F01442]") {
  EffectTokenPool pool{0x6, 0x2, 0, 0x80000000};
  REQUIRE(fx::ReleaseEffectToken(pool, 33));
  CHECK(pool == EffectTokenPool{0x6, 0, 0, 0x80000000});
  REQUIRE(fx::ReleaseEffectToken(pool, 127));
  CHECK(pool == EffectTokenPool{0x6, 0, 0, 0});
  // Token 0, the none token, clears the reserved bit as the original did.
  pool[0] = 0x7;
  REQUIRE(fx::ReleaseEffectToken(pool, 0));
  CHECK(pool[0] == 0x6);
}

TEST_CASE("F01442 a token outside the pool is refused",
          "[primary-effect][F01442]") {
  const int token = GENERATE(-1, -31, -33, 128, 255, INT_MAX);
  EffectTokenPool pool{0x6, 0x2, 0x2, 0x2};
  CHECK_FALSE(fx::ReleaseEffectToken(pool, token));
  CHECK(pool == EffectTokenPool{0x6, 0x2, 0x2, 0x2});
}

TEST_CASE("F01441 F01442 the Live token pool is the token globals",
          "[primary-effect][F01441][F01442]") {
  const PrimaryEffectGlobals globals;
  const auto tokens = PrimaryEffectGlobals::Tokens();
  tokens[0] = -2;
  CHECK(fx::LiveAllocateEffectToken() == 33);
  CHECK(tokens[1] == 0x2);
  fx::LiveReleaseEffectToken(128);
  CHECK(tokens[1] == 0x2);
  fx::LiveReleaseEffectToken(33);
  CHECK(tokens[1] == 0);
  CHECK(tokens[0] == -2);
}
