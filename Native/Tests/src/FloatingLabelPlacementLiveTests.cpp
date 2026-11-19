#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Battle/FloatingLabelPlacementLive.hpp"
#include "Field/RoomRenderingLiveState.hpp"
#include "Input/ControllerLiveState.hpp"
#include "State/Timers.hpp"
#include "TestSupport/FloatingLabelRenderingLiveFixture.hpp"

// The generated F00719-F00742 bodies are retired. Each parity case runs the
// Live entry over the inputs the generated body ran and compares a digest of
// every result with the digest the generated body produced.
namespace {
namespace b = VS::Battle;
namespace f = VS::test_support::floating_label_rendering;
constexpr int kX = 100;
constexpr int kY = 100;
constexpr std::size_t kEquipmentBucket = f::kSceneHead + 2;
constexpr std::size_t kPlacedCapacity = 32;
constexpr std::uint8_t kEquipmentKind = 6;
constexpr std::uint8_t kPendingKind = 7;
constexpr std::int16_t kSeededLabels = 6;
constexpr u_char kOversizedText = 5;

// Digests of the generated bodies over the inputs below, recorded while they
// still ran (generated BATTLE/BATTLE.PRG Feedback.cpp at 3452e95db, #276).
constexpr std::uint64_t kValueDigitsDigest = 0x87054A21D3647511ULL;
constexpr std::uint64_t kStatusCaptionsDigest = 0xB5BEF1461E8FDCB9ULL;
constexpr std::uint64_t kStatChangeRowsDigest = 0x9A9B970E0E8EF84AULL;
constexpr std::uint64_t kPlacementDigest = 0x479814208778490DULL;
constexpr std::uint64_t kEquipmentQueueDigest = 0x41F945FFD2FC2FF6ULL;
constexpr std::uint64_t kUpdateDigest = 0x7B15AD159C3767D1ULL;

struct Capture {
  std::vector<f::Packet> quads;
  std::vector<f::Packet> text;
  std::vector<f::Packet> equipment;
  std::size_t used = 0;
};

struct Placement {
  std::array<DVECTOR, kPlacedCapacity> placed{};
  std::array<std::int16_t, 2> position{};
  std::int16_t count = 0;
  f::LabelWords label{};
};

struct QueueState {
  std::array<f::LabelWords, f::kLabelCapacity> labels{};
  std::int16_t count = 0;
};

struct UpdateResult {
  QueueState queue;
  Capture draws;
  std::array<std::int16_t, 3> timers{};
};

// FNV-1a 64 over each word's bytes, low byte first, with every sequence's
// length mixed in ahead of its contents.
class Digest {
 public:
  void Add(std::uint32_t word) {
    for (unsigned shift = 0; shift < kWordBits; shift += kByteBits) {
      hash_ = (hash_ ^ ((word >> shift) & kByteMask)) * kPrime;
    }
  }
  void AddHalf(std::int16_t half) {
    Add(static_cast<std::uint32_t>(static_cast<std::uint16_t>(half)));
  }
  void Add(std::span<const std::uint32_t> words) {
    Add(static_cast<std::uint32_t>(words.size()));
    for (const auto word : words) {
      Add(word);
    }
  }
  void Add(const std::vector<f::Packet>& packets) {
    Add(static_cast<std::uint32_t>(packets.size()));
    for (const auto& packet : packets) {
      Add(std::span<const std::uint32_t>(packet));
    }
  }
  void Add(const Capture& capture) {
    Add(capture.quads);
    Add(capture.text);
    Add(capture.equipment);
    Add(static_cast<std::uint32_t>(capture.used));
  }
  void Add(const Placement& placement) {
    using PlacedWords = std::array<std::uint32_t, kPlacedCapacity>;
    Add(std::bit_cast<PlacedWords>(placement.placed));
    Add(std::bit_cast<std::uint32_t>(placement.position));
    AddHalf(placement.count);
    Add(placement.label);
  }
  void Add(const QueueState& queue) {
    for (const auto& label : queue.labels) {
      Add(label);
    }
    AddHalf(queue.count);
  }
  void Add(const UpdateResult& result) {
    Add(result.queue);
    Add(result.draws);
    for (const auto timer : result.timers) {
      AddHalf(timer);
    }
  }
  [[nodiscard]] std::uint64_t Value() const { return hash_; }

