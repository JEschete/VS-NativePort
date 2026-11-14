"""Shared MENU1 MIPS callbacks for the Break Arts differential cases."""


def menu_item_stub(calls, text_base, text_kind, item_address, text_stride=2):
    def set_menu_item(machine):
        stack = machine.r[29]
        text_pointer = machine.load(stack + 20)
        calls.extend(
            [
                1,
                8,
                machine.r[4],
                machine.r[5],
                machine.r[6],
                machine.r[7],
                machine.load(stack + 16),
                text_kind,
                (text_pointer - text_base) // text_stride,
                -1,
            ]
        )
        machine.r[2] = item_address

    return set_menu_item
