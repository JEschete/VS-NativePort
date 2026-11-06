#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Audio/ActorSound.hpp"
#include "Audio/ActorSoundLive.hpp"
#include "Audio/Ambience.hpp"
#include "Audio/Music.hpp"
#include "Audio/MusicLive.hpp"
#include "Audio/SeqRecordsLive.hpp"
#include "Audio/Sfx.hpp"
#include "Audio/SoundBanks.hpp"
#include "Audio/SoundControl.hpp"
#include "Audio/Stream.hpp"
#include "Audio/StreamBuffers.hpp"
#include "Audio/StreamBuffersLive.hpp"
#include "Audio/StreamControl.hpp"
#include "Audio/StreamLive.hpp"
#include "Content/CdDrive.hpp"
#include "Content/ModuleData.h"

extern "C" {
#include "Audio/Cutscene/Stream.h"
#include "Audio/SoundBanks.h"
#include "DiscQueue.h"
}

namespace {
namespace Audio = VS::Audio;
namespace Content = VS::Content;
namespace Control = VS::Audio::Control;
namespace Cutscene = VS::Audio::Cutscene;
namespace Seq = VS::Audio::Seq;
using Audio::BankBuffer;
using Audio::DiscRequestRef;
using Audio::kNoMusicHandle;
using Audio::MusicPlayback;
using Audio::MusicRefusal;
using Audio::MusicSlot;
using Audio::RequestMark;
using Audio::SfxMix;
using Content::DiscRequestState;

BankBuffer Buffer(std::uintptr_t value) {
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  return static_cast<BankBuffer>(value);
}

DiscRequestRef Request(std::uintptr_t value) {
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  return static_cast<DiscRequestRef>(value);
}

std::int64_t Raw(BankBuffer buffer) {
  return static_cast<std::int64_t>(buffer);
}

std::int64_t Raw(DiscRequestRef request) {
  return static_cast<std::int64_t>(request);
}

// One host call, named, with up to three of its arguments.
struct HostCall {
  std::string_view name;
  std::int64_t first = 0;
  std::int64_t second = 0;
  std::int64_t third = 0;

  friend bool operator==(const HostCall&, const HostCall&) = default;
};

// Restores a legacy global the Live entries touch.
template <typename T>
class GlobalScope {
 public:
  explicit GlobalScope(T& global) : global_(global), saved_(global) {
    global_ = T{};
  }
  ~GlobalScope() { global_ = saved_; }
  GlobalScope(const GlobalScope&) = delete;
  GlobalScope& operator=(const GlobalScope&) = delete;
  GlobalScope(GlobalScope&&) = delete;
  GlobalScope& operator=(GlobalScope&&) = delete;

 private:
  T& global_;
  T saved_;
};

constexpr std::int32_t kSlotPastLast = 5;
// The last music file; past 127, so it tells unsigned id reads apart.
constexpr std::uint8_t kHighMusicId = 199;
constexpr std::uint32_t kPollPastLast = 5;
constexpr std::uint8_t kLowMusicId = 7;
constexpr std::uint8_t kOtherMusicId = 9;
constexpr std::uint32_t kHandle = 0x21;
constexpr std::uint32_t kOtherHandle = 0x35;
constexpr std::uintptr_t kMusicBuffer = 0x5000;
constexpr std::uintptr_t kOtherMusicBuffer = 0x6000;
constexpr std::uintptr_t kFirstAllocation = 0x9000;
constexpr std::uintptr_t kQueuedRequest = 0x700;
constexpr std::uintptr_t kRefusedRequest = 0x7F0;
constexpr std::int32_t kMusicSectorBase = 0x4000;
constexpr std::int32_t kMusicBytesBase = 0x10000;
constexpr std::int32_t kFadeTicks = 30;
constexpr std::int32_t kFadeTarget = 100;
constexpr std::int32_t kSlotVolume = 0x64;
constexpr std::int32_t kBeatPosition = 0x123;
constexpr std::int32_t kJumpThreshold = 0x40;
constexpr std::size_t kSecondSlot = 1;
constexpr std::size_t kThirdSlot = 2;
constexpr std::size_t kFourthSlot = 3;

MusicSlot HeldMusic(std::uint8_t id, std::uintptr_t buffer,
                    std::uint32_t handle = kNoMusicHandle) {
  return {.id = id, .handle = handle, .data = Buffer(buffer), .request = {}};
}

MusicSlot LoadingMusic(std::uint8_t id, std::uintptr_t buffer) {
  MusicSlot music = HeldMusic(id, buffer);
  music.request = {.mark = RequestMark::kActive,
                   .request = Request(kQueuedRequest)};
  return music;
}

class FakeMusic final : public Audio::MusicHost {
 public:
  std::array<MusicSlot, Audio::kMusicSlotCount> slots{};
  MusicPlayback playback;
  std::vector<HostCall> calls;
  std::optional<DiscRequestRef> next_request = Request(kQueuedRequest);
  // The request states the polls read in turn, then `request_state`.
  std::deque<DiscRequestState> request_states;
  DiscRequestState request_state = DiscRequestState::kReading;
  std::uint32_t next_handle = kHandle;
  std::uintptr_t next_buffer = kFirstAllocation;
  std::vector<std::uint32_t> diagnostics;
  std::vector<MusicRefusal> refusals;

  MusicSlot Music(std::size_t index) override { return slots.at(index); }
  void StoreMusic(std::size_t index, const MusicSlot& slot) override {
    slots.at(index) = slot;
  }
  MusicPlayback Playback() override { return playback; }
  void StorePlayback(const MusicPlayback& stored) override {
    playback = stored;
  }
  std::optional<Content::DiscFileExtent> MusicFile(std::int32_t id) override {
    if (id < 0 || id >= Audio::kMusicFileCount) return std::nullopt;
    return Content::DiscFileExtent{.sector = kMusicSectorBase + id,
                                   .bytes = kMusicBytesBase + id};
  }
  std::optional<DiscRequestRef> AllocateRequest(
      const Content::DiscFileExtent& file) override {
    calls.push_back({"request", file.sector, file.bytes});
    return next_request;
  }
  void EnqueueRequest(DiscRequestRef request, BankBuffer destination) override {
    calls.push_back({"enqueue", Raw(request), Raw(destination)});
  }
  DiscRequestRef CompleteRefusedRequest(const Content::DiscFileExtent& file,
                                        BankBuffer destination) override {
    calls.push_back({"complete", file.sector, Raw(destination)});
    return Request(kRefusedRequest);
  }
  DiscRequestState RequestState(DiscRequestRef request) override {
    calls.push_back({"poll", Raw(request)});
    if (request_states.empty()) return request_state;
    const DiscRequestState state = request_states.front();
    request_states.pop_front();
    return state;
  }
  void ReleaseRequest(DiscRequestRef request) override {
    calls.push_back({"release", Raw(request)});
  }
  void WaitFrame() override { calls.push_back({"wait"}); }
  BankBuffer AllocateBuffer(std::int32_t bytes) override {
    calls.push_back({"allocate", bytes});
    return Buffer(next_buffer);
  }
  void FreeBuffer(BankBuffer buffer) override {
    calls.push_back({"free", Raw(buffer)});
  }
  std::uint32_t StartMusic(BankBuffer music) override {
    calls.push_back({"start", Raw(music)});
    return next_handle;
  }
  std::uint32_t QueueMusicTransition(BankBuffer music) override {
    calls.push_back({"queue", Raw(music)});
    return next_handle;
  }
  std::uint32_t CrossfadeMusic(BankBuffer music, std::uint32_t fade_ticks,
                               std::int32_t target_volume) override {
    calls.push_back({"crossfade", Raw(music), fade_ticks, target_volume});
    return next_handle;
  }
  void StopMusic(std::uint32_t handle) override {
    calls.push_back({"stop", handle});
  }
  std::int32_t MusicVolume(std::uint32_t handle) override {
    calls.push_back({"volume", handle});
    return kSlotVolume;
  }
  std::int32_t MusicPosition() override { return kBeatPosition; }
  void SaveMusicState() override { calls.push_back({"save"}); }
  void SetMusicBranchThreshold(std::int32_t threshold) override {
    calls.push_back({"threshold", threshold});
  }
  void FadeMusicVolume(std::uint32_t handle, Control::Fade fade) override {
    calls.push_back({"fade", handle, fade.ticks, fade.target});
  }
  void FadeMusicTempoScale(Control::Fade fade) override {
    calls.push_back({"tempo", fade.ticks, fade.target});
  }
  void FadeMusicPitchScale(Control::Fade fade) override {
    calls.push_back({"pitch", fade.ticks, fade.target});
  }
  void StopAllMusic() override { calls.push_back({"stop all"}); }
  void StoreZoneMusicSwitch(std::int32_t enabled) override {
    calls.push_back({"zone", enabled});
  }
  void Diagnose(std::uint32_t code) override { diagnostics.push_back(code); }
  void Refuse(MusicRefusal refusal) override { refusals.push_back(refusal); }
};

constexpr Control::Fade kFade{.ticks = kFadeTicks, .target = kFadeTarget};
}  // namespace

TEST_CASE(
    "The music position, state save, jump threshold and zone switch reach the "
    "host",
    "[audio-music-streams][audio]") {
  FakeMusic host;
  CHECK(Audio::MusicBeatPosition(host) == kBeatPosition);
  Audio::SaveMusicPlaybackState(host);
  Audio::SetMusicJumpThreshold(host, kJumpThreshold);
  CHECK(Audio::SetZoneMusicSwitchEnabled(host, 1) == 1);
  CHECK(Audio::SetZoneMusicSwitchEnabled(host, 0) == 0);
  CHECK(host.calls ==
        std::vector<HostCall>{
            {"save"}, {"threshold", kJumpThreshold}, {"zone", 1}, {"zone", 0}});
}

TEST_CASE(
    "Stopping one music slot forgets the playing music only for that slot",
    "[audio-music-streams][audio]") {
  FakeMusic host;
  host.slots[kSecondSlot] = HeldMusic(kLowMusicId, kMusicBuffer, kHandle);
  host.playback = {.slot = 2,
                   .handle = kHandle,
                   .music_id = kLowMusicId,
                   .data = Buffer(kMusicBuffer)};
  CHECK(Audio::StopMusicSlotPlayback(host, 3) == 1);
  CHECK(host.playback.slot == 2);
  CHECK(Audio::StopMusicSlotPlayback(host, 2) == 1);
  CHECK(host.calls == std::vector<HostCall>{{"stop", kHandle}});
  CHECK(host.slots[kSecondSlot].handle == kNoMusicHandle);
  CHECK(host.slots[kSecondSlot].data == Buffer(kMusicBuffer));
  CHECK(host.playback == MusicPlayback{.music_id = kLowMusicId});
}

