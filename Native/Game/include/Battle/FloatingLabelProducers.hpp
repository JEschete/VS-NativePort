#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstdint>
#include <optional>

// The floating label producers (BATTLE.PRG 2842C.c:1544-1755): actor value
// numbers, ability timing results, the chain counter and actor status labels.
// Each scans the published labels, reserves a record through the floating
// label queue (FloatingLabelQueue.hpp, func_800962E4), writes the fields its
// kind uses and publishes it. A record field a producer does not write keeps
// whatever the reserved record held, as in the original.
namespace VS::Battle {

// VS::Battle::FloatingLabel.key.bytes[0] of the labels these producers write.
enum class FloatingLabelKind : std::uint8_t {
  kInactive = 0,
  kActorValue = 1,
  kActorStatus = 2,
  kWorldLocked = 3,
  kAbilityTimingResult = 4,
  kChainCount = 5,
};

// An SVECTOR: a camera-relative bone position or a projected screen point
// (x, y screen, z depth). `w` is its fourth halfword.
struct LabelPoint {
  std::int16_t x{}, y{}, z{}, w{};
  friend bool operator==(const LabelPoint&, const LabelPoint&) = default;
};

// The text bytes a label record carries (a printed amount and its NUL).
inline constexpr std::size_t kLabelTextBytes = 4;

// One VS::Battle::FloatingLabel record (24 bytes, original field order).
struct FloatingLabelRecord {
  // position.w is the label's stacking order (status and value labels) or
  // its waiting steps (the queue's advance).
  LabelPoint position{};
  std::uint8_t kind{};
  std::uint8_t actor{};
  std::uint8_t limb{};
  std::uint8_t frame{};
  std::uint16_t value{};
  std::uint8_t color_flags{};
  std::uint8_t text_length{};
  std::int16_t scale_x{};
  std::int16_t scale_y{};
  std::array<std::uint8_t, kLabelTextBytes> text{};
  friend bool operator==(const FloatingLabelRecord&,
                         const FloatingLabelRecord&) = default;
};

// The bone tag argument that asks for the actor's target point rather than a
// tagged limb (F00714's limb tag), and the target point selector itself.
inline constexpr std::int32_t kNoLimbTag = -1;
inline constexpr std::int32_t kTargetPointSelector = 0xFF;
// The label anchor bone tag the producers measure from and project.
inline constexpr std::int32_t kLabelAnchorTag = 0xFB;
// The bone tag kind (func_800A152C) a value label's limb tag names.
inline constexpr std::int32_t kValueLabelBoneTagKind = 2;
// The endpoint the value label's bone positions use.
inline constexpr std::int32_t kValueLabelEndpoint = 2;
// label_flags bit asking for an on-screen anchor; the rest are color flags.
inline constexpr std::int32_t kRequireOnScreenFlag = 0x80;
inline constexpr std::int32_t kLabelColorMask = 0x7F;
// color_flags bit set when the value label follows the previous one on the
// same limb.
inline constexpr std::uint8_t kRepeatedLimbFlag = 0x80;
// The last on-screen x and y the producers accept.
inline constexpr std::uint16_t kScreenRight = 320;
inline constexpr std::uint16_t kScreenBottom = 240;
// The returned status for a missing bone or anchor.
inline constexpr std::int32_t kLabelBoneMissing = -2;
// The largest displayed value, the amount mask and its sign bits.
inline constexpr std::uint16_t kMaxLabelValue = 999;
inline constexpr std::uint32_t kLabelAmountMask = 0x7FFF;
inline constexpr std::uint16_t kLabelNegativeBit = 0x8000;
// Status labels: the status bits a mask names, the frame lead of a follower
// and the first frame that restarts it.
inline constexpr std::int32_t kStatusLabelBits = 29;
inline constexpr std::int32_t kStatusFrameLead = 12;
inline constexpr std::int32_t kStatusFrameLimit = 0x25;
// The random text-length bits a timing result keeps.
inline constexpr std::int32_t kTimingJitterMask = 7;
// The scale a value label starts at (ONE).
inline constexpr std::int16_t kLabelUnitScale = 0x1000;

// The label queue and the skeleton and random services the producers use.
// The queue methods return false (or nullopt) when the queue refuses.
class FloatingLabelProducerWorld {
 public:
  virtual ~FloatingLabelProducerWorld() = default;
  // VS::Battle::floatingLabelCount.
  [[nodiscard]] virtual std::int16_t LabelCount() = 0;
  // The published record `index`; nullopt outside the queue's records.
  [[nodiscard]] virtual std::optional<FloatingLabelRecord> Label(
      std::int32_t index) = 0;
  // func_80096444: removes the record at `index`.
  virtual bool RemoveLabel(std::int32_t index) = 0;
  // func_800962E4: reserves a record and returns what it holds.
  [[nodiscard]] virtual std::optional<FloatingLabelRecord> ReserveLabel() = 0;
  // Stores the whole reserved record, or only its text length.
  virtual bool WriteReservedLabel(const FloatingLabelRecord& record) = 0;
  virtual bool WriteReservedTextLength(std::uint8_t text_length) = 0;
  // Publishes the reserved record (++VS::Battle::floatingLabelCount).
  virtual bool CommitReservedLabel() = 0;
  // func_800A152C: the bone whose label of `kind` is `tag`, or negative.
  [[nodiscard]] virtual std::int32_t FindBoneTag(std::int32_t actor,
                                                 std::int32_t tag,
                                                 std::int32_t kind) = 0;
  // func_800A1AF8, func_800A190C and func_800A1280. Each writes `point` as
  // the original callee writes its output and returns its status (0 found).
  virtual std::int32_t BonePosition(std::int32_t actor, std::int32_t bone,
                                    std::int32_t endpoint,
                                    LabelPoint& point) = 0;
  virtual std::int32_t TaggedBonePosition(std::int32_t actor, std::int32_t tag,
                                          std::int32_t endpoint,
                                          LabelPoint& point) = 0;
  virtual std::int32_t ProjectTaggedBone(std::int32_t actor, std::int32_t tag,
                                         std::int32_t endpoint,
                                         LabelPoint& point) = 0;
  // rand().
  [[nodiscard]] virtual std::int32_t Random() = 0;

