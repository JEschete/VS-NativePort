#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>

struct SVECTOR;

// Bindings from the event VM, screen and dialog callers to the typed actor
// selection, activity and dialog-anchor helpers (ScriptActorCommands.hpp), in
// the original argument order. A null anchor is refused with a warning on the
// "event" channel.
namespace VS::Event {
struct ActorSelection;

// decomp: func_800BAF6C (src/BATTLE/BATTLE.PRG/4A0A8.c:2634)
void SetLiveEventActorActivity(std::int16_t activity_enabled);
// decomp: expandActorSelector (func_800BB68C,
// src/BATTLE/BATTLE.PRG/4DEA8.c:3529)
void BuildLiveScriptActorSelection(std::uint16_t actor_selector,
                                   ActorSelection& selection);
// decomp: nextActor (func_800BB788, src/BATTLE/BATTLE.PRG/4DEA8.c:3559): the
// next id, or kSelectionEnd once the selection is spent.
std::uint16_t NextLiveScriptSelectedActor(ActorSelection& selection);
// decomp: func_800BB7C4 (src/BATTLE/BATTLE.PRG/4A0A8.c:2920)
int LiveActorDialogAnchor(int actor_id, SVECTOR* dialog_anchor);
}  // namespace VS::Event