TEST_CASE("Stopping music slot 0 stops all music and clears every slot handle",
          "[audio-music-streams][audio]") {
  FakeMusic host;
  for (MusicSlot& slot : host.slots) slot.handle = kHandle;
  host.playback = {.slot = 1, .handle = kHandle, .data = Buffer(kMusicBuffer)};
  CHECK(Audio::StopMusicSlotPlayback(host, 0) == 1);
  CHECK(host.calls == std::vector<HostCall>{{"stop all"}});
  for (const MusicSlot& slot : host.slots) {
    CHECK(slot.handle == kNoMusicHandle);
  }
  CHECK(host.playback == MusicPlayback{});
}

TEST_CASE("Music slot stops refuse slots past the four slots",
          "[audio-music-streams][audio]") {
  FakeMusic host;
  host.slots[0] = HeldMusic(kLowMusicId, kMusicBuffer, kHandle);
  CHECK(Audio::StopMusicSlotPlayback(host, kSlotPastLast) == 0);
  CHECK(Audio::StopMusicSlotPlayback(host, -1) == 0);
  CHECK(host.calls.empty());
  CHECK(host.slots[0].handle == kHandle);
  CHECK(host.refusals == std::vector{MusicRefusal::kSlotOutOfRange,
                                     MusicRefusal::kSlotOutOfRange});
}

TEST_CASE(
    "Resuming a music slot makes it the playing slot with the queued handle",
    "[audio-music-streams][audio]") {
  FakeMusic host;
  host.slots[0] = HeldMusic(kHighMusicId, kMusicBuffer);
  CHECK(Audio::ResumeMusicSlot(host, 1) == static_cast<std::int32_t>(kHandle));
  CHECK(host.calls == std::vector<HostCall>{
                          {"queue", static_cast<std::int64_t>(kMusicBuffer)}});
  CHECK(host.slots[0].handle == kHandle);
  CHECK(host.playback == MusicPlayback{.slot = 1,
                                       .handle = kHandle,
                                       .music_id = kHighMusicId,
                                       .data = Buffer(kMusicBuffer)});
}

TEST_CASE(
    "A music slot that starts no handle keeps the previous playing handle",
    "[audio-music-streams][audio]") {
  FakeMusic host;
  host.playback.handle = kOtherHandle;
  host.slots[kSecondSlot] = HeldMusic(kLowMusicId, kOtherMusicBuffer, kHandle);
  host.next_handle = 0;
  CHECK(Audio::ResumeMusicSlot(host, 2) ==
        static_cast<std::int32_t>(kOtherHandle));
  CHECK(host.slots[kSecondSlot].handle == kHandle);
  CHECK(host.playback == MusicPlayback{.slot = 2,
                                       .handle = kOtherHandle,
                                       .music_id = kLowMusicId,
                                       .data = Buffer(kOtherMusicBuffer)});
}

TEST_CASE("Crossfading a music slot passes the fade ticks and target volume",
          "[audio-music-streams][audio]") {
  FakeMusic host;
  host.slots[kFourthSlot] = HeldMusic(kLowMusicId, kMusicBuffer);
  CHECK(Audio::CrossfadeMusicSlot(host, 4, kFadeTarget, kFadeTicks) ==
        static_cast<std::int32_t>(kHandle));
  CHECK(host.calls == std::vector<HostCall>{
                          {"crossfade", static_cast<std::int64_t>(kMusicBuffer),
                           kFadeTicks, kFadeTarget}});
  CHECK(host.playback.slot == 4);
  CHECK(host.slots[kFourthSlot].handle == kHandle);
}

TEST_CASE(
    "Playing a music slot fades the volume before and after a started handle",
    "[audio-music-streams][audio]") {
  FakeMusic host;
  host.slots[0] = HeldMusic(kLowMusicId, kMusicBuffer);
  CHECK(Audio::PlayMusicSlot(host, 1, kFade) ==
        static_cast<std::int32_t>(kHandle));
  const HostCall fade{"fade", 0, kFadeTicks, kFadeTarget};
  const HostCall start{"start", static_cast<std::int64_t>(kMusicBuffer)};
  CHECK(host.calls == std::vector{fade, start, fade});
  host.calls.clear();
  host.next_handle = 0;
  Audio::PlayMusicSlot(host, 1, kFade);
  CHECK(host.calls == std::vector{fade, start});
}

TEST_CASE(
    "Music slot players ignore slot 0 and empty slots and refuse slots past "
    "the four",
    "[audio-music-streams][audio]") {
  FakeMusic host;
  host.playback = {.slot = 2, .handle = kOtherHandle};
  CHECK(Audio::ResumeMusicSlot(host, 0) == 0);
  CHECK(Audio::PlayMusicSlot(host, 3, kFade) ==
        static_cast<std::int32_t>(kOtherHandle));
  CHECK(Audio::CrossfadeMusicSlot(host, kSlotPastLast, kFadeTarget,
                                  kFadeTicks) == 0);
  CHECK(host.calls.empty());
  CHECK(host.playback.slot == 2);
  CHECK(host.refusals == std::vector{MusicRefusal::kSlotOutOfRange});
}

TEST_CASE("The current music id reads the playing slot's id unsigned",
          "[audio-music-streams][audio]") {
  FakeMusic host;
  host.slots[kThirdSlot].id = kHighMusicId;
  CHECK(Audio::CurrentMusicId(host) == 0);
  host.playback.slot = 3;
  CHECK(Audio::CurrentMusicSlot(host) == 3);
  CHECK(Audio::CurrentMusicId(host) == kHighMusicId);
  host.playback.slot = kSlotPastLast;
  CHECK(Audio::CurrentMusicId(host) == 0);
  CHECK(host.refusals == std::vector{MusicRefusal::kSlotOutOfRange});
}

TEST_CASE(
    "Loading music refuses slots and ids it cannot load with the slot "
    "diagnostic",
    "[audio-music-streams][audio]") {
  FakeMusic host;
  CHECK(Audio::LoadMusicSlot(host, 0, 1) == 0);
  CHECK(Audio::LoadMusicSlot(host, kLowMusicId, 0) == 0);
  CHECK(Audio::LoadMusicSlot(host, kLowMusicId, kSlotPastLast) == 0);
  CHECK(Audio::LoadMusicIntoFreeSlot(host, -1) == 0);
  CHECK(host.diagnostics ==
        std::vector{Audio::kMusicSlotDiagnostic, Audio::kMusicSlotDiagnostic,
                    Audio::kMusicSlotDiagnostic, Audio::kMusicSlotDiagnostic});
  CHECK(host.calls.empty());
  CHECK(host.refusals.empty());
}

TEST_CASE(
    "Loading music past the music file table is refused before the slot is "
    "freed",
    "[audio-music-streams][audio]") {
  FakeMusic host;
  host.slots[0] = HeldMusic(kLowMusicId, kMusicBuffer, kHandle);
  CHECK(Audio::LoadMusicSlot(host, Audio::kMusicFileCount, 1) == 0);
  CHECK(host.refusals == std::vector{MusicRefusal::kMusicFileOutOfRange});
  CHECK(host.calls.empty());
  CHECK(host.diagnostics.empty());
  CHECK(host.slots[0] == HeldMusic(kLowMusicId, kMusicBuffer, kHandle));
}

TEST_CASE("Loading music queues a disc read into a fresh heap buffer",
          "[audio-music-streams][audio]") {
  FakeMusic host;
  CHECK(Audio::LoadMusicSlot(host, kHighMusicId, 2) == 2);
  const std::int32_t bytes = kMusicBytesBase + kHighMusicId;
  CHECK(host.calls == std::vector<HostCall>{
                          {"request", kMusicSectorBase + kHighMusicId, bytes},
                          {"allocate", bytes},
                          {"enqueue", static_cast<std::int64_t>(kQueuedRequest),
                           static_cast<std::int64_t>(kFirstAllocation)}});
  CHECK(host.slots[kSecondSlot] ==
        LoadingMusic(kHighMusicId, kFirstAllocation));
  CHECK(host.diagnostics.empty());
  host.calls.clear();
  CHECK(Audio::LoadMusicSlot(host, kHighMusicId, 2) == 2);
  CHECK(host.calls.empty());
}

TEST_CASE(
    "Loading other music into a held slot stops and frees the old music first",
    "[audio-music-streams][audio]") {
  FakeMusic host;
  host.slots[0] = HeldMusic(kLowMusicId, kMusicBuffer, kHandle);
  CHECK(Audio::LoadMusicSlot(host, kOtherMusicId, 1) == 1);
  const std::int32_t bytes = kMusicBytesBase + kOtherMusicId;
  CHECK(host.calls == std::vector<HostCall>{
                          {"stop", kHandle},
                          {"free", static_cast<std::int64_t>(kMusicBuffer)},
                          {"request", kMusicSectorBase + kOtherMusicId, bytes},
                          {"allocate", bytes},
                          {"enqueue", static_cast<std::int64_t>(kQueuedRequest),
                           static_cast<std::int64_t>(kFirstAllocation)}});
  CHECK(host.diagnostics == std::vector{Audio::kMusicSlotDiagnostic});
  CHECK(host.slots[0].handle == kNoMusicHandle);
  CHECK(host.slots[0].data == Buffer(kFirstAllocation));
}

TEST_CASE("Loading music over a read in flight diagnoses the held request",
          "[audio-music-streams][audio]") {
  FakeMusic host;
  host.slots[0].request = {.mark = RequestMark::kActive,
                           .request = Request(kRefusedRequest)};
  CHECK(Audio::LoadMusicSlot(host, kLowMusicId, 1) == 1);
  CHECK(host.diagnostics == std::vector{Audio::kMusicRequestHeldDiagnostic});
  host.diagnostics.clear();
  host.slots[0] = {.request = {.mark = RequestMark::kFailed}};
  CHECK(Audio::LoadMusicSlot(host, kLowMusicId, 1) == 1);
  CHECK(host.diagnostics.empty());
  CHECK(host.slots[0] == LoadingMusic(kLowMusicId, kFirstAllocation));
}

TEST_CASE(
    "A music read the disc queue refuses still completes into the music buffer",
    "[audio-music-streams][audio]") {
  FakeMusic host;
  host.next_request.reset();
  CHECK(Audio::LoadMusicSlot(host, kLowMusicId, 3) == 3);
  CHECK(host.calls.back() ==
        HostCall{"complete", kMusicSectorBase + kLowMusicId,
                 static_cast<std::int64_t>(kFirstAllocation)});
  CHECK(host.slots[kThirdSlot].request ==
        Audio::BankRequest{.mark = RequestMark::kActive,
                           .request = Request(kRefusedRequest)});
}