 private:
  static constexpr unsigned kWordBits = 32;
  static constexpr unsigned kByteBits = 8;
  static constexpr std::uint64_t kByteMask = 0xFF;
  static constexpr std::uint64_t kOffsetBasis = 0xCBF29CE484222325ULL;
  static constexpr std::uint64_t kPrime = 0x100000001B3ULL;
  std::uint64_t hash_ = kOffsetBasis;
};

// Runs one draw on freshly seeded frame storage and keeps every bucket the
// label leaves submit to.
Capture Run(f::LiveFrame& frame, const std::function<void()>& draw) {
  std::ranges::fill(frame.primitives, f::kStorageSeed);
  frame.ClearDraws();
  draw();
  return {frame.Traversal(f::kQuadBucket), frame.Traversal(f::kTextBucket),
          frame.Traversal(kEquipmentBucket), frame.UsedWords()};
}

void SetDigits(Battle_FloatingLabel& label, std::string_view digits) {
  label.textLength = static_cast<u_char>(digits.size());
  for (std::size_t index = 0; index < digits.size(); ++index) {
    std::span(label.text)[index] = static_cast<signed char>(digits[index]);
  }
}

// Saves the scalar state the updater and the equipment producer read.
class SavedState {
 public:
  SavedState() = default;
  ~SavedState() {
    Battle_placedLabelPositions = positions_;
    Battle_ashleyStatusLabelsSuppressed = suppressed_;
    VS::Field::areRoomUpdatesPaused = paused_;
    VS::State::tickSpeed = tick_;
    VS::Input::settings = settings_;
  }
  SavedState(const SavedState&) = delete;
  SavedState& operator=(const SavedState&) = delete;

