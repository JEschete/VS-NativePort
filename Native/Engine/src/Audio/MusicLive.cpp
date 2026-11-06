// Hand-maintained (tools/port/owned.csv).
// Live binding for Music.hpp: the slot record (vs_main_soundData), the music
// file table (_musicLBAs, _musicFileSizes), the disc request queue, the high
// end of the game heap, the playback front end, the sound control requests
// and the zone context's music switch (D_800F18B0).
#include "Audio/MusicLive.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>

#include "Audio/Music.hpp"
#include "Audio/SeqRecordsLive.hpp"
#include "Audio/SoundBanks.hpp"
#include "Audio/SoundControl.hpp"
#include "Audio/SoundControlLive.hpp"
#include "Audio/SoundPlaybackLive.hpp"
#include "Audio/SoundSlotsLive.hpp"
#include "Content/CdDrive.hpp"
#include "Core/Runtime.hpp"
#include "State/GameClockLive.hpp"

extern "C" {
#include "Audio/LegacyFiles.h"
#include "Audio/SoundBanks.h"
#include "Field/RoomLoadingLiveState.h"
#include "Field/Zone.h"
#include "Legacy/Psx.h"
}

namespace VS::Audio {
namespace {
static_assert(std::size(Audio_soundState.musicIds) == kMusicSlotCount);
static_assert(std::size(Audio_soundState.musicHandles) == kMusicSlotCount);
static_assert(std::size(Audio_soundState.musicData) == kMusicSlotCount);
static_assert(std::size(Audio_soundState.musicRequests) == kMusicSlotCount);
static_assert(sizeof(Audio_soundState.musicIds[0]) == sizeof(std::uint8_t));
static_assert(sizeof(Audio_soundState.currentMusicSlot) ==
              sizeof(std::int32_t));
static_assert(sizeof(Field_ZoneContext::isMusicFadeEnabled) ==
              sizeof(std::int32_t));

const char* RefusalReason(MusicRefusal refusal) {
  switch (refusal) {
    case MusicRefusal::kSlotOutOfRange:
      return "the music slot is past the four slots";
    case MusicRefusal::kFreeWithoutSlot:
      return "music slot 0 names no buffer to free";
    case MusicRefusal::kMusicFileOutOfRange:
      return "the music id is past the music file table";
  }
  return "refused";
}

class LiveMusic final : public MusicHost {
 public:
  explicit LiveMusic(const char* operation) : operation_(operation) {}

  MusicPlayback Playback() override {
    const Audio_SoundState& state = Audio_soundState;
    return {.slot = state.currentMusicSlot,
            .handle = state.playingMusicHandle,
            .music_id = state.currentMusicId,
            .data = LiveBankBuffer(state.currentMusicData)};
  }

  void StorePlayback(const MusicPlayback& playback) override {
    Audio_SoundState& state = Audio_soundState;
    state.currentMusicSlot = playback.slot;
    state.playingMusicHandle = playback.handle;
    state.currentMusicId = playback.music_id;
    state.currentMusicData = LiveBufferPointer(playback.data);
  }

  // The ids are bytes the original read with lbu.
  MusicSlot Music(std::size_t index) override {
    if (index >= kMusicSlotCount) return {};
    const Audio_SoundState& state = Audio_soundState;
    return {.id = static_cast<std::uint8_t>(state.musicIds[index]),
            .handle = state.musicHandles[index],
            .data = LiveBankBuffer(state.musicData[index]),
            .request = LiveBankRequest(state.musicRequests[index])};
  }

  void StoreMusic(std::size_t index, const MusicSlot& slot) override {
    if (index >= kMusicSlotCount) return;
    Audio_SoundState& state = Audio_soundState;
    state.musicIds[index] = static_cast<char>(slot.id);
    state.musicHandles[index] = slot.handle;
    state.musicData[index] = LiveBufferPointer(slot.data);
    state.musicRequests[index] = LiveRequestSlot(slot.request);
  }

  std::optional<Content::DiscFileExtent> MusicFile(std::int32_t id) override {
    return LiveTableFile(Audio_legacyMusicDiscOffsets,
                         Audio_legacyMusicFileSizes, kMusicFileCount, id);
  }

  std::uint32_t StartMusic(BankBuffer music) override {
    return Playback::LiveStartMusic(LiveBufferPointer(music));
  }

  BankBuffer AllocateBuffer(std::int32_t bytes) override {
    return AllocateLiveSlotBuffer(bytes);
  }

  std::uint32_t QueueMusicTransition(BankBuffer music) override {
    return Playback::LiveQueueMusicTransition(LiveBufferPointer(music));
  }

  std::optional<DiscRequestRef> AllocateRequest(
      const Content::DiscFileExtent& file) override {
    return AllocateLiveSlotRequest(file);
  }

