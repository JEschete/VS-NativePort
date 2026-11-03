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

#include "Actors/ActorTableLive.hpp"
#include "Actors/TextureAnimation.hpp"
#include "Render/UiPacketsLive.hpp"

extern "C" {
#include "Actors/ActorRecord.h"
#include "Actors/Animation.h"
#include "Actors/Movement.h"
#include "Actors/TextureAnimation.h"
#include "Actors/Textures.h"
#include "Core/Diagnostics.h"
#include "State/Timers.h"
}

namespace VS::Actors {
namespace {
constexpr std::size_t kTextureBanks = 3;
static_assert(sizeof(Actors_ActorRecord::textureRowOffsets) == kTextureBanks);
// Each slot origin is a halfword column and a halfword page.
constexpr std::size_t kSlotOriginBytes = 4;
constexpr unsigned kByteBits = 8;
constexpr std::size_t kTextureSlots = 38;
static_assert(sizeof Actors_textureSlotOrigins ==
              kTextureSlots * kSlotOriginBytes);
static_assert(sizeof Actors_textureSlotRowOffsets >= kTextureSlots);
constexpr unsigned kSheetPageMask = 0xF;
// BATTLE.PRG 40564.c D_800E909C[][28] (this port's
// Actors_sheathedWeaponAnimationBases) holds channel 0's timelines and
// D_800E916C[][8] (Actors_secondarySlotSequences) channel 1's.
constexpr std::size_t kChannel0TimelineKeys = 28;
constexpr std::size_t kChannel1TimelineKeys = 8;
constexpr std::size_t kChannel1Timelines = 188;
static_assert(std::size(Actors_secondarySlotSequences) == kChannel1Timelines);
static_assert(std::size(Actors_secondarySlotSequences[0]) ==
              kChannel1TimelineKeys);
constexpr std::size_t kKeyBytes = sizeof(Actors_TextureAnimationKey);
constexpr auto kChannel1Keys = kChannel1Timelines * kChannel1TimelineKeys;
// A four-bit timeline and the byte key index past it stay inside both tables.
constexpr std::size_t kLastKeyIndex = 0x100;
static_assert((((kTextureTimelineMask * kChannel0TimelineKeys) + kLastKeyIndex +
                1) *
               kKeyBytes) <= sizeof Actors_sheathedWeaponAnimationBases);
static_assert(((kTextureTimelineMask * kChannel1TimelineKeys) + kLastKeyIndex +
               1) <= kChannel1Keys);
static_assert(sizeof(Actors_ActorRecord::textureAnimationSlots) ==
              kTextureAnimationChannels * sizeof(Actors_TextureAnimationSlot));
static_assert(sizeof(Actors_ActorRecord::textureOverlays) ==
              kTextureOverlayCount * sizeof(Actors_TextureOverlayRect));
static_assert(sizeof(Actors_ActorRecord::textureScroll) ==
              kTextureScrollLayers * sizeof(Actors_TextureScrollState));

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
      if (byte + 1 >= sizeof Actors_sheathedWeaponAnimationBases) return {};
      return {.frame = Actors_sheathedWeaponAnimationBases[byte],
              .duration = Actors_sheathedWeaponAnimationBases[byte + 1]};
    }
    const std::size_t key =
        (static_cast<std::size_t>(timeline) * kChannel1TimelineKeys) +
        static_cast<std::size_t>(index);
    if (key >= kChannel1Keys) return {};
    const Actors_TextureAnimationKey& stored =
        Actors_secondarySlotSequences[key / kChannel1TimelineKeys]
                                     [key % kChannel1TimelineKeys];
    return {.frame = stored.frame, .duration = stored.duration};
  }
};

[[nodiscard]] TextureAnimationSlot SlotOf(
    const Actors_TextureAnimationSlot& slot) {
  return {.timeline = slot.timeline,
          .mode = static_cast<TextureAnimationMode>(slot.mode),
          .current_frame = slot.currentFrame,
          .key_index = slot.keyIndex,
          .ticks = slot.ticks};
}