 private:
  DVECTOR* positions_ = Battle_placedLabelPositions;
  std::int16_t suppressed_ = Battle_ashleyStatusLabelsSuppressed;
  u_short paused_ = VS::Field::areRoomUpdatesPaused;
  int tick_ = VS::State::tickSpeed;
  VS::Input::Settings settings_ = VS::Input::settings;
};

using LabelDraw = void (*)(int, int, Battle_FloatingLabel*);

// Draws the frame's first record and checks the leaf left the record as it
// was.
Capture RunLeaf(f::LiveFrame& frame, LabelDraw draw, int x) {
  auto& label = frame.labels.front();
  const auto before = f::Words(label);
  auto capture = Run(frame, [&] { draw(x, kY, &label); });
  CHECK(f::Words(label) == before);
  return capture;
}

std::uint64_t ValueDigitsDigest(LabelDraw draw) {
  constexpr std::array<std::uint16_t, 2> kValues{25, 0x8000 | 25};
  constexpr std::array<std::uint8_t, 3> kFrames{0, 2, 20};
  constexpr std::array<int, 3> kColumns{kX, 320, -2};
  f::LiveFrame frame;
  auto& label = frame.labels.front();
  SetDigits(label, "925");
  Digest digest;
  for (const auto value : kValues) {
    for (const auto elapsed : kFrames) {
      for (const auto x : kColumns) {
        label.value = value;
        label.frame = elapsed;
        label.colorFlags = static_cast<u_char>(elapsed & 1U);
        digest.Add(RunLeaf(frame, draw, x));
      }
    }
  }
  return digest.Value();
}

std::uint64_t StatusCaptionsDigest(LabelDraw draw) {
  constexpr std::uint8_t kLastFrame = 30;
  constexpr std::array<std::uint16_t, 3> kLayouts{0, 1, 2};
  f::LiveFrame frame;
  auto& label = frame.labels.front();
  Digest digest;
  for (const auto layout : kLayouts) {
    for (std::uint8_t elapsed = 0; elapsed <= kLastFrame; ++elapsed) {
      for (const u_char removal : {u_char{0}, u_char{1}}) {
        label.value = layout;
        label.frame = elapsed;
        label.colorFlags = removal;
        digest.Add(RunLeaf(frame, draw, kX));
      }
    }
  }
  return digest.Value();
}

std::uint64_t StatChangeRowsDigest(LabelDraw draw) {
  constexpr u_char kFirstWords = 7;
  constexpr u_char kSecondWords = 13;
  constexpr std::int16_t kLift = 20;
  constexpr int kStackX = 8;
  constexpr u_short kLowered = 0x8000;
  f::LiveFrame frame;
  auto& label = frame.labels.front();
  SetDigits(label, "315");
  label.scaleY = kLift;
  Digest digest;
  for (u_char first = 0; first < kFirstWords; ++first) {
    for (u_char second = 0; second < kSecondWords; ++second) {
      label.colorFlags = first;
      label.limb = second;
      label.value = (second & 1U) != 0 ? kLowered : 1;
      digest.Add(RunLeaf(frame, draw, kStackX));
    }
  }
  return digest.Value();
}

using PlaceEntry = void (*)(std::int16_t*, Battle_FloatingLabel*);

// Places the frame's first record, which the Live entry requires to be owned.
Placement Place(f::LiveFrame& frame, PlaceEntry place,
                const Battle_FloatingLabel& label,
                std::array<std::int16_t, 2> position,
                const std::array<DVECTOR, kPlacedCapacity>& placed,
                std::int16_t count) {
  Placement result{placed, position, count, {}};
  auto& owned = frame.labels.front();
  owned = label;
  Battle_placedLabelPositions = result.placed.data();
  Battle_placedLabelCount = count;
  place(result.position.data(), &owned);
  result.count = Battle_placedLabelCount;
  result.label = f::Words(owned);
  return result;
}

std::uint64_t PlacementDigest(PlaceEntry place) {
  constexpr std::array<std::array<std::int16_t, 2>, 4> kAnchors{
      {{100, 80}, {10, 250}, {300, 40}, {124, 96}}};
  constexpr std::int16_t kLastCount = 4;
  constexpr std::int16_t kUnitScale = 0x1000;
  constexpr std::int16_t kOffsetScale = -3;
  SavedState saved;
  f::LiveFrame frame;
  std::array<DVECTOR, kPlacedCapacity> placed{};
  placed[0] = {116, 80};
  placed[1] = {100, 80};
  placed[2] = {84, 88};
  placed[3] = {300, 40};
  Battle_FloatingLabel label{};
  label.scaleY = kUnitScale;
  Digest digest;
  for (const auto& anchor : kAnchors) {
    for (std::int16_t count = 0; count <= kLastCount; ++count) {
      for (const std::int16_t scale : {kUnitScale, kOffsetScale}) {
        label.scaleX = scale;
        digest.Add(Place(frame, place, label, anchor, placed, count));
      }
    }
  }
  return digest.Value();
}

QueueState Snapshot(const f::LiveFrame& frame) {
  QueueState state{.count = Battle_floatingLabelCount};
  for (std::size_t index = 0; index < f::kLabelCapacity; ++index) {
    state.labels[index] = f::Words(frame.labels[index]);
  }
  return state;
}

void SeedQueue(f::LiveFrame& frame) {
  constexpr std::array<std::uint8_t, 4> kKinds{6, 1, 7, 2};
  std::ranges::fill(frame.labels, Battle_FloatingLabel{});
  for (std::size_t index = 0; index < kKinds.size(); ++index) {
    frame.labels[index].key.bytes[0] = static_cast<char>(kKinds[index]);
  }
  constexpr std::size_t kSpare = 5;
  SetDigits(frame.labels[kSpare], "ZZZZ");
  frame.labels[kSpare].scaleX = 0x1000;
  Battle_floatingLabelCount = static_cast<std::int16_t>(kKinds.size());
}

using EquipmentQueue = int (*)(int, int, int);
using Commit = void (*)();

QueueState RunEquipment(f::LiveFrame& frame, EquipmentQueue queue,
                        Commit commit) {
  constexpr std::array<int, 6> kKinds{240, 1, 4, 6, 241, 9};
  constexpr std::array<int, 6> kAmounts{
      42, static_cast<int>(0x80000000U | 1234U), 7, 0x7FFF, 0, 999};
  SeedQueue(frame);
  for (std::size_t index = 0; index < kKinds.size(); ++index) {
    CHECK(queue(kKinds[index], static_cast<int>(index) + 0x100,
                kAmounts[index]) == 0);
  }
  commit();
  return Snapshot(frame);
}

std::uint64_t EquipmentQueueDigest(EquipmentQueue queue, Commit commit) {
  SavedState saved;
  f::LiveFrame frame;
  Digest digest;
  for (const bool weapon : {false, true}) {
    for (const bool armor : {false, true}) {
      VS::Input::settings.weapon_stat_change = weapon ? 1 : 0;
      VS::Input::settings.armor_stat_change = armor ? 1 : 0;
      digest.Add(RunEquipment(frame, queue, commit));
    }
  }
  CHECK(frame.labels[4].key.bytes[0] == kEquipmentKind);
  CHECK(frame.labels[2].key.bytes[0] == kEquipmentKind);
  return digest.Value();
}

void SeedUpdate(f::LiveFrame& frame) {
  constexpr std::array<std::uint8_t, 6> kKinds{6, 6, 0, 4, 5, 6};
  constexpr std::array<std::int16_t, 6> kWaiting{2, 0, 0, 0, 0, -1};
  constexpr std::array<std::uint8_t, 6> kFrames{0, 3, 0, 45, 4, 60};
  std::ranges::fill(frame.labels, Battle_FloatingLabel{});
  for (std::size_t index = 0; index < kKinds.size(); ++index) {
    auto& label = frame.labels[index];
    label.key.bytes[0] = static_cast<char>(kKinds[index]);
    label.position.pad = kWaiting[index];
    label.frame = kFrames[index];
    SetDigits(label, "12");
    label.limb = static_cast<u_char>(index);
    label.colorFlags = static_cast<u_char>(index);
  }
  Battle_floatingLabelCount = static_cast<std::int16_t>(kKinds.size());
  Battle_abilityTimingCueFrames = 0;
  Battle_statusLabelStepTimer = 20;
  Battle_placedLabelCount = 0;
}

std::uint64_t UpdateDigest(Commit update) {
  constexpr int kTick = 4;
  SavedState saved;
  f::ProductionTables tables;
  f::LiveFrame frame;
  VS::State::tickSpeed = kTick;
  VS::Field::areRoomUpdatesPaused = 0;
  SeedUpdate(frame);
  UpdateResult result;
  result.draws = Run(frame, update);
  result.draws.used = frame.UsedWords();
  result.queue = Snapshot(frame);
  result.timers = {Battle_abilityTimingCueFrames, Battle_statusLabelStepTimer,
                   Battle_placedLabelCount};
  CHECK_FALSE(result.draws.equipment.empty());
  CHECK_FALSE(result.draws.text.empty());
  CHECK(result.queue.count == kSeededLabels - 2);
  Digest digest;
  digest.Add(result);
  return digest.Value();
}
}  // namespace

