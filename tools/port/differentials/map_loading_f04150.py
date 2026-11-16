"""F04150 _loadArmFile (MENU5.PRG 4D8.c:448), typed Menu::map::LoadSceneMap."""

from map_loading_common import SCENE, configure

globals().update(
    configure(
        4150,
        SCENE,
        0x80103418,
        0x80103684,
        {
            "paling-flag-not-set": {0x80103448: 0x00000000},
            "restore-flag-replaces": {0x80103468: 0x00000000},
            "pitch": {0x801034AC: 0x24020239},
            "yaw-base": {0x801034BC: 0x24020801},
            "scene-limit": {0x801034D8: 0x2842001F},
            "row-not-stored": {0x80103508: 0x00000000},
            "brightness": {0x80103518: 0x24020081},
            "geometry-x": {0x8010352C: 0x2402FE81},
            "camera-distance": {0x8010354C: 0x24020301},
            "commands-state": {0x80103558: 0x24020004},
            "paling-not-stored": {0x80103570: 0x00000000},
            "loaded-state-three": {0x801035B4: 0x24020003},
            "names-not-stored": {0x801035E0: 0x00000000},
            "room-scale-two": {0x801035EC: 0x24050002},
            "initial-not-stored": {0x8010360C: 0x00000000},
            "fog-near": {0x8010361C: 0x24040301},
            "name-color-two": {0x8010364C: 0x24050002},
            "loaded-result-zero": {0x80103660: 0x24020000},
        },
    )
)