void StoreSlot(const TextureAnimationSlot& slot,
               Actors_TextureAnimationSlot& record) {
  constexpr unsigned kModeMask = 0xF;
  record.timeline = static_cast<u_char>(slot.timeline & kTextureTimelineMask);
  record.mode =
      static_cast<u_char>(static_cast<unsigned>(slot.mode) & kModeMask);
  record.currentFrame = slot.current_frame;
  record.keyIndex = slot.key_index;
  record.ticks = slot.ticks;
}

[[nodiscard]] TextureOverlay OverlayOf(const Actors_TextureOverlayRect& rect) {
  return {.x = rect.x, .y = rect.y, .width = rect.width, .height = rect.height};
}

[[nodiscard]] std::int16_t SlotOriginHalfword(std::size_t slot,
                                              std::size_t half) {
  const std::size_t byte = (slot * kSlotOriginBytes) + (half * 2);
  return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
      Actors_textureSlotOrigins[byte] |
      (Actors_textureSlotOrigins[byte + 1] << kByteBits)));
}

// Where the actor's drawn texture lives, or none for a slot past the tables.
// The sheet's page and row are left for the bank.
[[nodiscard]] std::optional<ActorTextureLayout> SlotLayout(
    const Actors_ActorRecord& actor) {
  const std::size_t slot = actor.textureSlot;
  if (slot >= kTextureSlots) return std::nullopt;
  // The original tested bit 27 of the word eight bytes before sequenceFlags:
  // this flag. It marks the 8-bit textures.
  return ActorTextureLayout{.depth = actor.textureHeaderBit != 0
                                         ? TextureDepth::k8Bit
                                         : TextureDepth::k4Bit,
                            .slot_x = SlotOriginHalfword(slot, 0),
                            .slot_y = SlotOriginHalfword(slot, 1),
                            .slot_row = Actors_textureSlotRowOffsets[slot]};
}

void WarnSlotRefused(const Actors_ActorRecord& actor, const char* operation) {
  Core_Warnf("actors", "%s refused: texture slot %u is outside the slot tables",
             operation, static_cast<unsigned>(actor.textureSlot));
}

[[nodiscard]] std::optional<std::size_t> LiveBank(const char* operation) {
  const std::size_t bank = Actors_sharedSequenceBankIndex;
  if (bank >= kTextureBanks) {
    Core_Warnf("actors", "%s refused: sequence bank %zu has no texture row",
               operation, bank);
    return std::nullopt;
  }
  return bank;
}

[[nodiscard]] unsigned SheetPage() {
  return Actors_sharedSequenceBankTexturePages[Actors_sharedSequenceBankIndex];
}

// The actor's layout for copying from the shared bank's sheet.
[[nodiscard]] std::optional<ActorTextureLayout> SheetLayout(
    const Actors_ActorRecord& actor, std::size_t bank, const char* operation) {
  std::optional<ActorTextureLayout> layout = SlotLayout(actor);
  if (!layout.has_value()) {
    WarnSlotRefused(actor, operation);
    return std::nullopt;
  }
  layout->sheet_page = static_cast<int>(SheetPage() & kSheetPageMask);
  layout->sheet_row = actor.textureRowOffsets[bank];
  return layout;
}

void QueueMove(const TextureMove& move) {
  Render::QueueLiveTextureRectangleMove(move.source, move.x, move.y);
}

[[nodiscard]] TextureScrolling ScrollingOf(const Actors_ActorRecord& actor) {
  TextureScrolling scrolling;
  for (std::size_t layer = 0; layer < kTextureScrollLayers; ++layer) {
    const Actors_TextureScrollBounds& bounds = actor.textureScrollBounds[layer];
    const Actors_TextureScrollState& state = actor.textureScroll[layer];
    scrolling.bounds.at(layer) = {.x = bounds.x,
                                  .y = bounds.y,
                                  .width = bounds.width,
                                  .height = bounds.height,
                                  .bounces = bounds.bounces != 0};
    scrolling.states.at(layer) = {.x = state.x,
                                  .y = state.y,
                                  .step_x = state.stepX,
                                  .step_y = state.stepY,
                                  .interval = state.interval,
                                  .timer = state.timer};
    scrolling.overlays.at(layer) = OverlayOf(actor.textureOverlays[layer]);
  }
  return scrolling;
}

