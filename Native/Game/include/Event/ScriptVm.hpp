#pragma once
// Hand-maintained (tools/port/owned.csv).
// The event script VM (BATTLE.PRG 4A0A8.c:588-2854 and 4246-4633, 573B8.c:67-
// 231, 40564.c:1280 and INITBTL.PRG 12AC.c:69): the control-flow, flag, wait
// and thread opcodes, the thread dispatch loop, event playback from the room
// entry script, a door script or an event file, cutscene skipping and the
// easing curves. A script position is an offset into one of the script
// regions the host lends, never an address; the host checks every jump and
// cursor against its region.
#include <cstddef>
#include <cstdint>
#include <span>

#include "Event/ScriptCommand.hpp"

namespace VS::Event {

// The event script buffer (the room entry script, an event file or a chained
// event copied over it) and the built-in subroutine of special operation 16.
inline constexpr std::size_t kEventScriptBytes = 0x1800;
inline constexpr std::size_t kSubroutineScriptBytes = 132;
// The four script threads, their wait counters and the text box waits the
// skip clears.
inline constexpr int kScriptThreadCount = 4;
// Deterministic work limit per thread/frame; exhaustion refuses the thread.
inline constexpr std::size_t kScriptDispatchBudget = 65'536;
inline constexpr int kLoopCounterCount = 16;
inline constexpr int kCustomEasingCurveCount = 4;
inline constexpr int kScriptControlledActorCount = 17;
inline constexpr int kTextBoxFrameStyleFirst = 9;
inline constexpr int kTextBoxFrameStyleCount = 16;
inline constexpr int kVibrationPatternSlots = 8;
// The script flags an event script owns: 16..31 are cleared for each event.
inline constexpr int kEventLocalFlagFirst = 16;
inline constexpr int kEventLocalFlagEnd = 32;
// Set while a cutscene skip runs; cleared when the skip finishes.
inline constexpr int kSkipActiveFlag = 0xA8;

// Where a script lies.
enum class ScriptRegion : std::uint8_t {
  kNone,
  kEventScript,
  kDoorEvents,
  kSubroutine,
};

// A position in one mounted generation; kNone is a stopped thread.
struct ScriptLocation {
  ScriptRegion region = ScriptRegion::kNone;
  std::uint32_t offset = 0;
  std::uint64_t generation = 0;

  [[nodiscard]] constexpr bool Present() const {
    return region != ScriptRegion::kNone;
  }
  friend constexpr bool operator==(const ScriptLocation&,
                                   const ScriptLocation&) = default;
};

// What the dispatch loop does after an opcode (573B8.c:67): the first five
// are its return codes, the jumps its pointer results.
enum class ScriptFlowKind : std::uint8_t {
  kAdvance = 0,
  kHold = 1,
  kFinish = 2,
  kStop = 3,
  kAdvanceAndYield = 4,
  // `displacement` bytes from the instruction's opcode byte.
  kJumpBy,
  // To `target`.
  kJumpTo,
};

struct ScriptFlow {
  ScriptFlowKind kind = ScriptFlowKind::kAdvance;
  std::int32_t displacement = 0;
  ScriptLocation target{};

