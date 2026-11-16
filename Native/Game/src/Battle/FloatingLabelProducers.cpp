// Hand-maintained (tools/port/owned.csv).
#include "Battle/FloatingLabelProducers.hpp"

#include <array>
#include <bit>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace VS::Battle {
namespace {
constexpr std::uint32_t kNegativeAmountShift = 16;
constexpr std::uint8_t kTimingResultActor = 0;
constexpr std::uint8_t kChainCountActor = 0;

[[nodiscard]] bool IsActorLabel(const FloatingLabelRecord& label,
                                FloatingLabelKind kind,
                                std::int32_t actor_slot) {
  return label.kind == static_cast<std::uint8_t>(kind) &&
         static_cast<std::int32_t>(label.actor) == actor_slot;
}

[[nodiscard]] bool IsSharedLabel(const FloatingLabelRecord& label,
                                 FloatingLabelKind kind) {
  return label.kind == static_cast<std::uint8_t>(kind) && label.actor == 0;
}

// The anchor must project inside [0, kScreenRight] x [0, kScreenBottom]; a
// negative coordinate is a large unsigned one.
[[nodiscard]] bool OnScreen(const LabelPoint& screen) {
  return std::bit_cast<std::uint16_t>(screen.x) <= kScreenRight &&
         std::bit_cast<std::uint16_t>(screen.y) <= kScreenBottom;
}

[[nodiscard]] std::int16_t Minus(std::int16_t value, std::int16_t subtrahend) {
  return std::bit_cast<std::int16_t>(
      static_cast<std::uint16_t>(std::bit_cast<std::uint16_t>(value) -
                                 std::bit_cast<std::uint16_t>(subtrahend)));
}

// sprintf("%d") of the shown amount, NUL included; the bytes past the NUL
// keep their contents. Returns the strlen.
[[nodiscard]] std::uint8_t PrintAmount(
    std::uint16_t amount, std::array<std::uint8_t, kLabelTextBytes>& text) {
  std::array<char, kLabelTextBytes> digits{};
  const auto printed = std::to_chars(digits.data(), &digits.back(), amount);
  const auto length = static_cast<std::size_t>(printed.ptr - digits.data());
  for (std::size_t index = 0; index < length; ++index) {
    text.at(index) = static_cast<std::uint8_t>(digits.at(index));
  }
  text.at(length) = 0;
  return static_cast<std::uint8_t>(length);
}

// Removes the first published label `matches` accepts. `reload_count` reads
// the count before each comparison (F00715) instead of once (F00716).
template <typename Matches>
bool RemoveFirstLabel(FloatingLabelProducerWorld& world, bool reload_count,
                      Matches matches) {
  const std::int16_t count = world.LabelCount();
  for (std::int32_t index = 0;
       index < (reload_count ? world.LabelCount() : count); ++index) {
    const auto label = world.Label(index);
    if (!label) return false;
    if (matches(*label)) return world.RemoveLabel(index);
  }
  return true;
}

struct ValueLabelStack {
  std::int16_t order = 0;
  std::uint8_t frame = 0;
  std::uint8_t repeated = 0;
};

// The actor's stacked value labels and whether the newest of them is on
// `limb` (the original's two scans).
std::optional<ValueLabelStack> ScanValueLabels(
    FloatingLabelProducerWorld& world, std::int32_t actor_slot,
    std::int32_t limb) {
  ValueLabelStack stack;
  std::int32_t stacked = 0;
  const std::int16_t first_count = world.LabelCount();
  for (std::int32_t index = 0; index < first_count; ++index) {
    const auto label = world.Label(index);
    if (!label) return std::nullopt;
    if (IsActorLabel(*label, FloatingLabelKind::kActorValue, actor_slot) &&
        label->position.w >= 0) {
      stack.frame = label->frame;
      ++stacked;
    }
  }
  const std::int16_t second_count = world.LabelCount();
  for (std::int32_t index = 0; index < second_count; ++index) {
    const auto label = world.Label(index);
    if (!label) return std::nullopt;
    if (IsActorLabel(*label, FloatingLabelKind::kActorValue, actor_slot) &&
        static_cast<std::int32_t>(label->limb) == limb &&
        label->position.w == stacked - 1) {
      stack.repeated = kRepeatedLimbFlag;
      break;
    }
  }
  stack.order = static_cast<std::int16_t>(stacked);
  return stack;
}
}  // namespace

