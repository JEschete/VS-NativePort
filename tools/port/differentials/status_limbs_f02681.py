"""F02681 original limb defenses, source/destination aliases and bounded stores."""

from status_limbs_common import configure

globals().update(
    configure(
        2681,
        {
            "actor-offset-zero": {0x80106428: 0x00000000},
            "limb-stride-half": {0x80106448: 0x00021040},
            "affinity-mask-four": {0x8010645C: 0x30C20003},
            "affinity-from-types": {0x80106468: 0x94420008},
            "only-fifteen-affinities": {0x80106474: 0x28C2000F},
            "byte-affinity-store": {0x80106470: 0xA0620000},
            "type-source-next": {0x80106494: 0x94A3000A},
            "only-three-types": {0x801064A8: 0x28C20003},
            "type-destination-next": {0x80106490: 0x24040042},
            "no-dimming-selection": {0x801064C0: 0x24840001},
        },
    )
)