TEST_CASE("Loading music into a free slot takes the first slot without music",
          "[audio-music-streams][audio]") {
  FakeMusic host;
  host.slots[0] = HeldMusic(kLowMusicId, kMusicBuffer);
  host.slots[kThirdSlot] = HeldMusic(kOtherMusicId, kOtherMusicBuffer);
  CHECK(Audio::LoadMusicIntoFreeSlot(host, kHighMusicId) == 2);
  CHECK(host.slots[kSecondSlot].id == kHighMusicId);
  host.slots[kFourthSlot] = HeldMusic(kOtherMusicId, kOtherMusicBuffer);
  host.calls.clear();
  CHECK(Audio::LoadMusicIntoFreeSlot(host, kHighMusicId) == 0);
  CHECK(host.calls.empty());
  CHECK(host.diagnostics.empty());
}

TEST_CASE(
    "Polling a music load releases a loaded read and reports one in flight",
    "[audio-music-streams][audio]") {
  FakeMusic host;
  host.slots[kSecondSlot] = LoadingMusic(kLowMusicId, kMusicBuffer);
  CHECK(Audio::PollMusicSlotLoad(host, 2) == Audio::kLoadInFlight);
  host.request_state = DiscRequestState::kLoaded;
  CHECK(Audio::PollMusicSlotLoad(host, 2) == Audio::kLoadIdle);
  const HostCall poll{"poll", static_cast<std::int64_t>(kQueuedRequest)};
  CHECK(host.calls == std::vector{poll, poll,
                                  HostCall{"release", static_cast<std::int64_t>(
                                                          kQueuedRequest)}});
  CHECK(host.slots[kSecondSlot].request == Audio::BankRequest{});
  host.calls.clear();
  CHECK(Audio::PollMusicSlotLoad(host, 2) == Audio::kLoadIdle);
  CHECK(Audio::PollMusicSlotLoad(host, 0) == Audio::kLoadIdle);
  CHECK(Audio::PollMusicSlotLoad(host, kPollPastLast) == Audio::kLoadIdle);
  CHECK(host.calls.empty());
}

TEST_CASE("A failed music read keeps the failed mark, which then polls idle",
          "[audio-music-streams][audio]") {
  FakeMusic host;
  host.slots[0] = LoadingMusic(kLowMusicId, kMusicBuffer);
  host.request_state = DiscRequestState::kError;
  CHECK(Audio::PollMusicSlotLoad(host, 1) == Audio::kLoadFailed);
  CHECK(host.slots[0].request ==
        Audio::BankRequest{.mark = RequestMark::kFailed});
  CHECK(host.calls.back() ==
        HostCall{"release", static_cast<std::int64_t>(kQueuedRequest)});
  host.calls.clear();
  CHECK(Audio::PollMusicSlotLoad(host, 1) == Audio::kLoadIdle);
  CHECK(host.calls.empty());
}

TEST_CASE("Polling every music load combines the slot results",
          "[audio-music-streams][audio]") {
  FakeMusic host;
  host.slots[0] = LoadingMusic(kLowMusicId, kMusicBuffer);
  host.slots[kThirdSlot] = LoadingMusic(kOtherMusicId, kOtherMusicBuffer);
  host.request_states = {DiscRequestState::kLoaded,
                         DiscRequestState::kEnqueued};
  CHECK(Audio::PollMusicLoads(host) == Audio::kLoadInFlight);
  host.request_states = {DiscRequestState::kError};
  CHECK(Audio::PollMusicLoads(host) == Audio::kLoadFailed);
  CHECK(host.slots[0].request == Audio::BankRequest{});
  CHECK(host.slots[kThirdSlot].request ==
        Audio::BankRequest{.mark = RequestMark::kFailed});
  CHECK(Audio::PollMusicLoads(host) == Audio::kLoadIdle);
}

TEST_CASE(
    "Blocking music loads wait a frame for each poll until the read lands",
    "[audio-music-streams][audio]") {
  FakeMusic host;
  host.request_states = {DiscRequestState::kReading,
                         DiscRequestState::kEnqueued};
  host.request_state = DiscRequestState::kLoaded;
  CHECK(Audio::LoadMusicSlotBlocking(host, kLowMusicId, 4) == 4);
  std::size_t waits = 0;
  for (const HostCall& call : host.calls) waits += call.name == "wait" ? 1 : 0;
  CHECK(waits == 2);
  CHECK(host.slots[kFourthSlot].request == Audio::BankRequest{});
  host.calls.clear();
  CHECK(Audio::LoadFreeMusicSlotBlocking(host, kOtherMusicId) == 1);
  CHECK(host.calls.back() ==
        HostCall{"release", static_cast<std::int64_t>(kQueuedRequest)});
  CHECK(Audio::LoadMusicSlotBlocking(host, 0, 1) == 0);
}

TEST_CASE("Freeing a music slot stops its music and frees its buffer once",
          "[audio-music-streams][audio]") {
  FakeMusic host;
  host.slots[0] = HeldMusic(kLowMusicId, kMusicBuffer, kHandle);
  CHECK(Audio::FreeMusicSlot(host, 1) == 1);
  CHECK(host.calls == std::vector<HostCall>{
                          {"stop", kHandle},
                          {"free", static_cast<std::int64_t>(kMusicBuffer)}});
  CHECK(host.slots[0] == MusicSlot{});
  host.calls.clear();
  CHECK(Audio::FreeMusicSlot(host, 1) == 0);
  CHECK(host.calls.empty());
  CHECK(host.refusals.empty());
}

TEST_CASE(
    "Freeing music slot 0 stops all music and is refused before the buffers",
    "[audio-music-streams][audio]") {
  FakeMusic host;
  host.slots[0] = HeldMusic(kLowMusicId, kMusicBuffer, kHandle);
  CHECK(Audio::FreeMusicSlot(host, 0) == 0);
  CHECK(host.calls == std::vector<HostCall>{{"stop all"}});
  CHECK(host.refusals == std::vector{MusicRefusal::kFreeWithoutSlot});
  CHECK(host.slots[0].data == Buffer(kMusicBuffer));
  host.refusals.clear();
  constexpr std::int32_t kSlotFarPast = 9;
  CHECK(Audio::FreeMusicSlot(host, kSlotFarPast) == 0);
  CHECK(host.refusals == std::vector{MusicRefusal::kSlotOutOfRange});
}

TEST_CASE("Music slot volume asks for the slot's own handle",
          "[audio-music-streams][audio]") {
  FakeMusic host;
  host.slots[kSecondSlot] = HeldMusic(kLowMusicId, kMusicBuffer, kHandle);
  CHECK(Audio::MusicSlotVolume(host, 2) == kSlotVolume);
  CHECK(Audio::QueryMusicSlotVolume(host, 2) == kSlotVolume);
  CHECK(Audio::MusicSlotVolume(host, 1) == 0);
  CHECK(Audio::MusicSlotVolume(host, 0) == 0);
  CHECK(Audio::QueryMusicSlotVolume(host, kSlotPastLast) == 0);
  CHECK(host.calls ==
        std::vector<HostCall>{{"volume", kHandle}, {"volume", kHandle}});
  CHECK(host.refusals == std::vector{MusicRefusal::kSlotOutOfRange});
}

TEST_CASE("Music slot fades act only when the slot is the playing slot",
          "[audio-music-streams][audio]") {
  FakeMusic host;
  host.playback = {.slot = 3, .handle = kOtherHandle};
  CHECK(Audio::FadeMusicSlotVolume(host, 2, kFade) == 0);
  CHECK(Audio::FadeMusicSlotTempo(host, 2, kFade) == 0);
  CHECK(Audio::FadeMusicSlotPitch(host, kSlotPastLast, kFade) == 0);
  CHECK(host.calls.empty());
  CHECK(Audio::FadeMusicSlotVolume(host, 3, kFade) == 1);
  CHECK(Audio::FadeMusicSlotTempo(host, 3, kFade) == 1);
  CHECK(Audio::FadeMusicSlotPitch(host, 3, kFade) == 1);
  CHECK(host.calls ==
        std::vector<HostCall>{{"fade", kOtherHandle, kFadeTicks, kFadeTarget},
                              {"tempo", kFadeTicks, kFadeTarget},
                              {"pitch", kFadeTicks, kFadeTarget}});
}

TEST_CASE("Stopping the current music stops and frees the playing slot",
          "[audio-music-streams][audio]") {
  FakeMusic host;
  Audio::StopAndFreeCurrentMusic(host);
  CHECK(host.calls.empty());
  host.slots[kFourthSlot] = HeldMusic(kLowMusicId, kMusicBuffer, kHandle);
  host.playback = {.slot = 4,
                   .handle = kHandle,
                   .music_id = kLowMusicId,
                   .data = Buffer(kMusicBuffer)};
  Audio::StopAndFreeCurrentMusic(host);
  CHECK(host.calls == std::vector<HostCall>{
                          {"stop", kHandle},
                          {"free", static_cast<std::int64_t>(kMusicBuffer)}});
  CHECK(host.slots[kFourthSlot] == MusicSlot{});
  CHECK(host.playback == MusicPlayback{.music_id = kLowMusicId});
}

namespace {
constexpr std::int32_t kXaSector = 0x1234;
constexpr std::int32_t kXaBytes = 0x8000;
constexpr std::uint32_t kStaleCount = 0x11;
constexpr std::uint8_t kStaleFailures = 3;

Audio::XaStreamDrive StaleDrive(Content::DiscDriveState state) {
  return {.state = state,
          .start_sector = 1,
          .byte_count = 2,
          .failure_count = kStaleFailures,
          .sectors_received = kStaleCount,
          .destination_sector = kStaleCount,
          .buffered_sectors = 1,
          .ring_sectors_written = kStaleCount,
          .ring_write_index = 2,
          .ring_read_index = 1};
}
}  // namespace

TEST_CASE(
    "An XA stream begins only on an idle drive and resets the read counters",
    "[audio-music-streams][audio]") {
  Audio::XaStreamDrive drive = StaleDrive(Content::DiscDriveState::kIdle);
  CHECK(Audio::BeginXaStream(drive, kXaSector, kXaBytes));
  CHECK(drive ==
        Audio::XaStreamDrive{.state = Content::DiscDriveState::kReadXaInit,
                             .start_sector = kXaSector,
                             .byte_count = kXaBytes});
  const Audio::XaStreamDrive busy = drive;
  CHECK_FALSE(Audio::BeginXaStream(drive, 1, 2));
  CHECK(drive == busy);
}

TEST_CASE("An XA stream stop request ends only a streaming drive",
          "[audio-music-streams][audio]") {
  Audio::XaStreamDrive drive = StaleDrive(Content::DiscDriveState::kStreamXa);
  CHECK(Audio::RequestXaStreamStop(drive));
  CHECK(drive == StaleDrive(Content::DiscDriveState::kStreamXaEnd));
  CHECK_FALSE(Audio::RequestXaStreamStop(drive));
  drive.state = Content::DiscDriveState::kIdle;
  CHECK_FALSE(Audio::RequestXaStreamStop(drive));
  CHECK(drive.state == Content::DiscDriveState::kIdle);
}

