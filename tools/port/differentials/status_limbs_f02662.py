"""F02662 original limb indicator: full packet, chain, carries and palette aliases."""

from status_limbs_common import configure

globals().update(
    configure(
        2662,
        {
            "outer-size-six": {0x80103E64: 0x354A0006},
            "inner-size-four": {0x80103E74: 0x358C0004},
            "inset-two": {0x80103E6C: 0x356B0002},
            "palette-stride-two": {0x80103E84: 0x00042040},
            "arithmetic-dimming": {0x80103EA0: 0x00C21007},
            "inner-brightness-unchanged": {0x80103EA8: 0x00021000},
            "wrong-tile-command": {0x80103E90: 0x3C076200},
            "no-link-mask": {0x80103ED4: 0x00C03021},
            "packet-length-five": {0x80103EC0: 0x3C020500},
            "short-packet-advance": {0x80103EE8: 0x24840018},
        },
    )
)
