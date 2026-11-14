"""F01439 func_800CF484 (BATTLE.PRG 5BF94.c:5548), typed secondary effect phase."""

from effect_instances_differential import PHASE, configure

globals().update(
    configure(
        1439,
        PHASE,
        0x800CF484,
        0x800CF48C,
        {
            "effect-id-set": {0x800CF488: 0xA4A40D54},
            "parameter-set": {0x800CF488: 0xA4A40D58},
            "phase-kept": {0x800CF488: 0x00000000},
        },
    )
)