  [[nodiscard]] static constexpr ScriptFlow Of(ScriptFlowKind kind) {
    return {.kind = kind};
  }
  [[nodiscard]] static constexpr ScriptFlow JumpBy(std::int32_t displacement) {
    return {.kind = ScriptFlowKind::kJumpBy, .displacement = displacement};
  }
  [[nodiscard]] static constexpr ScriptFlow JumpTo(ScriptLocation target) {
    return {.kind = ScriptFlowKind::kJumpTo, .target = target};
  }
  friend constexpr bool operator==(const ScriptFlow&,
                                   const ScriptFlow&) = default;
};

// One instruction where it runs: its bytes up to the end of its region, its
// position and the thread running it.
struct ScriptInstruction {
  ScriptCommand bytes;
  ScriptLocation location{};
  int thread = 0;
};

enum class PlaybackPhase : std::uint8_t {
  kIdle = 0,
  kRequested = 1,
  kStarting = 2,
  kLoading = 3,
  kRunning = 4,
};

// Where the next event's script comes from.
enum class EventSource : std::uint16_t {
  kEventFile = 0,
  kRoomEntryScript = 1,
  kDoorScript = 2,
};

enum class CutsceneSkip : std::uint8_t {
  kNone = 0,
  kSkippable = 1,
  kSkipping = 2,
};

enum class EventFileStatus : std::uint8_t { kNone, kLoading, kReady, kFailed };

// The buffer an event file is read into.
enum class EventBuffer : std::uint8_t { kScript, kChained };

// A custom easing curve set by opcode 0x5F: the curve family and how many
// times the fraction is squared (towards 1 for a negative power).
struct EasingCurve {
  std::uint8_t family = 0;
  std::int8_t power = 0;

  friend constexpr bool operator==(const EasingCurve&,
                                   const EasingCurve&) = default;
};

// The easing modes of EvaluateInterpolationCurve; 4..7 are the custom curves.
enum class EasingMode : std::int16_t {
  kLinear = 0,
  kEaseOut = 1,
  kEaseInOut = 2,
  kEaseIn = 3,
  kCustomFirst = 4,
};

// A VRAM rectangle copy queued on the overlay ordering table.
struct TextureMove {
  std::int16_t x = 0;
  std::int16_t y = 0;
  std::int16_t width = 0;
  std::int16_t height = 0;
  std::int16_t destination_x = 0;
  std::int16_t destination_y = 0;

  friend constexpr bool operator==(const TextureMove&,
                                   const TextureMove&) = default;
};

// The operations of opcode 0xB7 (4A0A8.c:2651) the host carries out with
// the operation's argument byte.
enum class SpecialOperation : std::uint8_t {
  kQueueAshleyModel = 0,
  kRunRewardSequence = 1,
  kForceSkip = 2,
  kUnlockAction = 3,
  kCountDispositionActors = 4,
  kSetEventActorActivity = 5,
  kFreeChainedEvent = 6,
  kSetFieldCameraOverride = 7,
  kPauseSceneTimer = 8,
  kStartCameraOrbitReturn = 9,
  kSetCameraPitchPreset = 10,
  kScaleCameraDuration = 11,
  kFreezeMovingPlatforms = 12,
  kClearGeometryOcclusion = 13,
  kPauseMovingPlatforms = 14,
  kSetVignette = 15,
  kCallSubroutine = 16,
  kCancelRoomVisit = 17,
};

enum class ScriptVmRefusal : std::uint8_t {
  // An instruction shorter than the operands its opcode reads.
  kShortInstruction,
  // A loop counter past the sixteen.
  kLoopCounter,
  // A script thread past the four.
  kScriptThread,
  // A scene timer mode, timer scope or pitch preset past its table.
  kTimerMode,
  kTimerScope,
  kPitchPreset,
  // An easing mode below 0 or a custom curve past the four.
  kEasingMode,
  // A jump, call or thread start outside its script region.
  kJumpOutsideScript,
  // A thread run with no cursor.
  kMissingCursor,
  // A cursor at or past the end of its script, or an instruction running
  // past it.
  kCursorOutsideScript,
  // An opcode the size table gives no length, which would run forever.
  kZeroLengthOpcode,
  // A skip scan that reached the end of its script.
  kUnboundedSkipScan,
  // An event script header or table outside its script.
  kScriptHeader,
  // A door script outside the door event section.
  kDoorScript,
  // An event file wait with no event file queued.
  kMissingEventFile,
  // The location's source was replaced, even if its address/extent is
  // unchanged.
  kStaleScriptRegion,
  kEventBufferAllocation,
  kEventFileQueue,
  kEventFileRead,
  kEventFileBusy,
  kChainedEventAdoption,
  kEventSource,
  kDispatchBudget,
  kScriptRegionExtent,
};

// The event VM state, the script regions and the systems the opcodes and
// the event playback read and change.
class ScriptVmHost {
 public:
  ScriptVmHost() = default;
  ScriptVmHost(const ScriptVmHost&) = delete;
  ScriptVmHost& operator=(const ScriptVmHost&) = delete;
  ScriptVmHost(ScriptVmHost&&) = delete;
  ScriptVmHost& operator=(ScriptVmHost&&) = delete;
  virtual ~ScriptVmHost() = default;

