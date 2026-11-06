#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Actors/TextureAnimation.hpp"
#include "Actors/TextureAnimationLive.hpp"
#include "Content/ModuleData.h"
#include "Legacy/Psx.h"
#include "Render/UiPackets.hpp"
#include "TestSupport/ChannelWarnings.hpp"

extern "C" {
#include "Actors/ActorRecord.h"
#include "Actors/Animation.h"
#include "Actors/Movement.h"
#include "Actors/TextureAnimation.h"
#include "Actors/Textures.h"
#include "Render/Display.h"
#include "Render/Frame.h"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"
#include "State/Timers.h"
}

// Contracts of the #276 row 22 texture animation: the keyed channels, the
// frame and scripted cell copies and the scrolling regions (BATTLE.PRG
// 40564.c:1301-1640).
namespace {
namespace actors = VS::Actors;
using actors::TextureAnimationCommand;
using actors::TextureAnimationKey;
using actors::TextureAnimationMode;
using actors::TextureAnimationSlot;
using actors::TextureDepth;
using actors::TextureMove;
using VS::test_support::ChannelWarnings;
using VS::test_support::Refused;

// One timeline per channel; the keys past the list have no frame.
class TestKeys final : public actors::TextureAnimationKeys {
 public:
  int timeline = 1;
  std::array<std::vector<TextureAnimationKey>,
             actors::kTextureAnimationChannels>
      channels;

  [[nodiscard]] TextureAnimationKey Key(int channel, int requested,
                                        int index) const override {
    const auto& keys = channels.at(static_cast<std::size_t>(channel));
    if (requested != timeline || index < 0 ||
        static_cast<std::size_t>(index) >= keys.size()) {
      return {};
    }
    return keys.at(static_cast<std::size_t>(index));
  }
};

// A channel playing timeline 1.
constexpr TextureAnimationSlot kPlaying = [] {
  TextureAnimationSlot slot;
  slot.timeline = 1;
  slot.mode = TextureAnimationMode::kPlaying;
  return slot;
}();

// The frame's primitive buffer and ordering tables, replaced for one test.
struct LiveFrame {
  Render_FrameScratch saved_frame = Render_frameScratch;
  void* saved_buffer = Render_legacyPrimitiveBuffers[0];
  void* saved_table = Render_legacyOrderingTables[0];
  void* saved_overlay_table = Render_legacyCurrentOrderingTable;
  int saved_framebuffer = Render_frameBuf;
  std::vector<u_long> storage = std::vector<u_long>(0x20000 / 4, 0xDEADBEEF);
  std::vector<u_long> ordering = std::vector<u_long>(0x2088 / 4);

  LiveFrame() {
    Render_frameBuf = 0;
    Render_legacyPrimitiveBuffers[0] = storage.data();
    Render_legacyOrderingTables[0] = ordering.data();
    Render_legacyCurrentOrderingTable = ordering.data();
    Render_frameScratch.primitiveCursor = storage.data();
    Render_frameScratch.orderingTable = ordering.data() + 18;
    ClearOTagR(ordering.data(), static_cast<int>(ordering.size()));
  }
  LiveFrame(const LiveFrame&) = delete;
  LiveFrame& operator=(const LiveFrame&) = delete;
  LiveFrame(LiveFrame&&) = delete;
  LiveFrame& operator=(LiveFrame&&) = delete;
  ~LiveFrame() {
    Render_frameScratch = saved_frame;
    Render_legacyPrimitiveBuffers[0] = saved_buffer;
    Render_legacyOrderingTables[0] = saved_table;
    Render_legacyCurrentOrderingTable = saved_overlay_table;
    Render_frameBuf = saved_framebuffer;
  }

  // The queued moves as their source, destination and size words.
  struct Move {
    std::uint32_t source = 0;
    std::uint32_t destination = 0;
    std::uint32_t size = 0;

    friend bool operator==(const Move&, const Move&) = default;
  };