std::uint16_t EncodeLabelValue(std::uint32_t encoded_amount) {
  const std::uint32_t amount = encoded_amount & kLabelAmountMask;
  const auto sign = static_cast<std::uint16_t>(
      (encoded_amount >> kNegativeAmountShift) & kLabelNegativeBit);
  const auto shown = static_cast<std::uint16_t>(
      amount > kMaxLabelValue ? kMaxLabelValue : amount);
  return static_cast<std::uint16_t>(shown | sign);
}

// decomp: func_80093B68 (src/BATTLE/BATTLE.PRG/2842C.c:1544)
std::optional<std::int32_t> QueueActorValueLabel(
    std::int32_t actor_slot, std::int32_t limb_tag,
    std::uint32_t encoded_amount, std::int32_t label_flags,
    FloatingLabelProducerWorld& world) {
  std::int32_t limb = kTargetPointSelector;
  if (limb_tag != kNoLimbTag) {
    limb = world.FindBoneTag(actor_slot, limb_tag, kValueLabelBoneTagKind);
    if (limb < 0) return kLabelBoneMissing;
  }
  const auto stack = ScanValueLabels(world, actor_slot, limb);
  if (!stack) return std::nullopt;
  const auto reserved = world.ReserveLabel();
  if (!reserved) return std::nullopt;
  FloatingLabelRecord record = *reserved;
  // A selector equal to the target point uses it even when a tag found it.
  const std::int32_t status =
      limb != kTargetPointSelector
          ? world.BonePosition(actor_slot, limb, kValueLabelEndpoint,
                               record.position)
          : world.TaggedBonePosition(actor_slot, kTargetPointSelector,
                                     kValueLabelEndpoint, record.position);
  // The callee wrote into the reserved record itself, found or not.
  if (!world.WriteReservedLabel(record)) return std::nullopt;
  if (status != 0) return kLabelBoneMissing;
  LabelPoint anchor{};
  if ((label_flags & kRequireOnScreenFlag) != 0) {
    if (world.ProjectTaggedBone(actor_slot, kLabelAnchorTag, 0, anchor) != 0 ||
        !OnScreen(anchor)) {
      return 0;
    }
  }
  if (world.TaggedBonePosition(actor_slot, kLabelAnchorTag, 0, anchor) != 0) {
    return kLabelBoneMissing;
  }
  const std::uint16_t value = EncodeLabelValue(encoded_amount);
  record.scale_x = kLabelUnitScale;
  record.scale_y = kLabelUnitScale;
  record.position.x = Minus(record.position.x, anchor.x);
  record.position.y = Minus(record.position.y, anchor.y);
  record.position.z = Minus(record.position.z, anchor.z);
  record.kind = static_cast<std::uint8_t>(FloatingLabelKind::kActorValue);
  record.position.w = stack->order;
  record.frame = stack->frame;
  record.actor = static_cast<std::uint8_t>(actor_slot);
  record.limb = static_cast<std::uint8_t>(limb);
  record.color_flags = static_cast<std::uint8_t>(
      static_cast<std::uint32_t>(label_flags & kLabelColorMask) |
      stack->repeated);
  record.value = value;
  record.text_length = PrintAmount(
      static_cast<std::uint16_t>(value & kLabelAmountMask), record.text);
  if (!world.WriteReservedLabel(record) || !world.CommitReservedLabel()) {
    return std::nullopt;
  }
  return 0;
}

// decomp: vs_battle_renderBattleAbilityTimingResult
// (src/BATTLE/BATTLE.PRG/2842C.c:1643)
std::optional<std::int32_t> QueueAbilityTimingResult(
    std::int32_t timing_result, FloatingLabelProducerWorld& world) {
  if (!RemoveFirstLabel(world, true, [](const FloatingLabelRecord& label) {
        return IsSharedLabel(label, FloatingLabelKind::kAbilityTimingResult);
      })) {
    return std::nullopt;
  }
  const auto reserved = world.ReserveLabel();
  if (!reserved) return std::nullopt;
  FloatingLabelRecord record = *reserved;
  record.frame = 0;
  record.kind =
      static_cast<std::uint8_t>(FloatingLabelKind::kAbilityTimingResult);
  record.actor = kTimingResultActor;
  record.value = static_cast<std::uint16_t>(timing_result);
  if (!world.WriteReservedLabel(record)) return std::nullopt;
  // rand() runs after the fields above are stored; only the text length
  // follows it.
  const auto jitter =
      static_cast<std::uint8_t>(world.Random() & kTimingJitterMask);
  if (!world.WriteReservedTextLength(jitter) || !world.CommitReservedLabel()) {
    return std::nullopt;
  }
  return 0;
}

