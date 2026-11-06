// Hand-maintained (tools/port/owned.csv).
// Typed music slots (Music.hpp).
#include <cstddef>
#include <cstdint>
#include <optional>

#include "Audio/Music.hpp"
#include "Audio/SoundBanks.hpp"
#include "Audio/SoundControl.hpp"
#include "Content/CdDrive.hpp"

namespace VS::Audio {
namespace {
// Slot `id`'s index; nullopt outside 1 to kMusicSlotCount.
std::optional<std::size_t> SlotIndex(std::int32_t id) {
  if (id < 1 || id > static_cast<std::int32_t>(kMusicSlotCount)) {
    return std::nullopt;
  }
  return static_cast<std::size_t>(id - 1);
}

// Slot `id`'s index, refusing a slot past the slots.
std::optional<std::size_t> CheckedSlotIndex(MusicHost& host, std::int32_t id) {
  const std::optional<std::size_t> index = SlotIndex(id);
  if (!index) host.Refuse(MusicRefusal::kSlotOutOfRange);
  return index;
}

void ForgetPlayingMusic(MusicHost& host) {
  MusicPlayback playback = host.Playback();
  playback.slot = 0;
  playback.data = BankBuffer::kNone;
  playback.handle = kNoMusicHandle;
  host.StorePlayback(playback);
}

// The three slot players (func_80044EC8, func_80044F60, func_80045000): when
// the slot holds music, it becomes the playing slot, `start` plays it, a
// handle it returns becomes the slot's and the playing one, and the slot's
// music id becomes the playing id. Returns the playing handle.
template <typename Start>
std::int32_t PlaySlotMusic(MusicHost& host, std::int32_t id, Start start) {
  if (id == 0) return 0;
  const std::optional<std::size_t> index = CheckedSlotIndex(host, id);
  if (!index) return 0;
  MusicSlot music = host.Music(*index);
  if (music.data != BankBuffer::kNone) {
    MusicPlayback playback = host.Playback();
    playback.slot = id;
    playback.data = music.data;
    host.StorePlayback(playback);
    const std::uint32_t handle = start(music.data);
    playback = host.Playback();
    if (handle != 0) {
      music.handle = handle;
      host.StoreMusic(*index, music);
      playback.handle = handle;
    }
    playback.music_id = music.id;
    host.StorePlayback(playback);
  }
  return static_cast<std::int32_t>(host.Playback().handle);
}
}  // namespace

// decomp: BATTLE.PRG func_8007BE10 (src/BATTLE/BATTLE.PRG/146C.c:8579)
std::int32_t SetZoneMusicSwitchEnabled(MusicHost& host, std::int32_t enabled) {
  host.StoreZoneMusicSwitch(enabled);
  return enabled;
}

// decomp: SLUS_010.40 func_80044D94 (src/SLUS_010.40/main.c:9490)
std::int32_t MusicBeatPosition(MusicHost& host) { return host.MusicPosition(); }

// decomp: SLUS_010.40 func_80044DB4 (src/SLUS_010.40/main.c:9492)
void SaveMusicPlaybackState(MusicHost& host) { host.SaveMusicState(); }

// decomp: SLUS_010.40 func_80044DD4 (src/SLUS_010.40/main.c:9494)
void SetMusicJumpThreshold(MusicHost& host, std::int32_t threshold) {
  host.SetMusicBranchThreshold(threshold);
}

// decomp: SLUS_010.40 func_80044DF4 (src/SLUS_010.40/main.c:9496)
std::int32_t StopMusicSlotPlayback(MusicHost& host, std::int32_t id) {
  if (id == 0) {
    host.StopAllMusic();
    ForgetPlayingMusic(host);
    for (std::size_t index = kMusicSlotCount; index-- > 0;) {
      MusicSlot music = host.Music(index);
      music.handle = kNoMusicHandle;
      host.StoreMusic(index, music);
    }
    return 1;
  }
  const std::optional<std::size_t> index = CheckedSlotIndex(host, id);
  if (!index) return 0;
  MusicSlot music = host.Music(*index);
  if (music.handle != kNoMusicHandle) {
    host.StopMusic(music.handle);
    music.handle = kNoMusicHandle;
    host.StoreMusic(*index, music);
  }
  if (host.Playback().slot == id) ForgetPlayingMusic(host);
  return 1;
}

// decomp: SLUS_010.40 func_80044EC8 (src/SLUS_010.40/main.c:9525)
std::int32_t ResumeMusicSlot(MusicHost& host, std::int32_t id) {
  return PlaySlotMusic(host, id, [&host](BankBuffer music) {
    return host.QueueMusicTransition(music);
  });
}

// decomp: SLUS_010.40 func_80044F60 (src/SLUS_010.40/main.c:9545)
std::int32_t CrossfadeMusicSlot(MusicHost& host, std::int32_t id,
                                std::int32_t target_volume,
                                std::uint32_t fade_ticks) {
  return PlaySlotMusic(host, id, [&](BankBuffer music) {
    return host.CrossfadeMusic(music, fade_ticks, target_volume);
  });
}

// decomp: SLUS_010.40 func_80045000 (src/SLUS_010.40/main.c:9565)
std::int32_t PlayMusicSlot(MusicHost& host, std::int32_t id,
                           Control::Fade volume) {
  return PlaySlotMusic(host, id, [&](BankBuffer music) {
    host.FadeMusicVolume(0, volume);
    const std::uint32_t handle = host.StartMusic(music);
    if (handle != 0) host.FadeMusicVolume(0, volume);
    return handle;
  });
}

// decomp: SLUS_010.40 vs_main_getcurrentMusicSlot (src/SLUS_010.40/main.c:9587)
std::int32_t CurrentMusicSlot(MusicHost& host) { return host.Playback().slot; }

// decomp: SLUS_010.40 vs_main_getCurrentMusicId (src/SLUS_010.40/main.c:9592)
std::int32_t CurrentMusicId(MusicHost& host) {
  const std::int32_t slot = host.Playback().slot;
  if (slot == 0) return 0;
  const std::optional<std::size_t> index = CheckedSlotIndex(host, slot);
  if (!index) return 0;
  return host.Music(*index).id;
}

// decomp: SLUS_010.40 vs_main_loadMusicSlot (src/SLUS_010.40/main.c:9600)
std::int32_t LoadMusicSlot(MusicHost& host, std::int32_t id,
                           std::int32_t target_slot) {
  const std::optional<std::size_t> index =
      id > 0 ? SlotIndex(target_slot) : std::nullopt;
  if (!index) {
    host.Diagnose(kMusicSlotDiagnostic);
    return 0;
  }
  if (host.Music(*index).id == id) return target_slot;
  const std::optional<Content::DiscFileExtent> file = host.MusicFile(id);
  if (!file) {
    host.Refuse(MusicRefusal::kMusicFileOutOfRange);
    return 0;
  }
  if (host.Music(*index).data != BankBuffer::kNone) {
    host.Diagnose(kMusicSlotDiagnostic);
    FreeMusicSlot(host, target_slot);
  }
  MusicSlot music = host.Music(*index);
  if (music.request.mark == RequestMark::kActive) {
    host.Diagnose(kMusicRequestHeldDiagnostic);
  }
  const std::optional<DiscRequestRef> queued = host.AllocateRequest(*file);
  if (music.data != BankBuffer::kNone) host.Diagnose(kMusicSlotDiagnostic);
  music.data = host.AllocateBuffer(file->bytes);
  DiscRequestRef request = DiscRequestRef::kNone;
  if (queued) {
    request = *queued;
    host.EnqueueRequest(request, music.data);
  } else {
    request = host.CompleteRefusedRequest(*file, music.data);
  }
  music.request = {.mark = RequestMark::kActive, .request = request};
  music.id = static_cast<std::uint8_t>(id);
  host.StoreMusic(*index, music);
  return target_slot;
}

// decomp: SLUS_010.40 _loadMusicIntoFreeSlot (src/SLUS_010.40/main.c:9643)
std::int32_t LoadMusicIntoFreeSlot(MusicHost& host, std::int32_t id) {
  for (std::size_t index = 0; index < kMusicSlotCount; ++index) {
    if (host.Music(index).data == BankBuffer::kNone) {
      return LoadMusicSlot(host, id, static_cast<std::int32_t>(index + 1));
    }
  }
  return 0;
}

// decomp: SLUS_010.40 vs_main_clearMusicLoadSlot (src/SLUS_010.40/main.c:9655)
std::int32_t PollMusicSlotLoad(MusicHost& host, std::uint32_t slot) {
  if (slot - 1U >= kMusicSlotCount) return kLoadIdle;
  const std::size_t index = slot - 1U;
  MusicSlot music = host.Music(index);
  if (music.request.mark != RequestMark::kActive) return kLoadIdle;
  const Content::DiscRequestState state =
      host.RequestState(music.request.request);
  const bool loaded = state == Content::DiscRequestState::kLoaded;
  if (!loaded && state != Content::DiscRequestState::kError) {
    return kLoadInFlight;
  }
  host.ReleaseRequest(music.request.request);
  music.request =
      loaded ? BankRequest{} : BankRequest{.mark = RequestMark::kFailed};
  host.StoreMusic(index, music);
  return loaded ? kLoadIdle : kLoadFailed;
}

// decomp: SLUS_010.40 vs_main_clearMusicLoadQueue (src/SLUS_010.40/main.c:9677)
std::int32_t PollMusicLoads(MusicHost& host) {
  std::int32_t loads = kLoadIdle;
  for (std::uint32_t slot = 1; slot <= kMusicSlotCount; ++slot) {
    loads |= PollMusicSlotLoad(host, slot);
  }
  return loads;
}

// decomp: SLUS_010.40 vs_main_loadAndWaitMusicSlot
// (src/SLUS_010.40/main.c:9691)
std::int32_t LoadMusicSlotBlocking(MusicHost& host, std::int32_t id,
                                   std::int32_t target_slot) {
  const std::int32_t loaded = LoadMusicSlot(host, id, target_slot);
  if (loaded != 0) {
    while (PollMusicSlotLoad(host, static_cast<std::uint32_t>(target_slot)) !=
           kLoadIdle) {
      host.WaitFrame();
    }
  }
  return loaded;
}

// decomp: SLUS_010.40 _loadMusicIntoFreeSlotAndClearQueue
// (src/SLUS_010.40/main.c:9704)
std::int32_t LoadFreeMusicSlotBlocking(MusicHost& host, std::int32_t id) {
  const std::int32_t loaded = LoadMusicIntoFreeSlot(host, id);
  if (loaded != 0) {
    while (PollMusicLoads(host) != kLoadIdle) host.WaitFrame();
  }
  return loaded;
}

// decomp: SLUS_010.40 vs_main_freeMusic (src/SLUS_010.40/main.c:9717)
std::int32_t FreeMusicSlot(MusicHost& host, std::int32_t id) {
  if (StopMusicSlotPlayback(host, id) == 0) return 0;
  const std::optional<std::size_t> index = SlotIndex(id);
  if (!index) {
    host.Refuse(MusicRefusal::kFreeWithoutSlot);
    return 0;
  }
  MusicSlot music = host.Music(*index);
  if (music.data == BankBuffer::kNone) return 0;
  host.FreeBuffer(music.data);
  music.data = BankBuffer::kNone;
  music.id = 0;
  host.StoreMusic(*index, music);
  return 1;
}

// decomp: SLUS_010.40 func_800454B8 (src/SLUS_010.40/main.c:9733)
std::int32_t MusicSlotVolume(MusicHost& host, std::int32_t id) {
  if (id == 0) return 0;
  const std::optional<std::size_t> index = CheckedSlotIndex(host, id);
  if (!index) return 0;
  const std::uint32_t handle = host.Music(*index).handle;
  if (handle == kNoMusicHandle) return 0;
  return host.MusicVolume(handle);
}

// decomp: SLUS_010.40 func_8004550C (src/SLUS_010.40/main.c:9743)
std::int32_t QueryMusicSlotVolume(MusicHost& host, std::int32_t id) {
  return MusicSlotVolume(host, id);
}

// decomp: SLUS_010.40 func_8004552C (src/SLUS_010.40/main.c:9745)
std::int32_t FadeMusicSlotVolume(MusicHost& host, std::int32_t id,
                                 Control::Fade volume) {
  const MusicPlayback playback = host.Playback();
  if (playback.slot != id) return 0;
  host.FadeMusicVolume(playback.handle, volume);
  return 1;
}

// decomp: SLUS_010.40 func_80045574 (src/SLUS_010.40/main.c:9754)
std::int32_t FadeMusicSlotTempo(MusicHost& host, std::int32_t id,
                                Control::Fade tempo_scale) {
  if (host.Playback().slot != id) return 0;
  host.FadeMusicTempoScale(tempo_scale);
  return 1;
}

// decomp: SLUS_010.40 func_800455AC (src/SLUS_010.40/main.c:9763)
std::int32_t FadeMusicSlotPitch(MusicHost& host, std::int32_t id,
                                Control::Fade pitch_scale) {
  if (host.Playback().slot != id) return 0;
  host.FadeMusicPitchScale(pitch_scale);
  return 1;
}

// decomp: SLUS_010.40 vs_main_stopMusic (src/SLUS_010.40/main.c:9776)
void StopAndFreeCurrentMusic(MusicHost& host) {
  const std::int32_t id = host.Playback().slot;
  if (id == 0) return;
  StopMusicSlotPlayback(host, id);
  FreeMusicSlot(host, id);
}
}  // namespace VS::Audio