  virtual void Refuse(ScriptVmRefusal refusal, int value) = 0;
  virtual void NoteInvalidOpcode(const ScriptInstruction& instruction) = 0;
  virtual void NoteUndefinedSpecialOperation(int operation) = 0;
  virtual void NoteCutsceneSkip(int thread) = 0;

  // The script regions (empty when absent), the opcode sizes and the
  // opcode table, which runs the opcode at `location` and returns its flow
  // with any jump resolved to a location (a stop for a jump outside every
  // region).
  [[nodiscard]] virtual std::span<const std::uint8_t> Region(
      ScriptRegion region) const = 0;
  // Versions the mounted VM source, not a newly loaded, unmounted room section.
  // Keep returned region bytes alive through each operation and its callbacks.
  [[nodiscard]] virtual std::uint64_t RegionGeneration(
      ScriptRegion region) const = 0;
  [[nodiscard]] virtual std::span<const std::uint8_t> OpcodeSizes() const = 0;
  [[nodiscard]] virtual ScriptFlow RunOpcode(ScriptLocation location,
                                             int thread) = 0;

  // BATTLE.PRG's tables: the skip's palette reset command and sound restore
  // marker, the scene timer modes and scopes and the camera pitch presets.
  [[nodiscard]] virtual std::span<const std::uint8_t> PaletteResetCommand()
      const = 0;
  [[nodiscard]] virtual std::span<const std::uint8_t> SoundRestoreMarker()
      const = 0;
  [[nodiscard]] virtual std::span<const std::int16_t> SceneTimerModes()
      const = 0;
  [[nodiscard]] virtual std::span<const std::int16_t> SceneTimerScopes()
      const = 0;
  [[nodiscard]] virtual std::span<const std::int16_t> CameraPitchPresets()
      const = 0;

  // Event playback.
  [[nodiscard]] virtual PlaybackPhase Phase() const = 0;
  virtual void SetPhase(PlaybackPhase phase) = 0;
  [[nodiscard]] virtual bool StartConfirmed() const = 0;
  virtual void SetStartConfirmed(bool confirmed) = 0;
  [[nodiscard]] virtual EventSource Source() const = 0;
  virtual void SetSource(EventSource source) = 0;
  [[nodiscard]] virtual std::int16_t EventId() const = 0;
  virtual void SetEventId(std::int16_t event_id) = 0;
  [[nodiscard]] virtual bool MainThreadFinished() const = 0;
  virtual void SetMainThreadFinished(bool finished) = 0;
  [[nodiscard]] virtual bool ChainedEventLoaded() const = 0;
  virtual void SetChainedEventLoaded(bool loaded) = 0;
  [[nodiscard]] virtual std::int16_t RoomChangeState() const = 0;
  virtual void SetRoomChangeState(std::int16_t state) = 0;
  [[nodiscard]] virtual bool FieldCameraRestoreScripted() const = 0;
  virtual void SetFieldCameraRestoreScripted(bool scripted) = 0;
  [[nodiscard]] virtual bool ScanlineOverlayEnabled() const = 0;
  virtual void SetScanlineOverlayEnabled(bool enabled) = 0;
  virtual void SetScriptBackdropEnabled(bool enabled) = 0;
  [[nodiscard]] virtual bool MenuTransitionActive() const = 0;
  virtual void SetMenuTransitionActive(bool active) = 0;
  virtual void SetAmbientSoundSuspended(bool suspended) = 0;

