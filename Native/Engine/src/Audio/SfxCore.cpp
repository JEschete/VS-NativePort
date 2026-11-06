// Hand-maintained (tools/port/owned.csv).
// Typed sound-effect front end (Sfx.hpp).
#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>

#include "Audio/Sfx.hpp"

namespace VS::Audio {
namespace {
using Playback::DataRef;
using Playback::EffectSelector;

// vs_main_computeSfxPan: the screen's center column, the pan's 3/10 scale and
// its swing either side of center; and the depth past which the volume falls
// one step per 32 units, to silence at the end of its range.
constexpr std::int32_t kScreenCenterX = 160;
constexpr std::int32_t kPanScaleNumerator = 3;
constexpr std::int32_t kPanScaleDenominator = 10;
constexpr std::int32_t kPanSwing = 96;
constexpr std::int32_t kAudibleDepth = 0x300;
constexpr std::int32_t kDepthRange = 0xFFF;
constexpr unsigned kDepthShift = 5;
// func_80046310: the screen's width and the pan of its left edge; and the
// depth range over which the volume falls one step per 8 units.
constexpr std::int32_t kScreenWidth = 0x140;
constexpr std::int32_t kBankPanLeft = 0x30;
constexpr std::int32_t kBankDepthRange = 0x400;
constexpr unsigned kBankDepthShift = 3;
// A projection's screen x: its low halfword.
constexpr std::int32_t kHalfwordModulus = 0x10000;

// The group's tag a play takes; advances `cursor` to the group's next tag,
// or back to its first past its last (the original's 32-bit shift).
std::int32_t NextTag(std::int32_t& cursor, std::int32_t group_tags) {
  const std::int32_t tag = cursor;
  cursor = static_cast<std::int32_t>(static_cast<std::uint32_t>(cursor) << 1U);
  if ((cursor & group_tags) == 0) cursor = LowestEffectTag(group_tags);
  return tag;
}

// Word `index` of a bank: its count, then one offset per effect; empty when
// it lies outside `bytes`.
std::optional<std::uint32_t> BankWord(std::span<const std::byte> bytes,
                                      std::size_t index) {
  const std::size_t offset = index * sizeof(std::uint32_t);
  if (offset > bytes.size() || bytes.size() - offset < sizeof(std::uint32_t)) {
    return std::nullopt;
  }
  std::uint32_t value = 0;
  std::memcpy(&value, bytes.data() + offset, sizeof(value));
  if constexpr (std::endian::native == std::endian::big) {
    value = std::byteswap(value);
  }
  return value;
}

// The program `offset` bytes from `bank`'s start (the original's 32-bit
// signed offset).
DataRef ProgramAt(DataRef bank, std::int32_t offset) {
  return static_cast<DataRef>(
      static_cast<std::uintptr_t>(bank) +
      static_cast<std::uintptr_t>(static_cast<std::intptr_t>(offset)));
}

EffectSelector SelectorOf(DataRef program) {
  return static_cast<EffectSelector>(
      static_cast<std::intptr_t>(static_cast<std::uintptr_t>(program)));
}

// The selector names only kNone; a built-in effect's id is carried here.
EffectSelector BuiltInSelector(std::int32_t effect_id) {
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  return static_cast<EffectSelector>(std::intptr_t{effect_id});
}

// The program of effect `request.effect_id`, a non-negative id, in `bank`;
// read without the original's count check, within the bank's memory.
std::optional<DataRef> BankProgram(SfxHost& host, DataRef bank,
                                   const SfxRequest& request) {
  const std::optional<std::uint32_t> offset = BankWord(
      host.BankBytes(bank), static_cast<std::size_t>(request.effect_id) + 1);
  if (!offset) {
    host.Refuse(SfxRefusal::kBankUnreadable, request);
    return std::nullopt;
  }
  return ProgramAt(bank, static_cast<std::int32_t>(*offset));
}

// vs_main_playSfx's private bank: the selected slot's. The original indexes
// the slots without checking the selection or the bank; both are refused.
std::optional<DataRef> SelectedBank(const SfxState& state, SfxHost& host,
                                    const SfxRequest& request) {
  if (state.selected_bank == 0) {
    host.Refuse(SfxRefusal::kNoSelectedBank, request);
    return std::nullopt;
  }
  if (state.selected_bank > kSfxBankSlotCount) {
    host.Refuse(SfxRefusal::kSelectedBankOutOfRange, request);
    return std::nullopt;
  }
  const DataRef bank = state.banks[state.selected_bank - 1];
  if (bank == DataRef::kNone || request.effect_id < 0) {
    host.Refuse(SfxRefusal::kBankUnavailable, request);
    return std::nullopt;
  }
  return bank;
}

// vs_main_playSfx's room bank: the room slot's. The original reads it
// without checking the bank or the id; both are refused.
std::optional<DataRef> RoomBank(const SfxState& state, SfxHost& host,
                                const SfxRequest& request) {
  const DataRef bank = state.banks[kRoomSfxBankSlot - 1];
  if (bank == DataRef::kNone) {
    host.Refuse(SfxRefusal::kNoRoomBank, request);
    return std::nullopt;
  }
  if (request.effect_id < 0) {
    host.Refuse(SfxRefusal::kRoomEffectNegative, request);
    return std::nullopt;
  }
  return bank;
}

// Plays a bank's program, on the group's next tag when it uses its tags.
void PlayBankProgram(SfxHost& host, DataRef program, std::int32_t& cursor,
                     std::int32_t group_tags, const SfxMix& mix) {
  const std::int32_t tag =
      host.ProgramUsesTags(program) ? NextTag(cursor, group_tags) : 0;
  PlaySfxProgramChannel(host, program, tag, mix);
}

void PlayPrivateSfx(SfxState& state, SfxHost& host, std::int32_t effect_id,
                    const SfxMix& mix) {
  const SfxRequest request{.group = kSfxBankEffectTags,
                           .effect_id = effect_id,
                           .bank = state.selected_bank};
  const std::optional<DataRef> bank = SelectedBank(state, host, request);
  if (!bank) return;
  const std::optional<DataRef> program = BankProgram(host, *bank, request);
  if (!program) return;
  PlayBankProgram(host, *program, state.cursors.group_ff000, kSfxBankEffectTags,
                  mix);
}

void PlayRoomSfx(SfxState& state, SfxHost& host, std::int32_t effect_id,
                 const SfxMix& mix) {
  const SfxRequest request{.group = kRoomSfxBankEffectTags,
                           .effect_id = effect_id,
                           .bank = kRoomSfxBankSlot};
  const std::optional<DataRef> bank = RoomBank(state, host, request);
  if (!bank) return;
  const std::optional<DataRef> program = BankProgram(host, *bank, request);
  if (!program) return;
  PlayBankProgram(host, *program, state.cursors.group_f00000,
                  kRoomSfxBankEffectTags, mix);
}

// func_80045B28's banked program, with the selection, the id and the offset
// checked.
std::optional<DataRef> ResolveBanked(const SfxState& state, SfxHost& host,
                                     SfxRequest request) {
  if (state.selected_bank == 0 || state.selected_bank > kSfxBankSlotCount) {
    host.Refuse(SfxRefusal::kUnresolvableBank, request);
    return std::nullopt;
  }
  const DataRef bank = state.banks[state.selected_bank - 1];
  if (bank == DataRef::kNone || request.effect_id < 0) {
    host.Refuse(SfxRefusal::kProgramUnavailable, request);
    return std::nullopt;
  }
  const std::span<const std::byte> bytes = host.BankBytes(bank);
  const std::optional<std::uint32_t> count = BankWord(bytes, 0);
  if (!count) {
    host.Refuse(SfxRefusal::kBankUnreadable, request);
    return std::nullopt;
  }
  const auto index = static_cast<std::uint32_t>(request.effect_id);
  if (index >= *count) {
    host.Refuse(SfxRefusal::kProgramUnavailable, request);
    return std::nullopt;
  }
  const std::optional<std::uint32_t> offset =
      BankWord(bytes, std::size_t{index} + 1);
  if (!offset) {
    host.Refuse(SfxRefusal::kBankUnreadable, request);
    return std::nullopt;
  }
  request.offset = static_cast<std::int32_t>(*offset);
  if (request.offset < 0 ||
      static_cast<std::uint64_t>(request.offset) <
          (std::uint64_t{*count} + 1) * sizeof(std::uint32_t)) {
    host.Refuse(SfxRefusal::kProgramOffsetInvalid, request);
    return std::nullopt;
  }
  return ProgramAt(bank, request.offset);
}

// The voices a stop or fade of `target` names: a banked effect's program, a
// built-in effect's id, or the whole group's tags when the id is 0; empty
// when the program's resolution was refused.
struct Selection {
  EffectSelector selector = EffectSelector::kNone;
  std::int32_t effect_tag_mask = 0;
};
std::optional<Selection> Select(const SfxState& state, SfxHost& host,
                                const SfxTarget& target) {
  if (target.effect_id == 0) return Selection{.effect_tag_mask = target.group};
  const std::optional<DataRef> program =
      ResolveSfxProgram(state, host, target.group, target.effect_id);
  if (!program) return std::nullopt;
  if (*program != DataRef::kNone) {
    return Selection{.selector = SelectorOf(*program)};
  }
  return Selection{.selector = BuiltInSelector(target.effect_id)};
}

void FadeTarget(const SfxState& state, SfxHost& host, SfxFade fade,
                const SfxTarget& target, const SfxFadeSpec& spec) {
  const std::optional<Selection> selection = Select(state, host, target);
  if (!selection) return;
  host.FadeSoundEffect(
      fade, selection->selector, selection->effect_tag_mask,
      {.target = spec.target, .ticks = spec.ticks & kSfxFadeTicksMask});
}

// func_800461CC's tag for group `group`: the next of group 0x180, 0x200 or
// 0xF00000; none for the shared group, or after the diagnostic for another.
std::int32_t BankTag(SfxState& state, SfxHost& host, std::int32_t group) {
  switch (group) {
    case kEffectTagGroup180:
      return NextTag(state.cursors.group_180, kEffectTagGroup180);
    case kEffectTagGroup200:
      return NextTag(state.cursors.group_200, kEffectTagGroup200);
    case kRoomSfxBankEffectTags:
      return NextTag(state.cursors.group_f00000, kRoomSfxBankEffectTags);
    case kSharedEffectTags:
      return 0;
    default:
      host.Diagnose(kUnknownSfxGroupDiagnostic);
      return 0;
  }
}

// func_8004644C and func_80046494's voices: the bank effect's program, or the
// group's tags when the index is 0. The original reads the offset for any
// index; a read outside the bank's memory is refused.
std::optional<Selection> SelectBankEffect(SfxHost& host, std::int32_t group,
                                          const BankSfx& effect) {
  if (effect.index == 0) return Selection{.effect_tag_mask = group};
  const SfxRequest request{.group = group, .effect_id = effect.index};
  if (effect.index < 0) {
    host.Refuse(SfxRefusal::kBankUnreadable, request);
    return std::nullopt;
  }
  const std::optional<DataRef> program =
      BankProgram(host, effect.bank, request);
  if (!program) return std::nullopt;
  return Selection{.selector = SelectorOf(*program)};
}
}  // namespace

// decomp: SLUS_010.40 func_80045630 (src/SLUS_010.40/main.c:9785)
void PlaySfxOnPrimaryChannel(SfxState& state, SfxHost& host,
                             std::int32_t effect_id) {
  host.PlaySoundEffect(effect_id & kBuiltInSfxIdMask,
                       NextTag(state.cursors.group_1, kPrimaryEffectTags),
                       SfxMix{});
}

// decomp: SLUS_010.40 func_80045684 (src/SLUS_010.40/main.c:9794)
void PlayBuiltInSfxChannel(SfxHost& host, std::int32_t effect_id,
                           std::int32_t effect_tag, const SfxMix& mix) {
  if (effect_tag == kEffectTagGroup200 &&
      host.SoundEffectIdForTag(kEffectTagGroup200) ==
          BuiltInSelector(effect_id)) {
    return;
  }
  host.PlaySoundEffect(effect_id, effect_tag, mix);
}

// decomp: SLUS_010.40 func_800456EC (src/SLUS_010.40/main.c:9805)
void PlaySfxProgramChannel(SfxHost& host, DataRef program,
                           std::int32_t effect_tag, const SfxMix& mix) {
  if (effect_tag == kEffectTagGroup200 &&
      host.SoundEffectIdForTag(kEffectTagGroup200) == SelectorOf(program)) {
    return;
  }
  host.PlaySoundEffectProgram(program, effect_tag, mix);
}

// decomp: SLUS_010.40 vs_main_playSfx (src/SLUS_010.40/main.c:9816)
// Groups 0x100 and 0x200 play on group 0x200's current tag without advancing
// it, as the original does.
void PlaySfx(SfxState& state, SfxHost& host, std::int32_t group,
             std::int32_t effect_id, const SfxMix& mix) {
  const std::int32_t built_in = effect_id & kBuiltInSfxIdMask;
  switch (group) {
    case kSharedEffectTags:
    case kSharedEffectGroupAlias:
      PlayBuiltInSfxChannel(
          host, built_in,
          host.SharedEffectUsesTags(built_in)
              ? NextTag(state.cursors.group_7e, kSharedEffectTags)
              : 0,
          mix);
      return;
    case kEffectTagGroup100:
    case kEffectTagGroup200:
      PlayBuiltInSfxChannel(
          host, built_in,
          host.SharedEffectUsesTags(built_in) ? state.cursors.group_200 : 0,
          mix);
      return;
    case kSfxBankEffectTags:
      PlayPrivateSfx(state, host, effect_id, mix);
      return;
    case kRoomSfxBankEffectTags:
      PlayRoomSfx(state, host, effect_id, mix);
      return;
    default:
      host.Diagnose(kUnknownSfxGroupDiagnostic);
      return;
  }
}

// decomp: SLUS_010.40 vs_main_playSfxDefault (src/SLUS_010.40/main.c:9877)
void PlaySfxCentered(SfxState& state, SfxHost& host, std::int32_t group,
                     std::int32_t effect_id) {
  PlaySfx(state, host, group, effect_id, SfxMix{});
}

// decomp: SLUS_010.40 vs_main_computeSfxPan (src/SLUS_010.40/main.c:9879)
SfxMix SfxPanAndVolume(const SfxProjection& projection) {
  const std::int32_t x =
      static_cast<std::int16_t>(projection.screen_xy) - kScreenCenterX;
  const std::int32_t offset = std::clamp(
      x * kPanScaleNumerator / kPanScaleDenominator, -kPanSwing, kPanSwing);
  // The original's 32-bit wrapping subtraction.
  const auto past_audible = static_cast<std::int32_t>(
      static_cast<std::uint32_t>(projection.depth) - kAudibleDepth);
  const std::int32_t depth = std::clamp(past_audible, 0, kDepthRange);
  return {.pan = offset + kCenteredSfxPan,
          .volume = kFullSfxVolume - (depth >> kDepthShift)};
}

// decomp: SLUS_010.40 _do_panSfx (src/SLUS_010.40/main.c:9927)
void PlayProjectedSfx(SfxState& state, SfxHost& host, std::int32_t group,
                      std::int32_t effect_id, const SfxProjection& projection) {
  const SfxMix mix = SfxPanAndVolume(projection);
  if (mix.volume > 0) PlaySfx(state, host, group, effect_id, mix);
}

// decomp: SLUS_010.40 vs_main_panSfx (src/SLUS_010.40/main.c:9937)
void PlayWorldSfx(SfxState& state, SfxHost& host, std::int32_t group,
                  std::int32_t effect_id, const SfxPoint& point) {
  PlayProjectedSfx(state, host, group, effect_id, host.Project(point));
}

// decomp: SLUS_010.40 func_80045B28 (src/SLUS_010.40/main.c:9950)
// Both banked groups resolve within the selected slot's bank, as the
// original does; the original reads it without checks.
std::optional<DataRef> ResolveSfxProgram(const SfxState& state, SfxHost& host,
                                         std::int32_t group,
                                         std::int32_t effect_id) {
  switch (group) {
    case kPrimaryEffectTags:
    case kSharedEffectTags:
    case kSharedEffectGroupAlias:
    case kEffectTagGroup100:
    case kEffectTagGroup200:
      return DataRef::kNone;
    case kSfxBankEffectTags:
    case kRoomSfxBankEffectTags:
      return ResolveBanked(state, host,
                           {.group = group,
                            .effect_id = effect_id,
                            .bank = state.selected_bank});
    default:
      host.Diagnose(kUnknownSfxGroupDiagnostic);
      return DataRef::kNone;
  }
}

// decomp: SLUS_010.40 func_80045BFC (src/SLUS_010.40/main.c:9975)
void FadeSfxVolume(const SfxState& state, SfxHost& host,
                   const SfxTarget& target, const SfxFadeSpec& spec) {
  FadeTarget(state, host, SfxFade::kVolume, target, spec);
}

// decomp: SLUS_010.40 func_80045C74 (src/SLUS_010.40/main.c:9994)
void FadeSfxPitch(const SfxState& state, SfxHost& host, const SfxTarget& target,
                  const SfxFadeSpec& spec) {
  FadeTarget(state, host, SfxFade::kPitch, target, spec);
}

// decomp: SLUS_010.40 func_80045CEC (src/SLUS_010.40/main.c:10012)
void FadeSfxPan(const SfxState& state, SfxHost& host, const SfxTarget& target,
                const SfxFadeSpec& spec) {
  FadeTarget(state, host, SfxFade::kPan, target, spec);
}

// decomp: SLUS_010.40 func_80045D64 (src/SLUS_010.40/main.c:10028)
void StopSfx(const SfxState& state, SfxHost& host, const SfxTarget& target) {
  const std::optional<Selection> selection = Select(state, host, target);
  if (!selection) return;
  host.StopSoundEffects(selection->selector, selection->effect_tag_mask);
}

// decomp: SLUS_010.40 func_80045DC0 (src/SLUS_010.40/main.c:10043)
void StopAllSfxPlayback(SfxHost& host) { host.StopSceneSoundEffects(); }

// decomp: SLUS_010.40 func_800461CC (src/SLUS_010.40/main.c:10178)
// The index compares unsigned with the bank's count, as the original's does.
void PlayBankSfx(SfxState& state, SfxHost& host, std::int32_t group,
                 const BankSfx& effect, const SfxMix& mix) {
  const SfxRequest request{.group = group, .effect_id = effect.index};
  const std::span<const std::byte> bytes = host.BankBytes(effect.bank);
  const std::optional<std::uint32_t> count = BankWord(bytes, 0);
  if (!count) {
    host.Refuse(SfxRefusal::kBankUnreadable, request);
    return;
  }
  const auto index = static_cast<std::uint32_t>(effect.index);
  if (index >= *count) return;
  const std::optional<std::uint32_t> offset =
      BankWord(bytes, std::size_t{index} + 1);
  if (!offset) {
    host.Refuse(SfxRefusal::kBankUnreadable, request);
    return;
  }
  const DataRef program =
      ProgramAt(effect.bank, static_cast<std::int32_t>(*offset));
  PlaySfxProgramChannel(host, program, BankTag(state, host, group), mix);
}

// decomp: SLUS_010.40 func_800462E8 (src/SLUS_010.40/main.c:10215)
void PlayBankSfxCentered(SfxState& state, SfxHost& host, std::int32_t group,
                         const BankSfx& effect) {
  PlayBankSfx(state, host, group, effect, SfxMix{});
}

// decomp: SLUS_010.40 func_80046310 (src/SLUS_010.40/main.c:10220)
// The screen x is the projection's remainder by 0x10000, as the original's
// is: a point above the screen's top edge takes the left edge's pan.
void PlayProjectedBankSfx(SfxState& state, SfxHost& host, std::int32_t group,
                          const BankSfx& effect,
                          const SfxProjection& projection) {
  const std::int32_t x =
      std::clamp(projection.screen_xy % kHalfwordModulus, 0, kScreenWidth);
  const std::int32_t depth = std::clamp(projection.depth, 0, kBankDepthRange);
  const SfxMix mix{.pan = (x / 2) + kBankPanLeft,
                   .volume = kFullSfxVolume - (depth >> kBankDepthShift)};
  if (mix.volume > 0) PlayBankSfx(state, host, group, effect, mix);
}

// decomp: SLUS_010.40 func_800463BC (src/SLUS_010.40/main.c:10272)
void PlayWorldBankSfx(SfxState& state, SfxHost& host, std::int32_t group,
                      const BankSfx& effect, const SfxPoint& point) {
  PlayProjectedBankSfx(state, host, group, effect, host.Project(point));
}

// decomp: SLUS_010.40 func_8004644C (src/SLUS_010.40/main.c:10285)
void StopBankSfx(SfxHost& host, std::int32_t group, const BankSfx& effect) {
  const std::optional<Selection> selection =
      SelectBankEffect(host, group, effect);
  if (!selection) return;
  host.StopSoundEffects(selection->selector, selection->effect_tag_mask);
}

// decomp: SLUS_010.40 func_80046494 (src/SLUS_010.40/main.c:10299)
void FadeBankSfxVolume(SfxHost& host, std::int32_t group, const BankSfx& effect,
                       const SfxFadeSpec& spec) {
  const std::optional<Selection> selection =
      SelectBankEffect(host, group, effect);
  if (!selection) return;
  host.FadeSoundEffect(
      SfxFade::kVolume, selection->selector, selection->effect_tag_mask,
      {.target = spec.target, .ticks = spec.ticks & kSfxFadeTicksMask});
}
}  // namespace VS::Audio
