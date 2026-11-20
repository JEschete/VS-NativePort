// Hand-maintained (tools/port/owned.csv).
// Live binding for the actor texture animation (TextureAnimation.hpp): the
// actor records, the shared sequence bank's texture page and rows, the
// texture slot tables and the timeline key tables.
#include "Actors/TextureAnimationLive.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>

#include "Actors/ActorRecord.hpp"
#include "Actors/ActorTableLive.hpp"
#include "Actors/Animation.hpp"
#include "Actors/TextureAnimation.hpp"
#include "Actors/TextureAnimationRecords.hpp"
#include "Actors/Textures.hpp"
#include "Core/Diagnostics.hpp"
#include "Render/UiPacketsLive.hpp"
#include "State/Timers.hpp"

namespace VS::Actors {
namespace {
constexpr std::size_t kTextureBanks = 3;
static_assert(sizeof(VS::Actors::ActorRecord::texture_row_offsets) ==
              kTextureBanks);
// Each slot origin is a halfword column and a halfword page.
constexpr std::size_t kSlotOriginBytes = 4;
constexpr unsigned kByteBits = 8;
constexpr std::size_t kTextureSlots = 38;
static_assert(sizeof VS::Actors::textureSlotOrigins ==
              kTextureSlots * kSlotOriginBytes);
static_assert(sizeof VS::Actors::textureSlotRowOffsets >= kTextureSlots);
constexpr unsigned kSheetPageMask = 0xF;
// BATTLE.PRG 40564.c D_800E909C[][28] (this port's
// VS::Actors::sheathedWeaponAnimationBases) holds channel 0's timelines and
// D_800E916C[][8] (VS::Actors::secondarySlotSequences) channel 1's.
constexpr std::size_t kChannel0TimelineKeys = 28;
constexpr std::size_t kChannel1TimelineKeys = 8;
constexpr std::size_t kChannel1Timelines = 188;
static_assert(std::size(VS::Actors::secondarySlotSequences) ==
              kChannel1Timelines);
static_assert(std::size(VS::Actors::secondarySlotSequences[0]) ==
              kChannel1TimelineKeys);
constexpr std::size_t kKeyBytes = sizeof(VS::Actors::TextureAnimationKeyRecord);
constexpr auto kChannel1Keys = kChannel1Timelines * kChannel1TimelineKeys;
// A four-bit timeline and the byte key index past it stay inside both tables.
constexpr std::size_t kLastKeyIndex = 0x100;
static_assert((((kTextureTimelineMask * kChannel0TimelineKeys) + kLastKeyIndex +
                1) *
               kKeyBytes) <= sizeof VS::Actors::sheathedWeaponAnimationBases);
static_assert(((kTextureTimelineMask * kChannel1TimelineKeys) + kLastKeyIndex +
               1) <= kChannel1Keys);
static_assert(sizeof(VS::Actors::ActorRecord::texture_animation_slots) ==
              kTextureAnimationChannels *
                  sizeof(VS::Actors::TextureAnimationSlotRecord));
static_assert(sizeof(VS::Actors::ActorRecord::texture_overlays) ==
              kTextureOverlayCount * sizeof(VS::Actors::TextureOverlayRect));
static_assert(sizeof(VS::Actors::ActorRecord::texture_scroll) ==
              kTextureScrollLayers *
                  sizeof(VS::Actors::TextureScrollStateRecord));

class LiveTextureAnimationKeys final : public TextureAnimationKeys {
 public:
  [[nodiscard]] TextureAnimationKey Key(int channel, int timeline,
                                        int index) const override {
    if (timeline < 0 || index < 0) return {};
    if (channel == 0) {
      const std::size_t byte =
          ((static_cast<std::size_t>(timeline) * kChannel0TimelineKeys) +
           static_cast<std::size_t>(index)) *
          kKeyBytes;
      if (byte + 1 >= sizeof VS::Actors::sheathedWeaponAnimationBases) {
        return {};
      }
      return {.frame = VS::Actors::sheathedWeaponAnimationBases[byte],
              .duration = VS::Actors::sheathedWeaponAnimationBases[byte + 1]};
    }
    const std::size_t key =
        (static_cast<std::size_t>(timeline) * kChannel1TimelineKeys) +
        static_cast<std::size_t>(index);
    if (key >= kChannel1Keys) return {};
    const VS::Actors::TextureAnimationKeyRecord& stored =
        VS::Actors::secondarySlotSequences[key / kChannel1TimelineKeys]
                                          [key % kChannel1TimelineKeys];
    return {.frame = stored.frame, .duration = stored.duration};
  }
};

[[nodiscard]] TextureAnimationSlot SlotOf(
    const VS::Actors::TextureAnimationSlotRecord& slot) {
  return {.timeline = slot.timeline,
          .mode = static_cast<TextureAnimationMode>(slot.mode),
          .current_frame = slot.current_frame,
          .key_index = slot.key_index,
          .ticks = slot.ticks};
}

void StoreSlot(const TextureAnimationSlot& slot,
               VS::Actors::TextureAnimationSlotRecord& record) {
  constexpr unsigned kModeMask = 0xF;
  record.timeline =
      static_cast<std::uint8_t>(slot.timeline & kTextureTimelineMask);
  record.mode =
      static_cast<std::uint8_t>(static_cast<unsigned>(slot.mode) & kModeMask);
  record.current_frame = slot.current_frame;
  record.key_index = slot.key_index;
  record.ticks = slot.ticks;
}

[[nodiscard]] TextureOverlay OverlayOf(
    const VS::Actors::TextureOverlayRect& rect) {
  return {.x = rect.x, .y = rect.y, .width = rect.width, .height = rect.height};
}

[[nodiscard]] std::int16_t SlotOriginHalfword(std::size_t slot,
                                              std::size_t half) {
  const std::size_t byte = (slot * kSlotOriginBytes) + (half * 2);
  return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
      VS::Actors::textureSlotOrigins[byte] |
      (VS::Actors::textureSlotOrigins[byte + 1] << kByteBits)));
}