  std::uint32_t CrossfadeMusic(BankBuffer music, std::uint32_t fade_ticks,
                               std::int32_t target_volume) override {
    return Playback::LiveCrossfadeMusic(LiveBufferPointer(music), fade_ticks,
                                        target_volume);
  }

  void EnqueueRequest(DiscRequestRef request, BankBuffer destination) override {
    EnqueueLiveSlotRequest(request, destination);
  }

  void StopMusic(std::uint32_t handle) override {
    Playback::LiveStopMusic(handle);
  }

  DiscRequestRef CompleteRefusedRequest(const Content::DiscFileExtent& file,
                                        BankBuffer destination) override {
    return CompleteLiveRefusedSlotRequest(file, destination);
  }

  std::int32_t MusicVolume(std::uint32_t handle) override {
    return Playback::LiveMusicVolume(handle);
  }

  Content::DiscRequestState RequestState(DiscRequestRef request) override {
    return LiveSlotRequestState(request);
  }

  std::int32_t MusicPosition() override {
    return Playback::LiveMusicPosition();
  }

  void ReleaseRequest(DiscRequestRef request) override {
    ReleaseLiveSlotRequest(request);
  }

  void SaveMusicState() override { Playback::LiveSaveMusicState(); }

  void WaitFrame() override { State::UpdateLivePlayTimeAndLoading(0); }

  // func_80012468 takes the threshold in its pointer argument.
  void SetMusicBranchThreshold(std::int32_t threshold) override {
    Control::LiveSetMusicBranchThreshold(
        std::bit_cast<void*>(static_cast<std::intptr_t>(threshold)));
  }

  void FreeBuffer(BankBuffer buffer) override { FreeLiveSlotBuffer(buffer); }

  void FadeMusicVolume(std::uint32_t handle, Control::Fade fade) override {
    Control::LiveFadeMusicVolume(handle, fade.ticks, fade.target);
  }

  void FadeMusicTempoScale(Control::Fade fade) override {
    Control::LiveFadeMusicTempoScale(fade.ticks, fade.target);
  }

  void FadeMusicPitchScale(Control::Fade fade) override {
    Control::LiveFadeMusicPitchScale(fade.ticks, fade.target);
  }

  void StopAllMusic() override { Control::LiveStopAllMusic(); }

  void StoreZoneMusicSwitch(std::int32_t enabled) override {
    Field_zoneContext.isMusicFadeEnabled = enabled;
  }

  void Diagnose(std::uint32_t code) override {
    Core::IgnoreDiagnostic(static_cast<int>(code), 0);
  }

  void Refuse(MusicRefusal refusal) override {
    Seq::ReportRefusal(operation_, RefusalReason(refusal));
  }