namespace {
constexpr std::uintptr_t kStreamBuffer = 0x3000;
constexpr std::int32_t kStreamPan = 0x40;
constexpr std::int32_t kStreamVolume = 0x50;

class FakeEffectStreams final : public Audio::EffectStreamHost {
 public:
  std::array<BankBuffer, Audio::kEffectStreamSlotCount> streams{};
  std::vector<HostCall> calls;

  BankBuffer EffectStream(std::size_t index) override {
    return streams.at(index);
  }
  void StoreEffectStream(std::size_t index, BankBuffer stream) override {
    streams.at(index) = stream;
  }
  void FreeBuffer(BankBuffer buffer) override {
    calls.push_back({"free", Raw(buffer)});
  }
  void FadeCutsceneVolume(Control::Fade fade) override {
    calls.push_back({"fade", fade.ticks, fade.target});
  }
  void StartCutsceneStream(BankBuffer stream, std::int32_t pan,
                           std::int32_t reverb) override {
    calls.push_back({"start", Raw(stream), pan, reverb});
  }
  void StopCutsceneStream() override { calls.push_back({"stop"}); }
  void SetDiscVolume(std::int32_t volume) override {
    calls.push_back({"disc volume", volume});
  }
};
}  // namespace

TEST_CASE("An effect stream slot plays its stream at once with reverb",
          "[audio-music-streams][audio]") {
  FakeEffectStreams host;
  host.streams[kThirdSlot] = Buffer(kStreamBuffer);
  CHECK(Audio::PlayCutsceneStreamSlot(
      host, 3, SfxMix{.pan = kStreamPan, .volume = kStreamVolume}));
  Audio::PlayCutsceneStreamSlotCentered(host, 3);
  const auto stream = static_cast<std::int64_t>(kStreamBuffer);
  CHECK(host.calls ==
        std::vector<HostCall>{
            {"fade", 0, kStreamVolume},
            {"start", stream, kStreamPan, Audio::kEffectStreamReverb},
            {"fade", 0, Audio::kFullSfxVolume},
            {"start", stream, Audio::kCenteredSfxPan,
             Audio::kEffectStreamReverb}});
}

TEST_CASE("Effect stream slots outside 1 to 3 or without a stream do nothing",
          "[audio-music-streams][audio]") {
  FakeEffectStreams host;
  host.streams.fill(Buffer(kStreamBuffer));
  host.streams[0] = BankBuffer::kNone;
  constexpr std::uint32_t kSlotPastStreams = 4;
  constexpr std::uint32_t kWrappedSlot = 0xFFFFFFFF;
  CHECK_FALSE(Audio::PlayCutsceneStreamSlot(host, 1, SfxMix{}));
  CHECK_FALSE(Audio::PlayCutsceneStreamSlot(host, 0, SfxMix{}));
  CHECK_FALSE(Audio::PlayCutsceneStreamSlot(host, kSlotPastStreams, SfxMix{}));
  CHECK_FALSE(Audio::FreeCutsceneStreamSlot(host, kWrappedSlot));
  CHECK_FALSE(Audio::FreeCutsceneStreamSlot(host, 1));
  Audio::PlayCutsceneStreamSlotCentered(host, kSlotPastStreams);
  CHECK(host.calls.empty());
}

TEST_CASE("Freeing an effect stream slot stops the stream and frees its buffer",
          "[audio-music-streams][audio]") {
  FakeEffectStreams host;
  host.streams[kSecondSlot] = Buffer(kStreamBuffer);
  CHECK(Audio::FreeCutsceneStreamSlot(host, 2));
  CHECK(host.streams[kSecondSlot] == BankBuffer::kNone);
  CHECK_FALSE(Audio::FreeCutsceneStreamSlot(host, 2));
  Audio::StopCutsceneStream(host);
  Audio::SetXaVolumeMaximum(host);
  CHECK(host.calls == std::vector<HostCall>{
                          {"stop"},
                          {"free", static_cast<std::int64_t>(kStreamBuffer)},
                          {"stop"},
                          {"disc volume", Audio::kMaximumXaVolume}});
}

namespace {
using Cutscene::StreamBufferState;
using Cutscene::StreamBufferStatus;
using Cutscene::StreamPanMode;
using Cutscene::StreamRefill;
using Cutscene::StreamTransferCompletion;
using Cutscene::StreamVoiceGains;

constexpr std::size_t kStreamByteCount = 0x4000;
constexpr std::int32_t kPairVoice = 20;
constexpr std::uint32_t kPairMask = 0x00300000;
constexpr std::uint8_t kStreamVoicePan = 0x30;
constexpr std::int16_t kPanGain = 0x4000;
constexpr std::int16_t kMirrorPanGain = 0x2000;
constexpr std::int16_t kCenterPanGain = 0x6000;
constexpr std::size_t kCenterPanIndex = 128;
constexpr std::size_t kMirrorPan = 0xFF - kStreamVoicePan;
constexpr std::int32_t kFullStreamVolume = 0x7FFF;
constexpr std::int32_t kStreamSampleRate = 0x1000;
constexpr std::uint32_t kInterruptOffset = Cutscene::kBufferInterruptOffset;
constexpr std::int64_t kStreamEndHandler = -1;

std::int64_t RefillId(StreamRefill refill) {
  return static_cast<std::int64_t>(refill);
}

std::int64_t CompletionId(StreamTransferCompletion completion) {
  return static_cast<std::int64_t>(completion);
}

class FakeStreamBuffers final : public Cutscene::StreamBufferHost {
 public:
  StreamBufferState state;
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(kStreamByteCount);
  Cutscene::StreamVoiceMix mix{.volume = kFullStreamVolume,
                               .pan = kStreamVoicePan,
                               .sample_rate = kStreamSampleRate};
  std::array<std::int16_t, Cutscene::kPanGainCount> pan_gains{};
  std::uint32_t busy = 0;
  std::optional<std::uint32_t> busy_after_release;
  std::optional<Cutscene::StreamVoiceEnvelope> envelope;
  std::vector<HostCall> calls;

  FakeStreamBuffers() {
    pan_gains.at(kStreamVoicePan) = kPanGain;
    pan_gains.at(kMirrorPan) = kMirrorPanGain;
    pan_gains.at(kCenterPanIndex) = kCenterPanGain;
  }

  // A stream playing on voices 20-21 from `chunk` with `pages` left.
  void Play(std::optional<std::size_t> chunk, std::uint32_t pages) {
    state = {.chunk = chunk,
             .repeat_chunk = std::nullopt,
             .pages = pages,
             .repeat_pages = 0,
             .voice = kPairVoice,
             .voice_mask = kPairMask,
             .transfer_busy = true};
  }

  // The calls named `name`.
  [[nodiscard]] std::vector<HostCall> Named(std::string_view name) const {
    std::vector<HostCall> named;
    for (const HostCall& call : calls) {
      if (call.name == name) named.push_back(call);
    }
    return named;
  }

  StreamBufferState State() override { return state; }
  void StoreState(const StreamBufferState& stored) override { state = stored; }
  std::span<const std::uint8_t> StreamBytes() override { return bytes; }
  Cutscene::StreamVoiceMix VoiceMix() override { return mix; }
  std::span<const std::int16_t, Cutscene::kPanGainCount> PanGains() override {
    return pan_gains;
  }
  std::uint32_t BusyVoices() override { return busy; }
  void ReleaseLowestEffectVoice() override {
    calls.push_back({"release"});
    if (busy_after_release) busy = *busy_after_release;
  }
  void SetVoiceGains(std::int32_t voice, StreamVoiceGains gains) override {
    calls.push_back({"gains", voice, gains.left, gains.right});
  }
  void SetVoiceSampleRate(std::int32_t voice,
                          std::int32_t sample_rate) override {
    calls.push_back({"rate", voice, sample_rate});
  }
  void SetVoiceStartAddress(std::int32_t voice,
                            std::uint32_t address) override {
    calls.push_back({"start", voice, address});
  }
  void SetVoiceLoopAddress(std::int32_t voice, std::uint32_t address) override {
    calls.push_back({"loop", voice, address});
  }
  void SetVoiceEnvelope(std::int32_t voice,
                        const Cutscene::StreamVoiceEnvelope& set) override {
    calls.push_back({"envelope", voice});
    envelope = set;
  }
  void KeyOnVoices(std::uint32_t mask) override {
    calls.push_back({"key on", mask});
  }
  void SetTransferAddress(std::uint32_t address) override {
    calls.push_back({"transfer", address});
  }
  void SetTransferCompletion(StreamTransferCompletion completion) override {
    calls.push_back({"completion", CompletionId(completion)});
  }
  void WriteSamples(std::span<const std::uint8_t> samples) override {
    calls.push_back({"write", samples.data() - bytes.data(),
                     static_cast<std::int64_t>(samples.size())});
  }
  void DisarmBufferInterrupt() override { calls.push_back({"disarm"}); }
  void ArmBufferInterrupt(std::optional<StreamRefill> next,
                          std::uint32_t address) override {
    calls.push_back(
        {"arm", next ? RefillId(*next) : kStreamEndHandler, address});
  }
};

Cutscene::StreamVoiceSetup VoiceSetup(std::int32_t voice, StreamPanMode mode) {
  return {.voice = voice,
          .mode = mode,
          .start_address = Cutscene::kFirstBufferAddress,
          .loop_address = Cutscene::kSecondBufferAddress};
}

// The gains InitializeStreamVoice sets on voice 20 in `mode`.
HostCall GainsIn(FakeStreamBuffers& host, StreamPanMode mode) {
  host.calls.clear();
  Cutscene::InitializeStreamVoice(host, VoiceSetup(kPairVoice, mode));
  return host.Named("gains").at(0);
}
}  // namespace

TEST_CASE("The stream voice pair search takes the highest free pair",
          "[audio-music-streams][audio]") {
  FakeStreamBuffers host;
  constexpr std::int32_t kHighestPair = 22;
  constexpr std::int32_t kNextPair = 20;
  constexpr std::int32_t kLowestPair = 12;
  constexpr std::uint32_t kVoice23 = 0x00800000;
  constexpr std::uint32_t kEveryPair = 0x00FFF000;
  constexpr std::uint32_t kPairs14To23 = 0x00FFC000;
  CHECK(Cutscene::FindFreeVoicePair(host) == kHighestPair);
  host.busy = kVoice23;
  CHECK(Cutscene::FindFreeVoicePair(host) == kNextPair);
  host.busy = kEveryPair;
  host.busy_after_release = kPairs14To23;
  CHECK(Cutscene::FindFreeVoicePair(host) == kLowestPair);
  CHECK(host.calls == std::vector<HostCall>{{"release"}});
}