// Where the actor's drawn texture lives, or none for a slot past the tables.
// The sheet's page and row are left for the bank.
[[nodiscard]] std::optional<ActorTextureLayout> SlotLayout(
    const VS::Actors::ActorRecord& actor) {
  const std::size_t slot = actor.texture_slot;
  if (slot >= kTextureSlots) return std::nullopt;
  // The original tested bit 27 of the word eight bytes before sequence_flags:
  // this flag. It marks the 8-bit textures.
  return ActorTextureLayout{
      .depth = actor.texture_header_bit != 0 ? TextureDepth::k8Bit
                                             : TextureDepth::k4Bit,
      .slot_x = SlotOriginHalfword(slot, 0),
      .slot_y = SlotOriginHalfword(slot, 1),
      .slot_row = VS::Actors::textureSlotRowOffsets[slot]};
}

void WarnSlotRefused(const VS::Actors::ActorRecord& actor,
                     const char* operation) {
  VS::Core::Warnf("actors",
                  "%s refused: texture slot %u is outside the slot tables",
                  operation, static_cast<unsigned>(actor.texture_slot));
}

[[nodiscard]] std::optional<std::size_t> LiveBank(const char* operation) {
  const std::size_t bank = VS::Actors::sharedSequenceBankIndex;
  if (bank >= kTextureBanks) {
    VS::Core::Warnf("actors",
                    "%s refused: sequence bank %zu has no texture row",
                    operation, bank);
    return std::nullopt;
  }
  return bank;
}

[[nodiscard]] unsigned SheetPage() {
  return VS::Actors::sharedSequenceBankTexturePages
      [VS::Actors::sharedSequenceBankIndex];
}

// The actor's layout for copying from the shared bank's sheet.
[[nodiscard]] std::optional<ActorTextureLayout> SheetLayout(
    const VS::Actors::ActorRecord& actor, std::size_t bank,
    const char* operation) {
  std::optional<ActorTextureLayout> layout = SlotLayout(actor);
  if (!layout.has_value()) {
    WarnSlotRefused(actor, operation);
    return std::nullopt;
  }
  layout->sheet_page = static_cast<int>(SheetPage() & kSheetPageMask);
  layout->sheet_row = actor.texture_row_offsets[bank];
  return layout;
}

void QueueMove(const TextureMove& move) {
  Render::QueueLiveTextureRectangleMove(move.source, move.x, move.y);
}

[[nodiscard]] TextureScrolling ScrollingOf(
    const VS::Actors::ActorRecord& actor) {
  TextureScrolling scrolling;
  for (std::size_t layer = 0; layer < kTextureScrollLayers; ++layer) {
    const VS::Actors::TextureScrollBoundsRecord& bounds =
        actor.texture_scroll_bounds[layer];
    const VS::Actors::TextureScrollStateRecord& state =
        actor.texture_scroll[layer];
    scrolling.bounds.at(layer) = {.x = bounds.x,
                                  .y = bounds.y,
                                  .width = bounds.width,
                                  .height = bounds.height,
                                  .bounces = bounds.bounces != 0};
    scrolling.states.at(layer) = {.x = state.x,
                                  .y = state.y,
                                  .step_x = state.step_x,
                                  .step_y = state.step_y,
                                  .interval = state.interval,
                                  .timer = state.timer};
    scrolling.overlays.at(layer) = OverlayOf(actor.texture_overlays[layer]);
  }
  return scrolling;
}