  [[nodiscard]] std::vector<Move> Moves() const {
    const auto* cursor =
        static_cast<const u_long*>(Render_frameScratch.primitiveCursor);
    const auto words = static_cast<std::size_t>(cursor - storage.data());
    std::vector<Move> moves;
    for (std::size_t packet = 0;
         packet + VS::Render::kTextureMovePacketWords <= words;
         packet += VS::Render::kTextureMovePacketWords) {
      moves.push_back(
          {.source = static_cast<std::uint32_t>(storage[packet + 3]),
           .destination = static_cast<std::uint32_t>(storage[packet + 4]),
           .size = static_cast<std::uint32_t>(storage[packet + 5])});
    }
    return moves;
  }
};

[[nodiscard]] constexpr std::uint32_t Packed(std::uint32_t x, std::uint32_t y) {
  return (y << 16) | x;
}

// The actor table, the shared bank, the slot tables, the key tables and the
// tick speed, set for one test and then restored. Slot 0 sits at column 4,
// page 0, row 1; the shared bank is 0 with sheet page 0x1A.
struct LiveTextureState {
  decltype(std::to_array(Actors_actorRecords)) saved_actors =
      std::to_array(Actors_actorRecords);
  u_char saved_bank = Actors_sharedSequenceBankIndex;
  std::array<u_char, 4> saved_pages{};
  decltype(std::to_array(Actors_textureSlotOrigins)) saved_origins =
      std::to_array(Actors_textureSlotOrigins);
  decltype(std::to_array(Actors_textureSlotRowOffsets)) saved_rows =
      std::to_array(Actors_textureSlotRowOffsets);
  decltype(std::to_array(
      Actors_sheathedWeaponAnimationBases)) saved_channel0_keys =
      std::to_array(Actors_sheathedWeaponAnimationBases);
  decltype(std::to_array(
      Actors_secondarySlotSequences[2])) saved_channel1_keys =
      std::to_array(Actors_secondarySlotSequences[2]);
  int saved_tick_speed = State_tickSpeed;
  std::vector<std::unique_ptr<Actors_ActorRecord>> records;

  LiveTextureState() {
    std::copy_n(std::begin(Actors_sharedSequenceBankTexturePages),
                saved_pages.size(), saved_pages.begin());
    std::fill(std::begin(Actors_actorRecords), std::end(Actors_actorRecords),
              nullptr);
    Actors_sharedSequenceBankIndex = 0;
    Actors_sharedSequenceBankTexturePages[0] = 0x1A;
    const std::array<u_char, 4> origin{4, 0, 0, 0};
    std::ranges::copy(origin, std::begin(Actors_textureSlotOrigins));
    Actors_textureSlotRowOffsets[0] = 1;
  }
  LiveTextureState(const LiveTextureState&) = delete;
  LiveTextureState& operator=(const LiveTextureState&) = delete;
  LiveTextureState(LiveTextureState&&) = delete;
  LiveTextureState& operator=(LiveTextureState&&) = delete;
  ~LiveTextureState() {
    std::ranges::copy(saved_actors, std::begin(Actors_actorRecords));
    Actors_sharedSequenceBankIndex = saved_bank;
    std::ranges::copy(saved_pages,
                      std::begin(Actors_sharedSequenceBankTexturePages));
    std::ranges::copy(saved_origins, std::begin(Actors_textureSlotOrigins));
    std::ranges::copy(saved_rows, std::begin(Actors_textureSlotRowOffsets));
    std::ranges::copy(saved_channel0_keys,
                      std::begin(Actors_sheathedWeaponAnimationBases));
    std::ranges::copy(saved_channel1_keys,
                      std::begin(Actors_secondarySlotSequences[2]));
    State_tickSpeed = saved_tick_speed;
  }

  // A 4-bit actor in slot 0 with bank 0's row 7 and overlay 0 at (16, 20),
  // 32x12, whose channels copy cells of two columns from (32, 5) over it.
  Actors_ActorRecord& AddActor(int actor_id) {
    records.push_back(std::make_unique<Actors_ActorRecord>());
    Actors_ActorRecord& actor = *records.back();
    actor.instance.slot = static_cast<u_char>(actor_id);
    actor.textureSlot = 0;
    actor.textureRowOffsets[0] = 7;
    actor.textureOverlays[0] = {.x = 16, .y = 20, .width = 32, .height = 12};
    for (Actors_TextureAnimationFrame& frames : actor.textureAnimationFrames) {
      frames = {.overlayIndex = 1, .sourceX = 32, .sourceY = 5, .columns = 2};
    }
    Actors_actorRecords[actor_id] = &actor;
    return actor;
  }
};

// The move of the 4-bit cell `frame` of LiveTextureState::AddActor's sheet.
[[nodiscard]] LiveFrame::Move ActorFrameMove(std::uint32_t frame) {
  return {.source = Packed(640 + 8 + ((frame % 2) * 8),
                           256 + 5 + 7 + ((frame / 2) * 12)),
          .destination = Packed(256 + 4, 64 + 20),
          .size = Packed(8, 12)};
}
}  // namespace