void StoreScrolling(const TextureScrolling& scrolling,
                    Actors_ActorRecord& actor) {
  for (std::size_t layer = 0; layer < kTextureScrollLayers; ++layer) {
    const TextureScrollState& state = scrolling.states.at(layer);
    Actors_TextureScrollState& stored = actor.textureScroll[layer];
    stored.x = state.x;
    stored.y = state.y;
    stored.stepX = state.step_x;
    stored.stepY = state.step_y;
    stored.interval = state.interval;
    stored.timer = state.timer;
  }
}
}  // namespace

// decomp: BATTLE.PRG func_800AAD4C (src/BATTLE/BATTLE.PRG/40564.c:1301)
int SetLiveActorTextureAnimation(int actor_id, int channel, int timeline,
                                 int command) {
  constexpr const char* kOperation = "SetActorTextureAnimation";
  Actors_ActorRecord* actor = LiveActorRecord(actor_id, kOperation);
  if (actor == nullptr || channel >= kTextureAnimationChannels) return -1;
  if (channel < 0) {
    Core_Warnf("actors", "%s refused: channel %d is negative", kOperation,
               channel);
    return -1;
  }
  if (command < static_cast<int>(TextureAnimationCommand::kStop) ||
      command > static_cast<int>(TextureAnimationCommand::kTogglePause)) {
    return 0;
  }
  Actors_TextureAnimationSlot& stored = actor->textureAnimationSlots[channel];
  TextureAnimationSlot slot = SlotOf(stored);
  SetTextureAnimation(slot, channel, actor_id, timeline,
                      static_cast<TextureAnimationCommand>(command));
  StoreSlot(slot, stored);
  return 0;
}