void StoreScrolling(const TextureScrolling& scrolling,
                    VS::Actors::ActorRecord& actor) {
  for (std::size_t layer = 0; layer < kTextureScrollLayers; ++layer) {
    const TextureScrollState& state = scrolling.states.at(layer);
    VS::Actors::TextureScrollStateRecord& stored = actor.texture_scroll[layer];
    stored.x = state.x;
    stored.y = state.y;
    stored.step_x = state.step_x;
    stored.step_y = state.step_y;
    stored.interval = state.interval;
    stored.timer = state.timer;
  }
}
}  // namespace

// decomp: BATTLE.PRG func_800AAD4C (src/BATTLE/BATTLE.PRG/40564.c:1301)
int SetLiveActorTextureAnimation(int actor_id, int channel, int timeline,
                                 int command) {
  constexpr const char* kOperation = "SetActorTextureAnimation";
  VS::Actors::ActorRecord* actor = LiveActorRecord(actor_id, kOperation);
  if (actor == nullptr || channel >= kTextureAnimationChannels) return -1;
  if (channel < 0) {
    VS::Core::Warnf("actors", "%s refused: channel %d is negative", kOperation,
                    channel);
    return -1;
  }
  if (command < static_cast<int>(TextureAnimationCommand::kStop) ||
      command > static_cast<int>(TextureAnimationCommand::kTogglePause)) {
    return 0;
  }
  VS::Actors::TextureAnimationSlotRecord& stored =
      actor->texture_animation_slots[channel];
  TextureAnimationSlot slot = SlotOf(stored);
  SetTextureAnimation(slot, channel, actor_id, timeline,
                      static_cast<TextureAnimationCommand>(command));
  StoreSlot(slot, stored);
  return 0;
}

// decomp: BATTLE.PRG func_800AAE9C (src/BATTLE/BATTLE.PRG/40564.c:1357)
void UpdateLiveActorTextureAnimations(VS::Actors::ActorRecord* actor) {
  if (actor == nullptr) {
    WarnActorRefused("UpdateActorTextureAnimations", "null actor");
    return;
  }
  if (SheetPage() == 0) return;
  std::array<TextureAnimationSlot, kTextureAnimationChannels> slots{
      SlotOf(actor->texture_animation_slots[0]),
      SlotOf(actor->texture_animation_slots[1])};
  const TextureAnimationFrames frames =
      StepTextureAnimations(slots, LiveTextureAnimationKeys{});
  for (std::size_t channel = 0; channel < slots.size(); ++channel) {
    StoreSlot(slots.at(channel), actor->texture_animation_slots[channel]);
  }
  // The frames copy from the sheets, which the slots never touch, so copying
  // after both steps queues the same moves in the same order.
  for (std::size_t channel = 0; channel < frames.size(); ++channel) {
    if (const std::optional<int> frame = frames.at(channel)) {
      UploadLiveActorTextureFrame(actor, static_cast<int>(channel), *frame);
    }
  }
}

// decomp: BATTLE.PRG func_800AB098 (src/BATTLE/BATTLE.PRG/40564.c:1417)
void UploadLiveActorTextureFrame(VS::Actors::ActorRecord* actor, int channel,
                                 int frame) {
  constexpr const char* kOperation = "UploadActorTextureFrame";
  if (actor == nullptr) {
    WarnActorRefused(kOperation, "null actor");
    return;
  }
  if (static_cast<unsigned>(channel) >= kTextureAnimationChannels) {
    VS::Core::Warnf("actors", "%s refused: channel %d is outside the channels",
                    kOperation, channel);
    return;
  }
  const std::optional<std::size_t> bank = LiveBank(kOperation);
  if (!bank.has_value()) return;
  if (actor->texture_row_offsets[*bank] == kNoTextureRow) return;
  const VS::Actors::TextureAnimationFrame& frames =
      actor->texture_animation_frames[channel];
  const VS::Actors::TextureCellOriginRecord& origin =
      actor->texture_cell_origins[channel];
  const std::optional<TextureCellSheet> found = ChannelCellSheet(
      {.overlay = frames.overlay_index,
       .source_x = frames.source_x,
       .source_y = frames.source_y,
       .columns = frames.columns},
      {.x = origin.x, .y = origin.y, .columns = origin.columns}, channel);
  if (!found.has_value()) return;
  const TextureCellSheet& sheet = *found;
  if (sheet.overlay == 0 || sheet.overlay > kTextureOverlayCount) {
    VS::Core::Warnf("actors", "%s refused: sheet overlay %u is outside 1..%d",
                    kOperation, static_cast<unsigned>(sheet.overlay),
                    kTextureOverlayCount);
    return;
  }
  const std::optional<ActorTextureLayout> layout =
      SheetLayout(*actor, *bank, kOperation);
  if (!layout.has_value()) return;
  const TextureMove move = TextureFrameMove(
      sheet, OverlayOf(actor->texture_overlays[sheet.overlay - 1]), *layout,
      frame);
  VS::Core::Debugf(
      "texture",
      "actor=%u model=%u bank=%u slot=%u clut=%u channel=%d frame=%d "
      "overlay=%u row=%u src=(%d,%d) dst=(%d,%d) size=(%d,%d)",
      static_cast<unsigned>(actor->instance.slot),
      static_cast<unsigned>(actor->model_id), static_cast<unsigned>(*bank),
      static_cast<unsigned>(actor->texture_slot),
      static_cast<unsigned>(actor->clut_a), channel, frame,
      static_cast<unsigned>(sheet.overlay),
      static_cast<unsigned>(layout->sheet_row), move.source.x, move.source.y,
      move.x, move.y, move.source.width, move.source.height);
  QueueMove(move);
}