TEST_CASE("Texture animation play starts a timeline at its first key",
          "[actors-appearance-rendering][actors]") {
  TextureAnimationSlot slot{.current_frame = 4, .key_index = 6, .ticks = 9};
  actors::SetTextureAnimation(slot, 0, 3, 5, TextureAnimationCommand::kPlay);
  CHECK(slot == TextureAnimationSlot{.timeline = 5,
                                     .mode = TextureAnimationMode::kPlaying,
                                     .current_frame = 4});

  // Channel 1 starts ten ticks per actor id along, keeping the low byte.
  TextureAnimationSlot second;
  actors::SetTextureAnimation(second, 1, 3, 5, TextureAnimationCommand::kPlay);
  CHECK(second.ticks == 30);
  actors::SetTextureAnimation(second, 1, 30, 6, TextureAnimationCommand::kPlay);
  CHECK(second.ticks == 300 % 256);

  // The record keeps four bits of the timeline.
  TextureAnimationSlot masked;
  actors::SetTextureAnimation(masked, 0, 0, 0x13,
                              TextureAnimationCommand::kPlay);
  CHECK(masked.timeline == 3);
}

TEST_CASE("Playing timeline 0 or the timeline already playing keeps its place",
          "[actors-appearance-rendering][actors]") {
  const TextureAnimationSlot playing{.timeline = 5,
                                     .mode = TextureAnimationMode::kPlaying,
                                     .current_frame = 2,
                                     .key_index = 3,
                                     .ticks = 4};
  TextureAnimationSlot slot = playing;
  actors::SetTextureAnimation(slot, 0, 1, 5, TextureAnimationCommand::kPlay);
  CHECK(slot == playing);
  actors::SetTextureAnimation(slot, 0, 1, 0, TextureAnimationCommand::kPlay);
  CHECK(slot == playing);

  // A paused timeline played again restarts.
  slot.mode = TextureAnimationMode::kPaused;
  actors::SetTextureAnimation(slot, 0, 1, 5, TextureAnimationCommand::kPlay);
  CHECK(slot == TextureAnimationSlot{.timeline = 5,
                                     .mode = TextureAnimationMode::kPlaying,
                                     .current_frame = 2});
}

TEST_CASE("Texture animation stop and pause toggle only change the mode",
          "[actors-appearance-rendering][actors]") {
  TextureAnimationSlot slot{.timeline = 5,
                            .mode = TextureAnimationMode::kPlaying,
                            .current_frame = 2,
                            .key_index = 3,
                            .ticks = 4};
  TextureAnimationSlot expected = slot;
  actors::SetTextureAnimation(slot, 0, 0, 7,
                              TextureAnimationCommand::kTogglePause);
  expected.mode = TextureAnimationMode::kPaused;
  CHECK(slot == expected);
  actors::SetTextureAnimation(slot, 0, 0, 7,
                              TextureAnimationCommand::kTogglePause);
  expected.mode = TextureAnimationMode::kPlaying;
  CHECK(slot == expected);
  actors::SetTextureAnimation(slot, 0, 0, 7, TextureAnimationCommand::kStop);
  expected.mode = TextureAnimationMode::kStopping;
  CHECK(slot == expected);
  // A stopping timeline is not paused.
  actors::SetTextureAnimation(slot, 0, 0, 7,
                              TextureAnimationCommand::kTogglePause);
  CHECK(slot == expected);
}

