#pragma once

#include <cstdint>
#include <optional>
#include <span>

namespace VS::Event {

inline constexpr int kCutsceneActorCount = 16;
inline constexpr std::uint32_t kNoCutsceneMusicHandle = 0xFFFF;

enum class CutscenePreparation : std::uint8_t { kWaiting, kPreparing };

struct CutscenePreferences {
  bool weapon_drawn = false;
  int camera_pitch = 0;
  int camera_distance = 0;
};

struct CutsceneMusic {
  int room_slot = 0;
  std::uint32_t playing_handle = kNoCutsceneMusicHandle;
  std::span<const std::uint32_t> handles;
};

enum class CutsceneRefusal : std::uint8_t {
  kMissingPlayer,
  kMusicSlot,
  kMissingActor,
};

class CutsceneHost {
 public:
  CutsceneHost() = default;
  CutsceneHost(const CutsceneHost&) = delete;
  CutsceneHost& operator=(const CutsceneHost&) = delete;
  CutsceneHost(CutsceneHost&&) = delete;
  CutsceneHost& operator=(CutsceneHost&&) = delete;
  virtual ~CutsceneHost() = default;

  virtual void Refuse(CutsceneRefusal reason, int value) = 0;
  [[nodiscard]] virtual std::optional<CutscenePreferences> Preferences() = 0;
  virtual void SetPreparation(CutscenePreparation preparation) = 0;
  virtual void SetFlag(int flag, std::uint8_t value) = 0;
  [[nodiscard]] virtual std::optional<std::uint8_t> ActorCommand(int actor) = 0;
  virtual void SetActorCommand(int actor, std::uint8_t command) = 0;
  virtual void ResetActorHeadTracking(int actor) = 0;
  virtual void RefreshActorReservations(int actor) = 0;
  virtual void SetMarkersHidden(bool hidden) = 0;
  virtual void ClearMenuRequest() = 0;
  virtual void PauseMovingPlatforms(bool paused) = 0;
  virtual void RequestEventStart() = 0;
  [[nodiscard]] virtual std::uint16_t PendingEventSelector() = 0;
  virtual void DisableFirstPersonView() = 0;
  virtual void RebuildActionPlans() = 0;
  virtual void UpdateRouteGates() = 0;
  [[nodiscard]] virtual bool EvaluateFollowOnTrigger() = 0;
  [[nodiscard]] virtual bool SceneTimerActive() = 0;
  [[nodiscard]] virtual bool EvaluateTimerTrigger() = 0;
  virtual void ClearActorSoundOverrides() = 0;
  virtual void ResetActorPaletteFade(int actor) = 0;
  virtual void ResetActorTint(int actor) = 0;
  virtual void RestoreRoomEffectVolume() = 0;
  virtual void RestoreCamera() = 0;
  virtual void ShowGameplayHud() = 0;
  [[nodiscard]] virtual CutsceneMusic Music() = 0;
  virtual void PlayRoomMusic(int slot) = 0;
  virtual void FadeRoomMusic(int slot) = 0;
  virtual void EnterFieldExploration() = 0;
};

// decomp: BATTLE.PRG func_80073718 (src/BATTLE/BATTLE.PRG/146C.c:4720)
[[nodiscard]] bool PrepareCutsceneActors(CutsceneHost& host);
// decomp: BATTLE.PRG func_80073870 (src/BATTLE/BATTLE.PRG/146C.c:4761)
[[nodiscard]] bool StartPreparedCutscene(CutsceneHost& host);
// decomp: BATTLE.PRG func_80073898 (src/BATTLE/BATTLE.PRG/146C.c:4767)
[[nodiscard]] bool BeginQueuedCutscene(CutsceneHost& host);
// decomp: BATTLE.PRG func_800738E4 (src/BATTLE/BATTLE.PRG/146C.c:4777)
void EndFieldCutscene(CutsceneHost& host);

}  // namespace VS::Event
