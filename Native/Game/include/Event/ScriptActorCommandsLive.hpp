#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>

struct Event_ActorSelection;
struct SVECTOR;

// Bindings from the generated event VM, screen and dialog callers to the
// typed actor selection, activity and dialog-anchor helpers
// (ScriptActorCommands.hpp), in the original argument order
// (tools/port/copy_fixups.py OWNED_CALL_BINDINGS). A null selection or anchor
// is refused with a warning on the "event" channel.
namespace VS::Event {
// decomp: func_800BAF6C (src/BATTLE/BATTLE.PRG/4A0A8.c:2634)
void SetLiveEventActorActivity(std::int16_t activity_enabled);
// decomp: func_800BB68C (src/BATTLE/BATTLE.PRG/4A0A8.c:2880)
void BuildLiveScriptActorSelection(std::uint16_t actor_selector,
                                   Event_ActorSelection* selection);
// decomp: func_800BB788 (src/BATTLE/BATTLE.PRG/4A0A8.c:2910): the next id,
// or kSelectionEnd once the selection is spent.
std::uint16_t NextLiveScriptSelectedActor(Event_ActorSelection* selection);
// decomp: func_800BB7C4 (src/BATTLE/BATTLE.PRG/4A0A8.c:2920)
int LiveActorDialogAnchor(int actor_id, SVECTOR* dialog_anchor);
}  // namespace VS::Event