TEST_CASE("Texture animation steps through its keys by duration and wraps",
          "[actors-appearance-rendering][actors]") {
  TestKeys keys;
  keys.channels[0] = {{.frame = 3, .duration = 1}, {.frame = 5}};
  std::array<TextureAnimationSlot, 2> slots{kPlaying, {}};

  CHECK(actors::StepTextureAnimations(slots, keys) ==
        actors::TextureAnimationFrames{2, std::nullopt});
  CHECK(slots[0] == TextureAnimationSlot{.timeline = 1,
                                         .mode = TextureAnimationMode::kPlaying,
                                         .current_frame = 3,
                                         .ticks = 1});
  // Past key 0's duration of one tick.
  CHECK(actors::StepTextureAnimations(slots, keys) ==
        actors::TextureAnimationFrames{4, std::nullopt});
  CHECK(slots[0].key_index == 1);
  CHECK(slots[0].ticks == 0);
  // Key 2 has no frame, so the timeline wraps to key 0.
  CHECK(actors::StepTextureAnimations(slots, keys) ==
        actors::TextureAnimationFrames{2, std::nullopt});
  CHECK(slots[0].key_index == 0);
}

TEST_CASE("A due key without a frame ends the texture animation update",
          "[actors-appearance-rendering][actors]") {
  TestKeys keys;
  keys.channels[1] = {{.frame = 2}};
  std::array<TextureAnimationSlot, 2> slots{kPlaying, kPlaying};

  // Channel 0's timeline has no frames: channel 1 neither steps nor copies.
  CHECK(actors::StepTextureAnimations(slots, keys) ==
        actors::TextureAnimationFrames{});
  CHECK(slots[1] == kPlaying);
  CHECK(slots[0].ticks == 0);

  // Channel 0 copies before channel 1 ends the update.
  keys.channels = {
      std::vector<TextureAnimationKey>{{.frame = 6, .duration = 9}},
      std::vector<TextureAnimationKey>{}};
  slots = {kPlaying, kPlaying};
  CHECK(actors::StepTextureAnimations(slots, keys) ==
        actors::TextureAnimationFrames{5, std::nullopt});
}

TEST_CASE("Stopping texture animations end at frame 1 and paused ones hold",
          "[actors-appearance-rendering][actors]") {
  TestKeys keys;
  keys.channels[0] = {{.frame = 1, .duration = 9}};
  keys.channels[1] = {{.frame = 4, .duration = 9}};
  std::array<TextureAnimationSlot, 2> slots{
      TextureAnimationSlot{.timeline = 1,
                           .mode = TextureAnimationMode::kStopping,
                           .current_frame = 2},
      TextureAnimationSlot{.timeline = 1,
                           .mode = TextureAnimationMode::kPaused,
                           .current_frame = 3}};
  const TextureAnimationSlot paused = slots[1];

  // Stopping plays on until the shown frame is 1.
  CHECK(actors::StepTextureAnimations(slots, keys) ==
        actors::TextureAnimationFrames{0, std::nullopt});
  CHECK(slots[1] == paused);
  CHECK(actors::StepTextureAnimations(slots, keys) ==
        actors::TextureAnimationFrames{});
  CHECK(slots[0].timeline == 0);

  // Another mode copies the shown frame without stepping.
  slots[1].mode = std::bit_cast<TextureAnimationMode>(std::uint8_t{5});
  CHECK(actors::StepTextureAnimations(slots, keys) ==
        actors::TextureAnimationFrames{std::nullopt, 2});
  CHECK(slots[1].ticks == 0);
}

TEST_CASE("A channel without sheet columns falls back on its cell origin",
          "[actors-appearance-rendering][actors]") {
  const actors::TextureCellSheet sheet{
      .overlay = 2, .source_x = 8, .source_y = 9, .columns = 3};
  const actors::TextureCellOrigin origin{.x = 4, .y = 6, .columns = 2};
  CHECK(actors::ChannelCellSheet(sheet, origin, 1) == sheet);
  CHECK(actors::ChannelCellSheet({}, origin, 0) ==
        actors::TextureCellSheet{
            .overlay = 5, .source_x = 4, .source_y = 6, .columns = 2});
  CHECK(actors::ChannelCellSheet({}, origin, 1) ==
        actors::TextureCellSheet{
            .overlay = 7, .source_x = 4, .source_y = 6, .columns = 2});
  CHECK_FALSE(actors::ChannelCellSheet({}, {}, 0).has_value());
}

