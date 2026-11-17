"""F02668 original limb panel with actual nested limb drawing and state stores."""

from status_limbs_common import configure

globals().update(
    configure(
        2668,
        {
            "selection-no-decrement": {0x80104624: 0x02001021},
            "wide-selection-store": {0x8010462C: 0xA4624F6A},
            "query-signed-phase": {0x80104640: 0x804280C5},
            "leave-phase-two": {0x8010465C: 0x24020002},
            "actor-load-next-byte": {0x80104660: 0x904280A9},
            "init-actor-no-decrement": {0x80104668: 0x00000000},
            "no-comparison-service": {0x80104674: 0x00000000},
            "initial-last-step-18": {0x80104688: 0x24030012},
            "signed-row-offset": {0x801046FC: 0x80450000},
            "label-x-217": {0x80104758: 0x24A500D9},
            "indicator-x-217": {0x80104780: 0x24A500D9},
            "condition-x-225": {0x801047A8: 0x24A500E1},
            "leader-threshold-seven": {0x801047B4: 0x2A820007},
            "signed-anchor-bone": {0x801047C4: 0x82650005},
            "step-decrement-two": {0x801047F0: 0x2682FFFE},
            "ready-phase-three": {0x8010482C: 0x24020003},
            "signed-selection": {0x8010484C: 0x80544F6A},
            "highlight-offset-127": {0x80104858: 0x2688FF81},
            "wrong-neutral-color": {0x8010486C: 0x36B58081},
            "ignore-fade-line-gate": {0x80104968: 0x00000000},
            "leave-slide-16": {0x801049C0: 0x00052900},
            "leave-limit-nine": {0x801049FC: 0x28A20009},
            "no-idle-store": {0x80104A90: 0x00000000},
        },
    )
)
