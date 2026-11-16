"""F00742 stat-change commit (func_800971D4) against its complete original instructions."""

from differentials.floating_placement_common import COMMIT, configure

globals().update(
    configure(
        742,
        COMMIT,
        0x800971D4,
        0x8009722C,
        {
            "commits-kind-five": {0x800971EC: 0x24070005},
            "publishes-kind-five": {0x800971F0: 0x24060005},
            "skips-to-next-record": {0x800971F4: 0x24630020},
        },
    )
)