TEST_CASE("The stream voice pair search gives up once a release frees no voice",
          "[audio-music-streams][audio]") {
  FakeStreamBuffers host;
  constexpr std::uint32_t kEveryVoice = 0x00FFFFFF;
  host.busy = kEveryVoice;
  CHECK_FALSE(Cutscene::FindFreeVoicePair(host).has_value());
  CHECK(host.calls == std::vector<HostCall>{{"release"}});
}

TEST_CASE(
    "A panned stream voice takes the pan gain and its mirror times the volume",
    "[audio-music-streams][audio]") {
  FakeStreamBuffers host;
  constexpr std::int64_t kLeftGain = 0x1FFF;
  constexpr std::int64_t kRightGain = 0xFFF;
  CHECK(Cutscene::InitializeStreamVoice(
            host, VoiceSetup(kPairVoice, StreamPanMode::kPanned)) ==
        StreamBufferStatus::kApplied);
  CHECK(host.calls == std::vector<HostCall>{
                          {"gains", kPairVoice, kLeftGain, kRightGain},
                          {"rate", kPairVoice, kStreamSampleRate},
                          {"start", kPairVoice, Cutscene::kFirstBufferAddress},
                          {"loop", kPairVoice, Cutscene::kSecondBufferAddress},
                          {"envelope", kPairVoice}});
  CHECK(host.envelope == Cutscene::StreamVoiceEnvelope{});
}

TEST_CASE(
    "Stream voices split half the volume by mode, or center it on mono output",
    "[audio-music-streams][audio]") {
  FakeStreamBuffers host;
  constexpr std::int64_t kHalf = 0x3FFF;
  constexpr std::int64_t kThreeQuarters = 0x2FFE;
  constexpr std::int64_t kMonoGain = 0x2FFF;
  CHECK(GainsIn(host, StreamPanMode::kLeft) ==
        HostCall{"gains", kPairVoice, kHalf, 0});
  CHECK(GainsIn(host, StreamPanMode::kRight) ==
        HostCall{"gains", kPairVoice, 0, kHalf});
  CHECK(GainsIn(host, StreamPanMode::kCentered) ==
        HostCall{"gains", kPairVoice, kThreeQuarters, kThreeQuarters});
  host.mix.mono_output = true;
  CHECK(GainsIn(host, StreamPanMode::kLeft) ==
        HostCall{"gains", kPairVoice, kMonoGain, kMonoGain});
  CHECK(GainsIn(host, StreamPanMode::kPanned) ==
        HostCall{"gains", kPairVoice, kMonoGain, kMonoGain});
}

TEST_CASE(
    "Stream voice gains keep the sign the 16-bit register gives a full volume",
    "[audio-music-streams][audio]") {
  FakeStreamBuffers host;
  constexpr std::int32_t kWrappingVolume = 0x10000;
  constexpr std::int64_t kWrappedHalf = -32768;
  constexpr std::int64_t kWrappedLevel = -24576;
  host.mix.volume = kWrappingVolume;
  CHECK(GainsIn(host, StreamPanMode::kLeft) ==
        HostCall{"gains", kPairVoice, kWrappedHalf, 0});
  CHECK(GainsIn(host, StreamPanMode::kCentered) ==
        HostCall{"gains", kPairVoice, kWrappedLevel, kWrappedLevel});
}

TEST_CASE("A stream voice outside the 24 voices is refused untouched",
          "[audio-music-streams][audio]") {
  FakeStreamBuffers host;
  constexpr std::int32_t kVoicePastLast = Cutscene::kStreamVoiceCount;
  CHECK(Cutscene::InitializeStreamVoice(
            host, VoiceSetup(kVoicePastLast, StreamPanMode::kPanned)) ==
        StreamBufferStatus::kInvalidVoice);
  CHECK(Cutscene::InitializeStreamVoice(host,
                                        VoiceSetup(-1, StreamPanMode::kLeft)) ==
        StreamBufferStatus::kInvalidVoice);
  CHECK(host.calls.empty());
}

TEST_CASE(
    "Mono playback starts both voices on the first buffer and arms the first "
    "refill",
    "[audio-music-streams][audio]") {
  FakeStreamBuffers host;
  constexpr std::uint32_t kPages = 0x3000;
  host.Play(0, kPages);
  CHECK(Cutscene::BeginMonoPlayback(host) == StreamBufferStatus::kApplied);
  CHECK(host.Named("start") ==
        std::vector<HostCall>{
            {"start", kPairVoice, Cutscene::kFirstBufferAddress},
            {"start", kPairVoice + 1, Cutscene::kFirstBufferAddress}});
  CHECK(host.Named("loop") ==
        std::vector<HostCall>{
            {"loop", kPairVoice, Cutscene::kSecondBufferAddress},
            {"loop", kPairVoice + 1, Cutscene::kSecondBufferAddress}});
  constexpr std::ptrdiff_t kTailCalls = 3;
  const std::vector tail(host.calls.end() - kTailCalls, host.calls.end());
  CHECK(tail ==
        std::vector<HostCall>{
            {"completion", CompletionId(StreamTransferCompletion::kNone)},
            {"key on", kPairMask},
            {"arm", RefillId(StreamRefill::kMonoFirst),
             Cutscene::kSecondBufferAddress + kInterruptOffset}});
  constexpr std::uint32_t kPagesLeft = 0x2000;
  CHECK(host.state == StreamBufferState{.chunk = 2 * Cutscene::kMonoBufferBytes,
                                        .repeat_chunk = std::nullopt,
                                        .pages = kPagesLeft,
                                        .repeat_pages = 0,
                                        .voice = kPairVoice,
                                        .voice_mask = kPairMask,
                                        .transfer_busy = false});
}

TEST_CASE(
    "A stream no longer than its first buffers arms the end handler on the "
    "silent loop",
    "[audio-music-streams][audio]") {
  FakeStreamBuffers host;
  host.Play(0, Cutscene::kStereoBufferBytes);
  CHECK(Cutscene::BeginMonoPlayback(host) == StreamBufferStatus::kApplied);
  CHECK(host.calls.back() ==
        HostCall{"arm", kStreamEndHandler,
                 Cutscene::kSilentLoopAddress + kInterruptOffset});
  CHECK(host.state.pages == 0);
  CHECK(host.state.chunk == std::size_t{0});
  CHECK_FALSE(host.state.transfer_busy);
}

TEST_CASE("Stereo playback starts the right voice on its own buffers",
          "[audio-music-streams][audio]") {
  FakeStreamBuffers host;
  constexpr std::uint32_t kPages = 0x3000;
  constexpr std::size_t kStereoAdvance = 0x2000;
  constexpr std::int64_t kHalf = 0x3FFF;
  host.Play(0, kPages);
  CHECK(Cutscene::BeginStereoPlayback(host) == StreamBufferStatus::kApplied);
  CHECK(host.Named("gains") ==
        std::vector<HostCall>{{"gains", kPairVoice, kHalf, 0},
                              {"gains", kPairVoice + 1, 0, kHalf}});
  CHECK(host.Named("start") ==
        std::vector<HostCall>{
            {"start", kPairVoice, Cutscene::kFirstBufferAddress},
            {"start", kPairVoice + 1, Cutscene::kFirstRightBufferAddress}});
  CHECK(host.Named("loop") ==
        std::vector<HostCall>{
            {"loop", kPairVoice, Cutscene::kSecondBufferAddress},
            {"loop", kPairVoice + 1, Cutscene::kSecondRightBufferAddress}});
  CHECK(host.calls.back() ==
        HostCall{"arm", RefillId(StreamRefill::kStereoFirst),
                 Cutscene::kSecondBufferAddress + kInterruptOffset});
  CHECK(host.state.chunk == kStereoAdvance);
}

TEST_CASE(
    "Stream playback refuses an idle stream, a voice past the pairs and a "
    "short chunk",
    "[audio-music-streams][audio]") {
  FakeStreamBuffers host;
  constexpr std::uint32_t kPages = 0x3000;
  constexpr std::int32_t kVoice23 = 23;
  host.Play(0, kPages);
  host.state.voice_mask = 0;
  CHECK(Cutscene::BeginMonoPlayback(host) == StreamBufferStatus::kIdle);
  CHECK(host.Named("key on").empty());
  host.Play(0, kPages);
  host.state.voice = kVoice23;
  CHECK(Cutscene::BeginStereoPlayback(host) ==
        StreamBufferStatus::kInvalidVoice);
  host.Play(std::nullopt, kPages);
  CHECK(Cutscene::BeginStereoPlayback(host) ==
        StreamBufferStatus::kChunkOutOfRange);
  host.Play(kStreamByteCount - Cutscene::kStereoBufferBytes, kPages);
  const StreamBufferState unchanged = host.state;
  CHECK(Cutscene::BeginStereoPlayback(host) ==
        StreamBufferStatus::kChunkOutOfRange);
  CHECK(host.state == unchanged);
  CHECK(host.Named("key on").empty());
  CHECK(host.Named("arm").empty());
}

TEST_CASE(
    "A buffer refill uploads the next chunk and arms the other buffer's refill",
    "[audio-music-streams][audio]") {
  FakeStreamBuffers host;
  constexpr std::size_t kChunk = 0x1000;
  constexpr std::uint32_t kPages = 0x1800;
  host.Play(kChunk, kPages);
  CHECK(Cutscene::RefillMonoFirstBuffer(host) == StreamBufferStatus::kApplied);
  CHECK(host.calls ==
        std::vector<HostCall>{
            {"transfer", Cutscene::kFirstBufferAddress},
            {"completion",
             CompletionId(StreamTransferCompletion::kSampleTransfer)},
            {"write", kChunk, Cutscene::kMonoBufferBytes},
            {"disarm"},
            {"loop", kPairVoice, Cutscene::kFirstBufferAddress},
            {"loop", kPairVoice + 1, Cutscene::kFirstBufferAddress},
            {"arm", RefillId(StreamRefill::kMonoSecond),
             Cutscene::kFirstBufferAddress + kInterruptOffset}});
  CHECK(host.state.chunk == kChunk + Cutscene::kMonoBufferBytes);
  CHECK(host.state.pages == kPages - Cutscene::kMonoBufferBytes);
}

