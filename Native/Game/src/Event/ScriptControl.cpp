// Hand-maintained (tools/port/owned.csv).
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

#include "Core/Math/ScalarMath.hpp"
#include "Event/ScriptDialogCommands.hpp"
#include "Event/ScriptVm.hpp"

namespace VS::Event {
namespace {

// Operand offsets after the opcode byte.
constexpr std::size_t kFirstOperand = 1;
constexpr std::size_t kSecondOperand = 2;
constexpr std::size_t kThirdOperand = 3;
constexpr std::size_t kFourthOperand = 4;
constexpr std::size_t kFifthOperand = 5;
constexpr std::size_t kSixthOperand = 6;

// The bytes each handler reads, its opcode byte included.
constexpr std::size_t kOneOperandBytes = 2;
constexpr std::size_t kTwoOperandBytes = 3;
constexpr std::size_t kThreeOperandBytes = 4;
constexpr std::size_t kFourOperandBytes = 5;
constexpr std::size_t kFiveOperandBytes = 6;
constexpr std::size_t kSixOperandBytes = 7;

// Flag ids packed in the branch and arithmetic operands: ten bits split over
// the operation byte's low nibble and the next byte's top six bits, and a
// second ten bits over the next byte's low two bits and the byte after it.
constexpr unsigned kOperationMask = 0xF0;
constexpr unsigned kFlagHighMask = 0x0F;
constexpr unsigned kFlagHighShift = 6;
constexpr unsigned kFlagLowShift = 2;
constexpr unsigned kSecondFlagHighMask = 0x03;
constexpr unsigned kSecondFlagHighShift = 8;

// Branch tests (the operation nibble): the flag against the operand.
enum class FlagTest : std::uint8_t {
  kAtMost = 0x00,
  kAtLeast = 0x10,
  kEqual = 0x20,
  kNotEqual = 0x30,
  kBelow = 0x40,
  kAbove = 0x50,
  kAnyBits = 0x60,
};

// Flag arithmetic (the operation nibble).
enum class FlagOperation : std::uint8_t {
  kAdd = 0x00,
  kSubtract = 0x10,
  kMultiply = 0x20,
  kDivide = 0x30,
  kRemainder = 0x40,
  kAnd = 0x50,
  kOr = 0x60,
  kAssign = 0x70,
  kAbsoluteDifference = 0x80,
  kSwap = 0x90,
};

// A loop counter or loop-back operand of 0xFF is no counter.
constexpr std::uint8_t kNoLoopCounter = 0xFF;

// The button-state flag operand: the flag id in the low ten bits and, in the
// top bit, whether the pressed byte rather than the previous buttons is
// stored.
constexpr unsigned kButtonFlagMask = 0x3FF;
constexpr unsigned kPressedButtonsSource = 0x8000;

// The scene timer duration bytes, most significant first, above a zero low
// byte.
constexpr unsigned kTimerHighShift = 24;
constexpr unsigned kTimerMiddleShift = 16;
constexpr unsigned kTimerLowShift = 8;

// The easing curve operand: curve index, family and a four-bit signed power.
constexpr unsigned kCurveIndexShift = 6;
constexpr unsigned kCurveFamilyShift = 4;
constexpr unsigned kCurveFamilyMask = 0x3;
constexpr unsigned kCurvePowerMask = 0x7;
constexpr unsigned kCurvePowerSign = 0x8;
constexpr unsigned kCurvePowerExtension = 0xF8;

// Special operation results and arguments.
constexpr int kRewardSequenceFlag = 0xB6;
constexpr std::uint8_t kRewardSequenceRunning = 3;
constexpr int kSpecialResultFlag = 0xB1;
constexpr int kEnemyDisposition = 4;
constexpr std::uint32_t kSubroutineCallBytes = 6;

// The wait-context operand: the thread in the low two bits and, in the top
// bit, a release of that thread instead of a wait.
constexpr unsigned kContextThreadMask = 0x3;
constexpr unsigned kReleaseContext = 0x80;
// A started context resumes after its start instruction's four bytes.
constexpr std::uint32_t kContextStartBytes = 4;

// The tick speed once an event finishes.
constexpr int kFieldTickSpeed = 2;

// The skip: the end-of-cutscene opcode its sound scan stops at, the text
// box waits it clears and the palette reset passes it runs.
constexpr std::uint8_t kEndCutsceneOpcode = 0x0F;
constexpr std::uint8_t kSkipActiveValue = 1;
constexpr int kSkippedTextBoxes = 4;
constexpr std::uint8_t kPalettePassShift = 4;
constexpr std::uint8_t kPaletteResetMode = 8;
constexpr int kPalettePasses = 3;
constexpr std::size_t kPaletteCommandBytes = 8;

// The 18x36 block at (470, 384) the skip copies back to (16, 400).
constexpr TextureMove kSkipTextureMove{470, 384, 18, 36, 16, 400};

// The event script header: offsets of the dialog strings, the frame style
// table and the vibration pattern table, then the main thread's script.
constexpr std::uint32_t kDialogStringsOffset = 2;
constexpr std::uint32_t kFrameStyleTableOffset = 4;
constexpr std::uint32_t kVibrationTableOffset = 6;
constexpr std::uint32_t kEventScriptHeaderBytes = 16;
constexpr std::uint32_t kHalfBytes = 2;
// Frame style 9 is the style table's entry 1.
constexpr int kFrameStyleEntryBias = 8;

// Actor references: the top nibble picks a model id (low twelve bits) or a
// slot (low five bits).
constexpr unsigned kReferenceKindShift = 12;
constexpr unsigned kModelReference = 0;
constexpr unsigned kSlotReference = 1;
constexpr unsigned kModelIdMask = 0xFFF;
constexpr unsigned kSlotMask = 0x1F;

// Q12 fractions and angles (4096 per turn).
constexpr std::int32_t kOne = 4096;
constexpr std::int32_t kQuadrants = 4;
constexpr std::int32_t kQuarterTurn = kOne / kQuadrants;
constexpr std::int32_t kHalfTurn = kOne / 2;
constexpr unsigned kFractionShift = 12;
enum class CurveFamily : std::uint8_t {
  kLinear = 0,
  kEaseOut = 1,
  kEaseInOut = 2,
  kEaseIn = 3,
};

[[nodiscard]] bool Fits(const ScriptInstruction& instruction,
                        std::size_t length, ScriptVmHost& host) {
  if (instruction.bytes.size() >= length) {
    return true;
  }
  host.Refuse(ScriptVmRefusal::kShortInstruction,
              static_cast<int>(instruction.bytes.size()));
  return false;
}

[[nodiscard]] bool ValidThread(int thread, ScriptVmHost& host) {
  if (thread >= 0 && thread < kScriptThreadCount) {
    return true;
  }
  host.Refuse(ScriptVmRefusal::kScriptThread, thread);
  return false;
}

[[nodiscard]] constexpr ScriptFlow Advance() {
  return ScriptFlow::Of(ScriptFlowKind::kAdvance);
}
[[nodiscard]] constexpr ScriptFlow Hold() {
  return ScriptFlow::Of(ScriptFlowKind::kHold);
}
[[nodiscard]] constexpr ScriptFlow Yield() {
  return ScriptFlow::Of(ScriptFlowKind::kAdvanceAndYield);
}

[[nodiscard]] ScriptCommand Bytes(const ScriptInstruction& instruction) {
  return instruction.bytes;
}

// The first flag of a branch or arithmetic instruction.
[[nodiscard]] int FirstFlagId(ScriptCommand command) {
  return static_cast<int>(
      (OperandByte(command, kSecondOperand) >> kFlagLowShift) +
      ((OperandByte(command, kFirstOperand) & kFlagHighMask)
       << kFlagHighShift));
}

// The second flag of a flag-pair instruction.
[[nodiscard]] int SecondFlagId(ScriptCommand command) {
  return static_cast<int>(
      OperandByte(command, kThirdOperand) +
      ((OperandByte(command, kSecondOperand) & kSecondFlagHighMask)
       << kSecondFlagHighShift));
}

[[nodiscard]] std::uint8_t OperationOf(ScriptCommand command) {
  return static_cast<std::uint8_t>(OperandByte(command, kFirstOperand) &
                                   kOperationMask);
}

[[nodiscard]] bool TestHolds(std::uint8_t test, int left, int right,
                             bool bit_test_allowed) {
  switch (static_cast<FlagTest>(test)) {
    case FlagTest::kAtMost:
      return left <= right;
    case FlagTest::kAtLeast:
      return left >= right;
    case FlagTest::kEqual:
      return left == right;
    case FlagTest::kNotEqual:
      return left != right;
    case FlagTest::kBelow:
      return left < right;
    case FlagTest::kAbove:
      return left > right;
    case FlagTest::kAnyBits:
      return bit_test_allowed && (left & right) != 0;
  }
  return false;
}

// The R3000 DIV remainder: a zero divisor leaves the dividend.
[[nodiscard]] constexpr std::int32_t R3000Remainder(std::int32_t dividend,
                                                    std::int32_t divisor) {
  if (divisor == 0) {
    return dividend;
  }
  if (divisor == -1) {
    return 0;
  }
  return dividend % divisor;
}

// The common arithmetic of the immediate and flag-pair operations; false for
// an operation that writes nothing.
[[nodiscard]] bool Arithmetic(std::uint8_t operation, std::int32_t current,
                              std::int32_t operand, std::int32_t& result) {
  switch (static_cast<FlagOperation>(operation)) {
    case FlagOperation::kAdd:
      result = Core::Math::WrappingAdd(current, operand);
      return true;
    case FlagOperation::kSubtract:
      result = Core::Math::WrappingSubtract(current, operand);
      return true;
    case FlagOperation::kMultiply:
      result = Core::Math::WrappingMultiply(current, operand);
      return true;
    case FlagOperation::kDivide:
      result = Core::Math::R3000Quotient(current, operand);
      return true;
    case FlagOperation::kRemainder:
      result = R3000Remainder(current, operand);
      return true;
    case FlagOperation::kAnd:
      result = current & operand;
      return true;
    case FlagOperation::kOr:
      result = current | operand;
      return true;
    case FlagOperation::kAssign:
      result = operand;
      return true;
    default:
      return false;
  }
}

[[nodiscard]] std::uint16_t ReadHalf(std::span<const std::uint8_t> region,
                                     std::uint32_t offset) {
  return static_cast<std::uint16_t>(OperandHalf(region, offset));
}

[[nodiscard]] bool HalfFits(std::span<const std::uint8_t> region,
                            std::uint32_t offset) {
  return offset <= region.size() && region.size() - offset >= kHalfBytes;
}

[[nodiscard]] bool IsCurrent(ScriptLocation location, ScriptVmHost& host) {
  if (location.generation == host.RegionGeneration(location.region)) {
    return true;
  }
  host.Refuse(ScriptVmRefusal::kStaleScriptRegion,
              static_cast<int>(location.region));
  return false;
}

[[nodiscard]] ScriptLocation StartOf(ScriptRegion region,
                                     const ScriptVmHost& host) {
  return {.region = region, .generation = host.RegionGeneration(region)};
}

[[nodiscard]] ScriptLocation After(ScriptLocation source, std::uint32_t bytes,
                                   ScriptVmHost& host) {
  if (bytes > std::numeric_limits<std::uint32_t>::max() - source.offset) {
    host.Refuse(ScriptVmRefusal::kJumpOutsideScript, static_cast<int>(bytes));
    return {};
  }
  source.offset += bytes;
  return source;
}

[[nodiscard]] bool RepresentableRegion(ScriptRegion region, ScriptCommand bytes,
                                       ScriptVmHost& host) {
  if (bytes.size() > std::numeric_limits<std::uint32_t>::max()) {
    host.Refuse(ScriptVmRefusal::kScriptRegionExtent, static_cast<int>(region));
    return false;
  }
  return true;
}

// A position `offset` bytes into `region`, or none (refused) outside it.
[[nodiscard]] ScriptLocation Within(ScriptLocation source,
                                    std::span<const std::uint8_t> bytes,
                                    std::uint64_t offset, ScriptVmHost& host,
                                    bool& valid) {
  if (!IsCurrent(source, host)) {
    valid = false;
    return {};
  }
  if (offset < bytes.size() &&
      offset <= std::numeric_limits<std::uint32_t>::max()) {
    source.offset = static_cast<std::uint32_t>(offset);
    return source;
  }
  valid = false;
  host.Refuse(ScriptVmRefusal::kScriptHeader, static_cast<int>(offset));
  return {};
}

// A header table's entry `entry`: zero is no data, else an offset from the
// table.
[[nodiscard]] ScriptLocation TableEntry(ScriptLocation table,
                                        std::span<const std::uint8_t> bytes,
                                        int entry, ScriptVmHost& host,
                                        bool& valid) {
  if (!table.Present()) {
    return {};
  }
  if (!IsCurrent(table, host)) {
    valid = false;
    return {};
  }
  const std::uint64_t at = std::uint64_t{table.offset} +
                           static_cast<std::uint64_t>(entry) * kHalfBytes;
  if (at > bytes.size() || bytes.size() - at < kHalfBytes) {
    valid = false;
    host.Refuse(ScriptVmRefusal::kScriptHeader, static_cast<int>(at));
    return {};
  }
  const std::uint16_t value = ReadHalf(bytes, static_cast<std::size_t>(at));
  if (value == 0) {
    return {};
  }
  return Within(table, bytes, std::uint64_t{table.offset} + value, host, valid);
}

[[nodiscard]] bool BindScriptHeader(ScriptLocation data,
                                    std::span<const std::uint8_t> bytes,
                                    ScriptVmHost& host) {
  bool valid = true;
  const ScriptLocation strings =
      Within(data, bytes,
             std::uint64_t{data.offset} +
                 ReadHalf(bytes, data.offset + kDialogStringsOffset),
             host, valid);
  const ScriptLocation styles =
      Within(data, bytes,
             std::uint64_t{data.offset} +
                 ReadHalf(bytes, data.offset + kFrameStyleTableOffset),
             host, valid);
  std::array<ScriptLocation, kTextBoxFrameStyleCount> frames{};
  for (int style = kTextBoxFrameStyleFirst; style < kTextBoxFrameStyleCount;
       ++style) {
    frames[style] =
        TableEntry(styles, bytes, style - kFrameStyleEntryBias, host, valid);
  }
  const ScriptLocation patterns =
      Within(data, bytes,
             std::uint64_t{data.offset} +
                 ReadHalf(bytes, data.offset + kVibrationTableOffset),
             host, valid);
  std::array<ScriptLocation, kVibrationPatternSlots> vibrations{};
  for (int pattern = 0; pattern < kVibrationPatternSlots; ++pattern) {
    vibrations[pattern] = TableEntry(patterns, bytes, pattern, host, valid);
  }
  if (!valid) {
    return false;
  }
  host.SetDialogStrings(strings);
  for (int style = kTextBoxFrameStyleFirst; style < kTextBoxFrameStyleCount;
       ++style) {
    host.SetTextBoxFrameStyle(style, frames[style]);
  }
  for (int pattern = 0; pattern < kVibrationPatternSlots; ++pattern) {
    host.SetVibrationPattern(pattern, vibrations[pattern]);
  }
  return IsCurrent(data, host);
}

void ClearScriptViews(ScriptVmHost& host) {
  host.SetDialogStrings({});
  for (int style = kTextBoxFrameStyleFirst; style < kTextBoxFrameStyleCount;
       ++style) {
    host.SetTextBoxFrameStyle(style, {});
  }
  for (int pattern = 0; pattern < kVibrationPatternSlots; ++pattern) {
    host.SetVibrationPattern(pattern, {});
  }
}

void ResetEventState(ScriptVmHost& host) {
  host.SetSkipState(CutsceneSkip::kNone);
  host.SetSkipForced(false);
  host.SetFlag(kSkipActiveFlag, 0);
  for (int thread = 0; thread < kScriptThreadCount; ++thread) {
    host.SetWaitFrames(thread, 0);
  }
  host.SetEffectWaitPending(false);
  host.SetWeaponTransitionPending(false);
  host.SetTextSelectionActive(false);
  host.SetScanlineOverlayEnabled(false);
  host.SetMenuTransitionActive(false);
  host.SetRoomChangeState(0);
  host.SetFieldCameraRestoreScripted(false);
  host.SetMainThreadFinished(false);
  for (int text_box = 0; text_box < kTextBoxCount; ++text_box) {
    host.SetTextBoxAwaitingClose(text_box, false);
  }
  for (int curve = 0; curve < kCustomEasingCurveCount; ++curve) {
    host.SetCustomEasingCurve(curve, EasingCurve{});
  }
  host.CancelImageTweens();
  host.CaptureEventMusicSlot();
  host.ClearEventFile();
  for (int flag = kEventLocalFlagFirst; flag < kEventLocalFlagEnd; ++flag) {
    host.SetFlag(flag, 0);
  }
}

// Clears the waits and stops the music, sound, vibration and actors of a
// cutscene the player skipped; the skip handlers then run the rest of the
// script at once behind a faded-out, disabled display.
void BeginCutsceneSkip(ScriptVmHost& host) {
  host.NoteCutsceneSkip(0);
  host.SetSkipForced(false);
  for (int thread = 0; thread < kScriptThreadCount; ++thread) {
    host.SetWaitFrames(thread, 0);
  }
  for (int text_box = 0; text_box < kSkippedTextBoxes; ++text_box) {
    host.SetTextBoxAwaitingClose(text_box, false);
  }
  if (host.StopsMusicOnSkip()) {
    host.StopEventMusic();
  }
  host.StopAllSfx();
  host.StopScriptVibration();
  ResetCutsceneActorsToIdle(host);
  host.BeginSkipFadeOut();
  host.DisableDisplay();
  host.SetFlag(kSkipActiveFlag, kSkipActiveValue);
  host.SetSkipState(CutsceneSkip::kSkipping);
}

// Resets the three palette banks the skipped cutscene may have transformed.
void ResetSkippedPalettes(ScriptVmHost& host) {
  const std::span<const std::uint8_t> source = host.PaletteResetCommand();
  std::array<std::uint8_t, kPaletteCommandBytes> command{};
  std::copy_n(source.begin(), std::min(source.size(), command.size()),
              command.begin());
  for (int pass = 0; pass < kPalettePasses; ++pass) {
    command[kFirstOperand] = static_cast<std::uint8_t>(
        (static_cast<unsigned>(pass) << kPalettePassShift) | kPaletteResetMode);
    host.TransformPalettes(command);
  }
}

// True when the sound restore marker comes before the next end of cutscene.
[[nodiscard]] bool FindSoundRestoreMarker(ScriptLocation scan,
                                          ScriptVmHost& host) {
  if (!IsCurrent(scan, host)) {
    return false;
  }
  const std::span<const std::uint8_t> region = host.Region(scan.region);
  if (!RepresentableRegion(scan.region, region, host)) {
    return false;
  }
  const std::span<const std::uint8_t> marker = host.SoundRestoreMarker();
  std::size_t offset = scan.offset;
  while (offset < region.size()) {
    const std::uint8_t opcode = region[offset];
    if (opcode == kEndCutsceneOpcode) {
      return false;
    }
    const std::span<const std::uint8_t> here = region.subspan(offset);
    if (!marker.empty() && here.size() >= marker.size() &&
        std::equal(marker.begin(), marker.end(), here.begin())) {
      return true;
    }
    const int size = OpcodeSize(opcode, host);
    if (size == 0) {
      break;
    }
    offset += static_cast<std::size_t>(size);
  }
  host.Refuse(ScriptVmRefusal::kUnboundedSkipScan,
              static_cast<int>(scan.offset));
  return false;
}

[[nodiscard]] ScriptFlow EndCutscene(const ScriptInstruction& instruction,
                                     ScriptFlow otherwise, ScriptVmHost& host) {
  const CutsceneSkip previous = host.SkipState();
  host.SetSkipState(CutsceneSkip::kNone);
  if (previous == CutsceneSkip::kSkipping) {
    FinishSkippedCutscene(instruction.location, host);
    return Yield();
  }
  return otherwise;
}

[[nodiscard]] ScriptFlow WaitOn(std::int16_t initial, bool released,
                                const ScriptInstruction& instruction,
                                ScriptVmHost& host) {
  const int thread = instruction.thread;
  const std::int16_t frames = host.WaitFrames(thread);
  if (frames == 0) {
    host.SetWaitFrames(thread, initial);
    return Hold();
  }
  if (released) {
    host.SetWaitFrames(thread, 0);
    return Advance();
  }
  const auto remaining = static_cast<std::int16_t>(frames - 1);
  host.SetWaitFrames(thread, remaining);
  if (remaining <= 0) {
    host.SetWaitFrames(thread, 0);
    return Advance();
  }
  return Hold();
}

// Special operation 1: the reward results, then the ending once they close.
[[nodiscard]] ScriptFlow RunRewardSequence(ScriptVmHost& host) {
  if (host.MenuTransitionActive()) {
    if (host.UpdateRewardSequence()) {
      return Hold();
    }
    host.SetFlag(kRewardSequenceFlag, 0);
    host.SetMenuTransitionActive(false);
    host.RunEndingAndReturnToTitle();
    return Advance();
  }
  host.SetFlag(kRewardSequenceFlag, kRewardSequenceRunning);
  host.BeginEventRewardResults();
  host.SetMenuTransitionActive(true);
  return Hold();
}

[[nodiscard]] ScriptFlow CallSubroutine(const ScriptInstruction& instruction,
                                        ScriptVmHost& host) {
  if (!IsCurrent(instruction.location, host)) {
    return ScriptFlow::Of(ScriptFlowKind::kStop);
  }
  const ScriptLocation return_to =
      After(instruction.location, kSubroutineCallBytes, host);
  if (!return_to.Present()) {
    return ScriptFlow::Of(ScriptFlowKind::kStop);
  }
  host.SetSubroutineReturn(return_to);
  return ScriptFlow::JumpTo(StartOf(ScriptRegion::kSubroutine, host));
}

void SetCameraPitchPreset(std::uint8_t preset, ScriptVmHost& host) {
  const std::span<const std::int16_t> presets = host.CameraPitchPresets();
  if (preset >= presets.size()) {
    host.Refuse(ScriptVmRefusal::kPitchPreset, preset);
    return;
  }
  host.ApplySpecialOperation(SpecialOperation::kSetCameraPitchPreset,
                             presets[preset]);
}

void UnlockActionOnce(std::uint8_t action, ScriptVmHost& host) {
  if (host.IsActionUnlocked(action)) {
    host.SetFlag(kSpecialResultFlag, 0);
    return;
  }
  host.UnlockAction(action);
  host.SetFlag(kSpecialResultFlag, 1);
}

// The scene timer duration packed in operands 4..6.
[[nodiscard]] int TimerDuration(ScriptCommand command) {
  return static_cast<int>(
      (static_cast<std::uint32_t>(OperandByte(command, kFourthOperand))
       << kTimerHighShift) |
      (static_cast<std::uint32_t>(OperandByte(command, kFifthOperand))
       << kTimerMiddleShift) |
      (static_cast<std::uint32_t>(OperandByte(command, kSixthOperand))
       << kTimerLowShift));
}

// The curve families on a Q12 fraction.
[[nodiscard]] int CurveOf(CurveFamily family, std::int32_t fraction) {
  switch (family) {
    case CurveFamily::kEaseOut:
      return Core::Math::Sine(fraction / kQuadrants);
    case CurveFamily::kEaseInOut:
      return (Core::Math::Cosine(fraction / 2 + kHalfTurn) + kOne) >> 1;
    case CurveFamily::kEaseIn:
      return Core::Math::Sine(fraction / kQuadrants - kQuarterTurn) + kOne;
    case CurveFamily::kLinear:
    default:
      return fraction;
  }
}

// Squares the fraction `power` times, towards 1 for a negative power.
[[nodiscard]] std::int32_t RaiseFraction(std::int32_t fraction, int power) {
  const auto square = [](std::int32_t value) {
    return Core::Math::WrappingMultiply(value, value) >> kFractionShift;
  };
  if (power >= 0) {
    for (int i = 0; i < power; ++i) {
      fraction = square(fraction);
    }
    return fraction;
  }
  fraction = Core::Math::WrappingSubtract(kOne, fraction);
  for (int i = 0; i < -power; ++i) {
    fraction = square(fraction);
  }
  return Core::Math::WrappingSubtract(kOne, fraction);
}

// The script position after `flow` from `cursor`, or none when it leaves its
// region.
[[nodiscard]] ScriptLocation JumpTarget(ScriptLocation cursor,
                                        std::int32_t displacement,
                                        ScriptVmHost& host) {
  if (!IsCurrent(cursor, host)) {
    return {};
  }
  const std::int64_t target =
      static_cast<std::int64_t>(cursor.offset) + displacement;
  const std::span<const std::uint8_t> region = host.Region(cursor.region);
  if (target < 0 ||
      static_cast<std::uint64_t>(target) >
          std::numeric_limits<std::uint32_t>::max() ||
      static_cast<std::uint64_t>(target) >= region.size()) {
    host.Refuse(ScriptVmRefusal::kJumpOutsideScript,
                static_cast<int>(displacement));
    return {};
  }
  cursor.offset = static_cast<std::uint32_t>(target);
  return cursor;
}

// What a thread does after an opcode: run on from `cursor` this frame,
// finish, or end its frame storing `cursor` (none stops the thread).
struct Step {
  bool keep_running = false;
  bool finished = false;
  ScriptLocation cursor{};
  bool store = false;
};

[[nodiscard]] Step Follow(ScriptFlow flow, ScriptLocation cursor, int size,
                          ScriptVmHost& host) {
  const bool reuses_cursor = flow.kind != ScriptFlowKind::kStop &&
                             flow.kind != ScriptFlowKind::kFinish &&
                             flow.kind != ScriptFlowKind::kJumpTo;
  if (reuses_cursor && !IsCurrent(cursor, host)) {
    return {.store = true};
  }
  switch (flow.kind) {
    case ScriptFlowKind::kAdvance:
      if (size == 0) {
        host.Refuse(ScriptVmRefusal::kZeroLengthOpcode,
                    static_cast<int>(cursor.offset));
        return {.store = true};
      }
      cursor = After(cursor, static_cast<std::uint32_t>(size), host);
      return {.keep_running = cursor.Present(),
              .cursor = cursor,
              .store = !cursor.Present()};
    case ScriptFlowKind::kHold:
      return {.cursor = cursor, .store = true};
    case ScriptFlowKind::kFinish:
      return {.finished = true};
    case ScriptFlowKind::kAdvanceAndYield:
      cursor = After(cursor, static_cast<std::uint32_t>(size), host);
      return {.cursor = cursor, .store = true};
    case ScriptFlowKind::kJumpBy:
      cursor = JumpTarget(cursor, flow.displacement, host);
      return {.keep_running = cursor.Present(),
              .cursor = cursor,
              .store = !cursor.Present()};
    case ScriptFlowKind::kJumpTo:
      return {.keep_running = flow.target.Present(),
              .cursor = flow.target,
              .store = !flow.target.Present()};
    case ScriptFlowKind::kStop:
    default:
      return {.store = true};
  }
}

// Refuses a cursor that is not at a whole instruction inside its script.
[[nodiscard]] bool RunnableAt(ScriptLocation cursor, ScriptVmHost& host,
                              int& size) {
  if (!IsCurrent(cursor, host)) {
    return false;
  }
  const std::span<const std::uint8_t> region = host.Region(cursor.region);
  if (!RepresentableRegion(cursor.region, region, host)) {
    return false;
  }
  if (cursor.offset < region.size()) {
    size = OpcodeSize(region[cursor.offset], host);
    if (static_cast<std::size_t>(std::max(size, 1)) <=
        region.size() - cursor.offset) {
      return true;
    }
  }
  host.Refuse(ScriptVmRefusal::kCursorOutsideScript,
              static_cast<int>(cursor.offset));
  return false;
}

// The script of a door event: an offset from the door event section, at the
// event id's entry of the section's leading table.
[[nodiscard]] ScriptLocation DoorScript(ScriptVmHost& host) {
  const std::span<const std::uint8_t> doors =
      host.Region(ScriptRegion::kDoorEvents);
  const int event_id = host.EventId();
  const auto entry = static_cast<std::uint32_t>(event_id) * kHalfBytes;
  if (event_id >= 0 && HalfFits(doors, entry)) {
    const std::int16_t offset = OperandHalf(doors, entry);
    if (offset >= 0 && static_cast<std::size_t>(offset) < doors.size()) {
      return {.region = ScriptRegion::kDoorEvents,
              .offset = static_cast<std::uint32_t>(offset),
              .generation = host.RegionGeneration(ScriptRegion::kDoorEvents)};
    }
  }
  host.Refuse(ScriptVmRefusal::kDoorScript, event_id);
  return {};
}

[[nodiscard]] int ConfirmRequestedEvent(ScriptVmHost& host) {
  if (!host.StartConfirmed()) {
    return 0;
  }
  host.SetPhase(PlaybackPhase::kStarting);
  host.SetStartConfirmed(false);
  host.InitializeEventCamera();
  host.ReturnAllActorsToIdle();
  host.ClearScriptControlledActors();
  return 2;
}

void StopEventStartup(ScriptVmHost& host) {
  if (host.EventFile() != EventFileStatus::kNone) {
    host.CancelEventFile();
  }
  host.DiscardChainedEventBuffer();
  host.SetChainedEventLoaded(false);
  host.SetStartConfirmed(false);
  host.SetMainThreadFinished(false);
  host.SetSubroutineReturn({});
  for (int thread = 0; thread < kScriptThreadCount; ++thread) {
    host.SetThreadCursor(thread, {});
    host.SetWaitFrames(thread, 0);
  }
  ClearScriptViews(host);
  ReleaseScriptActorControl(host);
  host.ClearScriptControlledActors();
  host.SetPhase(PlaybackPhase::kIdle);
}

void StartEventScript(ScriptVmHost& host) {
  if (host.EventFile() != EventFileStatus::kNone) {
    host.Refuse(ScriptVmRefusal::kEventFileBusy, host.EventId());
    StopEventStartup(host);
    return;
  }
  host.SetScriptBackdropEnabled(false);
  host.SetChainedEventLoaded(false);
  switch (host.Source()) {
    case EventSource::kRoomEntryScript:
      if (InitializeEventScriptData(StartOf(ScriptRegion::kEventScript, host),
                                    host)) {
        host.SetPhase(PlaybackPhase::kRunning);
        return;
      }
      break;
    case EventSource::kDoorScript: {
      if (!host.BindRoomDoorSource()) {
        host.Refuse(ScriptVmRefusal::kDoorScript, host.EventId());
        break;
      }
      const ScriptLocation door = DoorScript(host);
      if (door.Present() && InitializeEventScriptData(door, host)) {
        host.SetPhase(PlaybackPhase::kRunning);
        return;
      }
      break;
    }
    case EventSource::kEventFile:
      if (QueueEventScriptLoad(EventBuffer::kScript, host)) {
        host.SetPhase(PlaybackPhase::kLoading);
        return;
      }
      break;
    default:
      host.Refuse(ScriptVmRefusal::kEventSource,
                  static_cast<int>(host.Source()));
      break;
  }
  StopEventStartup(host);
}

void AwaitEventFile(ScriptVmHost& host) {
  const EventFileStatus status = host.EventFile();
  if (status == EventFileStatus::kNone) {
    host.Refuse(ScriptVmRefusal::kMissingEventFile, host.EventId());
    StopEventStartup(host);
    return;
  }
  if (status == EventFileStatus::kLoading) {
    return;
  }
  host.ReleaseEventFile();
  if (status != EventFileStatus::kReady) {
    host.Refuse(ScriptVmRefusal::kEventFileRead, host.EventId());
    StopEventStartup(host);
    return;
  }
  if (host.ChainedEventLoaded()) {
    if (!host.AdoptChainedEvent()) {
      host.Refuse(ScriptVmRefusal::kChainedEventAdoption, host.EventId());
      StopEventStartup(host);
      return;
    }
    host.SetChainedEventLoaded(false);
  }
  if (!InitializeEventScriptData(StartOf(ScriptRegion::kEventScript, host),
                                 host)) {
    StopEventStartup(host);
    return;
  }
  host.SetPhase(PlaybackPhase::kRunning);
}

void RunScriptThreads(ScriptVmHost& host) {
  if (ExecuteScriptThread(ScriptThreadId::kMain, host)) {
    if (!host.FieldCameraRestoreScripted() && !host.ChainedEventLoaded() &&
        host.RoomChangeState() == 0) {
      host.RestoreEventFieldCamera();
    }
    host.SetMainThreadFinished(true);
  }
  for (int thread = 1; thread < kScriptThreadCount; ++thread) {
    if (host.ThreadCursor(thread).Present()) {
      ExecuteScriptThread(static_cast<ScriptThreadId>(thread), host);
    }
  }
}

void RunEventFrame(ScriptVmHost& host) {
  constexpr std::uint8_t kCameraTracksComplete = 0xFF;
  host.UpdateEventCameraOrigin();
  if (!host.MainThreadFinished()) {
    RunScriptThreads(host);
  }
  host.UpdateEventCameraTracks();
  host.UpdateEventScreenTransforms();
  host.ApplyEventCameraState();
  host.UpdateImageTweens();
  host.UpdateWeatherTween();
  if (host.ScanlineOverlayEnabled()) {
    host.DrawScanlineOverlay();
  }
  if (host.MainThreadFinished() &&
      host.EventCameraCompletionMask() == kCameraTracksComplete) {
    if (host.ChainedEventLoaded()) {
      host.SetPhase(PlaybackPhase::kLoading);
      host.SetSource(EventSource::kEventFile);
    } else {
      ReleaseScriptActorControl(host);
      host.SetPhase(PlaybackPhase::kIdle);
    }
    host.SetMainThreadFinished(false);
  }
}

}  // namespace

void RestoreScriptTextureRectangle(ScriptVmHost& host) {
  host.MoveTextureRectangle(kSkipTextureMove);
}

ScriptFlow ScriptReportInvalidOpcode(const ScriptInstruction& instruction,
                                     ScriptVmHost& host) {
  host.NoteInvalidOpcode(instruction);
  return Advance();
}

// decomp: BATTLE.PRG func_800B66E4 (4A0A8.c:597), returns 0.
ScriptFlow ScriptNoOperation(const ScriptInstruction& /*instruction*/,
                             ScriptVmHost& /*host*/) {
  return Advance();
}

// decomp: BATTLE.PRG func_800B66EC (4A0A8.c:599), returns 4.
ScriptFlow ScriptYieldFrame(const ScriptInstruction& /*instruction*/,
                            ScriptVmHost& /*host*/) {
  return Yield();
}

ScriptFlow ScriptWaitForEventFile(const ScriptInstruction& /*instruction*/,
                                  ScriptVmHost& host) {
  if (host.EventFile() == EventFileStatus::kFailed) {
    host.Refuse(ScriptVmRefusal::kEventFileRead, host.EventId());
    return ScriptFlow::Of(ScriptFlowKind::kStop);
  }
  return host.EventFile() == EventFileStatus::kLoading ? Hold() : Advance();
}

ScriptFlow ScriptBeginSkippableCutscene(const ScriptInstruction& instruction,
                                        ScriptVmHost& host) {
  if (!Fits(instruction, kOneOperandBytes, host)) {
    return Advance();
  }
  host.SetSkipState(CutsceneSkip::kSkippable);
  host.SetStopsMusicOnSkip(
      (OperandByte(Bytes(instruction), kFirstOperand) & 1U) == 0);
  return host.SkipForced() ? Yield() : Advance();
}

void FinishSkippedCutscene(ScriptLocation scan, ScriptVmHost& host) {
  ResetSkippedPalettes(host);
  RestoreScriptTextureRectangle(host);
  if (FindSoundRestoreMarker(scan, host)) {
    host.SetFlag(kSkipActiveFlag, 0);
    return;
  }
  host.RestartRoomEffectSound();
  host.SetFlag(kSkipActiveFlag, 0);
}

ScriptFlow ScriptEndCutsceneAndJump(const ScriptInstruction& instruction,
                                    ScriptVmHost& host) {
  if (!Fits(instruction, kTwoOperandBytes, host)) {
    return Advance();
  }
  return EndCutscene(
      instruction,
      ScriptFlow::JumpBy(OperandHalf(Bytes(instruction), kFirstOperand)), host);
}

ScriptFlow ScriptEndCutscene(const ScriptInstruction& instruction,
                             ScriptVmHost& host) {
  return EndCutscene(instruction, Advance(), host);
}

ScriptFlow ScriptWaitForButtonAndYield(const ScriptInstruction& instruction,
                                       ScriptVmHost& host) {
  if (!Fits(instruction, kOneOperandBytes, host)) {
    return Advance();
  }
  const std::uint8_t buttons = OperandByte(Bytes(instruction), kFirstOperand);
  return (host.HeldButtons() & buttons) != 0 ? Yield() : Hold();
}

ScriptFlow ScriptBranchOnFlagImmediate(const ScriptInstruction& instruction,
                                       ScriptVmHost& host) {
  if (!Fits(instruction, kFiveOperandBytes, host)) {
    return Advance();
  }
  const ScriptCommand command = Bytes(instruction);
  const auto flag = host.Flag(FirstFlagId(command));
  const auto immediate = OperandByte(command, kThirdOperand);
  if (TestHolds(OperationOf(command), flag, immediate, true)) {
    return Advance();
  }
  return ScriptFlow::JumpBy(OperandHalf(command, kFourthOperand));
}

ScriptFlow ScriptBranchOnFlagPair(const ScriptInstruction& instruction,
                                  ScriptVmHost& host) {
  if (!Fits(instruction, kFiveOperandBytes, host)) {
    return Advance();
  }
  const ScriptCommand command = Bytes(instruction);
  const auto left = host.Flag(FirstFlagId(command));
  const auto right = host.Flag(SecondFlagId(command));
  if (TestHolds(OperationOf(command), left, right, false)) {
    return Advance();
  }
  return ScriptFlow::JumpBy(OperandHalf(command, kFourthOperand));
}

ScriptFlow ScriptJumpForward(const ScriptInstruction& instruction,
                             ScriptVmHost& host) {
  if (!Fits(instruction, kTwoOperandBytes, host)) {
    return Advance();
  }
  return ScriptFlow::JumpBy(OperandHalf(Bytes(instruction), kFirstOperand));
}

// decomp: BATTLE.PRG vs_battle_script_setFlagImmContext (4A0A8.c:1640).
ScriptFlow ScriptCacheFlagValue(const ScriptInstruction& instruction,
                                ScriptVmHost& host) {
  if (!Fits(instruction, kTwoOperandBytes, host)) {
    return Advance();
  }
  host.SetCachedFlag(static_cast<std::int8_t>(
      host.Flag(OperandHalf(Bytes(instruction), kFirstOperand))));
  return Advance();
}

// decomp: BATTLE.PRG vs_battle_script_jumpFwdIfFlagContext (4A0A8.c:1646).
ScriptFlow ScriptBranchOnCachedFlag(const ScriptInstruction& instruction,
                                    ScriptVmHost& host) {
  if (!Fits(instruction, kThreeOperandBytes, host)) {
    return Advance();
  }
  const ScriptCommand command = Bytes(instruction);
  const auto cached = static_cast<std::uint8_t>(host.CachedFlag());
  if (cached == OperandByte(command, kFirstOperand)) {
    return ScriptFlow::JumpBy(OperandHalf(command, kSecondOperand));
  }
  return Advance();
}

ScriptFlow ScriptReturnToSavedAddress(const ScriptInstruction& /*instruction*/,
                                      ScriptVmHost& host) {
  const ScriptLocation saved = host.SubroutineReturn();
  if (saved.Present() && !IsCurrent(saved, host)) {
    return ScriptFlow::Of(ScriptFlowKind::kStop);
  }
  return saved.Present() ? ScriptFlow::JumpTo(saved) : Advance();
}

ScriptFlow ScriptSetLoopCounter(const ScriptInstruction& instruction,
                                ScriptVmHost& host) {
  if (!Fits(instruction, kTwoOperandBytes, host)) {
    return Advance();
  }
  const ScriptCommand command = Bytes(instruction);
  const std::uint8_t counter = OperandByte(command, kFirstOperand);
  if (counter == kNoLoopCounter) {
    return Advance();
  }
  if (counter >= kLoopCounterCount) {
    host.Refuse(ScriptVmRefusal::kLoopCounter, counter);
    return Advance();
  }
  host.SetLoopCounter(counter, OperandByte(command, kSecondOperand));
  return Advance();
}

ScriptFlow ScriptLoopBackward(const ScriptInstruction& instruction,
                              ScriptVmHost& host) {
  if (!Fits(instruction, kThreeOperandBytes, host)) {
    return Advance();
  }
  const ScriptCommand command = Bytes(instruction);
  const ScriptFlow back = ScriptFlow::JumpBy(
      -static_cast<std::int32_t>(OperandHalf(command, kSecondOperand)));
  const std::uint8_t counter = OperandByte(command, kFirstOperand);
  if (counter == kNoLoopCounter) {
    return back;
  }
  if (counter >= kLoopCounterCount) {
    host.Refuse(ScriptVmRefusal::kLoopCounter, counter);
    return Advance();
  }
  const auto remaining =
      static_cast<std::uint8_t>(host.LoopCounter(counter) - 1U);
  host.SetLoopCounter(counter, remaining);
  return remaining != 0 ? back : Advance();
}

ScriptFlow ScriptApplyImmediateFlagOperation(
    const ScriptInstruction& instruction, ScriptVmHost& host) {
  if (!Fits(instruction, kThreeOperandBytes, host)) {
    return Advance();
  }
  const ScriptCommand command = Bytes(instruction);
  const int flag = FirstFlagId(command);
  const std::uint8_t operation = OperationOf(command);
  const std::int32_t operand = OperandByte(command, kThirdOperand);
  if (static_cast<FlagOperation>(operation) == FlagOperation::kAssign) {
    host.SetFlag(flag, static_cast<std::uint8_t>(operand));
    return Advance();
  }
  if (operation > static_cast<std::uint8_t>(FlagOperation::kAssign)) {
    return Advance();
  }
  std::int32_t result = 0;
  if (Arithmetic(operation, host.Flag(flag), operand, result)) {
    host.SetFlag(flag, static_cast<std::uint8_t>(result));
  }
  return Advance();
}

ScriptFlow ScriptApplyFlagPairOperation(const ScriptInstruction& instruction,
                                        ScriptVmHost& host) {
  if (!Fits(instruction, kThreeOperandBytes, host)) {
    return Advance();
  }
  const ScriptCommand command = Bytes(instruction);
  const int first = FirstFlagId(command);
  const int second = SecondFlagId(command);
  const std::uint8_t operation = OperationOf(command);
  switch (static_cast<FlagOperation>(operation)) {
    case FlagOperation::kAbsoluteDifference: {
      const std::int32_t difference = host.Flag(first) - host.Flag(second);
      host.SetFlag(first, static_cast<std::uint8_t>(
                              difference < 0 ? -difference : difference));
      return Advance();
    }
    case FlagOperation::kSwap: {
      const std::uint8_t previous = host.Flag(first);
      host.SetFlag(first, host.Flag(second));
      host.SetFlag(second, previous);
      return Advance();
    }
    case FlagOperation::kAssign:
      host.SetFlag(first, host.Flag(second));
      return Advance();
    default:
      break;
  }
  if (operation > static_cast<std::uint8_t>(FlagOperation::kAssign)) {
    return Advance();
  }
  std::int32_t result = 0;
  if (Arithmetic(operation, host.Flag(first), host.Flag(second), result)) {
    host.SetFlag(first, static_cast<std::uint8_t>(result));
  }
  return Advance();
}

ScriptFlow ScriptStoreInventoryItemPresence(
    const ScriptInstruction& instruction, ScriptVmHost& host) {
  if (!Fits(instruction, kFourOperandBytes, host)) {
    return Advance();
  }
  const ScriptCommand command = Bytes(instruction);
  const int count =
      host.CountInventoryItem(OperandHalf(command, kFirstOperand));
  host.SetFlag(OperandHalf(command, kThirdOperand),
               static_cast<std::uint8_t>(count));
  return Advance();
}

ScriptFlow ScriptStoreButtonState(const ScriptInstruction& instruction,
                                  ScriptVmHost& host) {
  if (!Fits(instruction, kTwoOperandBytes, host)) {
    return Advance();
  }
  const auto operand = static_cast<std::uint16_t>(
      OperandHalf(Bytes(instruction), kFirstOperand));
  const int value = (operand & kPressedButtonsSource) != 0
                        ? host.PressedButtonsLowByte()
                        : host.PreviousButtons();
  host.SetFlag(static_cast<int>(operand & kButtonFlagMask),
               static_cast<std::uint8_t>(value));
  return Advance();
}

ScriptFlow ScriptConfigureTimer(const ScriptInstruction& instruction,
                                ScriptVmHost& host) {
  if (!Fits(instruction, kSixOperandBytes, host)) {
    return Advance();
  }
  const ScriptCommand command = Bytes(instruction);
  if (OperandByte(command, kFirstOperand) == 0) {
    host.ResetSceneTimer();
    return Advance();
  }
  const std::uint8_t mode = OperandByte(command, kThirdOperand);
  const std::uint8_t scope = OperandByte(command, kSecondOperand);
  const std::span<const std::int16_t> modes = host.SceneTimerModes();
  const std::span<const std::int16_t> scopes = host.SceneTimerScopes();
  if (mode >= modes.size()) {
    host.Refuse(ScriptVmRefusal::kTimerMode, mode);
    return Advance();
  }
  if (scope >= scopes.size()) {
    host.Refuse(ScriptVmRefusal::kTimerScope, scope);
    return Advance();
  }
  host.StartSceneTimer(modes[mode], scopes[scope], TimerDuration(command));
  return Advance();
}

ScriptFlow ScriptWaitForButtonOrTimeout(const ScriptInstruction& instruction,
                                        ScriptVmHost& host) {
  if (!Fits(instruction, kTwoOperandBytes, host) ||
      !ValidThread(instruction.thread, host)) {
    return Advance();
  }
  const ScriptCommand command = Bytes(instruction);
  const bool pressed =
      (host.HeldButtons() & OperandByte(command, kFirstOperand)) != 0;
  const ScriptFlow flow =
      WaitOn(OperandByte(command, kSecondOperand), pressed, instruction, host);
  if (flow.kind == ScriptFlowKind::kAdvance) {
    host.ClearHeldButtons();
  }
  return flow;
}

ScriptFlow ScriptConfigureEasingCurve(const ScriptInstruction& instruction,
                                      ScriptVmHost& host) {
  if (!Fits(instruction, kOneOperandBytes, host)) {
    return Advance();
  }
  const unsigned operand = OperandByte(Bytes(instruction), kFirstOperand);
  unsigned power = operand & kCurvePowerMask;
  if ((operand & kCurvePowerSign) != 0) {
    power |= kCurvePowerExtension;
  }
  host.SetCustomEasingCurve(
      static_cast<int>(operand >> kCurveIndexShift),
      {.family = static_cast<std::uint8_t>((operand >> kCurveFamilyShift) &
                                           kCurveFamilyMask),
       .power = static_cast<std::int8_t>(static_cast<std::uint8_t>(power))});
  return Advance();
}

ScriptFlow CommandSpecialOperation(const ScriptInstruction& instruction,
                                   ScriptVmHost& host) {
  if (!Fits(instruction, kTwoOperandBytes, host)) {
    return Advance();
  }
  const ScriptCommand command = Bytes(instruction);
  const std::uint8_t operation = OperandByte(command, kFirstOperand);
  const std::uint8_t argument = OperandByte(command, kSecondOperand);
  switch (static_cast<SpecialOperation>(operation)) {
    case SpecialOperation::kRunRewardSequence:
      return RunRewardSequence(host);
    case SpecialOperation::kForceSkip:
      host.SetSkipForced(true);
      return Advance();
    case SpecialOperation::kUnlockAction:
      UnlockActionOnce(argument, host);
      return Advance();
    case SpecialOperation::kCountDispositionActors:
      host.SetFlag(kSpecialResultFlag,
                   static_cast<std::uint8_t>(
                       host.CountActorsByDisposition(kEnemyDisposition)));
      return Advance();
    case SpecialOperation::kSetCameraPitchPreset:
      SetCameraPitchPreset(argument, host);
      return Advance();
    case SpecialOperation::kCallSubroutine:
      return CallSubroutine(instruction, host);
    case SpecialOperation::kQueueAshleyModel:
    case SpecialOperation::kSetEventActorActivity:
    case SpecialOperation::kFreeChainedEvent:
    case SpecialOperation::kSetFieldCameraOverride:
    case SpecialOperation::kPauseSceneTimer:
    case SpecialOperation::kStartCameraOrbitReturn:
    case SpecialOperation::kScaleCameraDuration:
    case SpecialOperation::kFreezeMovingPlatforms:
    case SpecialOperation::kClearGeometryOcclusion:
    case SpecialOperation::kPauseMovingPlatforms:
    case SpecialOperation::kSetVignette:
    case SpecialOperation::kCancelRoomVisit:
      host.ApplySpecialOperation(static_cast<SpecialOperation>(operation),
                                 argument);
      return Advance();
  }
  host.NoteUndefinedSpecialOperation(operation);
  return Advance();
}

// decomp: BATTLE.PRG func_800BB474 (4A0A8.c:2799).
ScriptFlow CommandWaitFrames(const ScriptInstruction& instruction,
                             ScriptVmHost& host) {
  if (!Fits(instruction, kOneOperandBytes, host) ||
      !ValidThread(instruction.thread, host)) {
    return Advance();
  }
  return WaitOn(OperandByte(Bytes(instruction), kFirstOperand), false,
                instruction, host);
}

// decomp: BATTLE.PRG func_800BB4CC (4A0A8.c:2815).
ScriptFlow CommandSetTickSpeed(const ScriptInstruction& instruction,
                               ScriptVmHost& host) {
  if (!Fits(instruction, kOneOperandBytes, host)) {
    return Advance();
  }
  host.SetTickSpeed(OperandByte(Bytes(instruction), kFirstOperand));
  return Advance();
}

// decomp: BATTLE.PRG func_800BB504 (4A0A8.c:2827).
ScriptFlow CommandWaitScriptContext(const ScriptInstruction& instruction,
                                    ScriptVmHost& host) {
  if (!Fits(instruction, kOneOperandBytes, host)) {
    return Advance();
  }
  const unsigned operand = OperandByte(Bytes(instruction), kFirstOperand);
  const auto thread = static_cast<int>(operand & kContextThreadMask);
  if ((operand & kReleaseContext) != 0) {
    host.SetThreadCursor(thread, {});
    return Advance();
  }
  return host.ThreadCursor(thread).Present() ? Hold() : Advance();
}

// decomp: BATTLE.PRG func_800BB554 (4A0A8.c:2837).
ScriptFlow CommandStartScriptContext(const ScriptInstruction& instruction,
                                     ScriptVmHost& host) {
  if (!Fits(instruction, kThreeOperandBytes, host)) {
    return Advance();
  }
  if (!IsCurrent(instruction.location, host)) {
    return ScriptFlow::Of(ScriptFlowKind::kStop);
  }
  const ScriptCommand command = Bytes(instruction);
  const int thread = OperandByte(command, kFirstOperand);
  if (ValidThread(thread, host)) {
    const ScriptLocation start =
        After(instruction.location, kContextStartBytes, host);
    if (!start.Present()) {
      return ScriptFlow::Of(ScriptFlowKind::kStop);
    }
    host.SetThreadCursor(thread, start);
  }
  return ScriptFlow::JumpBy(OperandHalf(command, kSecondOperand));
}

// decomp: BATTLE.PRG func_800BB5A4 (4A0A8.c:2843), returns 3.
ScriptFlow CommandStopScriptContext(const ScriptInstruction& /*instruction*/,
                                    ScriptVmHost& /*host*/) {
  return {.kind = ScriptFlowKind::kStop};
}

ScriptFlow CommandLoadNextEvent(const ScriptInstruction& instruction,
                                ScriptVmHost& host) {
  if (!Fits(instruction, kTwoOperandBytes, host)) {
    return Advance();
  }
  if (host.EventFile() != EventFileStatus::kNone || host.ChainedEventLoaded()) {
    host.Refuse(ScriptVmRefusal::kEventFileBusy, host.EventId());
    return ScriptFlow::Of(ScriptFlowKind::kStop);
  }
  const std::int16_t previous = host.EventId();
  host.SetEventId(OperandHalf(Bytes(instruction), kFirstOperand));
  if (!host.AllocateChainedEventBuffer()) {
    host.Refuse(ScriptVmRefusal::kEventBufferAllocation, host.EventId());
    host.SetEventId(previous);
    return ScriptFlow::Of(ScriptFlowKind::kStop);
  }
  if (!QueueEventScriptLoad(EventBuffer::kChained, host)) {
    host.DiscardChainedEventBuffer();
    host.SetEventId(previous);
    return ScriptFlow::Of(ScriptFlowKind::kStop);
  }
  host.SetChainedEventLoaded(true);
  return Advance();
}

ScriptFlow CommandFinishEvent(const ScriptInstruction& /*instruction*/,
                              ScriptVmHost& host) {
  int open_text_boxes = 0;
  for (int text_box = 0; text_box < kTextBoxCount; ++text_box) {
    if (host.DismissTextBox(text_box)) {
      ++open_text_boxes;
    }
  }
  if (open_text_boxes != 0) {
    return Hold();
  }
  host.SetTickSpeed(kFieldTickSpeed);
  return ScriptFlow::Of(ScriptFlowKind::kFinish);
}

PlaybackPhase EventPlaybackState(const ScriptVmHost& host) {
  return host.Phase();
}

std::int16_t PendingEventSelector(const ScriptVmHost& host) {
  constexpr unsigned kSourceShift = 12;
  return static_cast<std::int16_t>(
      (static_cast<unsigned>(host.Source()) << kSourceShift) |
      static_cast<std::uint16_t>(host.EventId()));
}

int RequestEventStart(ScriptVmHost& host) {
  if (host.Phase() == PlaybackPhase::kRequested) {
    host.SetStartConfirmed(true);
  }
  return 1;
}

// decomp: BATTLE.PRG func_800BF5F0 (4A0A8.c:4588).
bool InitializeEventScriptData(ScriptLocation data, ScriptVmHost& host) {
  if (host.EventFile() != EventFileStatus::kNone) {
    host.Refuse(ScriptVmRefusal::kEventFileBusy, host.EventId());
    return false;
  }
  const bool current = IsCurrent(data, host);
  const std::span<const std::uint8_t> bytes =
      current ? host.Region(data.region) : ScriptCommand{};
  bool header = current && data.Present() &&
                bytes.size() <= std::numeric_limits<std::uint32_t>::max() &&
                data.offset <= bytes.size() &&
                bytes.size() - data.offset > kEventScriptHeaderBytes;
  if (header) {
    header = BindScriptHeader(data, bytes, host);
  } else {
    if (current) {
      host.Refuse(ScriptVmRefusal::kScriptHeader,
                  static_cast<int>(data.offset));
    }
  }
  if (!header) {
    ClearScriptViews(host);
  }
  host.SetThreadCursor(
      0, header
             ? ScriptLocation{.region = data.region,
                              .offset = data.offset + kEventScriptHeaderBytes,
                              .generation = data.generation}
             : ScriptLocation{});
  for (int thread = 1; thread < kScriptThreadCount; ++thread) {
    host.SetThreadCursor(thread, {});
  }
  ResetEventState(host);
  return header;
}

// decomp: BATTLE.PRG func_800BF850 (4A0A8.c:4633).
int UpdateEventPlayback(ScriptVmHost& host) {
  switch (host.Phase()) {
    case PlaybackPhase::kIdle:
      return 0;
    case PlaybackPhase::kRequested:
      return ConfirmRequestedEvent(host);
    case PlaybackPhase::kStarting:
      StartEventScript(host);
      return 2;
    case PlaybackPhase::kLoading:
      AwaitEventFile(host);
      return 2;
    case PlaybackPhase::kRunning:
    default:
      RunEventFrame(host);
      return 2;
  }
}

// decomp: BATTLE.PRG func_800BFBB8 (573B8.c:67).
bool ExecuteScriptThread(ScriptThreadId script_thread, ScriptVmHost& host) {
  const int thread = static_cast<int>(script_thread);
  if (!ValidThread(thread, host)) {
    return true;
  }
  if (thread == 0 && host.SkipState() == CutsceneSkip::kSkippable &&
      (host.StartPressed() || host.SkipForced())) {
    BeginCutsceneSkip(host);
  }
  ScriptLocation cursor = host.ThreadCursor(thread);
  if (!cursor.Present()) {
    host.Refuse(ScriptVmRefusal::kMissingCursor, thread);
    return true;
  }
  for (std::size_t steps = 0; steps < kScriptDispatchBudget; ++steps) {
    int size = 0;
    if (!RunnableAt(cursor, host, size)) {
      host.SetThreadCursor(thread, {});
      return false;
    }
    const Step step =
        Follow(host.RunOpcode(cursor, thread), cursor, size, host);
    if (step.finished) {
      return true;
    }
    if (!step.keep_running) {
      host.SetThreadCursor(thread, step.cursor);
      return false;
    }
    cursor = step.cursor;
  }
  host.Refuse(ScriptVmRefusal::kDispatchBudget,
              static_cast<int>(kScriptDispatchBudget));
  host.SetThreadCursor(thread, {});
  return false;
}

// decomp: BATTLE.PRG func_800BFD9C (573B8.c:133).
void ReleaseScriptActorControl(ScriptVmHost& host) {
  for (int actor = 0; actor < kScriptControlledActorCount; ++actor) {
    if (host.ScriptControlledActor(actor)) {
      host.ResetActorMovement(actor);
    }
  }
  host.FreeEventCamera();
}

int OpcodeSize(std::uint8_t opcode, const ScriptVmHost& host) {
  const std::span<const std::uint8_t> sizes = host.OpcodeSizes();
  return opcode < sizes.size() ? sizes[opcode] : 0;
}

int ResolveScriptActorReference(std::uint16_t reference, ScriptVmHost& host) {
  switch (reference >> kReferenceKindShift) {
    case kModelReference:
      return host.FindActorByModelId(
          static_cast<int>(reference & kModelIdMask));
    case kSlotReference:
      return static_cast<int>(reference & kSlotMask);
    default:
      return reference;
  }
}

int EvaluateInterpolationCurve(std::int16_t mode, std::int16_t frame,
                               std::int16_t duration, ScriptVmHost& host) {
  const std::int32_t position = std::min(frame, duration);
  const auto ratio = [&](std::int32_t scale) {
    return Core::Math::R3000Quotient(position * scale, duration);
  };
  switch (static_cast<EasingMode>(mode)) {
    case EasingMode::kLinear:
      return ratio(kOne);
    case EasingMode::kEaseOut:
      return Core::Math::Sine(ratio(kQuarterTurn));
    case EasingMode::kEaseInOut:
      return (Core::Math::Cosine(ratio(kHalfTurn) + kHalfTurn) + kOne) >> 1;
    case EasingMode::kEaseIn:
      return Core::Math::Sine(ratio(kQuarterTurn) - kQuarterTurn) + kOne;
    default:
      break;
  }
  const int curve = mode - static_cast<int>(EasingMode::kCustomFirst);
  if (curve < 0 || curve >= kCustomEasingCurveCount) {
    host.Refuse(ScriptVmRefusal::kEasingMode, mode);
    return ratio(kOne);
  }
  const EasingCurve easing = host.CustomEasingCurve(curve);
  return CurveOf(static_cast<CurveFamily>(easing.family),
                 RaiseFraction(ratio(kOne), easing.power));
}

bool QueueEventScriptLoad(EventBuffer destination, ScriptVmHost& host) {
  if (host.EventFile() != EventFileStatus::kNone) {
    host.Refuse(ScriptVmRefusal::kEventFileBusy, host.EventId());
    return false;
  }
  if (!host.QueueEventFileLoad(host.EventId(), destination)) {
    host.Refuse(ScriptVmRefusal::kEventFileQueue, host.EventId());
    return false;
  }
  return true;
}

void ResetCutsceneActorsToIdle(ScriptVmHost& host) {
  for (int actor = 0; actor < kScriptControlledActorCount; ++actor) {
    if (host.ActorPresent(actor)) {
      host.PlayActorIdleAnimation(actor);
    }
  }
}

bool InitializeScriptOpcodeTable(ScriptVmHost& host) {
  if (host.EventFile() != EventFileStatus::kNone) {
    host.CancelEventFile();
  }
  host.DiscardChainedEventBuffer();
  host.SetChainedEventLoaded(false);
  host.SetSubroutineReturn({});
  ClearScriptViews(host);
  host.SetAmbientSoundSuspended(false);
  host.SetPhase(PlaybackPhase::kIdle);
  host.SetStartConfirmed(false);
  host.SetMainThreadFinished(false);
  for (int thread = 0; thread < kScriptThreadCount; ++thread) {
    host.SetThreadCursor(thread, {});
    host.SetWaitFrames(thread, 0);
  }
  host.SetRoomChangeState(0);
  if (!host.AllocateEventBuffers()) {
    host.Refuse(ScriptVmRefusal::kEventBufferAllocation,
                static_cast<int>(kEventScriptBytes));
    return false;
  }
  return true;
}

}  // namespace VS::Event