TEST_CASE("Texture frames copy the sheet cell over the overlay",
          "[actors-appearance-rendering][actors]") {
  const actors::TextureCellSheet sheet{
      .overlay = 1, .source_x = 32, .source_y = 5, .columns = 2};
  const actors::TextureOverlay overlay{
      .x = 16, .y = 20, .width = 32, .height = 12};
  actors::ActorTextureLayout layout{.depth = TextureDepth::k8Bit,
                                    .sheet_page = 10,
                                    .sheet_row = 7,
                                    .slot_x = 4,
                                    .slot_row = 1};
  // Frame 3 is column 1 of sheet row 1, in halfwords of two texels.
  CHECK(actors::TextureFrameMove(sheet, overlay, layout, 3) ==
        TextureMove{.source = {.x = 640 + 16 + 16,
                               .y = 256 + 5 + 12 + 7,
                               .width = 16,
                               .height = 12},
                    .x = 256 + 8,
                    .y = 64 + 20});
  layout.depth = TextureDepth::k4Bit;
  CHECK(actors::TextureFrameMove(sheet, overlay, layout, 3) ==
        TextureMove{.source = {.x = 640 + 8 + 8,
                               .y = 256 + 5 + 12 + 7,
                               .width = 8,
                               .height = 12},
                    .x = 256 + 4,
                    .y = 64 + 20});
}

TEST_CASE("Reloading the shared bank restarts actors with a sheet row",
          "[actors-appearance-rendering][actors]") {
  CHECK(actors::RestartsTextureFrames(1, 1, 0));
  CHECK(actors::RestartsTextureFrames(2, 2, 0xFE));
  // No row in the bank.
  CHECK_FALSE(actors::RestartsTextureFrames(1, 1, actors::kNoTextureRow));
  // Another bank is not the shared one.
  CHECK_FALSE(actors::RestartsTextureFrames(0, 1, 0));
}

TEST_CASE("Scripted texture copies take the cell at their own position",
          "[actors-appearance-rendering][actors]") {
  const actors::TextureOverlay overlay{
      .x = 8, .y = 4, .width = 16, .height = 8};
  const actors::ActorTextureLayout layout{
      .sheet_page = 2, .sheet_row = 5, .slot_x = 1, .slot_y = 1, .slot_row = 2};
  CHECK(
      actors::TextureOverlayMove(40, 3, overlay, layout) ==
      TextureMove{
          .source = {.x = 128 + 10, .y = 256 + 3 + 5, .width = 4, .height = 8},
          .x = 64 + 2,
          .y = 256 + 128 + 4});
}

TEST_CASE("Texture scrolling steps a layer once its timer passes the interval",
          "[actors-appearance-rendering][actors]") {
  actors::TextureScrolling scrolling;
  scrolling.bounds[0] = {.width = 16, .height = 8};
  scrolling.states[0] = {
      .x = 2, .y = 1, .step_x = 2, .step_y = 1, .interval = 2};
  scrolling.overlays[0] = {.x = 4, .y = 6, .width = 8, .height = 4};
  // Layer 1 has no width and never runs.
  scrolling.states[1] = {.interval = 0, .timer = 9};
  const actors::ActorTextureLayout layout{.slot_x = 1, .slot_row = 1};

  CHECK(actors::StepTextureScrolling(scrolling, layout, 4,
                                     actors::kNoCarriedScrollHeight) ==
        actors::TextureScrollMoves{});
  CHECK(scrolling.states[0].timer == 2);
  CHECK(scrolling.states[1].timer == 9);
  // The window steps and is copied over the overlay, in 4-bit halfwords.
  CHECK(actors::StepTextureScrolling(scrolling, layout, 4,
                                     actors::kNoCarriedScrollHeight) ==
        actors::TextureScrollMoves{TextureMove{
            .source = {.x = 64 + 1, .y = 64 + 2, .width = 2, .height = 4},
            .x = 64 + 1,
            .y = 64 + 6}});
  CHECK(scrolling.states[0] ==
        actors::TextureScrollState{
            .x = 4, .y = 2, .step_x = 2, .step_y = 1, .interval = 2});
}