// decomp: BATTLE.PRG func_800AB2AC (src/BATTLE/BATTLE.PRG/40564.c:1486)
void ResetLiveActorTextureFrames(int bank_index) {
  const int shared_bank = VS::Actors::sharedSequenceBankIndex;
  // Another bank restarts nothing; the original read no rows for it.
  if (bank_index != shared_bank) return;
  const std::optional<std::size_t> bank = LiveBank("ResetActorTextureFrames");
  if (!bank.has_value()) return;
  for (VS::Actors::ActorRecord* actor : VS::Actors::actorRecords) {
    if (actor != nullptr &&
        RestartsTextureFrames(bank_index, shared_bank,
                              actor->texture_row_offsets[*bank])) {
      UploadLiveActorTextureFrame(actor, 0, 0);
      UploadLiveActorTextureFrame(actor, 1, 0);
    }
  }
}

// decomp: BATTLE.PRG func_800AB358 (src/BATTLE/BATTLE.PRG/40564.c:1502)
void UploadLiveActorTextureOverlay(int actor_id, TextureOverlayCopy copy) {
  constexpr const char* kOperation = "UploadActorTextureOverlay";
  if (SheetPage() == 0) return;
  VS::Actors::ActorRecord* actor = LiveActorRecord(actor_id, kOperation);
  if (actor == nullptr) {
    // The original read through the empty slot.
    if (IsActorTableId(actor_id)) {
      WarnActorRefused(kOperation, "empty actor slot");
    }
    return;
  }
  if (copy.overlay >= kTextureOverlayCount) {
    VS::Core::Warnf("actors", "%s refused: overlay %u is outside 0..%d",
                    kOperation, static_cast<unsigned>(copy.overlay),
                    kTextureOverlayCount - 1);
    return;
  }
  const std::optional<std::size_t> bank = LiveBank(kOperation);
  if (!bank.has_value()) return;
  const std::optional<ActorTextureLayout> layout =
      SheetLayout(*actor, *bank, kOperation);
  if (!layout.has_value()) return;
  // Unlike the keyed frames, a scripted copy adds even an unset (0xFF) row.
  QueueMove(TextureOverlayMove(copy.source_x, copy.source_y,
                               OverlayOf(actor->texture_overlays[copy.overlay]),
                               *layout));
}

// decomp: BATTLE.PRG func_800AB4F0 (src/BATTLE/BATTLE.PRG/40564.c:1552)
void UpdateLiveActorTextureScrolling(VS::Actors::ActorRecord* actor,
                                     int carried_height) {
  constexpr const char* kOperation = "UpdateActorTextureScrolling";
  if (actor == nullptr) {
    WarnActorRefused(kOperation, "null actor");
    return;
  }
  // The timers run for an actor with no usable slot; only its copies, which
  // would read past the slot tables, are refused.
  const std::optional<ActorTextureLayout> layout = SlotLayout(*actor);
  TextureScrolling scrolling = ScrollingOf(*actor);
  const TextureScrollMoves moves =
      StepTextureScrolling(scrolling, layout.value_or(ActorTextureLayout{}),
                           VS::State::tickSpeed, carried_height);
  StoreScrolling(scrolling, *actor);
  bool refused = false;
  for (const std::optional<TextureMove>& move : moves) {
    if (!move.has_value()) continue;
    if (!layout.has_value()) {
      refused = true;
      continue;
    }
    QueueMove(*move);
  }
  if (refused) WarnSlotRefused(*actor, kOperation);
}
}  // namespace VS::Actors