 private:
  const char* operation_;
};
}  // namespace

// decomp: BATTLE.PRG func_8007BE10 (src/BATTLE/BATTLE.PRG/146C.c:8579)
int LiveSetZoneMusicSwitchEnabled(int enabled) {
  LiveMusic host("SetZoneMusicSwitchEnabled");
  return SetZoneMusicSwitchEnabled(host, enabled);
}

// decomp: SLUS_010.40 func_80044D94 (src/SLUS_010.40/main.c:9490)
int LiveMusicBeatPosition() {
  LiveMusic host("MusicBeatPosition");
  return MusicBeatPosition(host);
}

// decomp: SLUS_010.40 func_80044DB4 (src/SLUS_010.40/main.c:9492)
void LiveSaveMusicPlaybackState() {
  LiveMusic host("SaveMusicPlaybackState");
  SaveMusicPlaybackState(host);
}

// decomp: SLUS_010.40 func_80044DD4 (src/SLUS_010.40/main.c:9494)
void LiveSetMusicJumpThreshold(int threshold) {
  LiveMusic host("SetMusicJumpThreshold");
  SetMusicJumpThreshold(host, threshold);
}

// decomp: SLUS_010.40 func_80044DF4 (src/SLUS_010.40/main.c:9496)
int LiveStopMusicSlotPlayback(int id) {
  LiveMusic host("StopMusicSlotPlayback");
  return StopMusicSlotPlayback(host, id);
}

// decomp: SLUS_010.40 func_80044EC8 (src/SLUS_010.40/main.c:9525)
int LiveResumeMusicSlot(int id) {
  LiveMusic host("ResumeMusicSlot");
  return ResumeMusicSlot(host, id);
}

// decomp: SLUS_010.40 func_80044F60 (src/SLUS_010.40/main.c:9545)
int LiveCrossfadeMusicSlot(int id, int target_volume, unsigned fade_ticks) {
  LiveMusic host("CrossfadeMusicSlot");
  return CrossfadeMusicSlot(host, id, target_volume, fade_ticks);
}

// decomp: SLUS_010.40 func_80045000 (src/SLUS_010.40/main.c:9565)
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
int LivePlayMusicSlot(int id, int target_volume, int fade_ticks) {
  LiveMusic host("PlayMusicSlot");
  return PlayMusicSlot(host, id,
                       {.ticks = fade_ticks, .target = target_volume});
}

// decomp: SLUS_010.40 vs_main_getcurrentMusicSlot (src/SLUS_010.40/main.c:9587)
int LiveCurrentMusicSlot() {
  LiveMusic host("CurrentMusicSlot");
  return CurrentMusicSlot(host);
}

// decomp: SLUS_010.40 vs_main_getCurrentMusicId (src/SLUS_010.40/main.c:9592)
int LiveCurrentMusicId() {
  LiveMusic host("CurrentMusicId");
  return CurrentMusicId(host);
}

// decomp: SLUS_010.40 vs_main_loadMusicSlot (src/SLUS_010.40/main.c:9600)
int LiveLoadMusicSlot(int id, int target_slot) {
  LiveMusic host("LoadMusicSlot");
  return LoadMusicSlot(host, id, target_slot);
}

// decomp: SLUS_010.40 _loadMusicIntoFreeSlot (src/SLUS_010.40/main.c:9643)
int LiveLoadMusicIntoFreeSlot(int id) {
  LiveMusic host("LoadMusicIntoFreeSlot");
  return LoadMusicIntoFreeSlot(host, id);
}

// decomp: SLUS_010.40 vs_main_clearMusicLoadSlot (src/SLUS_010.40/main.c:9655)
int LivePollMusicSlotLoad(unsigned slot) {
  LiveMusic host("PollMusicSlotLoad");
  return PollMusicSlotLoad(host, slot);
}

// decomp: SLUS_010.40 vs_main_clearMusicLoadQueue (src/SLUS_010.40/main.c:9677)
int LivePollMusicLoads() {
  LiveMusic host("PollMusicLoads");
  return PollMusicLoads(host);
}

// decomp: SLUS_010.40 vs_main_loadAndWaitMusicSlot
// (src/SLUS_010.40/main.c:9691)
int LiveLoadMusicSlotBlocking(int id, int target_slot) {
  LiveMusic host("LoadMusicSlotBlocking");
  return LoadMusicSlotBlocking(host, id, target_slot);
}

// decomp: SLUS_010.40 _loadMusicIntoFreeSlotAndClearQueue
// (src/SLUS_010.40/main.c:9704)
int LiveLoadFreeMusicSlotBlocking(int id) {
  LiveMusic host("LoadFreeMusicSlotBlocking");
  return LoadFreeMusicSlotBlocking(host, id);
}

// decomp: SLUS_010.40 vs_main_freeMusic (src/SLUS_010.40/main.c:9717)
int LiveFreeMusicSlot(int id) {
  LiveMusic host("FreeMusicSlot");
  return FreeMusicSlot(host, id);
}

// decomp: SLUS_010.40 func_800454B8 (src/SLUS_010.40/main.c:9733)
int LiveMusicSlotVolume(int id) {
  LiveMusic host("MusicSlotVolume");
  return MusicSlotVolume(host, id);
}

// decomp: SLUS_010.40 func_8004550C (src/SLUS_010.40/main.c:9743)
int LiveQueryMusicSlotVolume(int id) {
  LiveMusic host("QueryMusicSlotVolume");
  return QueryMusicSlotVolume(host, id);
}

// decomp: SLUS_010.40 func_8004552C (src/SLUS_010.40/main.c:9745)
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
int LiveFadeMusicSlotVolume(int id, int target_volume, int fade_ticks) {
  LiveMusic host("FadeMusicSlotVolume");
  return FadeMusicSlotVolume(host, id,
                             {.ticks = fade_ticks, .target = target_volume});
}

// decomp: SLUS_010.40 func_80045574 (src/SLUS_010.40/main.c:9754)
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
int LiveFadeMusicSlotTempo(int id, int target_scale, int fade_ticks) {
  LiveMusic host("FadeMusicSlotTempo");
  return FadeMusicSlotTempo(host, id,
                            {.ticks = fade_ticks, .target = target_scale});
}

// decomp: SLUS_010.40 func_800455AC (src/SLUS_010.40/main.c:9763)
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
int LiveFadeMusicSlotPitch(int id, int target_scale, int fade_ticks) {
  LiveMusic host("FadeMusicSlotPitch");
  return FadeMusicSlotPitch(host, id,
                            {.ticks = fade_ticks, .target = target_scale});
}

// decomp: SLUS_010.40 vs_main_stopMusic (src/SLUS_010.40/main.c:9776)
void LiveStopAndFreeCurrentMusic() {
  LiveMusic host("StopAndFreeCurrentMusic");
  StopAndFreeCurrentMusic(host);
}
}  // namespace VS::Audio
