# Cutscenes still to check (issue #149)

This checklist retains the 46 scenes originally unverified out of the 292 catalogued room and event scripts.
Owner follow-up has verified 5; 41 remain unchecked.
They are listed in rough story order: by the campaign progress value (flag 0x82) that their trigger
checks or that their room writes, otherwise by when their area is first entered. Areas whose scripts
never write flag 0x82 (Forgotten Pathway, Iron Maiden, City Walls South/North, Great Cathedral
B1-L3) are placed by estimate.

Each entry gives the location and the original trigger, decoded from the room's trigger table
(`Event_EvaluateRoomEventTriggers`, BATTLE.PRG `func_800BEC58`, ported in
`Native/Game/src/Event/Triggers.cpp`). Tiles are `x,z`. "Facing tile" means press Cross while facing
that tile. Zone and room numbers are the engine's (0-based); room names are from the Data Crystal room
list. Flags 32-63 are room-local and are cleared on room entry, so the room's own scripts must set them.

A scene passes when it starts, plays to the end without an error or hang, and returns control to Ashley
(or hands off to the next room).

"What you should see" is read statically from the original script (EVT file or the MPD room script):
dialogue is decoded from the script's own string table, and rooms, timers, door/geometry changes,
removed actors and chained events come from its commands. Nothing there was observed in a run.
Scripts that branch on flags list every branch's messages and destinations; only one path plays.
Durations are a lower bound (the sum of scripted waits, at 60 frames per second).

## Summary by kind

| Kind | Count |
|---|---|
| Puzzle time-trial exit (only fires while a time attack is running) | 13 |
| Room event | 9 |
| Room timer runs out | 7 |
| Cross at a room exit while room flag 40 is 1 | 7 |
| Enemy-defeat scene | 6 |
| Battle-stage scene (checked every frame) | 3 |
| Game over | 1 |

