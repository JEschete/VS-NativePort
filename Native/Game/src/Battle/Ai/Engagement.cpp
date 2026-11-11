// Hand-maintained (tools/port/owned.csv).
// C counterparts: rood-boi pc/src/battle_helpers.c:464/473/476,
// battle_movement.c:602-650 and actor_cleanup.c:19, revision
// 3be5ba38b008cad919a2b1ca196432c8d4817335 (CC0-1.0).
#include "Battle/Ai/Engagement.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>

#include "Battle/Ai/Relations.hpp"
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

// The engaged-list member whose successor is the removed actor; `valid` is
// false for a walk that revisits a member or meets an ID outside the slots.
struct Predecessor {
  bool valid = false;
  std::optional<int> id;
};

// Walks from the head only as far as the original's removal walk did.
Predecessor FindPredecessor(const EngagedList& list, int actor_id) {
  std::uint16_t visited = 0;
  std::optional<int> node = list.Head();
  while (node.has_value()) {
    const int id = *node;
    if (!IsEngagementSlot(id) || (visited & EngagementBit(id)) != 0) return {};
    visited = static_cast<std::uint16_t>(visited | EngagementBit(id));
    const std::optional<int> next = list.Next(id);
    if (next == actor_id) return {.valid = true, .id = id};
    node = next;
  }
  return {.valid = true, .id = std::nullopt};
}

// Whether the walk from the head ends without revisiting a member and holds
// only IDs within the slots.
bool IsWellFormed(const EngagedList& list) {
  std::uint16_t visited = 0;
  std::optional<int> node = list.Head();
  while (node.has_value()) {
    const int id = *node;
    if (!IsEngagementSlot(id) || (visited & EngagementBit(id)) != 0) {
      return false;
    }
    visited = static_cast<std::uint16_t>(visited | EngagementBit(id));
    node = list.Next(id);
  }
  return true;
}

int BuildupMarker(std::uint16_t engaged_frames) {
  const int step = engaged_frames / kBuildupMarkerStepFrames;
  return std::min(step, kBuildupMarkerSteps - 1) + 1;
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

// decomp: func_800D8260 (src/BATTLE/BATTLE.PRG/6FA60.c:407).
void SetTemporaryAwarenessIndicator(TemporaryAwareness& awareness, int marker,
                                    int frames) {
  awareness.marker = static_cast<std::uint16_t>(marker);
  awareness.frames_remaining = static_cast<std::uint8_t>(frames);
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

// decomp: func_800DBF00 (src/BATTLE/BATTLE.PRG/73700.c:414).
bool UnlinkEngagedActor(EngagedList& list, int actor_id) {
  if (!IsEngagementSlot(actor_id)) {
    Refuse("AI engaged-list removal actor ID is outside the AI state table");
    return false;
  }
  const std::optional<int> head = list.Head();
  const bool is_head = head == actor_id;
  const Predecessor predecessor =
      is_head ? Predecessor{.valid = true, .id = std::nullopt}
              : FindPredecessor(list, actor_id);
  if (!predecessor.valid) {
    Refuse("AI engaged list revisits an actor or leaves the AI state table");
    return false;
  }
  const bool is_tail = list.Tail() == actor_id;
  // A removed actor keeps a stale successor, read only for a linked actor.
  const bool linked = is_head || is_tail || predecessor.id.has_value();
  const std::optional<int> next =
      linked ? list.Next(actor_id) : std::optional<int>{};
  if (next.has_value() && !IsEngagementSlot(*next)) {
    Refuse("AI engaged-list successor is outside the AI state table");
    return false;
  }

  list.SetPhase(actor_id, {.phase = 0, .step = list.Phase(actor_id).step});
  list.SetPose(actor_id, kEngagementPoseFalling);
  if (is_tail) list.SetTail(next.has_value() ? next : head);
  if (is_head) {
    list.SetHead(next);
  } else if (predecessor.id.has_value()) {
    list.SetNext(*predecessor.id, next);
    return true;
  }
  if (!(is_head ? next : head).has_value()) list.SetTail(std::nullopt);
  return true;
}

// decomp: func_800DBFE4 (src/BATTLE/BATTLE.PRG/73700.c:463).
bool UpdateEngagementBuildup(EngagementBuildupHost& host, int actor_id,
                             EngagementTimers& timers,
                             EngagementRoster* roster) {
  if (!IsEngagementSlot(actor_id)) {
    Refuse("AI engagement buildup actor ID is outside the AI state table");
    return false;
  }
  if (timers.engaged_frames != 0) {
    const auto frames = static_cast<std::uint16_t>(timers.engaged_frames + 1);
    const bool leaves = frames > kEngagementBuildupLimitFrames;
    if (leaves && roster == nullptr) {
      Refuse("AI engagement buildup requires the room roster");
      return false;
    }
    timers.engaged_frames = frames;
    if (leaves) {
      host.RememberActionTarget();
      // An actor missing from the engaged mask stays and keeps counting.
      if (!DisengageActor(host, actor_id, timers, *roster).has_value()) {
        return false;
      }
    } else if (frames != 0) {
      SetActorAwarenessIndicator(host, actor_id, BuildupMarker(frames));
      return true;
    }
  }
  if (timers.grace_frames != 0) {
    --timers.grace_frames;
    SetActorAwarenessIndicator(
        host, actor_id, timers.grace_frames != 0 ? kGraceAwarenessMarker : 0);
  }
  if (timers.cooldown_frames != 0) --timers.cooldown_frames;
  return true;
}

// decomp: func_800DC0D0 (src/BATTLE/BATTLE.PRG/73700.c:499).
bool AnimateEngagementPhase(EngagedList& list) {
  // The list is built by pushing onto its head, so it cannot revisit a member;
  // the original would loop forever on such a list.
  if (!IsWellFormed(list)) {
    Refuse("AI engaged list revisits an actor or leaves the AI state table");
    return false;
  }
  for (std::optional<int> node = list.Head(); node.has_value();
       node = list.Next(*node)) {
    const int id = *node;
    if (list.AshleyClassification(id) == kAllyClassification) continue;
    EngagementPhase phase = list.Phase(id);
    std::optional<unsigned> pose;
    if (phase.phase == 0) {
      phase.step = 1;
      pose = kEngagementPoseRising;
    } else if (phase.phase == kEngagementPhaseTurn) {
      phase.step = -1;
      pose = kEngagementPoseFalling;
    }
    phase.phase = static_cast<std::int8_t>(phase.phase + phase.step);
    list.SetPhase(id, phase);
    if (pose.has_value()) {
      list.SetPose(id, *pose);
      break;
    }
  }
  return true;
}
}  // namespace VS::Battle::Ai