 protected:
  FloatingLabelProducerWorld() = default;
  FloatingLabelProducerWorld(const FloatingLabelProducerWorld&) = default;
  FloatingLabelProducerWorld& operator=(const FloatingLabelProducerWorld&) =
      default;
  FloatingLabelProducerWorld(FloatingLabelProducerWorld&&) = default;
  FloatingLabelProducerWorld& operator=(FloatingLabelProducerWorld&&) = default;
};

// The 16-bit value a value label shows: the amount's low 15 bits capped at
// 999, with bit 31 of `encoded_amount` as the negative bit.
[[nodiscard]] std::uint16_t EncodeLabelValue(std::uint32_t encoded_amount);

// func_80093B68 (2842C.c:1544): the value label at `actor_slot`'s limb with
// `limb_tag` (kNoLimbTag for the target point), stacked over the actor's
// earlier value labels and positioned relative to the label anchor. Returns
// 0, or kLabelBoneMissing for a missing limb or anchor; with
// kRequireOnScreenFlag an off-screen anchor queues nothing and returns 0.
// nullopt when the queue refuses.
std::optional<std::int32_t> QueueActorValueLabel(
    std::int32_t actor_slot, std::int32_t limb_tag,
    std::uint32_t encoded_amount, std::int32_t label_flags,
    FloatingLabelProducerWorld& world);

// vs_battle_renderBattleAbilityTimingResult (2842C.c:1643): replaces the
// first timing result label with `timing_result`. Returns 0.
std::optional<std::int32_t> QueueAbilityTimingResult(
    std::int32_t timing_result, FloatingLabelProducerWorld& world);

// func_80093F24 (2842C.c:1666): replaces the first chain counter label with
// `chain_count` (capped at 999); 0 only removes it. Returns 0.
std::optional<std::int32_t> QueueChainCount(std::int32_t chain_count,
                                            FloatingLabelProducerWorld& world);

// func_80093FEC (2842C.c:1695): one status label per set bit 0-28 of
// `status_mask`, lowest bit first, at the label anchor. Returns 0.
std::optional<std::int32_t> QueueActorStatusLabels(
    std::int32_t actor_slot, std::int32_t status_mask, std::int32_t label_flags,
    FloatingLabelProducerWorld& world);

// func_8009406C (2842C.c:1708): the status label `status_index` at
// `actor_slot`'s `bone_tag`, following the actor's earlier status labels.
// With kRequireOnScreenFlag an off-screen anchor queues nothing. Returns 0.
std::optional<std::int32_t> QueueActorStatusLabel(
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters): original order.
    std::int32_t actor_slot, std::int32_t bone_tag, std::int32_t status_index,
    std::int32_t label_flags, FloatingLabelProducerWorld& world);
}  // namespace VS::Battle
