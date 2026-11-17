"""F02616: all unlock phases, filtered rewards, row callbacks and persistent state."""

from __future__ import annotations

import random

import menu2_ability_foundation as h
from differential import Fault, Machine

globals().update(h.case_settings())
ID = "F02616"
ENTRY, END = 0x80103E20, 0x80104578
DRIVER = "menu2_ability_unlock_driver.cpp"
SEED = 2616
MUTANTS = {
    "initialize-gate": {0x80103E3C: 0x14800069},
    "unlock-limit": {0x80103E50: 0x24020015},
    "tier-requirement-stride": {0x80103E60: 0x00111800},
    "threshold-gate": {0x80103E78: 0x104001B6},
    "cursor-memory-reset-size": {0x80103E8C: 0x24060005},
    "unlock-credit": {0x80103E90: 0x26220002},
    "memory-reset-call": {0x80103E94: 0},
    "chain-unlocked-bit": {0x80103EF0: 0x00021382},
    "chain-filter": {0x80103EF8: 0x10400008},
    "chain-count": {0x80103F08: 0x24430002},
    "chain-action-store": {0x80103F18: 0xA0400000},
    "chain-loop-limit": {0x80103F20: 0x2A22000A},
    "defense-filter": {0x80103F88: 0x10400008},
    "defense-action-store": {0x80103FA8: 0xA0400000},
    "defense-loop-limit": {0x80103FB0: 0x2A22000A},
    "first-visits": {0x80103FC4: 0x24030001},
    "message-timeout": {0x80103FD0: 0x24030077},
    "reset-phase": {0x80103FE0: 0},
    "phase-limit": {0x80103FF0: 0x2C620007},
    "defense-category-gate": {0x8010402C: 0x14600002},
    "completed-message": {0x8010407C: 0x24845028},
    "chain-row-x": {0x80104094: 0x2405013F},
    "chain-row-width": {0x8010409C: 0x2407008D},
    "chain-row-target": {0x801040C0: 0x240200B3},
    "chain-row-state": {0x801040CC: 0xA0800000},
    "chain-row-selected": {0x801040D0: 0xA0800006},
    "defense-row-id": {0x801040DC: 0x24040000},
    "defense-row-y": {0x801040E4: 0x24060013},
    "defense-row-background": {0x801040EC: 0x24020007},
    "defense-row-target": {0x80104108: 0x240200B3},
    "category-row-info": {0x8010412C: 0x2405001B},
    "category-force-cursor": {0x80104170: 0xA2000001},
    "category-restore-cursor": {0x80104180: 0},
    "category-selection-call": {0x80104184: 0x0C040233},
    "category-wait": {0x80104190: 0x162000EF},
    "category-clear-row": {0x801041A4: 0x24040027},
    "hoist-offset": {0x801041B4: 0x24050001},
    "chain-ready": {0x801041D8: 0x144000DD},
    "chain-one-row": {0x80104274: 0x24020002},
    "chain-visible-rows": {0x8010427C: 0x24040003},
    "chain-cursor-mask": {0x801042B4: 0x30620002},
    "chain-row-info": {0x801042CC: 0x2405011C},
    "chain-next-phase": {0x801042E4: 0x24020005},
    "defense-ready": {0x801042F0: 0x14400097},
    "defense-one-row": {0x8010438C: 0x24020002},
    "defense-visible-rows": {0x80104394: 0x24040003},
    "defense-cursor-mask": {0x801043CC: 0x30620001},
    "defense-row-info": {0x801043E4: 0x2405011B},
    "defense-next-phase": {0x801043F8: 0x24020004},
    "list-cursor-restore": {0x801043FC: 0},
    "chain-confirm-wait": {0x80104418: 0x1620004E},
    "chain-confirm-header": {0x80104434: 0x24040002},
    "defense-confirm-wait": {0x8010445C: 0x1620003D},
    "cancel-next-phase": {0x80104474: 0x24020006},
    "ability-unlock-message": {0x8010448C: 0x24844FC2},
    "learned-flag": {0x801044E4: 0x34424000},
    "done-bit": {0x801044FC: 0x00021742},
    "message-done": {0x80104504: 0x14400012},
    "zero-timeout": {0x80104514: 0x14600005},
    "pressed-buttons": {0x80104524: 0x14400003},
    "timeout-decrement": {0x80104528: 0x2462FFFE},
    "reinit-ready": {0x80104544: 0x14400002},
}
NATIVE_REPLACE = ("progress.SetBattleAbilitiesUnlocked(unlocked + 1);",
                  "progress.SetBattleAbilitiesUnlocked(unlocked + 2);")


