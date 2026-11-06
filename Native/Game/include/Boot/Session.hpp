#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstddef>
#include <cstdint>

#include "Core/Math/ScalarMath.hpp"

// Native title and gameplay sessions. The original resets the stack pointer to
// a saved checkpoint to abandon the current call stack (overlay.c:20-21). The
// native session loop keeps that as a nonlocal transition to the session
// boundary, which on this toolchain runs no destructors in the abandoned
// frames (#266). The policy below decides whether a transition may happen;
// the host performs it, so the core reaches no global state.
namespace VS::Boot {
enum class SessionDestination : std::uint8_t { kTitle = 1, kGameLoop = 2 };

enum class SessionRefusal : std::uint8_t {
  // A transition requested with no session loop running.
  kNoSession,
  // A transition requested while a scope that binds globals to stack storage
  // is live; jumping would leave those globals pointing at a dead frame.
  kInsideBarrier,
};

struct SessionState {
  bool active = false;
  int barrier_depth = 0;
};

class SessionHost {
 public:
  virtual ~SessionHost() = default;
  // Drops text-box and notification borrows of the scene heap.
  virtual void InvalidateSceneUi() = 0;
  virtual void ResetGameplayInspection() = 0;
  virtual void ResetNativeSession() = 0;
  // Returns to the session boundary with `destination`; never returns.
  [[noreturn]] virtual void Transfer(SessionDestination destination) = 0;
  // Reports the refused transition and stops; never returns.
  [[noreturn]] virtual void Refuse(SessionRefusal refusal) = 0;
};

// Starts a session; false if one is running or an entry is missing.
[[nodiscard]] bool BeginSession(SessionState& state, SessionHost& host,
                                bool entries_present);
// The entry for the value the session boundary resumed with (0 on entry).
[[nodiscard]] SessionDestination EntryFor(int boundary_value);
void EndSession(SessionState& state, SessionHost& host);

// RAII barrier: a transition requested while one is held is refused.
class SessionTransitionBarrier {
 public:
  explicit SessionTransitionBarrier(SessionState& state);
  ~SessionTransitionBarrier();
  SessionTransitionBarrier(const SessionTransitionBarrier&) = delete;
  SessionTransitionBarrier& operator=(const SessionTransitionBarrier&) = delete;
  SessionTransitionBarrier(SessionTransitionBarrier&&) = delete;
  SessionTransitionBarrier& operator=(SessionTransitionBarrier&&) = delete;