  // Threads, loops and the cached flag.
  [[nodiscard]] virtual ScriptLocation ThreadCursor(int thread) const = 0;
  virtual void SetThreadCursor(int thread, ScriptLocation cursor) = 0;
  [[nodiscard]] virtual std::int16_t WaitFrames(int thread) const = 0;
  virtual void SetWaitFrames(int thread, std::int16_t frames) = 0;
  [[nodiscard]] virtual ScriptLocation SubroutineReturn() const = 0;
  virtual void SetSubroutineReturn(ScriptLocation location) = 0;
  [[nodiscard]] virtual std::uint8_t LoopCounter(int counter) const = 0;
  virtual void SetLoopCounter(int counter, std::uint8_t value) = 0;
  // Signed, as the original's char was: a value past 127 never matches an
  // operand byte.
  [[nodiscard]] virtual std::int8_t CachedFlag() const = 0;
  virtual void SetCachedFlag(std::int8_t value) = 0;

  // Cutscene skipping.
  [[nodiscard]] virtual CutsceneSkip SkipState() const = 0;
  virtual void SetSkipState(CutsceneSkip state) = 0;
  [[nodiscard]] virtual bool SkipForced() const = 0;
  virtual void SetSkipForced(bool forced) = 0;
  [[nodiscard]] virtual bool StopsMusicOnSkip() const = 0;
  virtual void SetStopsMusicOnSkip(bool stops) = 0;

  // The event script's data and the per-event state of the other opcodes.
  virtual void SetDialogStrings(ScriptLocation strings) = 0;
  virtual void SetTextBoxFrameStyle(int style, ScriptLocation frame) = 0;
  virtual void SetVibrationPattern(int pattern, ScriptLocation data) = 0;
  [[nodiscard]] virtual EasingCurve CustomEasingCurve(int curve) const = 0;
  virtual void SetCustomEasingCurve(int curve, EasingCurve value) = 0;
  virtual void SetTextBoxAwaitingClose(int text_box, bool awaiting) = 0;
  virtual void SetWeaponTransitionPending(bool pending) = 0;
  virtual void SetTextSelectionActive(bool active) = 0;
  virtual void SetEffectWaitPending(bool pending) = 0;
  // Ends the weather tween and the first three image tweens.
  virtual void CancelImageTweens() = 0;
  virtual void CaptureEventMusicSlot() = 0;
  [[nodiscard]] virtual bool ScriptControlledActor(int actor) const = 0;
  virtual void ClearScriptControlledActors() = 0;

  // Event files and the script buffers.
  // Select the current room's door source only when starting a new door
  // event. Room replacement must not rebind an already executing source.
  // Failure preserves the previous mount.
  [[nodiscard]] virtual bool BindRoomDoorSource() = 0;
  [[nodiscard]] virtual EventFileStatus EventFile() const = 0;
  // False leaves no request or outstanding write to the destination.
  [[nodiscard]] virtual bool QueueEventFileLoad(int event_id,
                                                EventBuffer destination) = 0;
  // Release only terminal requests; keep destinations alive while loading.
  virtual void ReleaseEventFile() = 0;
  // Synchronously prevent all further writes before releasing request
  // ownership. Merely freeing a queue slot or clearing its pointer is not
  // cancellation.
  virtual void CancelEventFile() = 0;
  virtual void ClearEventFile() = 0;
  // Allocation/adoption failure leaves the existing owners unchanged.
  [[nodiscard]] virtual bool AllocateChainedEventBuffer() = 0;
  [[nodiscard]] virtual bool AdoptChainedEvent() = 0;
  virtual void DiscardChainedEventBuffer() = 0;
  [[nodiscard]] virtual bool AllocateEventBuffers() = 0;

  // Flags and the controller.
  [[nodiscard]] virtual std::uint8_t Flag(int flag) const = 0;
  virtual void SetFlag(int flag, std::uint8_t value) = 0;
  [[nodiscard]] virtual std::uint32_t HeldButtons() const = 0;
  virtual void ClearHeldButtons() = 0;
  [[nodiscard]] virtual bool StartPressed() const = 0;
  [[nodiscard]] virtual std::uint8_t PressedButtonsLowByte() const = 0;
  [[nodiscard]] virtual int PreviousButtons() const = 0;