Many of these are not story cutscenes. They are short system scripts:
- time-trial exit messages (flag 285 = a puzzle-room time attack is running);
- room-timer expiry;
- scenes started by pressing Cross at a room exit while room flag 40 is 1 (usually set by the room's entry script).

## Checklist

### 1. Anywhere (game over)

- Area: Any
- Scene: External event EVENT/0000.EVT (`EVENT_0000.EVT-external_event-0-0`)
- Kind: Game over
- Trigger (HP 0): Ashley's HP reaches 0. Battle_ResolveActorDefeat (BATTLE 146C.c:7421) requests it through func_800BEC58(0x40, 0); choose Title on the game-over menu afterwards.
- Automated result: never run (no start setup)
- What you should see (from the original script, not from a run):
  - Ashley has fallen; the game-over sequence plays.
  - The room timer stops and its display goes away.
  - A full-screen image is shown.
  - The music stops.
  - The music changes.
  - No dialogue text in this script.
  - The script has no branches; it ends on the game-over menu (opcode 89, menu 7), which handles Title or Load itself.
- Manual result (2026-10-30, Debug, revision d1166531): Ashley died in MAP047 (zone 15 room 6). EVENT/0000 started at
  tick 6155 and ran every command (offsets 16-108) in script order. GAMEOVER.GIM and MUSIC122 loaded, and the
  game-over menu opened at tick 6317. Choosing Title at tick 6517 returned to the title at tick 6593, and loading
  save slot 0 then resumed play. No warnings or errors were logged.
- [x] Plays and returns control

### 2. The Rich Drown in Wine (MAP020.MPD, zone 9 room 11)

- Area: Wine Cellar
- Scene: Room local event 4 (`MAP_MAP020.MPD-room_local-4-57344`)
- Kind: Room event
- Trigger (step on tile): flag 48 == 1; Ashley on tile 3,13; Ashley idle (not mid-action)
- Flag source: flag 48: local event 1 sets it to 1, local event 2 sets it to 0
- Automated result: trigger never fired in the automated run
- What you should see (from the original script, not from a run):
  - The room timer stops and its display goes away.
  - Message: "Ashley made it through!"
  - A doorway tile becomes passable or blocked.
  - Ashley leaves for one of: Ashley's flashback (MAP212, zone 19 room 0); Room of Rotten Grapes (MAP021, zone 9 room 12) (depends on the exit or branch).
  - Scale: 1 scripted actor move.
  - The script branches on flags, so only some of the messages/destinations above appear.
- Manual result (owner playtest, 2026-10-30): works. The room timer is started by the raised lever; the owner has records of clearing the room and reaching the exit with no issue.
- [x] Plays and returns control

### 3. The Gallows (MAP026.MPD, zone 12 room 0)

- Area: Wine Cellar
- Scene: External event EVENT/0010.EVT (`EVENT_0010.EVT-external_event-10-0`)
- Kind: Enemy-defeat scene
- Trigger (after a combat action / enemy defeat): actor model 0x35 killed
- Trigger (chained): started by a script in EVENT/0508.EVT
- Automated result: trigger never fired in the automated run
- What you should see (from the original script, not from a run):
  - Plays when the Minotaur (zone 12 enemy kind 0x35, the room's only enemy) dies.
  - The room timer stops and its display goes away.
  - A spell/visual effect plays on an actor.
  - Then EVENT/0021.EVT starts.
  - Scale: 9 camera commands, 1 sound effect.
  - No dialogue text in this script.
  - The script branches on flags, so only some of the messages/destinations above appear.
- Manual result (owner playtest, 2026-10-30): works; the scene plays when the Minotaur is killed.
- [x] Plays and returns control

### 4. The Gallows (MAP026.MPD, zone 12 room 0)

- Area: Wine Cellar
- Scene: External event EVENT/0021.EVT (`EVENT_0021.EVT-external_event-21-0`)
- Kind: Room event
- Trigger (chained): plays after EVENT/0010.EVT, which starts it
- Automated result: trigger never fired in the automated run
- What you should see (from the original script, not from a run):
  - Follow-up to the Minotaur's defeat in The Gallows.
  - The music stops.
  - The music changes.
  - The screen fades out.
  - Part of the room changes (a door, gate or platform opens, closes or moves).
  - 1 actor group is removed from the room (defeated or departing characters vanish).
  - Ashley leaves for A Storm of Arrows (MAP127, zone 29 room 3).
  - Scale: 73 camera commands, 4 animations, 158 scripted actor moves, 6 sound effects, at least 5 s of scripted waits.
  - No dialogue text in this script.
  - The script branches on flags, so only some of the messages/destinations above appear.
- Manual result (owner playtest, 2026-10-30): works; plays after the Minotaur scene and hands off as expected.
- [x] Plays and returns control

### 5. The Bloody Hallway (MAP038.MPD, zone 13 room 10)

- Area: Catacombs
- Scene: Room local event 0 (`MAP_MAP038.MPD-room_local-0-54960`)
- Kind: Puzzle time-trial exit (only fires while a time attack is running)
- Trigger (step on tile): flag 285 (time-trial running) == 1; Ashley on tile 0,3; Ashley idle (not mid-action)
- Trigger (step on tile): flag 285 (time-trial running) == 1; Ashley on tile 14,3; Ashley idle (not mid-action)
- Automated result: trigger never fired in the automated run
- What you should see (from the original script, not from a run):
  - The room timer stops. The cube-puzzle results show the completion time, the food-chain heading and the rank name. The frozen timer HUD remains visible until the results close; script op90 then clears it.
  - Message: "Solve the cube puzzles and beat your record time! To turn puzzles off, open the Puzzle Mode menu under Options, and select "OFF." This will save any solved puzzle routes."
  - Ashley leaves for one of: Faith Overcame Fear (MAP039, zone 13 room 11); Hall of Dying Hope (MAP036, zone 13 room 8) (depends on the exit or branch).
  - Scale: 1 scripted actor move.
  - The script branches on flags, so only some of the messages/destinations above appear.
- Owner playtest (2026-10-30, Debug run `1793394014536781-0`): owner reported that something did not load right after the clear. A subsequent screenshot confirms the food-chain heading and rank name are missing, while the completion time and following message render correctly (#244). Log check of that run:
  - Tick 7233: east exit (tile 14,3) fired; the script ran op31, cleared flag 285 and opened the cube-puzzle results screen (op89 / 12). MAINMENU, MENUBG, MENUF.PRG and IQ.DIS all loaded.
  - The results screen read the stopped time 0:36.23. The timer HUD stays drawn, frozen, until script op90 runs after the results screen closes; the original does the same (`_renderTimer` gated by `vs_battle_lowerScreenUiState`).
  - Message box shown and closed (tick 7927), fade, then MAP039 loaded at tick 7999 and its entry script ran to the end.
  - MAP039 showed a different enemy than on the first visit (Z013U20 instead of Z013U07/U14). This is correct: the MPD enemy table picks enemy 20 at 175 HP or more and enemy 14 at 174 HP or less (Ashley had 174 HP before, 197 after); enemy 7 was already killed.
  - No warnings or errors in `vs-native.log` or `events.jsonl`. Successful asset loading did not prove correct drawing: the shared texture-animation helpers were bound to the 69-entry ENDING atlas, but MENUF passes indices 105/106 for the food-chain heading and 107..122 for ranks.
- #244 fix: the three shared tint/flash/wipe helpers now take the caller's atlas explicitly. Generator-owned MENUF callers pass `Menu_Results_disMap`; ENDING callers retain `Ending_resultAtlasRegions`. Debug build and menu/ending regression selection pass (60 cases, 53,870 assertions), generator regression tests pass (171), repeat generation preserves owned sources, and changed-source quality reports zero new findings. These packet/source checks do not replace the owner's in-game replay.
- Post-fix owner replay (2026-10-30): owner confirmed "resolved." The screenshot shows time 0:30.81, "YOUR PLACE ON THE FOODCHAIN:" and the rank "LITTLE GREEN MAN" rendered correctly. Combined with the earlier message/exit evidence, this resolves checklist item 5.
- [x] Plays and returns control (owner confirmed the corrected results screen)

### 6. Tircolas Flow (MAP150.MPD, zone 32 room 3)

- Area: Town Center West
- Scene: Room local event 2 (`MAP_MAP150.MPD-room_local-2-70444`)
- Kind: Enemy-defeat scene
- Story gate: story progress (flag 0x82) < 33
- Trigger (after a combat action / enemy defeat): flag 0x82 (story progress) < 33; flag 193 == 0; flag 194 == 0; flag 195 == 0; enemy slot 2 killed; enemy slot 3 killed; enemy slot 4 killed
- Flag source: flag 193: local event 2 sets it to 1, local event 3 sets it to 1, local event 4 sets it to 1, local event 6 sets it to 1
- Flag source: flag 194: local event 2 sets it to 1, local event 3 sets it to 1, local event 5 sets it to 1, local event 7 sets it to 1
- Flag source: flag 195: local event 2 sets it to 1, local event 4 sets it to 1, local event 5 sets it to 1, local event 8 sets it to 1
- Automated result: trigger never fired in the automated run
- Note: Needs the room's enemies killed together (area spell).
- What you should see (from the original script, not from a run):
  - Duane, Sarjik and Bejart (room slots 2-4) fall together; flags 193-195 are still 0, so none of them died on an earlier action.
  - Message: "G-God... My soul...is not ready..."
  - Then EVENT/0036.EVT starts.
  - 3 actor groups are removed from the room (defeated or departing characters vanish).
  - Scale: 24 camera commands, 3 animations, at least 2 s of scripted waits.
  - The script branches on flags, so only some of the messages/destinations above appear.
- [ ] Plays and returns control

### 7. Tircolas Flow (MAP150.MPD, zone 32 room 3)

- Area: Town Center West
- Scene: Room local event 5 (`MAP_MAP150.MPD-room_local-5-72416`)
- Kind: Enemy-defeat scene
- Story gate: story progress (flag 0x82) < 33
- Trigger (after a combat action / enemy defeat): flag 0x82 (story progress) < 33; flag 194 == 0; flag 195 == 0; enemy slot 3 killed; enemy slot 4 killed
- Flag source: flag 194: local event 2 sets it to 1, local event 3 sets it to 1, local event 5 sets it to 1, local event 7 sets it to 1
- Flag source: flag 195: local event 2 sets it to 1, local event 4 sets it to 1, local event 5 sets it to 1, local event 8 sets it to 1
- Automated result: automated run was cut off (rejected, not a scene fault)
- What you should see (from the original script, not from a run):
  - Sarjik and Bejart (room slots 3-4) fall on the same action.
  - Then EVENT/0036.EVT starts.
  - 2 actor groups are removed from the room (defeated or departing characters vanish).
  - Scale: 2 animations.
  - No dialogue text in this script.
  - The script branches on flags, so only some of the messages/destinations above appear.
- [ ] Plays and returns control

### 8. The Suicide King (MAP264.MPD, zone 50 room 4)

- Area: Abandoned Mines B1
- Scene: Room local event 1 (`MAP_MAP264.MPD-room_local-1-52892`)
- Kind: Room event
- Trigger (press Cross (examine/talk)): flag 0 > 0; flag 177 > 0; flag 71 == 0; flag 70 < 2; facing tile 5,0
- Trigger (press Cross (examine/talk)): flag 0 > 0; flag 177 > 0; flag 71 == 0; flag 70 == 0; facing tile 5,15
- Flag source: flag 70: local event 2 sets it to 1, local event 2 sets it to 2, room entry sets it to 2
- Automated result: trigger never fired in the automated run
- What you should see (from the original script, not from a run):
  - Room entry message (shown on entering, before this scene): "The door has been locked!"
  - Message: "The door is locked!"
  - Message: "The door is still locked!"
  - Scale: 2 sound effects.
  - The script branches on flags, so only some of the messages/destinations above appear.
- [ ] Plays and returns control

### 9. The Suicide King (MAP264.MPD, zone 50 room 4)

- Area: Abandoned Mines B1
- Scene: Room local event 3 (`MAP_MAP264.MPD-room_local-3-53232`)
- Kind: Room timer runs out
- Trigger (room timer expires): flag 0 > 0; flag 33 == 0; flag 177 > 0; flag 71 == 0; flag 70 == 0; flag 162 == 0; flag 161 == 0; flag 160 == 0
- Flag source: flag 33: local event 3 sets it to 1
- Flag source: flag 70: local event 2 sets it to 1, local event 2 sets it to 2, room entry sets it to 2
- Automated result: trigger never fired in the automated run
- What you should see (from the original script, not from a run):
  - Room entry message (shown on entering, before this scene): "The door has been locked!"
  - The room timer stops and its display goes away.
  - No dialogue text in this script.
  - Nothing else is drawn: the script only clears the countdown and records the timeout (flag 33), then Ashley keeps control.
- [ ] Plays and returns control

### 10. The Passion of Lovers (MAP271.MPD, zone 50 room 11)

- Area: Abandoned Mines B1
- Scene: Room local event 0 (`MAP_MAP271.MPD-room_local-0-53980`)
- Kind: Room timer runs out
- Trigger (room timer expires): flag 66 == 2; flag 162 == 0; flag 161 == 0; flag 160 == 0
- Flag source: flag 66: local event 0 sets it to 0, local event 2 sets it to 0, room entry sets it to 2
- Automated result: trigger never fired in the automated run
- What you should see (from the original script, not from a run):
  - Room entry message (shown on entering, before this scene): "A door opened somewhere..."
  - The room timer stops and its display goes away.
  - Part of the room changes (a door, gate or platform opens, closes or moves).
  - Scale: 2 sound effects.
  - No dialogue text in this script.
  - The script branches on flags, so only some of the messages/destinations above appear.
- [ ] Plays and returns control

### 11. The Passion of Lovers (MAP271.MPD, zone 50 room 11)

- Area: Abandoned Mines B1
- Scene: Room local event 1 (`MAP_MAP271.MPD-room_local-1-54064`)
- Kind: Room event
- Trigger (press Cross (examine/talk)): flag 69 == 0; flag 67 == 1; flag 66 == 0; facing tile 2,13
- Flag source: flag 66: local event 0 sets it to 0, local event 2 sets it to 0, room entry sets it to 2
- Flag source: flag 69: local event 2 sets it to 1
- Automated result: automated run was cut off (rejected, not a scene fault)
- What you should see (from the original script, not from a run):
  - Room entry message (shown on entering, before this scene): "A door opened somewhere..."
  - Message: "The door is locked!"
  - Scale: 2 sound effects.
- [ ] Plays and returns control

### 12. The Passion of Lovers (MAP271.MPD, zone 50 room 11)

- Area: Abandoned Mines B1
- Scene: Room local event 2 (`MAP_MAP271.MPD-room_local-2-54184`)
- Kind: Room event
- Trigger (step on tile): flag 69 == 0; flag 67 == 1; flag 66 == 2; Ashley on tile 2,12; Ashley idle (not mid-action)
- Flag source: flag 66: local event 0 sets it to 0, local event 2 sets it to 0, room entry sets it to 2
- Flag source: flag 69: local event 2 sets it to 1
- Automated result: automated run was cut off (rejected, not a scene fault)
- What you should see (from the original script, not from a run):
  - Room entry message (shown on entering, before this scene): "A door opened somewhere..."
  - A doorway tile becomes passable or blocked.
  - The room timer stops and its display goes away.
  - Message: "Ashley made it through!"
  - Ashley leaves for The Earthquake's Mark (MAP269, zone 50 room 9).
  - Scale: 2 scripted actor moves.
- [ ] Plays and returns control

### 13. The Hall of Hope (MAP272.MPD, zone 50 room 12)

- Area: Abandoned Mines B1
- Scene: Room local event 0 (`MAP_MAP272.MPD-room_local-0-49932`)
- Kind: Room timer runs out
- Trigger (room timer expires): flag 66 == 2; flag 162 == 0; flag 161 == 0; flag 160 == 0
- Flag source: flag 66: local event 0 sets it to 0, local event 2 sets it to 0, room entry sets it to 2
- Automated result: trigger never fired in the automated run
- What you should see (from the original script, not from a run):
  - Room entry message (shown on entering, before this scene): "A door opened somewhere..."
  - The room timer stops and its display goes away.
  - Part of the room changes (a door, gate or platform opens, closes or moves).
  - Scale: 2 sound effects.
  - No dialogue text in this script.
  - The script branches on flags, so only some of the messages/destinations above appear.
- [ ] Plays and returns control

### 14. The Hall of Hope (MAP272.MPD, zone 50 room 12)

- Area: Abandoned Mines B1
- Scene: Room local event 2 (`MAP_MAP272.MPD-room_local-2-50136`)
- Kind: Room event
- Trigger (step on tile): flag 68 == 0; flag 67 == 0; flag 66 == 2; Ashley on tile 14,1; Ashley idle (not mid-action)
- Flag source: flag 66: local event 0 sets it to 0, local event 2 sets it to 0, room entry sets it to 2
- Flag source: flag 68: local event 2 sets it to 1
- Automated result: not run yet
- What you should see (from the original script, not from a run):
  - Room entry message (shown on entering, before this scene): "A door opened somewhere..."
  - A doorway tile becomes passable or blocked.
  - The room timer stops and its display goes away.
  - Message: "Ashley made it through!"
  - Ashley leaves for The Dark Tunnel (MAP273, zone 50 room 13).
  - Scale: 2 scripted actor moves.
- [ ] Plays and returns control

### 15. Clash of Hyaenas (MAP278.MPD, zone 50 room 18)

- Area: Abandoned Mines B1
- Scene: Room local event 0 (`MAP_MAP278.MPD-room_local-0-53424`)
- Kind: Puzzle time-trial exit (only fires while a time attack is running)
- Trigger (step on tile): flag 285 (time-trial running) == 1; Ashley on tile 12,2; Ashley idle (not mid-action)
- Trigger (step on tile): flag 285 (time-trial running) == 1; Ashley on tile 2,13; Ashley idle (not mid-action)
- Automated result: not run yet
- What you should see (from the original script, not from a run):
  - The room timer stops and its display goes away.
  - Ashley leaves for one of: Greed Knows No Bounds (MAP279, zone 50 room 19); The Smeltry (MAP277, zone 50 room 17) (depends on the exit or branch).
  - Scale: 1 scripted actor move.
  - No dialogue text in this script.
  - The script branches on flags, so only some of the messages/destinations above appear.
- [ ] Plays and returns control

### 16. Hewn from Nature (MAP203.MPD, zone 40 room 24)

- Area: Snowfly Forest
- Scene: Room local event 7 (`MAP_MAP203.MPD-room_local-7-59336`)
- Kind: Enemy-defeat scene
- Trigger (after a combat action / enemy defeat): flag 187 < 22; flag 70 == 0; flag 68 == 0; enemy slot 4 killed
- Trigger (after a combat action / enemy defeat): flag 187 < 22; flag 70 == 0; flag 69 == 0; enemy slot 4 killed
- Flag source: flag 68: local event 2 sets it to 1, local event 3 sets it to 1, local event 5 sets it to 1
- Flag source: flag 69: local event 2 sets it to 1, local event 4 sets it to 1, local event 6 sets it to 1
- Flag source: flag 70: local event 2 sets it to 1, local event 3 sets it to 1, local event 4 sets it to 1, local event 7 sets it to 1
- Flag source: flag 187: local event 2 sets it to 22, local event 3 sets it to 22, local event 4 sets it to 22, local event 5 sets it to 22, local event 6 sets it to 22
- Automated result: trigger never fired in the automated run
- What you should see (from the original script, not from a run):
  - Sydney (room slot 4) is beaten while Grissom (slot 2) or the Dark Crusader (slot 3) still stands; Sydney refuses to die and leaves.
  - A spell/visual effect plays on an actor.
  - Message: "It is not my time!"
  - 1 actor group is removed from the room (defeated or departing characters vanish).
  - Scale: 20 camera commands, 1 animation, 2 sound effects, at least 2 s of scripted waits.
- [ ] Plays and returns control

### 17. Revelation Shaft (MAP411.MPD, zone 51 room 28)

- Area: Abandoned Mines B2
- Scene: Room local event 0 (`MAP_MAP411.MPD-room_local-0-53656`)
- Kind: Puzzle time-trial exit (only fires while a time attack is running)
- Trigger (step on tile): flag 285 (time-trial running) == 1; Ashley on tile 0,11; Ashley idle (not mid-action)
- Trigger (step on tile): flag 285 (time-trial running) == 1; Ashley on tile 11,1; Ashley idle (not mid-action)
- Automated result: not run yet
- What you should see (from the original script, not from a run):
  - The room timer stops and its display goes away.
  - Part of the room changes (a door, gate or platform opens, closes or moves).
  - Ashley leaves for one of: Gambler's Passage (MAP284, zone 51 room 0); Corridor of Shade (MAP410, zone 51 room 27) (depends on the exit or branch).
  - Scale: 1 animation, 1 scripted actor move, 2 sound effects.
  - No dialogue text in this script.
  - The script branches on flags, so only some of the messages/destinations above appear.
- [ ] Plays and returns control

### 18. Gemsword Blackmarket (MAP252.MPD, zone 49 room 5)

- Area: Undercity East
- Scene: Room local event 0 (`MAP_MAP252.MPD-room_local-0-57680`)
- Kind: Cross at a room exit while room flag 40 is 1
- Trigger (press Cross (examine/talk)): flag 40 == 1; facing tile 5,18
- Trigger (press Cross (examine/talk)): flag 40 == 1; facing tile 5,0
- Flag source: flag 40: room entry sets it to 1
- Automated result: trigger never fired in the automated run
- What you should see (from the original script, not from a run):
  - Message: "The door is locked!"
  - Scale: 2 sound effects.
- [ ] Plays and returns control

### 19. Where Black Waters Ran (MAP257.MPD, zone 49 room 10)

- Area: Undercity East
- Scene: Room local event 0 (`MAP_MAP257.MPD-room_local-0-58020`)
- Kind: Cross at a room exit while room flag 40 is 1
- Trigger (press Cross (examine/talk)): flag 40 == 1; facing tile 5,18
- Trigger (press Cross (examine/talk)): flag 40 == 1; facing tile 5,0
- Flag source: flag 40: room entry sets it to 1
- Automated result: trigger never fired in the automated run
- What you should see (from the original script, not from a run):
  - Message: "The door is locked!"
  - Scale: 2 sound effects.
- [ ] Plays and returns control

### 20. Fear of the Fall (MAP235.MPD, zone 48 room 15)

- Area: Undercity West
- Scene: Room local event 1 (`MAP_MAP235.MPD-room_local-1-56736`)
- Kind: Room event
- Trigger (press Cross (examine/talk)): flag 853 == 1; flag 295 > 0; facing tile 5,18
- Flag source: flag 853: local event 1 sets it to 2, room entry sets it to 1
- Automated result: trigger never fired in the automated run
- What you should see (from the original script, not from a run):
  - Message: "Unlatched the door!"
  - Scale: 2 sound effects.
- [ ] Plays and returns control

### 21. The Fallen Hall (MAP318.MPD, zone 53 room 7)

- Area: Limestone Quarry
- Scene: Room local event 2 (`MAP_MAP318.MPD-room_local-2-55676`)
- Kind: Room timer runs out
- Trigger (room timer expires): flag 177 > 0; flag 33 == 0; flag 66 == 1; flag 162 == 0; flag 161 == 0; flag 160 == 0
- Flag source: flag 33: local event 2 sets it to 1
- Automated result: trigger never fired in the automated run
- What you should see (from the original script, not from a run):
  - Room entry message (shown on entering, before this scene): "The door has been locked!"
  - The room timer stops and its display goes away.
  - No dialogue text in this script.
  - Nothing else is drawn: the script only clears the countdown and records the timeout (flag 33), then Ashley keeps control.
- [ ] Plays and returns control

### 22. Screams of the Wounded (MAP321.MPD, zone 53 room 10)

- Area: Limestone Quarry
- Scene: Room local event 2 (`MAP_MAP321.MPD-room_local-2-55676`)
- Kind: Room timer runs out
- Trigger (room timer expires): flag 177 > 0; flag 33 == 0; flag 66 == 3; flag 162 == 0; flag 161 == 0; flag 160 == 0
- Flag source: flag 33: local event 2 sets it to 1
- Automated result: trigger never fired in the automated run
- What you should see (from the original script, not from a run):
  - Room entry message (shown on entering, before this scene): "The door has been locked!"
  - The room timer stops and its display goes away.
  - No dialogue text in this script.
  - Nothing else is drawn: the script only clears the countdown and records the timeout (flag 33), then Ashley keeps control.
- [ ] Plays and returns control

### 23. The Dreamer's Climb (MAP323.MPD, zone 53 room 12)

- Area: Limestone Quarry
- Scene: Room local event 0 (`MAP_MAP323.MPD-room_local-0-58528`)
- Kind: Puzzle time-trial exit (only fires while a time attack is running)
- Trigger (step on tile): flag 285 (time-trial running) == 1; Ashley on tile 0,3; Ashley idle (not mid-action)
- Trigger (step on tile): flag 285 (time-trial running) == 1; Ashley on tile 17,4; Ashley idle (not mid-action)
- Trigger (step on tile): flag 285 (time-trial running) == 1; Ashley on tile 8,14; Ashley idle (not mid-action)
- Automated result: not run yet
- What you should see (from the original script, not from a run):
  - The room timer stops and its display goes away.
  - Part of the room changes (a door, gate or platform opens, closes or moves).
  - Ashley leaves for one of: Sinner's Sustenence (MAP324, zone 53 room 13); The Rotten Core (MAP319, zone 53 room 8); The Ore-Bearers (MAP322, zone 53 room 11) (depends on the exit or branch).
  - Scale: 1 animation, 1 scripted actor move, 2 sound effects.
  - No dialogue text in this script.
  - The script branches on flags, so only some of the messages/destinations above appear.
- [ ] Plays and returns control

### 24. The Laborer's Bonfire (MAP332.MPD, zone 53 room 21)

- Area: Limestone Quarry
- Scene: Room local event 0 (`MAP_MAP332.MPD-room_local-0-58640`)
- Kind: Puzzle time-trial exit (only fires while a time attack is running)
- Trigger (step on tile): flag 285 (time-trial running) == 1; Ashley on tile 17,12; Ashley idle (not mid-action)
- Trigger (step on tile): flag 285 (time-trial running) == 1; Ashley on tile 0,11; Ashley idle (not mid-action)
- Trigger (step on tile): flag 285 (time-trial running) == 1; Ashley on tile 9,1; Ashley idle (not mid-action)
- Automated result: not run yet
- What you should see (from the original script, not from a run):
  - The room timer stops and its display goes away.
  - Part of the room changes (a door, gate or platform opens, closes or moves).
  - Ashley leaves for one of: Torture Without End (MAP334, zone 53 room 23); Stone and Sulfurous Fire (MAP333, zone 53 room 22); The Auction Block (MAP327, zone 53 room 16) (depends on the exit or branch).
  - Scale: 1 animation, 1 scripted actor move, 2 sound effects.
  - No dialogue text in this script.
  - The script branches on flags, so only some of the messages/destinations above appear.
- [ ] Plays and returns control

### 25. Hall of the Wage-Paying (MAP340.MPD, zone 53 room 29)

- Area: Limestone Quarry
- Scene: Room local event 0 (`MAP_MAP340.MPD-room_local-0-55916`)
- Kind: Cross at a room exit while room flag 40 is 1
- Trigger (press Cross (examine/talk)): flag 40 == 1; facing tile 6,17
- Trigger (press Cross (examine/talk)): flag 40 == 1; facing tile 6,0
- Flag source: flag 40: room entry sets it to 1
- Automated result: scene started but did not finish
- Note: Scene started in the Lab but a boss attack cut it short.
- What you should see (from the original script, not from a run):
  - Message: "The door is locked!"
  - Scale: 2 sound effects.
- [ ] Plays and returns control

### 26. Tunnel of the Heartless (MAP342.MPD, zone 53 room 31)

- Area: Limestone Quarry
- Scene: Room local event 0 (`MAP_MAP342.MPD-room_local-0-55124`)
- Kind: Puzzle time-trial exit (only fires while a time attack is running)
- Trigger (step on tile): flag 285 (time-trial running) == 1; Ashley on tile 6,0; Ashley idle (not mid-action)
- Trigger (step on tile): flag 285 (time-trial running) == 1; Ashley on tile 6,16; Ashley idle (not mid-action)
- Automated result: not run yet
- What you should see (from the original script, not from a run):
  - The room timer stops and its display goes away.
  - Part of the room changes (a door, gate or platform opens, closes or moves).
  - Ashley leaves for one of: The Dark Coast (MAP139, zone 30 room 0); Hall of the Wage-Paying (MAP340, zone 53 room 29) (depends on the exit or branch).
  - Scale: 1 animation, 1 scripted actor move, 2 sound effects.
  - No dialogue text in this script.
  - The script branches on flags, so only some of the messages/destinations above appear.
- [ ] Plays and returns control

### 27. Slaugher of the Innocent (MAP344.MPD, zone 54 room 1)

- Area: Forgotten Pathway
- Scene: Room local event 0 (`MAP_MAP344.MPD-room_local-0-55216`)
- Kind: Cross at a room exit while room flag 40 is 1
- Trigger (press Cross (examine/talk)): flag 40 == 1; facing tile 6,17
- Trigger (press Cross (examine/talk)): flag 40 == 1; facing tile 6,0
- Flag source: flag 40: local event 1 sets it to 0, room entry sets it to 1
- Automated result: trigger never fired in the automated run
- Note: Route around the wall to tile 6,16 was not found by the bot.
- What you should see (from the original script, not from a run):
  - Room entry message (shown on entering, before this scene): "The door has been locked!"
  - Message: "The door is locked!"
  - Message: "The door is locked!"
  - Scale: 2 sound effects.
  - The script branches on flags, so only some of the messages/destinations above appear.
- [ ] Plays and returns control

### 28. Wooden Horse (MAP361.MPD, zone 55 room 2)

- Area: Iron Maiden B1
- Scene: Room local event 0 (`MAP_MAP361.MPD-room_local-0-49604`)
- Kind: Puzzle time-trial exit (only fires while a time attack is running)
- Trigger (step on tile): flag 285 (time-trial running) == 1; Ashley on tile 2,0; Ashley idle (not mid-action)
- Trigger (step on tile): flag 285 (time-trial running) == 1; Ashley on tile 2,9; Ashley idle (not mid-action)
- Trigger (step on tile): Ashley on tile 2,10; Ashley idle (not mid-action)
- Automated result: not run yet
- What you should see (from the original script, not from a run):
  - The room timer stops and its display goes away.
  - Part of the room changes (a door, gate or platform opens, closes or moves).
  - Ashley leaves for one of: The Cauldron (MAP360, zone 55 room 1); Starvation (MAP362, zone 55 room 3) (depends on the exit or branch).
  - Scale: 1 animation, 1 scripted actor move, 2 sound effects.
  - No dialogue text in this script.
  - The script branches on flags, so only some of the messages/destinations above appear.
- [ ] Plays and returns control

### 29. The Pear (MAP364.MPD, zone 55 room 5)

- Area: Iron Maiden B1
- Scene: Room local event 0 (`MAP_MAP364.MPD-room_local-0-58876`)
- Kind: Puzzle time-trial exit (only fires while a time attack is running)
- Trigger (step on tile): flag 285 (time-trial running) == 1; Ashley on tile 8,15; Ashley idle (not mid-action)
- Trigger (step on tile): flag 285 (time-trial running) == 1; Ashley on tile 17,4; Ashley idle (not mid-action)
- Trigger (step on tile): flag 285 (time-trial running) == 1; Ashley on tile 1,3; Ashley idle (not mid-action)
- Automated result: not run yet
- What you should see (from the original script, not from a run):
  - The room timer stops and its display goes away.
  - Ashley leaves for one of: The Whirligig (MAP365, zone 55 room 6); The Breast Ripper (MAP363, zone 55 room 4); The Judas Cradle (MAP380, zone 55 room 21) (depends on the exit or branch).
  - Scale: 1 scripted actor move.
  - No dialogue text in this script.
  - The script branches on flags, so only some of the messages/destinations above appear.
- [ ] Plays and returns control

### 30. Burning (MAP372.MPD, zone 55 room 13)

- Area: Iron Maiden B1
- Scene: Room local event 0 (`MAP_MAP372.MPD-room_local-0-53560`)
- Kind: Puzzle time-trial exit (only fires while a time attack is running)
- Trigger (step on tile): flag 285 (time-trial running) == 1; Ashley on tile 1,2; Ashley idle (not mid-action)
- Trigger (step on tile): flag 285 (time-trial running) == 1; Ashley on tile 11,13; Ashley idle (not mid-action)
- Automated result: not run yet
- What you should see (from the original script, not from a run):
  - The room timer stops and its display goes away.
  - Part of the room changes (a door, gate or platform opens, closes or moves).
  - Ashley leaves for one of: Cleansing the Soul (MAP373, zone 55 room 14); Burial (MAP371, zone 55 room 12) (depends on the exit or branch).
  - Scale: 1 animation, 1 scripted actor move, 2 sound effects.
  - No dialogue text in this script.
  - The script branches on flags, so only some of the messages/destinations above appear.
- [ ] Plays and returns control

### 31. Impalement (MAP376.MPD, zone 55 room 17)

- Area: Iron Maiden B1
- Scene: Room local event 0 (`MAP_MAP376.MPD-room_local-0-49904`)
- Kind: Puzzle time-trial exit (only fires while a time attack is running)
- Trigger (step on tile): flag 285 (time-trial running) == 1; Ashley on tile 2,0; Ashley idle (not mid-action)
- Trigger (step on tile): flag 285 (time-trial running) == 1; Ashley on tile 2,9; Ashley idle (not mid-action)
- Trigger (step on tile): Ashley on tile 2,10; Ashley idle (not mid-action)
- Automated result: not run yet
- What you should see (from the original script, not from a run):
  - The room timer stops and its display goes away.
  - Part of the room changes (a door, gate or platform opens, closes or moves).
  - Ashley leaves for one of: Hanging (MAP375, zone 55 room 16); Knotting (MAP377, zone 55 room 18) (depends on the exit or branch).
  - Scale: 1 animation, 1 scripted actor move, 2 sound effects.
  - No dialogue text in this script.
  - The script branches on flags, so only some of the messages/destinations above appear.
- [ ] Plays and returns control

### 32. Swords for the Land (MAP109.MPD, zone 28 room 4)

- Area: City Walls South
- Scene: Room local event 3 (`MAP_MAP109.MPD-room_local-3-55008`)
- Kind: Room timer runs out
- Trigger (room timer expires): flag 177 > 0; flag 66 == 0; flag 33 == 0; flag 65 == 0; flag 162 == 0; flag 161 == 0; flag 160 == 0
- Flag source: flag 33: local event 3 sets it to 1
- Flag source: flag 65: local event 2 sets it to 1, local event 2 sets it to 2
- Automated result: trigger never fired in the automated run
- What you should see (from the original script, not from a run):
  - Room entry message (shown on entering, before this scene): "The door has been locked!"
  - The room timer stops and its display goes away.
  - No dialogue text in this script.
  - Nothing else is drawn: the script only clears the countdown and records the timeout (flag 33), then Ashley keeps control.
- [ ] Plays and returns control

### 33. Tunnel of the Heartless (MAP342.MPD, zone 53 room 31)

- Area: Limestone Quarry
- Scene: Room local event 1 (`MAP_MAP342.MPD-room_local-1-55292`)
- Kind: Room event
- Story gate: story progress (flag 0x82) == 71
- Trigger (step on tile): flag 0x82 (story progress) == 71; Ashley on tile 6,17; Ashley idle (not mid-action)
- Automated result: trigger never fired in the automated run
- What you should see (from the original script, not from a run):
  - Ashley leaves for Great Cathedral: Hardin's past (MAP103, zone 57 room 0).
  - No dialogue text in this script.
- [ ] Plays and returns control

### 34. Destruction and Rebirth (MAP121.MPD, zone 28 room 16)

- Area: City Walls North
- Scene: Room local event 3 (`MAP_MAP121.MPD-room_local-3-52708`)
- Kind: Room timer runs out
- Trigger (room timer expires): flag 177 > 0; flag 33 == 0; flag 66 == 0; flag 65 == 0; flag 162 == 0; flag 161 == 0; flag 160 == 0
- Flag source: flag 33: local event 3 sets it to 1
- Flag source: flag 65: local event 2 sets it to 1, local event 2 sets it to 2
- Automated result: trigger never fired in the automated run
- What you should see (from the original script, not from a run):
  - Room entry message (shown on entering, before this scene): "The door has been locked!"
  - The room timer stops and its display goes away.
  - No dialogue text in this script.
  - Nothing else is drawn: the script only clears the countdown and records the timeout (flag 33), then Ashley keeps control.
- [ ] Plays and returns control

### 35. Those who Drink the Dark (MAP141.MPD, zone 30 room 2)

- Area: Temple of Kiltia
- Scene: Room local event 0 (`MAP_MAP141.MPD-room_local-0-57008`)
- Kind: Puzzle time-trial exit (only fires while a time attack is running)
- Trigger (step on tile): flag 285 (time-trial running) == 1; Ashley on tile 13,7; Ashley idle (not mid-action)
- Trigger (step on tile): flag 285 (time-trial running) == 1; Ashley on tile 0,1; Ashley idle (not mid-action)
- Trigger (step on tile): flag 285 (time-trial running) == 1; Ashley on tile 9,18; Ashley idle (not mid-action)
- Trigger (step on tile): flag 285 (time-trial running) == 1; Ashley on tile 10,18; Ashley idle (not mid-action)
- Automated result: automated run was cut off (rejected, not a scene fault)
- What you should see (from the original script, not from a run):
  - The room timer stops and its display goes away.
  - Part of the room changes (a door, gate or platform opens, closes or moves).
  - Ashley leaves for one of: The Chapel of Meschaunce (MAP142, zone 30 room 3); Ants Prepare for Winter (MAP330, zone 53 room 19); Hall of Prayer (MAP140, zone 30 room 1) (depends on the exit or branch).
  - Scale: 1 animation, 1 scripted actor move, 2 sound effects.
  - No dialogue text in this script.
  - The script branches on flags, so only some of the messages/destinations above appear.
- [ ] Plays and returns control

### 36. The Resentful Ones (MAP143.MPD, zone 30 room 4)

- Area: Temple of Kiltia
- Scene: Room local event 0 (`MAP_MAP143.MPD-room_local-0-57228`)
- Kind: Puzzle time-trial exit (only fires while a time attack is running)
- Trigger (step on tile): flag 285 (time-trial running) == 1; Ashley on tile 17,16; Ashley idle (not mid-action)
- Trigger (step on tile): flag 285 (time-trial running) == 1; Ashley on tile 18,16; Ashley idle (not mid-action)
- Trigger (step on tile): flag 285 (time-trial running) == 1; Ashley on tile 10,3; Ashley idle (not mid-action)
- Automated result: trigger never fired in the automated run
- What you should see (from the original script, not from a run):
  - The room timer stops and its display goes away.
  - Part of the room changes (a door, gate or platform opens, closes or moves).
  - Ashley leaves for one of: Those who Fear the Light (MAP144, zone 30 room 5); Hall of Prayer (MAP140, zone 30 room 1) (depends on the exit or branch).
  - Scale: 1 animation, 1 scripted actor move, 2 sound effects.
  - No dialogue text in this script.
  - The script branches on flags, so only some of the messages/destinations above appear.
- [ ] Plays and returns control

### 37. Chamber of Reason (MAP145.MPD, zone 31 room 0)

- Area: Temple of Kiltia
- Scene: Room local event 0 (`MAP_MAP145.MPD-room_local-0-60472`)
- Kind: Cross at a room exit while room flag 40 is 1
- Trigger (press Cross (examine/talk)): flag 40 == 1; facing tile 6,18
- Trigger (press Cross (examine/talk)): flag 40 == 1; facing tile 5,0
- Trigger (press Cross (examine/talk)): flag 40 == 1; facing tile 6,0
- Trigger (press Cross (examine/talk)): flag 40 == 1; facing tile 5,18
- Automated result: trigger never fired in the automated run
- What you should see (from the original script, not from a run):
  - Message: "The door is locked!"
  - Scale: 2 sound effects.
- [ ] Plays and returns control

### 38. Truth and Lies (MAP067.MPD, zone 22 room 4)

- Area: Great Cathedral B1
- Scene: Room local event 0 (`MAP_MAP067.MPD-room_local-0-56500`)
- Kind: Cross at a room exit while room flag 40 is 1
- Trigger (press Cross (examine/talk)): flag 40 == 1; facing tile 12,1
- Trigger (press Cross (examine/talk)): flag 40 == 1; facing tile 12,13
- Trigger (press Cross (examine/talk)): flag 40 == 1; facing tile 0,13
- Flag source: flag 40: room entry sets it to 1
- Automated result: trigger never fired in the automated run
- What you should see (from the original script, not from a run):
  - Message: "The door is locked!"
  - Scale: 2 sound effects.
- [ ] Plays and returns control

### 39. The Heretics' Story (MAP077.MPD, zone 23 room 4)

- Area: Great Cathedral L3
- Scene: Room local event 1 (`MAP_MAP077.MPD-room_local-1-62588`)
- Kind: Room event
- Trigger (press Cross (examine/talk)): flag 279 == 1; facing tile 9,22
- Trigger (press Cross (examine/talk)): flag 279 == 1; facing tile 10,22
- Flag source: flag 279: local event 1 sets it to 2
- Automated result: trigger never fired in the automated run
- What you should see (from the original script, not from a run):
  - Part of the room changes (a door, gate or platform opens, closes or moves).
  - The screen fades out.
  - Ashley leaves for Where the Soul Rots (MAP097, zone 25 room 2).
  - Scale: 27 camera commands, 2 animations, 3 scripted actor moves, 1 sound effect, at least 4 s of scripted waits.
  - No dialogue text in this script.
  - The script branches on flags, so only some of the messages/destinations above appear.
- [ ] Plays and returns control

### 40. The Melodics of Madness (MAP091.MPD, zone 24 room 12)

- Area: Great Cathedral L2
- Scene: Room local event 0 (`MAP_MAP091.MPD-room_local-0-54792`)
- Kind: Puzzle time-trial exit (only fires while a time attack is running)
- Trigger (step on tile): flag 285 (time-trial running) == 1; Ashley on tile 4,15; Ashley idle (not mid-action)
- Trigger (step on tile): flag 285 (time-trial running) == 1; Ashley on tile 5,15; Ashley idle (not mid-action)
- Trigger (step on tile): flag 285 (time-trial running) == 1; Ashley on tile 4,1; Ashley idle (not mid-action)
- Trigger (step on tile): flag 285 (time-trial running) == 1; Ashley on tile 5,1; Ashley idle (not mid-action)
- Automated result: trigger never fired in the automated run
- What you should see (from the original script, not from a run):
  - The room timer stops and its display goes away.
  - Part of the room changes (a door, gate or platform opens, closes or moves).
  - Ashley leaves for one of: What Ails You, Kills You (MAP096, zone 25 room 1); The Hall of Broken Vows (MAP090, zone 24 room 11) (depends on the exit or branch).
  - Scale: 1 animation, 1 scripted actor move, 2 sound effects.
  - No dialogue text in this script.
  - The script branches on flags, so only some of the messages/destinations above appear.
- [ ] Plays and returns control

### 41. What Ails You, Kills You (MAP096.MPD, zone 25 room 1)

- Area: Great Cathedral L2
- Scene: Room local event 0 (`MAP_MAP096.MPD-room_local-0-56448`)
- Kind: Cross at a room exit while room flag 40 is 1
- Trigger (press Cross (examine/talk)): flag 40 == 1; facing tile 7,12
- Trigger (press Cross (examine/talk)): flag 40 == 1; facing tile 8,12
- Flag source: flag 40: room entry sets it to 1
- Automated result: trigger never fired in the automated run
- What you should see (from the original script, not from a run):
  - Message: "The door is locked!"
  - Scale: 2 sound effects.
- [ ] Plays and returns control

### 42. Ashley fights Guildestern, part 2 (MAP102.MPD, zone 27 room 1)

- Area: Miscellaneous
- Scene: Room local event 0 (`MAP_MAP102.MPD-room_local-0-45008`)
- Kind: Battle-stage scene (checked every frame)
- Trigger (checked every frame while Ashley is free): flag 166 == 1; Ashley idle (not mid-action)
- Flag source: flag 166: local event 0 sets it to 4, local event 1 sets it to 0, local event 2 sets it to 0
- Automated result: automated run was cut off (rejected, not a scene fault)
- What you should see (from the original script, not from a run):
  - Guildenstern fight stage change (flag 166 == 1).
  - The field camera angle changes (pitch preset / orbit back).
  - Scale: 98 camera commands, 1 animation, 6 scripted actor moves, 7 sound effects, at least 5 s of scripted waits.
  - No dialogue text in this script.
  - The script branches on flags, so only some of the messages/destinations above appear.
- [ ] Plays and returns control

### 43. Ashley fights Guildestern, part 2 (MAP102.MPD, zone 27 room 1)

- Area: Miscellaneous
- Scene: Room local event 1 (`MAP_MAP102.MPD-room_local-1-46184`)
- Kind: Battle-stage scene (checked every frame)
- Trigger (checked every frame while Ashley is free): flag 166 == 5; Ashley idle (not mid-action)
- Flag source: flag 166: local event 0 sets it to 4, local event 1 sets it to 0, local event 2 sets it to 0
- Automated result: automated run was cut off (rejected, not a scene fault)
- What you should see (from the original script, not from a run):
  - Guildenstern fight stage change (flag 166 == 5).
  - The field camera angle changes (pitch preset / orbit back).
  - Scale: 7 camera commands.
  - No dialogue text in this script.
- [ ] Plays and returns control

### 44. Ashley fights Guildestern, part 2 (MAP102.MPD, zone 27 room 1)

- Area: Miscellaneous
- Scene: Room local event 2 (`MAP_MAP102.MPD-room_local-2-46276`)
- Kind: Battle-stage scene (checked every frame)
- Trigger (checked every frame while Ashley is free): flag 166 == 6; Ashley idle (not mid-action)
- Flag source: flag 166: local event 0 sets it to 4, local event 1 sets it to 0, local event 2 sets it to 0
- Automated result: automated run was cut off (rejected, not a scene fault)
- What you should see (from the original script, not from a run):
  - Guildenstern fight stage change (flag 166 == 6).
  - The field camera angle changes (pitch preset / orbit back).
  - Scale: 7 camera commands.
  - No dialogue text in this script.
- [ ] Plays and returns control

### 45. The Shin-Vice (MAP396.MPD, zone 56 room 14)

- Area: Iron Maiden B2
- Scene: Room local event 3 (`MAP_MAP396.MPD-room_local-3-53192`)
- Kind: Enemy-defeat scene
- Trigger (after a combat action / enemy defeat): flag 40 == 1; actor model 0x63 killed; actor model 0x59 killed
- Flag source: flag 40: local event 1 sets it to 0, local event 2 sets it to 0, local event 3 sets it to 0, room entry sets it to 1
- Automated result: trigger never fired in the automated run
- What you should see (from the original script, not from a run):
  - Both the Ogre Zombie (kind 0x63) and Death (kind 0x59) are dead.
  - Room entry message (shown on entering, before this scene): "The door has been locked!"
  - The room timer stops and its display goes away.
  - The music stops.
  - The music changes.
  - Part of the room changes (a door, gate or platform opens, closes or moves).
  - Message: "The door has been unlocked!"
  - 4 actor groups are removed from the room (defeated or departing characters vanish).
  - Ashley leaves for Wiping Blood from Blades (MAP130, zone 29 room 6).
  - Scale: 4 animations, 3 sound effects, at least 3 s of scripted waits.
  - The script branches on flags, so only some of the messages/destinations above appear.
- [ ] Plays and returns control

### 46. The Iron Maiden (MAP404.MPD, zone 56 room 22)

- Area: Iron Maiden B3
- Scene: External event EVENT/0074.EVT (`EVENT_0074.EVT-external_event-74-0`)
- Kind: Enemy-defeat scene
- Trigger (after a combat action / enemy defeat): flag 283 <= 90; actor model 0x67 killed
- Trigger (after a combat action / enemy defeat): flag 186 > 0; actor model 0x67 killed
- Trigger (chained): started by a script in EVENT/0507.EVT
- Automated result: trigger never fired in the automated run
- What you should see (from the original script, not from a run):
  - Plays when Asura (zone 56 enemy kind 0x67) dies in The Iron Maiden (Iron Maiden B3).
  - The room timer stops and its display goes away.
  - The music stops.
  - A spell/visual effect plays on an actor.
  - The field camera angle changes (pitch preset / orbit back).
  - Part of the room changes (a door, gate or platform opens, closes or moves).
  - The music changes.
  - 2 actor groups are removed from the room (defeated or departing characters vanish).
  - Ashley leaves for Wiping Blood from Blades (MAP130, zone 29 room 6).
  - Scale: 47 camera commands, 6 animations, 2 scripted actor moves, 8 sound effects, at least 6 s of scripted waits.
  - No dialogue text in this script.
  - The script branches on flags, so only some of the messages/destinations above appear.
- [ ] Plays and returns control