def vector(rng: random.Random, number: int) -> list[int]:
    mode, group = number % 16, number // 16
    count = group % 11 + 1
    phase = (0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 255)[mode]
    if mode < 4:
        phase = group % 7 + 1
    chain_count = count if mode not in (4, 6) else 0
    defense_count = count if mode not in (4, 5) else 0
    unlocked = 22 if mode == 1 else group % 22
    requirement = rng.choice((1, 100, 32768, 65535))
    kills = requirement - 1 if mode == 3 else requirement
    selected = (-1, -2, 0, count - 1)[group % 4]
    if phase == 1:
        selected = (-1, -2, 0, 1)[group % 4]
    timeout = (0, 1, 119, 120, 255)[(group // 4) % 5]
    mask = rng.getrandbits(22)
    if group % 4 == 0:
        mask = (1 << 22) - 1
    elif group % 4 == 1:
        mask = 0
    return [int(mode < 4), unlocked, kills, requirement, phase, timeout,
            group % 4, chain_count, defense_count, selected, int(group % 3 != 0),
            int(group % 4 >= 2), 255 if group % 2 else 0,
            rng.choice((0, 1, 2, 255)), rng.getrandbits(32), mask]


def menu_stubs(machine: Machine, calls: list[list[int]], value: list[int]) -> None:
    def clear_memory(m: Machine) -> None:
        calls.append([3, m.r[4], m.r[5], m.r[6]])
        if m.r[6] > 64:
            raise Fault("oversized unlock-memory reset")
        for offset in range(m.r[6]):
            m.write(m.r[4] + offset, m.r[5], 1)
        m.r[2] = m.r[4]

    def selected(m: Machine) -> None:
        calls.append([4])
        m.r[2] = value[9] & 0xFFFFFFFF

    def item(m: Machine) -> None:
        calls.append([5, *m.r[4:8], m.load(m.r[29] + 16), m.load(m.r[29] + 20)])
        m.r[2] = h.ITEM

    def rows(m: Machine) -> None:
        row_info = m.r[5]
        if row_info == 0x1A:
            count = 2
        elif row_info == 0x11B:
            count = m.load(h.CHAIN_COUNT, 1)
        elif row_info == 0x11C:
            count = m.load(h.DEFENSE_COUNT, 1)
        else:
            raise Fault(f"unexpected row info {row_info:#x}")
        if count > 11:
            raise Fault("row text exceeds original stack arrays")
        strings = [m.load(m.r[6] + index * 4) for index in range(count * 2)]
        types = [m.load(m.r[7] + index * 4) for index in range(count)]
        calls.append([6, m.r[4], row_info, m.load(h.CURSOR, 1), count, *strings, *types])

    def ready(m: Machine) -> None:
        calls.append([9])
        m.r[2] = value[10]

    machine.stub(0x80026EB4, clear_memory)
    machine.stub(0x801008C8, selected)
    machine.stub(0x800C8E5C, item)
    machine.stub(0x801005E0, rows)
    machine.stub(0x800FA8E0, lambda m: calls.append([7, m.r[4]]))
    machine.stub(0x800FA92C, lambda m: calls.append([8, m.r[4], m.r[5]]))
    machine.stub(0x800FA9D0, ready)
    # F02615 is followed, not replaced by a guessed description-pointer stub.
    machine.follow.add(0x80103DD8)
    h.message_calls(machine, calls)


def run(machine: Machine, value: list[int]) -> tuple[dict, list[int]]:
    (initialize, unlocked, kills, requirement, phase, timeout, first_visit,
     chain_count, defense_count, _, _, done, buttons, cursor, seed, mask) = value
    h.setup(machine, seed, mask)
    machine.put(h.PROGRESS + 32, unlocked, 2)
    machine.put(h.PROGRESS + 34, kills, 2)
    for tier in range(22):
        machine.put(h.ABILITY_REQUIREMENTS + tier * 2, requirement, 2)
    machine.store(h.REMAINING_CHAIN, bytes((*h.CHAIN, *([0xCC] * 5))))
    machine.store(h.REMAINING_DEFENSE, bytes(h.DEFENSE))
    for address, byte in ((h.CHAIN_COUNT, chain_count), (h.DEFENSE_COUNT, defense_count),
                          (h.PHASE, phase), (h.TIMEOUT, timeout),
                          (h.FIRST_VISIT, first_visit), (h.CURSOR, cursor),
                          (h.INPUT, buttons)):
        machine.put(address, byte, 1)
    for index in range(64):
        machine.put(h.MENU_MEMORY + index, seed + index * 17, 1)
    machine.put(h.BOX_FLAGS, done << 30)
    machine.map_zero(h.ITEM, 64)
    machine.put(h.ITEM, 83, 1)
    machine.put(h.ITEM + 24, 1084, 2)
    calls = []
    menu_stubs(machine, calls, value)
    machine.r[4] = initialize
    machine.execute(ENTRY, limit=20000)
    values = [
        machine.r[2], machine.load(h.PROGRESS + 32, 2),
        *(machine.load(address, 1) for address in (
            h.CHAIN_COUNT, h.DEFENSE_COUNT, h.PHASE, h.TIMEOUT, h.FIRST_VISIT)),
        *(machine.load(h.REMAINING_CHAIN + index, 1) for index in range(16)),
        *(machine.load(h.REMAINING_DEFENSE + index, 1) for index in range(11)),
        *h.flag_values(machine),
        *(machine.load(h.MENU_MEMORY + index, 1) for index in range(64)),
        machine.load(h.CURSOR, 1), machine.load(h.CONTEXT + 40),
        machine.load(h.CONTEXT + 44), *h.item_values(machine),
    ]
    allowed = [
        (h.PROGRESS + 32, h.PROGRESS + 34), (h.REMAINING_CHAIN, h.FIRST_VISIT + 1),
        (h.MENU_MEMORY + 0x34, h.MENU_MEMORY + 0x3A), (h.CURSOR, h.CURSOR + 1),
        (h.CONTEXT + 44, h.CONTEXT + 48), (h.ITEM, h.ITEM + 1),
        (h.ITEM + 6, h.ITEM + 7), (h.ITEM + 24, h.ITEM + 26), *h.flag_spans(),
    ]
    names = ["result", "earned", "chain count", "defense count", "phase",
             "timeout", "first visits"]
    return h.observation(machine, values, calls, allowed, names), value


check = h.check
describe = h.describe