TEST_CASE("A refill past the stream's end continues from its repeat section",
          "[audio-music-streams][audio]") {
  FakeStreamBuffers host;
  constexpr std::size_t kChunk = 0x2000;
  constexpr std::size_t kRepeatChunk = 0x200;
  constexpr std::uint32_t kRepeatPages = 0x3000;
  host.Play(kChunk, Cutscene::kMonoBufferBytes);
  host.state.repeat_chunk = kRepeatChunk;
  host.state.repeat_pages = kRepeatPages;
  CHECK(Cutscene::RefillStereoSecondBuffer(host) ==
        StreamBufferStatus::kApplied);
  CHECK(host.Named("write") ==
        std::vector<HostCall>{{"write", kChunk, Cutscene::kStereoBufferBytes}});
  CHECK(host.Named("loop") ==
        std::vector<HostCall>{
            {"loop", kPairVoice, Cutscene::kSecondBufferAddress},
            {"loop", kPairVoice + 1, Cutscene::kSecondRightBufferAddress}});
  CHECK(host.calls.back() ==
        HostCall{"arm", RefillId(StreamRefill::kStereoFirst),
                 Cutscene::kSecondBufferAddress + kInterruptOffset});
  CHECK(host.state.chunk == kRepeatChunk);
  CHECK(host.state.pages == kRepeatPages);
}

TEST_CASE(
    "A refill past the end of a stream without a repeat falls back to the "
    "silent loop",
    "[audio-music-streams][audio]") {
  FakeStreamBuffers host;
  constexpr std::size_t kChunk = 0x2000;
  host.Play(kChunk, Cutscene::kMonoBufferBytes);
  CHECK(Cutscene::RefillStereoFirstBuffer(host) ==
        StreamBufferStatus::kApplied);
  CHECK(host.Named("loop") ==
        std::vector<HostCall>{
            {"loop", kPairVoice, Cutscene::kSilentLoopAddress},
            {"loop", kPairVoice + 1, Cutscene::kSilentLoopAddress}});
  CHECK(host.calls.back() ==
        HostCall{"arm", kStreamEndHandler,
                 Cutscene::kSilentLoopAddress + kInterruptOffset});
  CHECK(host.state.pages == 0);
  CHECK(host.state.chunk == kChunk);
}

TEST_CASE(
    "Buffer refills refuse an idle stream, a voice past the pairs and a chunk "
    "past the bytes",
    "[audio-music-streams][audio]") {
  FakeStreamBuffers host;
  constexpr std::uint32_t kPages = 0x1800;
  constexpr std::int32_t kVoice23 = 23;
  host.Play(0, 0);
  CHECK(Cutscene::RefillMonoSecondBuffer(host) == StreamBufferStatus::kIdle);
  host.Play(0, kPages);
  host.state.voice_mask = 0;
  CHECK(Cutscene::RefillMonoSecondBuffer(host) == StreamBufferStatus::kIdle);
  host.Play(0, kPages);
  host.state.voice = kVoice23;
  CHECK(Cutscene::RefillMonoSecondBuffer(host) ==
        StreamBufferStatus::kInvalidVoice);
  host.Play(kStreamByteCount - Cutscene::kMonoBufferBytes + 1, kPages);
  CHECK(Cutscene::RefillMonoSecondBuffer(host) ==
        StreamBufferStatus::kChunkOutOfRange);
  host.Play(std::nullopt, kPages);
  CHECK(Cutscene::RefillStereoFirstBuffer(host) ==
        StreamBufferStatus::kChunkOutOfRange);
  CHECK(host.calls.empty());
}

TEST_CASE(
    "The second mono buffer upload writes the chunk's second half and begins "
    "playback after",
    "[audio-music-streams][audio]") {
  FakeStreamBuffers host;
  constexpr std::size_t kChunk = 0x1000;
  host.Play(kChunk, Cutscene::kStereoBufferBytes);
  CHECK(Cutscene::UploadSecondMonoBuffer(host) == StreamBufferStatus::kApplied);
  CHECK(host.calls ==
        std::vector<HostCall>{
            {"transfer", Cutscene::kSecondBufferAddress},
            {"completion",
             CompletionId(StreamTransferCompletion::kBeginMonoPlayback)},
            {"write", kChunk + Cutscene::kMonoBufferBytes,
             Cutscene::kMonoBufferBytes}});
  host.calls.clear();
  host.Play(kStreamByteCount - Cutscene::kMonoBufferBytes, 0);
  CHECK(Cutscene::UploadSecondMonoBuffer(host) ==
        StreamBufferStatus::kChunkOutOfRange);
  host.Play(std::nullopt, 0);
  CHECK(Cutscene::UploadSecondMonoBuffer(host) ==
        StreamBufferStatus::kChunkOutOfRange);
  CHECK(host.calls.empty());
}

namespace {
using Audio::ActorContactSource;
using Audio::ActorModelSound;
using Audio::ActorSoundRecord;
using Audio::ActorSoundStatus;
using Audio::SfxPoint;

constexpr std::uint8_t kSoundActor = 3;
constexpr std::uint8_t kOtherSoundActor = 4;
constexpr std::uintptr_t kModelBank = 0x8800;
constexpr std::int16_t kScreenX = 480;
// Past the low halfword the pan reads, so a swapped packing would show.
constexpr std::int16_t kScreenY = 0x7FFF;
constexpr std::int16_t kSoundDepth = 0x320;
constexpr std::int32_t kModelSoundIndex = 0x15;
constexpr std::int32_t kBoundedPick = 2;
constexpr std::int32_t kRandomPick = 7;
constexpr std::uint8_t kOverridePan = 0x20;
constexpr std::uint8_t kOverrideVolume = 0x30;
constexpr std::int32_t kFirstPairSound = 0x10;
constexpr std::int32_t kSecondPairSound = 0x11;

// The mix the fake's projected position gives.
SfxMix ProjectedMix() {
  return Audio::SfxPanAndVolume({.screen_xy = kScreenX, .depth = kSoundDepth});
}

ActorModelSound SoundModel(std::int32_t actor = kSoundActor) {
  return {.actor = static_cast<std::uint8_t>(actor),
          .bank = Buffer(kModelBank)};
}

// One sound played: a bank's entry (bank nonzero) or an effect id.
struct SoundPlay {
  std::int32_t group = 0;
  std::int32_t sound = 0;
  std::int64_t bank = 0;
  SfxMix mix;

  friend bool operator==(const SoundPlay&, const SoundPlay&) = default;
};

SoundPlay BankPlay(std::int32_t group, std::int32_t sound) {
  return {.group = group,
          .sound = sound,
          .bank = static_cast<std::int64_t>(kModelBank),
          .mix = ProjectedMix()};
}

class FakeActorSound final : public Audio::ActorSoundHost {
 public:
  std::map<std::int32_t, ActorSoundRecord> records{{kSoundActor, {}}};
  std::map<std::int32_t, ActorModelSound> models;
  std::map<std::int32_t, ActorModelSound> script_models;
  std::set<std::int32_t> present;
  SfxPoint position{.x = kScreenX, .y = kScreenY, .z = kSoundDepth};
  std::vector<std::int32_t> projected;
  std::optional<std::uint8_t> top_gimmick;
  std::optional<std::uint8_t> surface;
  std::vector<HostCall> lookups;
  std::vector<std::uint8_t> object_sounds;
  std::vector<std::uint8_t> tile_sounds;
  Audio::RoomWeather weather;
  std::vector<std::int32_t> bounds;
  std::vector<SoundPlay> plays;
  std::vector<HostCall> world_plays;

  std::optional<ActorSoundRecord> ActorSound(std::int32_t actor) override {
    const auto found = records.find(actor);
    if (found == records.end()) return std::nullopt;
    return found->second;
  }
  std::optional<ActorModelSound> ActorModel(std::int32_t actor) override {
    const auto found = models.find(actor);
    if (found == models.end()) return std::nullopt;
    return found->second;
  }
  void ProjectSoundPosition(std::int32_t actor) override {
    projected.push_back(actor);
  }
  SfxPoint SoundPosition(std::int32_t /*actor*/) override { return position; }
  bool ScriptActorPresent(std::int32_t actor) override {
    lookups.push_back({"present", actor});
    return present.contains(actor);
  }
  std::optional<ActorModelSound> ScriptActorModel(std::int32_t actor) override {
    const auto found = script_models.find(actor);
    if (found == script_models.end()) return std::nullopt;
    return found->second;
  }
  std::optional<std::uint8_t> TopGimmickObject(std::uint8_t tile_x,
                                               std::uint8_t tile_z) override {
    lookups.push_back({"gimmick", tile_x, tile_z});
    return top_gimmick;
  }
  std::optional<std::uint8_t> TileSurfaceKind(std::uint8_t tile_x,
                                              std::uint8_t tile_z,
                                              std::uint8_t layer) override {
    lookups.push_back({"surface", tile_x, tile_z, layer});
    return surface;
  }
  std::span<const std::uint8_t> ObjectSurfaceSounds() override {
    return object_sounds;
  }
  std::span<const std::uint8_t> TileSurfaceSounds() override {
    return tile_sounds;
  }
  Audio::RoomWeather Weather() override { return weather; }
  std::int32_t BoundedRandom(std::int32_t bound) override {
    bounds.push_back(bound);
    return kBoundedPick;
  }
  std::int32_t Random() override { return kRandomPick; }
  void PlaySfx(std::int32_t group, std::int32_t effect_id,
               const SfxMix& mix) override {
    plays.push_back({.group = group, .sound = effect_id, .mix = mix});
  }
  void PlayBankSfx(std::int32_t group, BankBuffer bank, std::int32_t index,
                   const SfxMix& mix) override {
    plays.push_back(
        {.group = group, .sound = index, .bank = Raw(bank), .mix = mix});
  }
  void PlayWorldSfx(std::int32_t group, std::int32_t effect_id,
                    const SfxPoint& point) override {
    world_plays.push_back(
        {"world", group, effect_id,
         (point.x * kWorldPack + point.y) * kWorldPack + point.z});
  }

  // Packs a world sound's point into one HostCall argument.
  static constexpr std::int64_t kWorldPack = 0x10000;
};
}  // namespace

TEST_CASE("An actor's sound mix pans and attenuates its projected position",
          "[audio-music-streams][audio]") {
  FakeActorSound host;
  CHECK(ProjectedMix() != SfxMix{});
  CHECK(Audio::ActorSoundPanAndVolume(host, kSoundActor) == ProjectedMix());
  CHECK(host.projected.empty());
  CHECK_FALSE(
      Audio::ActorSoundPanAndVolume(host, kOtherSoundActor).has_value());
  const std::vector<Audio::ActorSoundMotion> stale{
      {.action_animation_state = 1}, {.preview_model = true},
      {.sound_enabled = false},      {.movement_mode = 1},
      {.locomotion_state = 1},       {.motion_locked = true}};
  for (const Audio::ActorSoundMotion& motion : stale) {
    host.records[kSoundActor].motion = motion;
    CHECK(Audio::ActorSoundPanAndVolume(host, kSoundActor) == ProjectedMix());
  }
  CHECK(host.projected == std::vector<std::int32_t>(stale.size(), kSoundActor));
}