TEST_CASE("Texture scroll windows snap back or bounce at their bounds",
          "[actors-appearance-rendering][actors]") {
  actors::TextureScrolling scrolling;
  scrolling.bounds[0] = {.x = 2, .y = 2, .width = 16, .height = 8};
  scrolling.states[0] = {.x = 8, .y = 5, .step_x = 3, .step_y = -4};
  scrolling.overlays[0] = {.width = 8, .height = 4};
  actors::TextureScrolling bouncing = scrolling;
  bouncing.bounds[0].bounces = true;

  (void)actors::StepTextureScrolling(scrolling, {}, 2,
                                     actors::kNoCarriedScrollHeight);
  CHECK(scrolling.states[0] ==
        actors::TextureScrollState{.x = 2, .y = 2, .step_x = 3, .step_y = -4});
  // Bouncing steps back by the step and reverses it.
  (void)actors::StepTextureScrolling(bouncing, {}, 2,
                                     actors::kNoCarriedScrollHeight);
  CHECK(bouncing.states[0] ==
        actors::TextureScrollState{.x = 5, .y = 9, .step_x = -3, .step_y = 4});
}

TEST_CASE(
    "Texture scrolling skips a due layer whose width is the carried height",
    "[actors-appearance-rendering][actors]") {
  actors::TextureScrolling scrolling;
  for (std::size_t layer = 0; layer < actors::kTextureScrollLayers; ++layer) {
    scrolling.bounds.at(layer) = {.width = 16, .height = 16};
    scrolling.states.at(layer) = {.step_x = 1};
    scrolling.overlays.at(layer) = {.width = 4, .height = 4};
  }
  // The caller's 16 skips layer 0. Layer 1 copies a window 4 high, so layer
  // 2, 4 wide, is skipped; the skip carries nothing, so layer 3 runs.
  scrolling.bounds[1].width = 8;
  scrolling.bounds[2].width = 4;
  scrolling.states[2].timer = 7;
  const actors::TextureScrollMoves moves =
      actors::StepTextureScrolling(scrolling, {}, 2, 16);
  CHECK(std::array<bool, 4>{moves[0].has_value(), moves[1].has_value(),
                            moves[2].has_value(), moves[3].has_value()} ==
        std::array<bool, 4>{false, true, false, true});
  CHECK(std::array<int, 4>{scrolling.states[0].x, scrolling.states[1].x,
                           scrolling.states[2].x, scrolling.states[3].x} ==
        std::array<int, 4>{0, 1, 0, 1});
  // A skipped layer still restarts its timer.
  CHECK(scrolling.states[2] == actors::TextureScrollState{.step_x = 1});
}

TEST_CASE("The texture depth flag is bit 27 of the word before sequenceFlags",
          "[actors-appearance-rendering][actors]") {
  // The original tested bit 27 of the word eight bytes before the sequence
  // flags; the record names that bit textureHeaderBit.
  auto actor = std::make_unique<Actors_ActorRecord>();
  actor->textureHeaderBit = 1;
  const auto bytes =
      std::bit_cast<std::array<std::byte, sizeof(Actors_ActorRecord)>>(*actor);
  std::uint32_t word = 0;
  std::memcpy(
      &word,
      std::span(bytes)
          .subspan(offsetof(Actors_ActorRecord, sequenceFlags) - 8, sizeof word)
          .data(),
      sizeof word);
  CHECK(word == (1U << 27));
}

TEST_CASE("Live texture animation commands bind the actor's channel",
          "[actors-appearance-rendering][actors]") {
  LiveTextureState state;
  ChannelWarnings warnings{"actors"};
  Actors_ActorRecord& actor = state.AddActor(2);

  CHECK(actors::SetLiveActorTextureAnimation(2, 1, 3, 1) == 0);
  const Actors_TextureAnimationSlot& slot = actor.textureAnimationSlots[1];
  CHECK(std::array<int, 5>{slot.timeline, slot.mode, slot.currentFrame,
                           slot.keyIndex,
                           slot.ticks} == std::array<int, 5>{3, 1, 0, 0, 20});
  // Another command changes nothing.
  CHECK(actors::SetLiveActorTextureAnimation(2, 1, 3, 3) == 0);
  CHECK(slot.mode == 1);
  // An empty slot or a channel past 1 is -1, as in the original.
  CHECK(actors::SetLiveActorTextureAnimation(4, 0, 3, 1) == -1);
  CHECK(actors::SetLiveActorTextureAnimation(2, 2, 3, 1) == -1);
  CHECK(warnings.messages.empty());
}