  // The systems the opcodes drive.
  [[nodiscard]] virtual int CountInventoryItem(int item) = 0;
  virtual void StartSceneTimer(int mode, int scope, int duration) = 0;
  virtual void ResetSceneTimer() = 0;
  virtual void SetTickSpeed(int speed) = 0;
  [[nodiscard]] virtual bool DismissTextBox(int text_box) = 0;
  [[nodiscard]] virtual bool UpdateRewardSequence() = 0;
  virtual void BeginEventRewardResults() = 0;
  virtual void RunEndingAndReturnToTitle() = 0;
  [[nodiscard]] virtual bool IsActionUnlocked(int action) = 0;
  virtual void UnlockAction(int action) = 0;
  [[nodiscard]] virtual int CountActorsByDisposition(int disposition) = 0;
  [[nodiscard]] virtual int FindActorByModelId(int model_id) = 0;
  // Operations 0, 5-15 and 17 with their argument; 10 receives the pitch.
  virtual void ApplySpecialOperation(SpecialOperation operation,
                                     int argument) = 0;

  // The event camera and the frame's presentation.
  virtual void InitializeEventCamera() = 0;
  virtual void FreeEventCamera() = 0;
  virtual void UpdateEventCameraOrigin() = 0;
  virtual void RestoreEventFieldCamera() = 0;
  virtual void UpdateEventCameraTracks() = 0;
  virtual void UpdateEventScreenTransforms() = 0;
  virtual void ApplyEventCameraState() = 0;
  [[nodiscard]] virtual std::uint8_t EventCameraCompletionMask() = 0;
  virtual void UpdateImageTweens() = 0;
  virtual void UpdateWeatherTween() = 0;
  virtual void DrawScanlineOverlay() = 0;
  virtual void TransformPalettes(ScriptCommand command) = 0;
  virtual void MoveTextureRectangle(const TextureMove& move) = 0;
  virtual void BeginSkipFadeOut() = 0;
  virtual void DisableDisplay() = 0;