 private:
  SessionState& state_;
};

// SLUS_010.40 vs_overlay_jumpToTitle (overlay.c:20): resets the native save
// session, then returns to the title entry.
[[noreturn]] void JumpToTitle(SessionState& state, SessionHost& host);
// SLUS_010.40 vs_overlay_jumpToBattle (overlay.c:21).
[[noreturn]] void JumpToGameLoop(SessionState& state, SessionHost& host);

class TitleSessionHost {
 public:
  virtual ~TitleSessionHost() = default;
  virtual void InitializeTitleSession() = 0;
  virtual void LoadTitleOverlay() = 0;
  // Runs the title module; returns the game entry mode it chose.
  virtual int RunTitle() = 0;
  virtual void SetGameEntryMode(int mode) = 0;
  virtual void ClearPendingClearSave() = 0;
  virtual void ShowLoadingScreen() = 0;
  virtual void LoadBattleOverlays() = 0;
  virtual int RunLoadedGame() = 0;
};

// SLUS_010.40 vs_main_execTitle (main.c:8344).
int RunTitleAndBattle(TitleSessionHost& host);

class GameHost {
 public:
  virtual ~GameHost() = default;
  virtual void InitializePlatformServices() = 0;
  virtual void RunSessions() = 0;
};

// SLUS_010.40 vs_main_exec (main.c:8361).
void RunGame(GameHost& host);

// SLUS_010.40 vs_overlay_wait (overlay.c:23): the original spins 4096 NOPs
// for the instruction cache after an overlay load; native code needs none.
void DelayAfterOverlayLoad();
// SLUS_010.40 vs_main_wait (main.c:8369): delegates to the overlay wait.
void WaitForOverlayReady();

enum class OverlayProgram : std::uint8_t {
  kBattle,
  kBattleInitialization,
  kTitle,
  kEnding,
};

enum class OverlaySlot : std::uint8_t { kPrimary, kSecondary };

struct OverlayRequest {
  OverlayProgram program;
  OverlaySlot slot;
};

class OverlayLoadHost {
 public:
  virtual ~OverlayLoadHost() = default;
  // Loads and releases one overlay request before returning.
  virtual void Load(OverlayRequest request) = 0;
};

// SLUS_010.40 _loadBattlePrg (main.c:8002), _loadTitlePrg (main.c:8031),
// and _loadEndingPrg (main.c:8050).
void LoadBattleOverlays(OverlayLoadHost& host);
void LoadTitleOverlay(OverlayLoadHost& host);
void LoadEndingOverlay(OverlayLoadHost& host);

struct SoftResetState {
  bool enabled = false;
};

void SetSoftResetEnabled(SoftResetState& state, bool enabled) noexcept;

class TransitionHost {
 public:
  virtual ~TransitionHost() = default;
  virtual void ResetNativeSession() = 0;
  virtual void HideDisplay() = 0;
  virtual void StopControllerMotors() = 0;
  virtual void RefreshControllers() = 0;
  virtual void ShutdownAudio() = 0;
  virtual void WaitForVerticalSync(int count) = 0;
  virtual void StopAllMusic() = 0;
  virtual void StopSceneSoundEffects() = 0;
  virtual void InvalidateSavedMusicId() = 0;
  virtual void ClearMusicBankIds() = 0;
  virtual void ShowLoadingScreen() = 0;
  virtual void SetGameEntryMode(int mode) = 0;
  virtual void SetGameplayActive(bool active) = 0;
  virtual void CaptureAshleySnapshot() = 0;
  virtual void RunStaffRoll() = 0;
  virtual void SetClearSavePending(bool pending) = 0;
  virtual void JumpToTitle() = 0;
  virtual void JumpToGameLoop() = 0;
};

// SLUS_010.40 vs_main_resetGame (main.c:8119),
// vs_main_jumpToBattle (main.c:8138), and
// vs_main_showEndingAndReturnToTitle (main.c:8158).
void ResetToTitle(TransitionHost& host);
void EnterBattle(TransitionHost& host, OverlayLoadHost& overlays);
void RunEndingAndReturnToTitle(TransitionHost& host, OverlayLoadHost& overlays);

class PlatformServicesHost {
 public:
  virtual ~PlatformServicesHost() = default;
  virtual void SelectNtscVideoMode() = 0;
  virtual void HideDisplay() = 0;
  virtual void InitializeMemoryCard() = 0;
  virtual void StartMemoryCard() = 0;
  virtual void InitializeControllers() = 0;
  virtual void StartControllerCommunication() = 0;
  virtual void UnlockAnalogModeSwitch() = 0;
  virtual void ResetVibrationPatterns() = 0;
  virtual void SetReverbEnabled(bool enabled) = 0;
  virtual void InitializeDiscDrive() = 0;
  virtual void InitializeRandomShuffleTable() = 0;
  virtual void StoreSoftResetEnabled(bool enabled) = 0;
  virtual void ClearPendingClearSave() = 0;
};

// SLUS_010.40 _sysInit (main.c:8286).
void InitializePlatformServices(SoftResetState& reset_state,
                                PlatformServicesHost& host);

class TitleSessionInitHost {
 public:
  virtual ~TitleSessionInitHost() = default;
  virtual void ResetNativeSession() = 0;
  virtual void ClearFramebuffer() = 0;
  virtual void ShowLoadingScreen() = 0;
  virtual void EnableReverb() = 0;
  virtual void RegisterVSyncCallback() = 0;
  virtual void ResetGameHeap() = 0;
  virtual void InvalidateAiBindings() = 0;
  virtual void InitializeDiscQueue() = 0;
  virtual void ResetDiscDrive() = 0;
  virtual void InitializeMenuSoundBanks() = 0;
  virtual void ResetVibrationPatterns() = 0;
  virtual void SetGameplayActive(bool active) = 0;
  virtual void UnlockAnalogModeSwitch() = 0;
  virtual void ResetButtonHeldFrames() = 0;
  virtual void ResetPlayTime() = 0;
  virtual void ClearAshleySnapshotPending() = 0;
};

// SLUS_010.40 _sysReinit (main.c:8309).
void InitializeTitleSession(TitleSessionInitHost& host);

// The values func_800FA35C (INITBTL.PRG/18.c:502) starts gameplay with.
inline constexpr std::size_t kGameplayOrderingTableBytes = 0x2088;
inline constexpr std::size_t kGameplayActorSlots = 16;
inline constexpr int kGameplayProjectionDistance = 0x200;
inline constexpr int kGameplayNearClip = 0x40;
inline constexpr int kGameplayFarClip = 0x1000;
inline constexpr std::uint32_t kGameplayTransitionStep = 4;
inline constexpr int kGameplayTransitionSpeed = 8;
inline constexpr int kGameplayTransitionWipeAngle = 0xC00;
inline constexpr int kGameplayCameraPitchDelta = 0x800;
inline constexpr int kGameplayCameraYaw = 0x800;
inline constexpr int kGameplayCameraPitch = -0x180;
inline constexpr int kGameplayCameraDistance = 0x600;
inline constexpr int kGameplayDisplayWidth = 320;
inline constexpr int kGameplayDisplayHeight = 240;
// The room actors' scale on every axis, in 4.12 fixed point.
inline constexpr std::int32_t kGameplayRoomActorScale = 0x71C;

struct GameplayOverlayDefaults {
  std::size_t ordering_table_bytes = kGameplayOrderingTableBytes;
  std::size_t actor_slot_count = kGameplayActorSlots;
  std::uint32_t field_control_mode = 0;
  int tick_speed = 2;
  int projection_distance = kGameplayProjectionDistance;
  int near_clip = kGameplayNearClip;
  int far_clip = kGameplayFarClip;
  int transition_effect = 0;
  std::uint32_t transition_step = kGameplayTransitionStep;
  int transition_speed = kGameplayTransitionSpeed;
  int transition_wipe_angle = kGameplayTransitionWipeAngle;
  int camera_pitch_delta = kGameplayCameraPitchDelta;
  int camera_yaw = kGameplayCameraYaw;
  int camera_pitch = kGameplayCameraPitch;
  int camera_distance = kGameplayCameraDistance;
  int camera_delta_mode = 1;
  int camera_yaw_delta = 0;
  int camera_distance_delta = 0;
  int camera_zoom_velocity = 0;
  int display_width = kGameplayDisplayWidth;
  int display_height = kGameplayDisplayHeight;
  int display_x = 0;
  int display_y = 0;
  int display_mode = 0;
  bool interaction_paused = false;
  bool zone_music_fade_enabled = true;
  bool field_rendering_enabled = true;
  Core::Math::Vec3 room_actor_scale{.x = kGameplayRoomActorScale,
                                    .y = kGameplayRoomActorScale,
                                    .z = kGameplayRoomActorScale};