TEST_CASE("Live texture animation commands refuse a negative channel or id",
          "[actors-appearance-rendering][actors]") {
  LiveTextureState state;
  ChannelWarnings warnings{"actors"};
  state.AddActor(2);

  CHECK(actors::SetLiveActorTextureAnimation(2, -1, 3, 1) == -1);
  CHECK(actors::SetLiveActorTextureAnimation(17, 0, 3, 1) == -1);
  const std::string operation = "SetActorTextureAnimation";
  CHECK(warnings.messages ==
        std::vector<std::string>{
            Refused(operation, "channel -1 is negative"),
            Refused(operation, "actor id 17 is outside the actor table")});
}

TEST_CASE("Live texture animation updates copy both channels' key frames",
          "[actors-appearance-rendering][actors]") {
  LiveTextureState state;
  LiveFrame frame;
  Actors_ActorRecord& actor = state.AddActor(1);
  // Timeline 2: channel 0 keys sit 28 apart, channel 1 keys 8 apart.
  constexpr std::size_t kChannel0Key = std::size_t{2} * 28 * 2;
  Actors_sheathedWeaponAnimationBases[kChannel0Key] = 3;
  Actors_sheathedWeaponAnimationBases[kChannel0Key + 1] = 4;
  Actors_secondarySlotSequences[2][0] = {.frame = 2, .duration = 4};
  for (Actors_TextureAnimationSlot& slot : actor.textureAnimationSlots) {
    slot.timeline = 2;
    slot.mode = 1;
  }

  actors::UpdateLiveActorTextureAnimations(&actor);
  CHECK(frame.Moves() ==
        std::vector<LiveFrame::Move>{ActorFrameMove(2), ActorFrameMove(1)});
  CHECK(actor.textureAnimationSlots[0].currentFrame == 3);
  CHECK(actor.textureAnimationSlots[1].currentFrame == 2);
  CHECK(actor.textureAnimationSlots[1].ticks == 1);

  // Without a sheet page nothing steps.
  Actors_sharedSequenceBankTexturePages[0] = 0;
  actors::UpdateLiveActorTextureAnimations(&actor);
  CHECK(frame.Moves().size() == 2);
  CHECK(actor.textureAnimationSlots[1].ticks == 1);
}

TEST_CASE("Live texture frames copy the sheet cell unless the row is unset",
          "[actors-appearance-rendering][actors]") {
  LiveTextureState state;
  LiveFrame frame;
  Actors_ActorRecord& actor = state.AddActor(1);

  actors::UploadLiveActorTextureFrame(&actor, 0, 3);
  CHECK(frame.Moves() == std::vector<LiveFrame::Move>{ActorFrameMove(3)});
  // An unset row copies nothing, as in the original.
  actor.textureRowOffsets[0] = 0xFF;
  actors::UploadLiveActorTextureFrame(&actor, 0, 3);
  CHECK(frame.Moves().size() == 1);
}

TEST_CASE("Live texture frames are refused outside the record's tables",
          "[actors-appearance-rendering][actors]") {
  LiveTextureState state;
  LiveFrame frame;
  ChannelWarnings warnings{"actors"};
  Actors_ActorRecord& actor = state.AddActor(1);

  actor.textureAnimationFrames[0].overlayIndex = 9;
  actors::UploadLiveActorTextureFrame(&actor, 0, 3);
  actor.textureAnimationFrames[0].overlayIndex = 1;
  actor.textureSlot = 38;
  actors::UploadLiveActorTextureFrame(&actor, 0, 3);
  Actors_sharedSequenceBankIndex = 3;
  actors::UploadLiveActorTextureFrame(&actor, 0, 3);
  actors::UploadLiveActorTextureFrame(&actor, 2, 3);
  actors::UploadLiveActorTextureFrame(nullptr, 0, 3);
  CHECK(frame.Moves().empty());
  const std::string operation = "UploadActorTextureFrame";
  CHECK(warnings.messages ==
        std::vector<std::string>{
            Refused(operation, "sheet overlay 9 is outside 1..8"),
            Refused(operation, "texture slot 38 is outside the slot tables"),
            Refused(operation, "sequence bank 3 has no texture row"),
            Refused(operation, "channel 2 is outside the channels"),
            Refused(operation, "null actor")});
}

