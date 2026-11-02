// Hand-maintained (tools/port/owned.csv).
// C counterparts: rood-boi pc/src/battle_helpers.c:464/473/476,
// battle_movement.c:602-650 and actor_cleanup.c:19, revision
// 3be5ba38b008cad919a2b1ca196432c8d4817335 (CC0-1.0).
#include "Battle/Ai/Engagement.hpp"

#include <cstdint>
#include <optional>

#include "Core/Diagnostics.h"

namespace VS::Battle::Ai {
namespace {
std::nullopt_t Refuse(const char* message) {
  Core_Debugf("ai", "%s", message);
  return std::nullopt;
}

bool IsEngagementSlot(int actor_id) {
  return actor_id >= 0 && actor_id < kEngagementActorSlots;
}

std::uint16_t EngagementBit(int actor_id) {
  return static_cast<std::uint16_t>(1u << static_cast<unsigned>(actor_id));
}
}  // namespace

// decomp: func_800D820C (src/BATTLE/BATTLE.PRG/6FA0C.s:9).
bool SetActorAwarenessIndicator(AwarenessMarkers& markers, int actor_id,
                                int marker) {
  if (!IsEngagementSlot(actor_id)) {
    Refuse("AI awareness marker actor ID is outside the AI state table");
    return false;
  }
  if (actor_id != 0) markers.SetMarker(actor_id, marker);
  return true;
}

// decomp: func_800D821C (src/BATTLE/BATTLE.PRG/6FA0C.s:18).
bool ResetActorAwarenessIndicator(AwarenessMarkers& markers, int actor_id) {
  return SetActorAwarenessIndicator(markers, actor_id, 0) &&
         SetActorAwarenessIndicator(markers, actor_id, kAwarenessMarkerReset);
}

// decomp: func_800D826C (src/BATTLE/BATTLE.PRG/6FA6C.s:10).
bool IsEngagementReady(std::uint16_t engaged_frames) {
  return engaged_frames >= kEngagementReadyFrames;
}

// decomp: func_800D8280 (src/BATTLE/BATTLE.PRG/6FA6C.s:22).
bool UpdateTemporaryAwarenessIndicator(AwarenessMarkers& markers, int actor_id,
                                       TemporaryAwareness& awareness) {
  if (awareness.frames_remaining == 0) return true;
  if (!IsEngagementSlot(actor_id)) {
    Refuse("AI temporary marker actor ID is outside the AI state table");
    return false;
  }
  --awareness.frames_remaining;
  const int marker = awareness.frames_remaining == 0 ? 0 : awareness.marker;
  return SetActorAwarenessIndicator(markers, actor_id, marker);
}

// decomp: func_800D82A8 (src/BATTLE/BATTLE.PRG/6FA6C.s:37).
std::optional<bool> DisengageActorById(EngagementTable& table, int actor_id) {
  if (!IsEngagementSlot(actor_id)) {
    return Refuse("AI disengagement actor ID is outside the AI state table");
  }
  if (actor_id == 0 || !table.HasActorState(actor_id)) return false;
  table.DisengageActorState(actor_id);
  return true;
}

// decomp: func_800D82CC (src/BATTLE/BATTLE.PRG/6FA6C.s:53).
std::optional<bool> DisengageActor(EngagementHost& host, int actor_id,
                                   EngagementTimers& timers,
                                   EngagementRoster& roster) {
  if (!IsEngagementSlot(actor_id)) {
    return Refuse("AI disengagement actor ID is outside the engaged mask");
  }
  const std::uint16_t bit = EngagementBit(actor_id);
  if ((roster.engaged_mask & bit) == 0) return false;
  if (roster.engaged_count == 0) {
    return Refuse("AI disengagement would underflow the engaged count");
  }
  const std::optional<bool> single_opponent = host.HasSingleOpponent();
  if (!single_opponent.has_value()) {
    return Refuse("AI disengagement requires the battle actor list");
  }
  roster.engaged_mask = static_cast<std::uint16_t>(roster.engaged_mask & ~bit);
  timers.engaged_frames = 0;
  timers.cooldown_frames =
      *single_opponent ? 0 : kEngagementLeaveCooldownFrames;
  timers.grace_frames = kReengagementGraceFrames;
  host.UnlinkEngagedActor();
  --roster.engaged_count;
  SetActorAwarenessIndicator(host, actor_id, 0);
  return true;
}

// decomp: func_800D836C (src/BATTLE/BATTLE.PRG/6FA6C.s:99).
std::optional<bool> TryEngageActor(EngagementHost& host, int actor_id,
                                   EngagementTimers& timers,
                                   EngagementRoster* roster) {
  if (!IsEngagementSlot(actor_id)) {
    return Refuse("AI engagement actor ID is outside the engaged mask");
  }
  if (timers.engaged_frames != 0) return true;
  if (roster == nullptr) {
    return Refuse("AI engagement requires the room roster");
  }
  if (roster->limit < roster->engaged_count || timers.cooldown_frames != 0 ||
      host.CandidateEngagedThisUpdate()) {
    return false;
  }
  if (roster->engaged_count >= kEngagementActorSlots) {
    return Refuse("AI engagement count exceeds the engaged mask");
  }
  host.ClearEngagementRequest();
  timers.engaged_frames = timers.grace_frames != 0 ? kEngagementReadyFrames : 1;
  roster->engaged_mask = static_cast<std::uint16_t>(roster->engaged_mask |
                                                    EngagementBit(actor_id));
  ++roster->engaged_count;
  host.LinkEngagedActor();
  return true;
}
}  // namespace VS::Battle::Ai