// decomp: BATTLE.PRG func_800AAE9C (src/BATTLE/BATTLE.PRG/40564.c:1357)
void UpdateLiveActorTextureAnimations(Actors_ActorRecord* actor) {
  if (actor == nullptr) {
    WarnActorRefused("UpdateActorTextureAnimations", "null actor");
    return;
  }
  if (SheetPage() == 0) return;
  std::array<TextureAnimationSlot, kTextureAnimationChannels> slots{
      SlotOf(actor->textureAnimationSlots[0]),
      SlotOf(actor->textureAnimationSlots[1])};
  const TextureAnimationFrames frames =
      StepTextureAnimations(slots, LiveTextureAnimationKeys{});
  for (std::size_t channel = 0; channel < slots.size(); ++channel) {
    StoreSlot(slots.at(channel), actor->textureAnimationSlots[channel]);
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
void UploadLiveActorTextureFrame(Actors_ActorRecord* actor, int channel,
                                 int frame) {
  constexpr const char* kOperation = "UploadActorTextureFrame";
  if (actor == nullptr) {
    WarnActorRefused(kOperation, "null actor");
    return;
  }
  if (static_cast<unsigned>(channel) >= kTextureAnimationChannels) {
    Core_Warnf("actors", "%s refused: channel %d is outside the channels",
               kOperation, channel);
    return;
  }
  const std::optional<std::size_t> bank = LiveBank(kOperation);
  if (!bank.has_value()) return;
  if (actor->textureRowOffsets[*bank] == kNoTextureRow) return;
  const Actors_TextureAnimationFrame& frames =
      actor->textureAnimationFrames[channel];
  const Actors_TextureCellOrigin& origin = actor->textureCellOrigins[channel];
  const std::optional<TextureCellSheet> found = ChannelCellSheet(
      {.overlay = frames.overlayIndex,
       .source_x = frames.sourceX,
       .source_y = frames.sourceY,
       .columns = frames.columns},
      {.x = origin.x, .y = origin.y, .columns = origin.columns}, channel);
  if (!found.has_value()) return;
  const TextureCellSheet& sheet = *found;
  if (sheet.overlay == 0 || sheet.overlay > kTextureOverlayCount) {
    Core_Warnf("actors", "%s refused: sheet overlay %u is outside 1..%d",
               kOperation, static_cast<unsigned>(sheet.overlay),
               kTextureOverlayCount);
    return;
  }
  const std::optional<ActorTextureLayout> layout =
      SheetLayout(*actor, *bank, kOperation);
  if (!layout.has_value()) return;
  const TextureMove move = TextureFrameMove(
      sheet, OverlayOf(actor->textureOverlays[sheet.overlay - 1]), *layout,
      frame);
  Core_Debugf(
      "texture",
      "actor=%u model=%u bank=%u slot=%u clut=%u channel=%d frame=%d "
      "overlay=%u row=%u src=(%d,%d) dst=(%d,%d) size=(%d,%d)",
      static_cast<unsigned>(actor->instance.slot),
      static_cast<unsigned>(actor->modelId), static_cast<unsigned>(*bank),
      static_cast<unsigned>(actor->textureSlot),
      static_cast<unsigned>(actor->clutA), channel, frame,
      static_cast<unsigned>(sheet.overlay),
      static_cast<unsigned>(layout->sheet_row), move.source.x, move.source.y,
      move.x, move.y, move.source.width, move.source.height);
  QueueMove(move);
}

// decomp: BATTLE.PRG func_800AB2AC (src/BATTLE/BATTLE.PRG/40564.c:1486)
void ResetLiveActorTextureFrames(int bank_index) {
  const int shared_bank = Actors_sharedSequenceBankIndex;
  // Another bank restarts nothing; the original read no rows for it.
  if (bank_index != shared_bank) return;
  const std::optional<std::size_t> bank = LiveBank("ResetActorTextureFrames");
  if (!bank.has_value()) return;
  for (Actors_ActorRecord* actor : Actors_actorRecords) {
    if (actor != nullptr &&
        RestartsTextureFrames(bank_index, shared_bank,
                              actor->textureRowOffsets[*bank])) {
      UploadLiveActorTextureFrame(actor, 0, 0);
      UploadLiveActorTextureFrame(actor, 1, 0);
    }
  }
}

// decomp: BATTLE.PRG func_800AB358 (src/BATTLE/BATTLE.PRG/40564.c:1502)
void UploadLiveActorTextureOverlay(int actor_id, TextureOverlayCopy copy) {
  constexpr const char* kOperation = "UploadActorTextureOverlay";
  if (SheetPage() == 0) return;
  Actors_ActorRecord* actor = LiveActorRecord(actor_id, kOperation);
  if (actor == nullptr) {
    // The original read through the empty slot.
    if (IsActorTableId(actor_id)) {
      WarnActorRefused(kOperation, "empty actor slot");
    }
    return;
  }
  if (copy.overlay >= kTextureOverlayCount) {
    Core_Warnf("actors", "%s refused: overlay %u is outside 0..%d", kOperation,
               static_cast<unsigned>(copy.overlay), kTextureOverlayCount - 1);
    return;
  }
  const std::optional<std::size_t> bank = LiveBank(kOperation);
  if (!bank.has_value()) return;
  const std::optional<ActorTextureLayout> layout =
      SheetLayout(*actor, *bank, kOperation);
  if (!layout.has_value()) return;
  // Unlike the keyed frames, a scripted copy adds even an unset (0xFF) row.
  QueueMove(TextureOverlayMove(copy.source_x, copy.source_y,
                               OverlayOf(actor->textureOverlays[copy.overlay]),
                               *layout));
}

// decomp: BATTLE.PRG func_800AB4F0 (src/BATTLE/BATTLE.PRG/40564.c:1552)
void UpdateLiveActorTextureScrolling(Actors_ActorRecord* actor,
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
                           State_tickSpeed, carried_height);
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