  friend bool operator==(const GameplayOverlayDefaults&,
                         const GameplayOverlayDefaults&) = default;
};

class GameplayOverlayHost {
 public:
  virtual ~GameplayOverlayHost() = default;
  virtual void ResetAudioSlots() = 0;
  virtual void ResetGameHeap() = 0;
  virtual void InitializeOrderingTables(
      const GameplayOverlayDefaults& defaults) = 0;
  virtual void InitializePaletteAnimation() = 0;
  virtual void InitializeRoomTransitionPrimitives() = 0;
  virtual void InitializeFramebufferEffect() = 0;
  virtual void InitializeScriptOpcodeTable() = 0;
  virtual void InitializeEffectRuntime() = 0;
  virtual void InitializeRoomRuntimeState() = 0;
  virtual void InitializeActorModelResources() = 0;
  virtual void InitializeTexturePages() = 0;
  virtual void ResetOverlayWorkspaceOwner() = 0;
  virtual void LoadSystemResources() = 0;
  virtual void InitializeActorAiHook() = 0;
  virtual void ResetVibrationPatterns() = 0;
  virtual void ApplyLearnedActionUnlocks() = 0;
  virtual void ApplyGameplayDefaults(
      const GameplayOverlayDefaults& defaults) = 0;
  virtual void SetRoomActorScale(const Core::Math::Vec3& scale) = 0;
  virtual void ClearActorSlots(std::size_t slot_count) = 0;
  virtual void SpawnAshleyWithEquipment() = 0;
  virtual void AllocateBattleActionState() = 0;
  virtual void SetFieldRenderingEnabled(bool enabled) = 0;
  virtual void ResetGameplayResetWordTwo() = 0;
  virtual void ConfigureFieldDisplay(
      const GameplayOverlayDefaults& defaults) = 0;
};

// SLUS_010.40 func_800FA35C (INITBTL.PRG/18.c:502).
void InitializeGameplayOverlay(GameplayOverlayHost& host);
}  // namespace VS::Boot
