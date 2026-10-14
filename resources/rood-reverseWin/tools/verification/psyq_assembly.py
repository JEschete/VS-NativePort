"""The fork's GNU-as PsyQ preprocessing, without a sed dependency."""

import re


MOVE = re.compile(rb"^\tmove\t([^,]+),(.+)$", re.MULTILINE)


def preprocess_psyq(assembly: bytes) -> bytes:
    assembly = assembly.replace(b"\r\n", b"\n")
    return b'.include "macro.inc"\n' + MOVE.sub(rb"\taddu\t\1,\2,$0", assembly)