TEST_CASE(
    "An actor sound override silences the actor or fixes its pan and volume",
    "[audio-music-streams][audio]") {
  FakeActorSound host;
  host.records[kSoundActor] = {
      .sound_override = {.kind = Audio::kSilencedActorSound,
                         .pan = kOverridePan,
                         .volume = kOverrideVolume},
      .motion = {.movement_mode = 1}};
  CHECK(Audio::ActorSoundPanAndVolume(host, kSoundActor) ==
        SfxMix{.pan = Audio::kCenteredSfxPan, .volume = 0});
  CHECK(host.projected.empty());
  host.records[kSoundActor].sound_override = {.pan = kOverridePan,
                                              .volume = kOverrideVolume};
  CHECK(Audio::ActorSoundPanAndVolume(host, kSoundActor) ==
        SfxMix{.pan = kOverridePan, .volume = kOverrideVolume});
  host.records[kSoundActor].sound_override = {.pan = kOverridePan};
  CHECK(Audio::ActorSoundPanAndVolume(host, kSoundActor) ==
        SfxMix{.pan = kOverridePan, .volume = ProjectedMix().volume});
  host.records[kSoundActor].sound_override = {.volume = kOverrideVolume};
  CHECK(Audio::ActorSoundPanAndVolume(host, kSoundActor) ==
        SfxMix{.pan = ProjectedMix().pan, .volume = kOverrideVolume});
}

TEST_CASE(
    "A model sound plays its bank's entry on the model group at its actor's "
    "mix",
    "[audio-music-streams][audio]") {
  FakeActorSound host;
  CHECK(Audio::PlayActorModelSound(host, SoundModel(), kModelSoundIndex) ==
        ActorSoundStatus::kDone);
  CHECK(host.plays ==
        std::vector{BankPlay(Audio::kModelSoundGroup, kModelSoundIndex)});
  CHECK(host.bounds.empty());
}

TEST_CASE(
    "Model impact and step sounds pick their bank entry at random on the actor "
    "group",
    "[audio-music-streams][audio]") {
  FakeActorSound host;
  CHECK(Audio::PlayActorModelSound(host, SoundModel(),
                                   Audio::kRandomImpactSound) ==
        ActorSoundStatus::kDone);
  CHECK(
      Audio::PlayActorModelSound(host, SoundModel(), Audio::kRandomStepSound) ==
      ActorSoundStatus::kDone);
  constexpr std::int32_t kStepPick =
      kRandomPick % Audio::kRandomStepSoundCount + Audio::kRandomStepSound;
  CHECK(host.plays ==
        std::vector{BankPlay(Audio::kActorSoundGroup,
                             kBoundedPick + Audio::kRandomImpactSound),
                    BankPlay(Audio::kActorSoundGroup, kStepPick)});
  CHECK(host.bounds == std::vector{Audio::kRandomImpactSoundCount});
}

TEST_CASE(
    "A model at the silent height, a silent actor or a model without a bank "
    "plays nothing",
    "[audio-music-streams][audio]") {
  FakeActorSound host;
  ActorModelSound model = SoundModel(kOtherSoundActor);
  model.height = Audio::kSilentModelHeight;
  CHECK(Audio::PlayActorModelSound(host, model, 0) == ActorSoundStatus::kDone);
  model.height = static_cast<std::int16_t>(Audio::kSilentModelHeight - 1);
  CHECK(Audio::PlayActorModelSound(host, model, 0) ==
        ActorSoundStatus::kNoActor);
  model = SoundModel();
  model.bank = BankBuffer::kNone;
  CHECK(Audio::PlayActorModelSound(host, model, 0) ==
        ActorSoundStatus::kNoSoundBank);
  host.records[kSoundActor].sound_override.kind = Audio::kSilencedActorSound;
  CHECK(Audio::PlayActorModelSound(host, model, 0) == ActorSoundStatus::kDone);
  CHECK(host.plays.empty());
}

TEST_CASE("A model sound variant plays the bank entry four past the variant",
          "[audio-music-streams][audio]") {
  FakeActorSound host;
  constexpr std::int32_t kVariant = 3;
  host.models[kSoundActor] = SoundModel();
  CHECK(Audio::PlayActorModelSoundVariant(host, kSoundActor, kVariant) ==
        ActorSoundStatus::kDone);
  CHECK(Audio::PlayActorModelSoundVariant(host, kOtherSoundActor, kVariant) ==
        ActorSoundStatus::kNoActor);
  CHECK(host.plays ==
        std::vector{BankPlay(Audio::kModelSoundGroup,
                             kVariant + Audio::kModelSoundVariantBase)});
}

TEST_CASE("An actor sound pair plays one or two effects at the actor's mix",
          "[audio-music-streams][audio]") {
  FakeActorSound host;
  CHECK(Audio::PlayActorSoundPair(host, kSoundActor,
                                  {.first = kFirstPairSound}) ==
        ActorSoundStatus::kDone);
  CHECK(Audio::PlayActorSoundPair(
            host, kSoundActor,
            {.first = kFirstPairSound, .second = kSecondPairSound}) ==
        ActorSoundStatus::kDone);
  const SoundPlay first{.group = Audio::kActorSoundGroup,
                        .sound = kFirstPairSound,
                        .mix = ProjectedMix()};
  SoundPlay second = first;
  second.sound = kSecondPairSound;
  CHECK(host.plays == std::vector{first, first, second});
  host.plays.clear();
  host.records[kSoundActor].sound_override.kind = Audio::kSilencedActorSound;
  CHECK(Audio::PlayActorSoundPair(host, kSoundActor, {.first = 1}) ==
        ActorSoundStatus::kDone);
  CHECK(Audio::PlayActorSoundPair(host, kOtherSoundActor, {.first = 1}) ==
        ActorSoundStatus::kNoActor);
  CHECK(host.plays.empty());
}

TEST_CASE("A gimmick sound pair plays one or two world effects at the gimmick",
          "[audio-music-streams][audio]") {
  FakeActorSound host;
  constexpr std::int16_t kGimmickX = 0x12;
  constexpr std::int16_t kGimmickY = -0x34;
  constexpr std::int16_t kGimmickZ = 0x56;
  const SfxPoint gimmick{.x = kGimmickX, .y = kGimmickY, .z = kGimmickZ};
  Audio::PlayGimmickSoundPair(host, gimmick, {.first = kFirstPairSound});
  Audio::PlayGimmickSoundPair(
      host, gimmick, {.first = kFirstPairSound, .second = kSecondPairSound});
  constexpr std::int64_t kPack = FakeActorSound::kWorldPack;
  constexpr std::int64_t kPacked =
      (kGimmickX * kPack + kGimmickY) * kPack + kGimmickZ;
  CHECK(host.world_plays ==
        std::vector<HostCall>{
            {"world", Audio::kActorSoundGroup, kFirstPairSound, kPacked},
            {"world", Audio::kActorSoundGroup, kFirstPairSound, kPacked},
            {"world", Audio::kActorSoundGroup, kSecondPairSound, kPacked}});
  CHECK(host.plays.empty());
}

namespace {
constexpr std::uint8_t kTileX = 5;
constexpr std::uint8_t kTileZ = 6;
constexpr std::uint8_t kTileLayer = 1;
constexpr std::uint8_t kContactLayer = 2;
// Surface kind 3 with flag bits above the kind mask.
constexpr std::uint8_t kSurfaceKind = 0x63;
constexpr std::uint8_t kTileSound = 0x20;
// Object 2 with bits above the gimmick object mask.
constexpr std::uint8_t kGimmickObject = 0x0A;
constexpr std::uint8_t kGimmickSound = 0x28;
constexpr std::int32_t kPlainContact = 0;

class FakeSurfaceContact {
 public:
  FakeActorSound host;
  ActorContactSource source{.tile_x = kTileX,
                            .tile_layer = kTileLayer,
                            .tile_z = kTileZ,
                            .model = SoundModel()};

  FakeSurfaceContact() {
    host.surface = kSurfaceKind;
    host.tile_sounds = {0, 0, 0, kTileSound};
    host.object_sounds = {0, 0, kGimmickSound};
  }

  // The bank entry the contact played, nullopt for none.
  std::optional<std::int32_t> Contact(std::int32_t contact_code,
                                      std::int32_t actor_index = 0) {
    host.plays.clear();
    last_status = Audio::PlayActorSurfaceContactSound(host, actor_index, source,
                                                      contact_code);
    if (host.plays.empty()) return std::nullopt;
    return host.plays.back().sound;
  }

  ActorSoundStatus last_status = ActorSoundStatus::kDone;
};
}  // namespace

TEST_CASE(
    "A surface contact plays its tile's surface sound with the landing offsets",
    "[audio-music-streams][audio]") {
  FakeSurfaceContact contact;
  CHECK(contact.Contact(kPlainContact) == kTileSound);
  CHECK(contact.Contact(Audio::kSoftLandingContact) ==
        kTileSound + Audio::kSoftLandingSoundOffset);
  CHECK(contact.Contact(Audio::kHardLandingContact) ==
        kTileSound + Audio::kHardLandingSoundOffset);
  CHECK(contact.host.plays.back().group == Audio::kModelSoundGroup);
  CHECK(contact.host.lookups.back() ==
        HostCall{"surface", kTileX, kTileZ, kTileLayer});
  contact.source.contact_layer = kContactLayer;
  contact.Contact(kPlainContact);
  CHECK(contact.host.lookups.back() ==
        HostCall{"surface", kTileX, kTileZ, kContactLayer});
}

TEST_CASE("Wet weather adds the wet offset to plain surface contacts only",
          "[audio-music-streams][audio]") {
  FakeSurfaceContact contact;
  const std::int32_t wet = kTileSound + Audio::kWetSurfaceSoundOffset;
  contact.host.weather = {.kind = Audio::kFirstWetWeather,
                          .intensity = Audio::kWetWeatherIntensity};
  CHECK(contact.Contact(kPlainContact) == wet);
  CHECK(contact.Contact(Audio::kSoftLandingContact) ==
        kTileSound + Audio::kSoftLandingSoundOffset);
  contact.host.weather.kind = Audio::kLastWetWeather;
  CHECK(contact.Contact(kPlainContact) == wet);
  contact.host.weather.kind = Audio::kLastWetWeather + 1;
  CHECK(contact.Contact(kPlainContact) == kTileSound);
  contact.host.weather = {.kind = Audio::kFirstWetWeather,
                          .intensity = Audio::kWetWeatherIntensity - 1};
  CHECK(contact.Contact(kPlainContact) == kTileSound);
  contact.host.weather.kind = 0;
  CHECK(contact.Contact(kPlainContact) == kTileSound);
}