TEST_CASE("F00722 Live matches the generated value digits digest",
          "[floating-placement-live][F00722]") {
  CHECK(ValueDigitsDigest(b::LiveDrawActorValueLabel) == kValueDigitsDigest);
}

TEST_CASE("F00725 Live matches the generated status captions digest",
          "[floating-placement-live][F00725]") {
  CHECK(StatusCaptionsDigest(b::LiveDrawActorStatusLabel) ==
        kStatusCaptionsDigest);
}

TEST_CASE("F00739 Live matches the generated stat-change rows digest",
          "[floating-placement-live][F00739]") {
  CHECK(StatChangeRowsDigest(b::LiveDrawEquipmentStatChange) ==
        kStatChangeRowsDigest);
}

TEST_CASE("F00720 Live matches the generated placement search digest",
          "[floating-placement-live][F00720]") {
  CHECK(PlacementDigest(b::LivePlaceActorValueLabel) == kPlacementDigest);
}

TEST_CASE("F00738 and F00742 Live match the generated equipment queue digest",
          "[floating-placement-live][F00738][F00742]") {
  CHECK(EquipmentQueueDigest(b::LiveQueueEquipmentStatChange,
                             b::LiveCommitEquipmentStatChanges) ==
        kEquipmentQueueDigest);
}

TEST_CASE("F00719 Live matches the generated stack update digest",
          "[floating-placement-live][F00719]") {
  CHECK(UpdateDigest(b::LiveUpdateAndDrawFloatingLabels) == kUpdateDigest);
}

TEST_CASE("Floating placement Live refuses foreign records without drawing",
          "[floating-placement-live][F00722][F00739]") {
  f::LiveFrame frame;
  Battle_FloatingLabel foreign{};
  SetDigits(foreign, "1");
  const auto foreign_draw = Run(frame, [&] {
    b::LiveDrawActorValueLabel(kX, kY, &foreign);
    b::LiveDrawEquipmentStatChange(kX, kY, &foreign);
  });
  CHECK(foreign_draw.used == 0);
  auto& label = frame.labels.front();
  label.textLength = kOversizedText;
  const auto long_text =
      Run(frame, [&] { b::LiveDrawActorValueLabel(kX, kY, &label); });
  CHECK(long_text.used == 0);
  CHECK(frame.labels.front().key.bytes[0] != kPendingKind);
}
