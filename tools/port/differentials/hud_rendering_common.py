"""Four HUD helpers, original instructions with explicitly bounded external render stubs."""

from __future__ import annotations

import random

from differential import (
    DEFAULT_STACK,
    MASK,
    Machine,
    call_problems,
    s32,
    store_problems,
)

FIELDS = (
    "operation",
    "index",
    "current",
    "maximum",
    "slide",
    "slot",
    "gauge_position",
    "label_position",
    "label_width",
    "label_texture",
    "status_texture",
    "restrictions",
    "spell_unlock",
    "ability_unlock",
    "art_unlock",
    "first_number_pen",
    "second_number_pen",
    "callback_changes",
    "updated_slide",
    "updated_label_position",
    "updated_label_width",
    "updated_label_texture",
    "scene_owner",
    "updated_scene_owner",
    "overlay_owner",
    "marker_position",
    "marker_brightness",
)
SPECS = {
    "F01354": (0, 0x800C9B38, 0x800C9CB4),
    "F01355": (1, 0x800C9CB4, 0x800C9E08),
    "F01358": (2, 0x800C9EB8, 0x800C9F88),
    "F01360": (3, 0x800C9F98, 0x800C9FE8),
}
SCRATCH = 0x1F800000
SLIDE = 0x800EB9B0
GAUGE_POSITION = 0x800EBBDC
LABEL_POSITION = 0x800EBBEC
LABEL_WIDTH = 0x800EBBFC
LABEL_TEXTURE = 0x800EBC00
STATUS_TEXTURE = 0x800EBC14
PACKETS, PACKET_STRIDE = 0x80600000, 32
RENDER_GAUGE = 0x800C99DC
RENDER_NUMBER = 0x800C9950
RENDER_SEPARATOR = 0x800C98C0
RENDER_SPRITE = 0x800C0224
RESTRICTIONS = 0x800CABE0
SPELL_UNLOCK = 0x800CAEAC
ABILITY_UNLOCK = 0x800CAFB4
ART_UNLOCK = 0x800CAF40
MUTATIONS = {
    "F01354": {
        "zero-maximum-gate": {0x800C9B88: 0x10000042},
        "fill-scale-five": {0x800C9B90: 0x00122940},
        "fill-rounding-bias": {0x800C9B98: 0x24A50000},
        "unsigned-division": {0x800C9B9C: 0x00B1001B},
        "missing-gauge-call": {0x800C9BAC: 0x00000000},
        "large-number-kind-bit": {0x800C9BB4: 0x32620001},
        "risk-number-x": {0x800C9BC0: 0x34420037},
        "fixed-point-rounding": {0x800C9BD4: 0x26420000},
        "maximum-number-font": {0x800C9BE8: 0x24040001},
        "missing-maximum-call": {0x800C9BF4: 0x00000000},
        "discard-maximum-pen": {0x800C9BFC: 0x02008021},
        "separator-font": {0x800C9C00: 0x24040001},
        "missing-separator-call": {0x800C9C0C: 0x00000000},
        "current-number-offset": {0x800C9C18: 0x3442FFF8},
        "current-number-font": {0x800C9C20: 0x24040000},
        "missing-current-call": {0x800C9C2C: 0x00000000},
        "label-transparency": {0x800C9C34: 0x24040080},
        "signed-label-width": {0x800C9C68: 0x80420000},
        "missing-label-call": {0x800C9C70: 0x00000000},
        "signed-label-u": {0x800C9C84: 0x80630000},
        "label-clut": {0x800C9C88: 0x3C0437F7},
        "label-texture-store": {0x800C9C90: 0xAC43000C},
    },
    "F01355": {
        "absent-icon-sentinel": {0x800C9CC8: 0x240200FE},
        "panel-visibility-gate": {0x800C9CE8: 0x10400040},
        "signed-icon-texture": {0x800C9CFC: 0x84720000},
        "slot-stride": {0x800C9D00: 0x001010C0},
        "icon-left": {0x800C9D08: 0x2450009F},
        "negative-slide-uv": {0x800C9D0C: 0x02459021},
        "negative-slide-width": {0x800C9D14: 0x24B1000F},
        "positive-slide-width": {0x800C9D1C: 0x2402000F},
        "base-sprite-height": {0x800C9D30: 0x3C06000F},
        "base-owner-offset": {0x800C9D44: 0x24E7FFF8},
        "status-clut-bit-zero": {0x800C9D4C: 0x3463906B},
        "overlay-status-bit": {0x800C9D70: 0x32620008},
        "overlay-x-gate": {0x800C9D78: 0x32020004},
        "overlay-width-threshold": {0x800C9D88: 0x2A22000A},
        "overlay-u-origin": {0x800C9D90: 0x24723069},
        "overlay-alignment": {0x800C9D94: 0x32020003},
        "overlay-right-edge": {0x800C9D9C: 0x2602FFF7},
        "overlay-narrow-u": {0x800C9DB8: 0x24623071},
        "overlay-height": {0x800C9DC4: 0x3C060009},
        "overlay-refreshed-owner": {0x800C9DD0: 0x8C470000},
        "overlay-clut": {0x800C9DE0: 0x3C0337FE},
        "overlay-texture-store": {0x800C9DE8: 0xAC43000C},
    },
    "F01358": {
        "restriction-query-mode": {0x800C9EC4: 0x24040001},
        "signed-shortcut-bound": {0x800C9EDC: 0x2A220008},
        "excluded-break-art-slot": {0x800C9EDC: 0x2E220007},
        "enabled-transparency": {0x800C9EE4: 0x24120080},
        "spell-selector-shift": {0x800C9F08: 0x001110C0},
        "spell-class-map": {0x800C9F0C: 0x24043020},
        "spell-unlock-gate": {0x800C9F1C: 0x1440000A},
        "spell-restriction-mask": {0x800C9F20: 0x320200B6},
        "item-restriction-mask": {0x800C9F30: 0x32020006},
        "ability-selector": {0x800C9F34: 0x24040005},
        "ability-unlock-gate": {0x800C9F40: 0x1040000B},
        "hidden-result": {0x800C9F4C: 0x24020001},
        "break-art-unlock-gate": {0x800C9F58: 0x1440FFFB},
        "break-art-restriction-mask": {0x800C9F5C: 0x3202005F},
        "restricted-transparency": {0x800C9F68: 0x24120040},
    },
    "F01360": {
        "marker-side": {0x800C9FA4: 0x34C60005},
        "marker-brightness-source": {0x800C9FAC: 0x00602021},
        "marker-overlay-owner": {0x800C9FB0: 0x8C470004},
        "marker-position-source": {0x800C9FBC: 0x00A02821},
        "marker-texture-page": {0x800C9FC4: 0x34840016},
        "marker-clut": {0x800C9FC8: 0x3C03373C},
        "marker-uv": {0x800C9FCC: 0x346380C1},
        "marker-texture-store": {0x800C9FD4: 0xAC43000C},
    },
}