TEST_CASE("A contact on a gimmick plays the gimmick object's surface sound",
          "[audio-music-streams][audio]") {
  FakeSurfaceContact contact;
  contact.source.on_gimmick = true;
  contact.host.top_gimmick = kGimmickObject;
  CHECK(contact.Contact(kPlainContact) == kGimmickSound);
  CHECK(contact.host.lookups ==
        std::vector<HostCall>{{"gimmick", kTileX, kTileZ}});
  contact.host.top_gimmick.reset();
  CHECK_FALSE(contact.Contact(kPlainContact).has_value());
  CHECK(contact.last_status == ActorSoundStatus::kNoGimmick);
  constexpr std::uint8_t kObjectPastTable = 7;
  contact.host.top_gimmick = kObjectPastTable;
  CHECK_FALSE(contact.Contact(kPlainContact).has_value());
  CHECK(contact.last_status == ActorSoundStatus::kNoSurfaceSound);
}

TEST_CASE(
    "Contact sounds skip the silent animations and actor 1 and default without "
    "a surface",
    "[audio-music-streams][audio]") {
  FakeSurfaceContact contact;
  constexpr auto kLastSilentAnimation =
      static_cast<std::uint8_t>(Audio::kSilentContactAnimationBase +
                                Audio::kSilentContactAnimationCount - 1);
  contact.source.animation_id = Audio::kSilentContactAnimationBase;
  CHECK_FALSE(contact.Contact(kPlainContact).has_value());
  contact.source.animation_id = kLastSilentAnimation;
  CHECK_FALSE(contact.Contact(kPlainContact).has_value());
  contact.source.animation_id =
      static_cast<std::uint8_t>(kLastSilentAnimation + 1);
  CHECK(contact.Contact(kPlainContact) == kTileSound);
  contact.source.animation_id =
      static_cast<std::uint8_t>(Audio::kSilentContactAnimationBase - 1);
  CHECK(contact.Contact(kPlainContact) == kTileSound);
  CHECK_FALSE(
      contact.Contact(kPlainContact, Audio::kSilentContactActor).has_value());
  contact.host.surface.reset();
  CHECK(contact.Contact(kPlainContact) == Audio::kDefaultSurfaceSound);
  contact.host.surface =
      static_cast<std::uint8_t>(contact.host.tile_sounds.size());
  CHECK_FALSE(contact.Contact(kPlainContact).has_value());
  CHECK(contact.last_status == ActorSoundStatus::kNoSurfaceSound);
}

TEST_CASE(
    "A script sound plays the battle actor model's sound for the first 16 "
    "actors",
    "[audio-music-streams][audio]") {
  FakeActorSound host;
  host.present.insert(kSoundActor);
  host.script_models[kSoundActor] = SoundModel();
  CHECK(Audio::PlayActorScriptSound(host, kSoundActor, kModelSoundIndex) ==
        ActorSoundStatus::kDone);
  CHECK(host.plays ==
        std::vector{BankPlay(Audio::kModelSoundGroup, kModelSoundIndex)});
  host.lookups.clear();
  CHECK(Audio::PlayActorScriptSound(host, -1, 0) == ActorSoundStatus::kNoActor);
  CHECK(Audio::PlayActorScriptSound(host, Audio::kScriptSoundActorCount, 0) ==
        ActorSoundStatus::kDone);
  CHECK(host.lookups.empty());
  CHECK(Audio::PlayActorScriptSound(host, kOtherSoundActor, 0) ==
        ActorSoundStatus::kDone);
  host.present.insert(kOtherSoundActor);
  CHECK(Audio::PlayActorScriptSound(host, kOtherSoundActor, 0) ==
        ActorSoundStatus::kNoModel);
  CHECK(host.plays.size() == 1U);
}

namespace {
class FakeAmbience final : public Audio::AmbienceHost {
 public:
  std::vector<HostCall> calls;

  void DuckRoomEffectSound(bool ducked) override {
    calls.push_back({"room", ducked ? 1 : 0});
  }
  void DuckWeatherAmbientSound(bool ducked) override {
    calls.push_back({"weather", ducked ? 1 : 0});
  }
};
}  // namespace

TEST_CASE(
    "Room ambience ducks the room's effects and then the weather together",
    "[audio-music-streams][audio]") {
  FakeAmbience host;
  Audio::SetRoomAmbienceDucked(host, true);
  Audio::SetRoomAmbienceDucked(host, false);
  CHECK(host.calls ==
        std::vector<HostCall>{
            {"room", 1}, {"weather", 1}, {"room", 0}, {"weather", 0}});
}

TEST_CASE("Live XA stream requests drive the disc drive record's state",
          "[audio-music-streams][audio]") {
  const GlobalScope drive(Content_discDriveState);
  const auto state = [] {
    return static_cast<Content::DiscDriveState>(
        static_cast<std::uint8_t>(Content_discDriveState.state));
  };
  CHECK(Audio::LiveBeginXaStream(kXaSector, kXaBytes) == 1);
  CHECK(state() == Content::DiscDriveState::kReadXaInit);
  CHECK(Content_discDriveState.startSector == kXaSector);
  CHECK(Content_discDriveState.byteCount == kXaBytes);
  CHECK(Audio::LiveBeginXaStream(1, 2) == 0);
  Content_discDriveState.state =
      static_cast<char>(Content::DiscDriveState::kStreamXa);
  CHECK(Audio::LiveRequestXaStreamStop() == 1);
  CHECK(state() == Content::DiscDriveState::kStreamXaEnd);
  CHECK(Audio::LiveRequestXaStreamStop() == 0);
}

TEST_CASE(
    "Live music entries refuse slots and ids past their tables and read ids "
    "unsigned",
    "[audio-music-streams][audio]") {
  const GlobalScope sound(Audio_soundState);
  const std::uint64_t refused = Seq::LiveRefusalCount();
  CHECK(Audio::LiveMusicSlotVolume(kSlotPastLast) == 0);
  CHECK(Audio::LiveStopMusicSlotPlayback(kSlotPastLast) == 0);
  CHECK(Audio::LiveLoadMusicSlot(Audio::kMusicFileCount, 1) == 0);
  CHECK(Seq::LiveRefusalCount() == refused + 3);
  Audio_soundState.musicIds[kSecondSlot] = static_cast<char>(kHighMusicId);
  Audio_soundState.currentMusicSlot = 2;
  CHECK(Audio::LiveCurrentMusicSlot() == 2);
  CHECK(Audio::LiveCurrentMusicId() == kHighMusicId);
  constexpr int kStreamSlotPastLast = 4;
  CHECK(Audio::LivePlayCutsceneStreamSlot(2, kStreamPan, kStreamVolume) == 0);
  CHECK(Audio::LiveFreeCutsceneStreamSlot(kStreamSlotPastLast) == 0);
  CHECK(Seq::LiveRefusalCount() == refused + 3);
}

TEST_CASE("Live actor sound entries refuse missing records, actors and outputs",
          "[audio-music-streams][audio]") {
  const std::uint64_t refused = Seq::LiveRefusalCount();
  constexpr int kUntouched = 0x5A;
  int pan = kUntouched;
  int volume = kUntouched;
  Audio::LiveActorSoundPanAndVolume(0, nullptr, &volume);
  Audio::LiveActorSoundPanAndVolume(-1, &pan, &volume);
  Audio::LivePlayActorModelSound(nullptr, 0);
  Audio::LivePlayActorModelSoundVariant(-1, 0);
  Audio::LivePlayActorSoundPair(nullptr, 1, 2);
  Audio::LivePlayGimmickSoundPair(nullptr, 1, 2);
  Audio::LivePlayActorSurfaceContactSound(0, nullptr, 0);
  Audio::LivePlayActorScriptSound(-1, 0);
  CHECK(Seq::LiveRefusalCount() == refused + 8);
  CHECK(pan == kUntouched);
  CHECK(volume == kUntouched);
  // Actors from 16 play no script sound, as in the original.
  Audio::LivePlayActorScriptSound(Audio::kScriptSoundActorCount, 0);
  CHECK(Seq::LiveRefusalCount() == refused + 8);
}

TEST_CASE(
    "Live stream buffer callbacks stay silent while idle and refuse a voice "
    "past the pairs",
    "[audio-music-streams][audio]") {
  const GlobalScope stream(Audio_Cutscene_streamState);
  const std::uint64_t refused = Seq::LiveRefusalCount();
  Cutscene::LiveRefillMonoFirstBuffer();
  CHECK(Seq::LiveRefusalCount() == refused);
  constexpr unsigned kPages = 0x2000;
  Audio_Cutscene_streamState.voiceMask = static_cast<int>(kPairMask);
  Audio_Cutscene_streamState.totalPages = kPages;
  Audio_Cutscene_streamState.voiceIndex = Cutscene::kStreamVoiceCount;
  Cutscene::LiveRefillStereoSecondBuffer();
  Cutscene::LiveBeginMonoPlayback();
  CHECK(Seq::LiveRefusalCount() == refused + 2);
  CHECK(Audio_Cutscene_streamState.totalPages == kPages);
  CHECK(Audio_Cutscene_streamState.voiceIndex == Cutscene::kStreamVoiceCount);
}

TEST_CASE("Promoted music and stream functions keep no module binding",
          "[audio-music-streams][audio][redirect-binding]") {
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  const std::array<std::uint32_t, 40> retired{
      0x80044D94U, 0x80044DB4U, 0x80044DD4U, 0x80044DF4U, 0x80044EC8U,
      0x80044F60U, 0x80045000U, 0x800450D4U, 0x800450E4U, 0x80045110U,
      0x80045270U, 0x800452C8U, 0x80045350U, 0x8004539CU, 0x800453F4U,
      0x80045440U, 0x800454B8U, 0x8004550CU, 0x8004552CU, 0x80045574U,
      0x800455ACU, 0x800455F4U, 0x800449E8U, 0x80044A38U, 0x800464DCU,
      0x800464FCU, 0x80046578U, 0x8004659CU, 0x800468DCU, 0x8001CDD0U,
      0x8001CE60U, 0x8001D0E4U, 0x8001D2A0U, 0x8001D370U, 0x8001D3D4U,
      0x8001D438U, 0x8001D584U, 0x8001D5B4U, 0x8001D5E4U, 0x8001D614U,
  };
  for (const std::uint32_t address : retired) {
    CAPTURE(address);
    CHECK(VS::Content::ResolveModuleAddress(symbols, "SLUS_010.40", address) ==
          nullptr);
  }
}

TEST_CASE("Promoted battle actor sound functions keep no module binding",
          "[audio-music-streams][audio][redirect-binding]") {
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  const std::array<std::uint32_t, 10> retired{
      0x8006CD60U, 0x8007BE10U, 0x8007C8A4U, 0x8008E480U, 0x800AD62CU,
      0x800AE4FCU, 0x800AE68CU, 0x800AE6C0U, 0x800AE7D8U, 0x800AE828U,
  };
  for (const std::uint32_t address : retired) {
    CAPTURE(address);
    CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                            address) == nullptr);
  }
}