// decomp: func_80093F24 (src/BATTLE/BATTLE.PRG/2842C.c:1666)
std::optional<std::int32_t> QueueChainCount(std::int32_t chain_count,
                                            FloatingLabelProducerWorld& world) {
  if (!RemoveFirstLabel(world, false, [](const FloatingLabelRecord& label) {
        return IsSharedLabel(label, FloatingLabelKind::kChainCount);
      })) {
    return std::nullopt;
  }
  if (chain_count == 0) return 0;
  const auto reserved = world.ReserveLabel();
  if (!reserved) return std::nullopt;
  FloatingLabelRecord record = *reserved;
  record.kind = static_cast<std::uint8_t>(FloatingLabelKind::kChainCount);
  record.frame = 0;
  record.actor = kChainCountActor;
  record.value = static_cast<std::uint16_t>(chain_count);
  if (record.value > kMaxLabelValue) record.value = kMaxLabelValue;
  if (!world.WriteReservedLabel(record) || !world.CommitReservedLabel()) {
    return std::nullopt;
  }
  return 0;
}

// decomp: func_80093FEC (src/BATTLE/BATTLE.PRG/2842C.c:1695)
std::optional<std::int32_t> QueueActorStatusLabels(
    std::int32_t actor_slot, std::int32_t status_mask, std::int32_t label_flags,
    FloatingLabelProducerWorld& world) {
  const auto mask = static_cast<std::uint32_t>(status_mask);
  for (std::int32_t index = 0; index < kStatusLabelBits; ++index) {
    if ((mask & (1U << static_cast<std::uint32_t>(index))) == 0) continue;
    if (!QueueActorStatusLabel(actor_slot, kLabelAnchorTag, index, label_flags,
                               world)) {
      return std::nullopt;
    }
  }
  return 0;
}

// decomp: func_8009406C (src/BATTLE/BATTLE.PRG/2842C.c:1708)
std::optional<std::int32_t> QueueActorStatusLabel(
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters): original order.
    std::int32_t actor_slot, std::int32_t bone_tag, std::int32_t status_index,
    std::int32_t label_flags, FloatingLabelProducerWorld& world) {
  std::int32_t found = 0;
  std::int32_t follow_frame = 0;
  std::uint8_t frame = 0;
  const std::int16_t count = world.LabelCount();
  for (std::int32_t index = 0; index < count; ++index) {
    const auto label = world.Label(index);
    if (!label) return std::nullopt;
    if (!IsActorLabel(*label, FloatingLabelKind::kActorStatus, actor_slot)) {
      continue;
    }
    follow_frame = label->text_length + kStatusFrameLead;
    if (label->position.w >= 0) {
      frame = label->frame;
      ++found;
    }
  }
  const auto reserved = world.ReserveLabel();
  if (!reserved) return std::nullopt;
  if ((label_flags & kRequireOnScreenFlag) != 0) {
    LabelPoint screen{};
    if (world.ProjectTaggedBone(actor_slot, kLabelAnchorTag, 0, screen) != 0 ||
        !OnScreen(screen)) {
      return 0;
    }
  }
  FloatingLabelRecord record = *reserved;
  record.kind = static_cast<std::uint8_t>(FloatingLabelKind::kActorStatus);
  record.position = {0, 0, 0, static_cast<std::int16_t>(found)};
  record.text_length = static_cast<std::uint8_t>(
      follow_frame < kStatusFrameLimit ? follow_frame : 0);
  record.frame = frame;
  record.actor = static_cast<std::uint8_t>(actor_slot);
  record.limb = static_cast<std::uint8_t>(bone_tag);
  record.color_flags = static_cast<std::uint8_t>(label_flags & kLabelColorMask);
  record.value = static_cast<std::uint16_t>(status_index);
  if (!world.WriteReservedLabel(record) || !world.CommitReservedLabel()) {
    return std::nullopt;
  }
  return 0;
}
}  // namespace VS::Battle