def vector_for(identifier: str, rng: random.Random, number: int) -> list[int]:
    operation = SPECS[identifier][0]
    values = {name: rng.getrandbits(32) for name in FIELDS}
    values.update(
        operation=operation,
        label_width=rng.choice((0, 1, 127, 128, 254, 255)),
        label_texture=rng.choice((0, 1, 127, 128, 254, 255)),
        status_texture=rng.choice((0, 1, 0x1080, 0x7FFF, 0x8000, 0xFFFF)),
        callback_changes=number % 8,
        updated_label_width=rng.getrandbits(8),
        updated_label_texture=rng.getrandbits(8),
        scene_owner=0x80500100,
        updated_scene_owner=0x80510100,
        overlay_owner=0x80502084,
    )
    if operation == 0:
        maximums = (0, 1, -1, 2, 63, 64, 100, 255, 256, 32767, 0x7FFFFFFF, -0x80000000)
        maximum = maximums[(number // 4) % len(maximums)]
        currents = (
            0,
            1,
            -1,
            63,
            64,
            65,
            255,
            256,
            maximum - 1,
            maximum,
            0x7FFFFFFF,
            -0x80000000,
        )
        values.update(
            index=number % 4,
            maximum=maximum,
            current=currents[(number * 5 + number // 48) % len(currents)],
        )
        if number >= 256 and number % 5 == 0:
            values.update(current=rng.getrandbits(32), maximum=rng.getrandbits(32))
    elif operation == 1:
        slides = (
            -0x80000000,
            -17,
            -16,
            -15,
            -9,
            -8,
            -7,
            -1,
            0,
            1,
            4,
            7,
            8,
            9,
            15,
            16,
            17,
            0x7FFFFFFF,
        )
        values.update(
            index=255 if number % 37 == 0 else number % 32,
            current=slides[(number * 7 + number // 32) % len(slides)],
            slide=1 if number % 11 == 0 else 0,
            slot=rng.choice((-11, -10, -1, 0, 1, 2, 0x7FFFFFFF, -0x80000000)),
        )
        if number >= 512 and number % 5 == 0:
            values.update(current=rng.getrandbits(32), slot=rng.getrandbits(32))
    elif operation == 2:
        values.update(
            index=number % 8
            if number % 17
            else rng.choice((-1, 8, 9, 255, -0x80000000, 0x7FFFFFFF)),
            restrictions=1 << ((number // 8) % 12)
            if number < 512
            else rng.getrandbits(32),
            spell_unlock=(0, 1, 0x80000000)[(number // 64) % 3],
            ability_unlock=(0, 1, 0xFFFFFFFF)[(number // 64) % 3],
            art_unlock=(0, 1, 0xFFFFFFFF)[(number // 64) % 3],
        )
    else:
        values.update(
            marker_position=rng.choice(
                (0, 1, -1, 0x8000FFFF, 0xFFFF0001, rng.getrandbits(32))
            ),
            marker_brightness=(
                0,
                1,
                127,
                128,
                255,
                256,
                0x140,
                0x180,
                0x1FF,
                0xFFFFFFFF,
            )[number % 10]
            if number < 512
            else rng.getrandbits(32),
        )
    return [values[name] & MASK for name in FIELDS]


def run_for(identifier: str, machine: Machine, vector: list[int]) -> tuple:
    values = dict(zip(FIELDS, vector, strict=True))
    operation, entry, _ = SPECS[identifier]
    index = s32(values["index"])
    calls: list[list[int]] = []
    sprites: list[tuple[list[int], int]] = []
    number_count = 0

    def update(callback: int) -> None:
        if not values["callback_changes"] & callback:
            return
        machine.put(SLIDE, values["updated_slide"])
        machine.put(SCRATCH + 4, values["updated_scene_owner"])
        if operation == 0:
            machine.put(LABEL_POSITION + index * 4, values["updated_label_position"])
            machine.put(LABEL_WIDTH + index, values["updated_label_width"], 1)
            machine.put(LABEL_TEXTURE + index, values["updated_label_texture"], 1)

    def render_gauge(m: Machine) -> None:
        calls.append([1, *m.r[4:8], 0, 0, 0])
        update(1)

    def render_number(m: Machine) -> None:
        nonlocal number_count
        pen = values["first_number_pen" if number_count == 0 else "second_number_pen"]
        number_count += 1
        calls.append([2, *m.r[4:8], pen, 0, 0])
        m.r[2] = pen
        update(2)

    def render_separator(m: Machine) -> None:
        calls.append([3, *m.r[4:8], 0, 0, 0])

    def render_sprite(m: Machine) -> None:
        packet = PACKETS + len(sprites) * PACKET_STRIDE
        event = [4, *m.r[4:8], 0, 0, packet]
        calls.append(event)
        sprites.append((event, packet))
        m.put(packet + 4, 0xE100000C)
        m.put(packet + 16, 0xA5A5A5A5)
        m.r[2] = packet
        update(4)

    def service(kind: int, field: str):
        def invoke(m: Machine) -> None:
            result = values[field]
            calls.append([kind, m.r[4], 0, 0, 0, result, 0, 0])
            m.r[2] = result

        return invoke

    machine.put(SCRATCH, 0x80700000)
    machine.put(SCRATCH + 4, values["scene_owner"])
    machine.put(SCRATCH + 8, values["overlay_owner"])
    machine.put(SLIDE, values["slide"])
    if operation == 0:
        machine.put(GAUGE_POSITION + index * 4, values["gauge_position"])
        machine.put(LABEL_POSITION + index * 4, values["label_position"])
        machine.put(LABEL_WIDTH + index, values["label_width"], 1)
        machine.put(LABEL_TEXTURE + index, values["label_texture"], 1)
    elif operation == 1 and 0 <= index < 32:
        machine.put(STATUS_TEXTURE + index * 2, values["status_texture"], 2)
    machine.stub(RENDER_GAUGE, render_gauge)
    machine.stub(RENDER_NUMBER, render_number)
    machine.stub(RENDER_SEPARATOR, render_separator)
    machine.stub(RENDER_SPRITE, render_sprite)
    machine.stub(RESTRICTIONS, service(5, "restrictions"))
    machine.stub(SPELL_UNLOCK, service(6, "spell_unlock"))
    machine.stub(ABILITY_UNLOCK, service(7, "ability_unlock"))
    machine.stub(ART_UNLOCK, service(8, "art_unlock"))
    if operation == 0:
        machine.r[4:7] = [values["index"], values["current"], values["maximum"]]
    elif operation == 1:
        machine.r[4:7] = [values["index"], values["current"], values["slot"]]
    elif operation == 2:
        machine.r[4] = values["index"]
    else:
        machine.r[4:6] = [values["marker_position"], values["marker_brightness"]]
    machine.execute(entry)
    for event, packet in sprites:
        event[5], event[6] = machine.load(packet + 16), machine.load(packet + 4)
    allowed = [(DEFAULT_STACK - 0x100, DEFAULT_STACK)]
    allowed += [(packet + 16, packet + 20) for _, packet in sprites]
    if operation == 3:
        allowed += [(packet + 4, packet + 8) for _, packet in sprites]
    observation = {
        "calls": calls,
        "result": machine.r[2] if operation == 2 else 0,
        "store_problems": store_problems(machine.stores, allowed),
    }
    return observation, vector


def check(_image, _vector, observation, native, _reference) -> list[str]:
    if len(native) < 3 or native[0]:
        return ["native refused or returned an incomplete observation"]
    problems = []
    if native[1] != observation["result"]:
        problems.append(f"result {native[1]:#x} != {observation['result']:#x}")
    expected_length = 3 + native[2] * 8
    if len(native) != expected_length:
        return [f"native event words {len(native)} != {expected_length}"]
    calls = [native[start : start + 8] for start in range(3, len(native), 8)]
    return (
        problems
        + call_problems(calls, observation["calls"])
        + observation["store_problems"]
    )


def describe(_image, results) -> list[str]:
    calls = [event for _, observation, _ in results for event in observation["calls"]]
    counts = {kind: sum(event[0] == kind for event in calls) for kind in range(1, 9)}
    return [
        f"Observed services {counts}; compared complete service order/arguments, consumed numeric return, "
        "post-callback label texture, refreshed status ordering owner, sprite draw mode/UV/CLUT, "
        "brightness return, and exact original-store spans.",
        "External rendering/unlock/restriction callees are labelled bounded stubs, not original-callee "
        "execution; production Live/helper/module tests accompany this packet for central execution.",
    ]


def bind_case(namespace: dict, identifier: str) -> None:
    """Install the shared-runner contract without copying four identical fixtures."""
    _, entry, end = SPECS[identifier]
    namespace.update(
        ID=identifier,
        ENTRY=entry,
        END=end,
        DRIVER="hud_rendering_driver.cpp",
        SOURCES=(r"Native\Game\src\Battle\HudRendering.cpp",),
        INCLUDES=(r"Native\Tests\include",),
        VECTORS=1024,
        SEED=int(identifier[1:]),
        MUTANT_VECTORS=256,
        MUTANTS=MUTATIONS[identifier],
        vector=lambda rng, number: vector_for(identifier, rng, number),
        run=lambda machine, inputs: run_for(identifier, machine, inputs),
        check=check,
        describe=describe,
    )