TEST_CASE("Live texture frame resets copy frame 0 for the shared bank's actors",
          "[actors-appearance-rendering][actors]") {
  LiveTextureState state;
  LiveFrame frame;
  state.AddActor(1);
  state.AddActor(5).textureRowOffsets[0] = 0xFF;
  Actors_ActorRecord& eight_bit = state.AddActor(9);
  eight_bit.textureHeaderBit = 1;

  // Another bank is not the shared one.
  actors::ResetLiveActorTextureFrames(1);
  CHECK(frame.Moves().empty());
  actors::ResetLiveActorTextureFrames(0);
  const LiveFrame::Move wide{.source = Packed(640 + 16, 256 + 5 + 7),
                             .destination = Packed(256 + 8, 64 + 20),
                             .size = Packed(16, 12)};
  CHECK(frame.Moves() == std::vector<LiveFrame::Move>{
                             ActorFrameMove(0), ActorFrameMove(0), wide, wide});
}

TEST_CASE("Live scripted texture copies take the actor's overlay",
          "[actors-appearance-rendering][actors]") {
  LiveTextureState state;
  LiveFrame frame;
  ChannelWarnings warnings{"actors"};
  Actors_ActorRecord& actor = state.AddActor(3);
  // A scripted copy adds even an unset row.
  actor.textureRowOffsets[0] = 0xFF;

  actors::UploadLiveActorTextureOverlay(
      3, {.overlay = 0, .source_x = 40, .source_y = 3});
  CHECK(frame.Moves() ==
        std::vector<LiveFrame::Move>{{.source = Packed(640 + 10, 256 + 3 + 255),
                                      .destination = Packed(256 + 4, 64 + 20),
                                      .size = Packed(8, 12)}});
  actors::UploadLiveActorTextureOverlay(3, {.overlay = 8});
  actors::UploadLiveActorTextureOverlay(4, {});
  actors::UploadLiveActorTextureOverlay(17, {});
  Actors_sharedSequenceBankTexturePages[0] = 0;
  actors::UploadLiveActorTextureOverlay(17, {});
  CHECK(frame.Moves().size() == 1);
  const std::string operation = "UploadActorTextureOverlay";
  CHECK(warnings.messages ==
        std::vector<std::string>{
            Refused(operation, "overlay 8 is outside 0..7"),
            Refused(operation, "empty actor slot"),
            Refused(operation, "actor id 17 is outside the actor table")});
}

TEST_CASE("Live texture scrolling runs its timers without a usable slot",
          "[actors-appearance-rendering][actors]") {
  LiveTextureState state;
  LiveFrame frame;
  ChannelWarnings warnings{"actors"};
  Actors_ActorRecord& actor = state.AddActor(1);
  State_tickSpeed = 2;
  actor.textureScrollBounds[0].width = 16;
  actor.textureScrollBounds[0].height = 16;
  actor.textureScroll[0].stepX = 1;
  actor.textureOverlays[0] = {.x = 8, .y = 0, .width = 4, .height = 4};

  actors::UpdateLiveActorTextureScrolling(&actor);
  CHECK(frame.Moves() ==
        std::vector<LiveFrame::Move>{{.source = Packed(256, 64),
                                      .destination = Packed(256 + 2, 64),
                                      .size = Packed(1, 4)}});
  CHECK(actor.textureScroll[0].x == 1);
  actor.textureSlot = 0xFF;
  actors::UpdateLiveActorTextureScrolling(&actor);
  CHECK(actor.textureScroll[0].x == 2);
  CHECK(frame.Moves().size() == 1);
  actors::UpdateLiveActorTextureScrolling(nullptr);
  const std::string operation = "UpdateActorTextureScrolling";
  CHECK(warnings.messages ==
        std::vector<std::string>{
            Refused(operation, "texture slot 255 is outside the slot tables"),
            Refused(operation, "null actor")});
}

TEST_CASE("Texture animation bindings retire the module entries they replace",
          "[actors-appearance-rendering][actors][redirect-binding]") {
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  // The texture animation is typed C++ now; no BATTLE.PRG image or table
  // stores these addresses.
  constexpr std::array<unsigned, 6> kRetired{0x800AAD4Cu, 0x800AAE9Cu,
                                             0x800AB098u, 0x800AB2ACu,
                                             0x800AB358u, 0x800AB4F0u};
  for (const auto address : kRetired) {
    INFO("address " << address);
    CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                            address) == nullptr);
  }
}
