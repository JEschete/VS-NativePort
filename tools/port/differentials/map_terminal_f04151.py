"""F04151 _menuManager, MENU5.PRG 4D8.c:523; typed map::UpdateMapMenu."""

from map_terminal_common import MENU, configure

globals().update(configure(4151, MENU, 0x80103684, 0x801042B0, {
    "label-y": {0x801036B4: 0x240200BB},
    "no-select-toggle": {0x801036E4: 0},
    "initial-brightness-step": {0x801037A0: 0x2484FFF1},
    "room-text-columns": {0x801037AC: 0x2402000F},
    "circle-volume": {0x8010384C: 0x24120004},
    "triangle-quit-result": {0x801038B0: 0x24020001},
    "yaw-step": {0x80103980: 0x2442003F},
    "no-room-change": {0x80103F48: 0},
    "no-room-text": {0x80103F90: 0},
    "skip-chooser-poll": {0x80103FAC: 0},
    "no-scene-free": {0x80104104: 0},
    "no-scene-copy": {0x801041B4: 0},
    "no-read-release": {0x80104164: 0},
    "no-centering": {0x80104284: 0},
}))