  // Actors and sound.
  virtual void ReturnAllActorsToIdle() = 0;
  virtual void ResetActorMovement(int actor) = 0;
  [[nodiscard]] virtual bool ActorPresent(int actor) = 0;
  virtual void PlayActorIdleAnimation(int actor) = 0;
  virtual void StopEventMusic() = 0;
  virtual void StopAllSfx() = 0;
  virtual void StopScriptVibration() = 0;
  virtual void RestartRoomEffectSound() = 0;
};

// decomp: BATTLE.PRG func_800AACDC (src/BATTLE/BATTLE.PRG/40564.c:1280)
// Copies the 18x36 texture block at (470, 384) back to (16, 400).
void RestoreScriptTextureRectangle(ScriptVmHost& host);

// The opcode handlers. Each reads its operands from the instruction, which
// must hold them; a shorter one is refused without reading its operands.
// decomp: BATTLE.PRG vs_battle_invalidOpcode
// (src/BATTLE/BATTLE.PRG/4A0A8.c:588)
ScriptFlow ScriptReportInvalidOpcode(const ScriptInstruction& instruction,
                                     ScriptVmHost& host);
// decomp: BATTLE.PRG func_800B66E4 (src/BATTLE/BATTLE.PRG/4A0A8.c:597)
ScriptFlow ScriptNoOperation(const ScriptInstruction& instruction,
                             ScriptVmHost& host);
// decomp: BATTLE.PRG func_800B66EC (src/BATTLE/BATTLE.PRG/4A0A8.c:599)
ScriptFlow ScriptYieldFrame(const ScriptInstruction& instruction,
                            ScriptVmHost& host);
// decomp: BATTLE.PRG func_800B66F4 (src/BATTLE/BATTLE.PRG/4A0A8.c:601)
ScriptFlow ScriptWaitForEventFile(const ScriptInstruction& instruction,
                                  ScriptVmHost& host);
// decomp: BATTLE.PRG func_800B6744 (src/BATTLE/BATTLE.PRG/4A0A8.c:611)
ScriptFlow ScriptBeginSkippableCutscene(const ScriptInstruction& instruction,
                                        ScriptVmHost& host);
// decomp: BATTLE.PRG func_800B6778 (src/BATTLE/BATTLE.PRG/4A0A8.c:618)
// Resets the palettes and texture block a skip left behind, then scans from
// `scan` for the sound restore marker before the next 0x0F: with the marker
// the room sound stays as the script left it, without it the room's effect
// sound restarts.
void FinishSkippedCutscene(ScriptLocation scan, ScriptVmHost& host);
// decomp: BATTLE.PRG func_800B6868 (src/BATTLE/BATTLE.PRG/4A0A8.c:642)
ScriptFlow ScriptEndCutsceneAndJump(const ScriptInstruction& instruction,
                                    ScriptVmHost& host);
// decomp: BATTLE.PRG func_800B68C4 (src/BATTLE/BATTLE.PRG/4A0A8.c:653)
ScriptFlow ScriptEndCutscene(const ScriptInstruction& instruction,
                             ScriptVmHost& host);
// decomp: BATTLE.PRG vs_battle_script_buttonPressed
// (src/BATTLE/BATTLE.PRG/4A0A8.c:843)
ScriptFlow ScriptWaitForButtonAndYield(const ScriptInstruction& instruction,
                                       ScriptVmHost& host);
// decomp: BATTLE.PRG vs_battle_script_jumpFwdIfFlag
// (src/BATTLE/BATTLE.PRG/4A0A8.c:1543)
ScriptFlow ScriptBranchOnFlagImmediate(const ScriptInstruction& instruction,
                                       ScriptVmHost& host);
// decomp: BATTLE.PRG vs_battle_script_jumpFwdIfFlags
// (src/BATTLE/BATTLE.PRG/4A0A8.c:1589)
ScriptFlow ScriptBranchOnFlagPair(const ScriptInstruction& instruction,
                                  ScriptVmHost& host);
// decomp: BATTLE.PRG vs_battle_script_jumpFwd
// (src/BATTLE/BATTLE.PRG/4A0A8.c:1630) Opcodes 0x42, 0x45, 0x4B and 0x4C.
ScriptFlow ScriptJumpForward(const ScriptInstruction& instruction,
                             ScriptVmHost& host);
// decomp: BATTLE.PRG vs_battle_script_setFlagImmContext
// (src/BATTLE/BATTLE.PRG/4A0A8.c:1640)
ScriptFlow ScriptCacheFlagValue(const ScriptInstruction& instruction,
                                ScriptVmHost& host);
// decomp: BATTLE.PRG vs_battle_script_jumpFwdIfFlagContext
// (src/BATTLE/BATTLE.PRG/4A0A8.c:1646)
ScriptFlow ScriptBranchOnCachedFlag(const ScriptInstruction& instruction,
                                    ScriptVmHost& host);
// decomp: BATTLE.PRG func_800B8C44 (src/BATTLE/BATTLE.PRG/4A0A8.c:1654)
// Returns to the call of special operation 16; with none saved it advances.
ScriptFlow ScriptReturnToSavedAddress(const ScriptInstruction& instruction,
                                      ScriptVmHost& host);
// decomp: BATTLE.PRG func_800B8C54 (src/BATTLE/BATTLE.PRG/4A0A8.c:1656)
ScriptFlow ScriptSetLoopCounter(const ScriptInstruction& instruction,
                                ScriptVmHost& host);
// decomp: BATTLE.PRG vs_battle_script_jumpBackIf
// (src/BATTLE/BATTLE.PRG/4A0A8.c:1664)
ScriptFlow ScriptLoopBackward(const ScriptInstruction& instruction,
                              ScriptVmHost& host);
// decomp: BATTLE.PRG vs_battle_script_setFlagImm
// (src/BATTLE/BATTLE.PRG/4A0A8.c:1685) Division and remainder by zero give the
// R3000's results.
ScriptFlow ScriptApplyImmediateFlagOperation(
    const ScriptInstruction& instruction, ScriptVmHost& host);
// decomp: BATTLE.PRG vs_battle_script_setFlag
// (src/BATTLE/BATTLE.PRG/4A0A8.c:1720)
ScriptFlow ScriptApplyFlagPairOperation(const ScriptInstruction& instruction,
                                        ScriptVmHost& host);
// decomp: BATTLE.PRG vs_battle_script_setFlagIfItemInInventory
// (src/BATTLE/BATTLE.PRG/4A0A8.c:1773)
ScriptFlow ScriptStoreInventoryItemPresence(
    const ScriptInstruction& instruction, ScriptVmHost& host);
// decomp: BATTLE.PRG vs_battle_script_setFlagIfButton
// (src/BATTLE/BATTLE.PRG/4A0A8.c:1798)
ScriptFlow ScriptStoreButtonState(const ScriptInstruction& instruction,
                                  ScriptVmHost& host);
// decomp: BATTLE.PRG func_800B94D8 (src/BATTLE/BATTLE.PRG/4A0A8.c:1886)
ScriptFlow ScriptConfigureTimer(const ScriptInstruction& instruction,
                                ScriptVmHost& host);
// decomp: BATTLE.PRG func_800B96B0 (src/BATTLE/BATTLE.PRG/4A0A8.c:1934)
ScriptFlow ScriptWaitForButtonOrTimeout(const ScriptInstruction& instruction,
                                        ScriptVmHost& host);
// decomp: BATTLE.PRG func_800B9724 (src/BATTLE/BATTLE.PRG/4A0A8.c:1948)
ScriptFlow ScriptConfigureEasingCurve(const ScriptInstruction& instruction,
                                      ScriptVmHost& host);
// decomp: BATTLE.PRG vs_battle_script_specialOp
// (src/BATTLE/BATTLE.PRG/4A0A8.c:2651)
ScriptFlow CommandSpecialOperation(const ScriptInstruction& instruction,
                                   ScriptVmHost& host);
// decomp: BATTLE.PRG func_800BB474 (src/BATTLE/BATTLE.PRG/4A0A8.c:2799)
ScriptFlow CommandWaitFrames(const ScriptInstruction& instruction,
                             ScriptVmHost& host);
// decomp: BATTLE.PRG func_800BB4CC (src/BATTLE/BATTLE.PRG/4A0A8.c:2815)
ScriptFlow CommandSetTickSpeed(const ScriptInstruction& instruction,
                               ScriptVmHost& host);
// decomp: BATTLE.PRG func_800BB504 (src/BATTLE/BATTLE.PRG/4A0A8.c:2827)
ScriptFlow CommandWaitScriptContext(const ScriptInstruction& instruction,
                                    ScriptVmHost& host);
// decomp: BATTLE.PRG func_800BB554 (src/BATTLE/BATTLE.PRG/4A0A8.c:2837)
ScriptFlow CommandStartScriptContext(const ScriptInstruction& instruction,
                                     ScriptVmHost& host);
// decomp: BATTLE.PRG func_800BB5A4 (src/BATTLE/BATTLE.PRG/4A0A8.c:2843)
ScriptFlow CommandStopScriptContext(const ScriptInstruction& instruction,
                                    ScriptVmHost& host);
// decomp: BATTLE.PRG func_800BB5AC (src/BATTLE/BATTLE.PRG/4A0A8.c:2845)
ScriptFlow CommandLoadNextEvent(const ScriptInstruction& instruction,
                                ScriptVmHost& host);
// decomp: BATTLE.PRG func_800BB604 (src/BATTLE/BATTLE.PRG/4A0A8.c:2854)
ScriptFlow CommandFinishEvent(const ScriptInstruction& instruction,
                              ScriptVmHost& host);

// decomp: BATTLE.PRG func_800BEB00 (src/BATTLE/BATTLE.PRG/4A0A8.c:4246)
[[nodiscard]] PlaybackPhase EventPlaybackState(const ScriptVmHost& host);
// decomp: BATTLE.PRG func_800BEB10 (src/BATTLE/BATTLE.PRG/4A0A8.c:4248)
// The source in the top nibble over the event id.
[[nodiscard]] std::int16_t PendingEventSelector(const ScriptVmHost& host);
// decomp: BATTLE.PRG func_800BEC30 (src/BATTLE/BATTLE.PRG/4A0A8.c:4287)
// Confirms a requested event; always 1.
int RequestEventStart(ScriptVmHost& host);
// decomp: BATTLE.PRG func_800BF5F0 (src/BATTLE/BATTLE.PRG/4A0A8.c:4588)
// Reads the script header at `data` (the dialog strings, frame styles and
// vibration patterns) and resets the threads and per-event state. A header
// or table outside its region refuses the complete header, clearing borrowed
// views and every cursor. True only when the script can start.
[[nodiscard]] bool InitializeEventScriptData(ScriptLocation data,
                                             ScriptVmHost& host);
// decomp: BATTLE.PRG func_800BF850 (src/BATTLE/BATTLE.PRG/4A0A8.c:4633)
// Advances event playback by a frame: 0 while idle or awaiting the start,
// else 2. A failed startup reports a refusal and returns to idle; its frame
// still returns 2, not a success or failure code.
int UpdateEventPlayback(ScriptVmHost& host);
// decomp: BATTLE.PRG func_800BFBB8 (src/BATTLE/BATTLE.PRG/573B8.c:67)
// Runs `thread` from its cursor until an opcode holds, yields, stops or
// finishes it, starting a requested skip on thread 0 first; true when the
// script finished (or the thread had no cursor).
bool ExecuteScriptThread(int thread, ScriptVmHost& host);
// decomp: BATTLE.PRG func_800BFD9C (src/BATTLE/BATTLE.PRG/573B8.c:133)
void ReleaseScriptActorControl(ScriptVmHost& host);
// decomp: BATTLE.PRG func_800BFE34 (src/BATTLE/BATTLE.PRG/573B8.c:154)
[[nodiscard]] int OpcodeSize(std::uint8_t opcode, const ScriptVmHost& host);
// decomp: BATTLE.PRG func_800BFE50 (src/BATTLE/BATTLE.PRG/573B8.c:156)
// A model id (top nibble 0) through the actor table, a slot (1) or the
// reference itself.
[[nodiscard]] int ResolveScriptActorReference(std::uint16_t reference,
                                              ScriptVmHost& host);
// decomp: BATTLE.PRG func_800BFEBC (src/BATTLE/BATTLE.PRG/573B8.c:168)
// The Q12 fraction `frame` of `duration` through the easing `mode`. A zero
// duration divides as the R3000 did; an unknown mode is refused as linear.
[[nodiscard]] int EvaluateInterpolationCurve(std::int16_t mode,
                                             std::int16_t frame,
                                             std::int16_t duration,
                                             ScriptVmHost& host);
// decomp: BATTLE.PRG func_800C00E8 (src/BATTLE/BATTLE.PRG/573B8.c:223)
// Reads the current event id's file into `destination`.
[[nodiscard]] bool QueueEventScriptLoad(EventBuffer destination,
                                        ScriptVmHost& host);
// decomp: BATTLE.PRG func_800C0150 (src/BATTLE/BATTLE.PRG/573B8.c:231)
void ResetCutsceneActorsToIdle(ScriptVmHost& host);
// decomp: INITBTL.PRG _initScriptFunctionTable
// (src/BATTLE/INITBTL.PRG/12AC.c:69)
[[nodiscard]] bool InitializeScriptOpcodeTable(ScriptVmHost& host);

}  // namespace VS::Event
