"""Audit the three reserved effect selectors across configured original images."""

import hashlib
import json
import re
import struct
from pathlib import Path

from differential import DISC, ROOT

ADDRESSES = (0x80084FF0, 0x80084FF8, 0x80085000)


def audit():
    images = []
    for config in sorted((ROOT / "resources" / "rood-reverseWin" / "config").rglob("splat.yaml")):
        text = config.read_text()
        if not re.search(r"type:\s*code\b", text):
            continue
        target = re.search(r"target_path:\s*data/([^\s]+)", text)
        if not target:
            raise ValueError(f"missing target_path: {config}")
        name = target[1]
        data = (DISC / Path(name)).read_bytes()
        words = struct.unpack(f"<{len(data)//4}I", data[:len(data)//4*4])
        refs = {}
        for address in ADDRESSES:
            refs[f"{address:08X}"] = {
                "pointers": [i * 4 for i, word in enumerate(words) if word == address],
                "jumps": [i * 4 for i, word in enumerate(words)
                          if word >> 26 in (2, 3) and ((word & 0x3FFFFFF) << 2) == address & 0xFFFFFFF],
                "address_lows": [i * 4 for i, word in enumerate(words)
                                 if word >> 26 in (9, 13) and word & 0xFFFF == address & 0xFFFF],
            }
        images.append({"name": name, "size": len(data),
                       "sha256": hashlib.sha256(data).hexdigest(), "references": refs})
    battle = (DISC / "BATTLE" / "BATTLE.PRG").read_bytes()
    executable = (DISC / "SLUS_010.40").read_bytes()
    effects = struct.unpack_from("<70I", battle, 0x7FB78)
    selected = [(struct.unpack_from("<I", executable, 0x3C1DC + action * 52 + 20 + hit * 4)[0] & 127)
                for action in range(256) for hit in range(2)]
    reserved = {}
    for index, address in enumerate(ADDRESSES):
        selector = 67 + index
        offset = address - 0x80068800
        reserved[f"F{503+index:05}"] = {
            "entry": f"{address:08X}", "end": f"{address+8:08X}",
            "sha256": hashlib.sha256(battle[offset:offset+8]).hexdigest(),
            "instructions": 2, "selector": selector, "table_pointer": f"{effects[selector]:08X}",
            "selected_hits": selected.count(selector),
        }
        assert effects[selector] == address and selected.count(selector) == 0
        assert battle[offset:offset+8] == bytes.fromhex("0800e00300000000")
    return {"configured_code_images": len(images), "action_hits": len(selected),
            "reserved": reserved, "images": images}


if __name__ == "__main__":
    output = ROOT / "build" / "scratch" / "resource-dispatch" / "reserved-audit.json"
    output.parent.mkdir(parents=True, exist_ok=True)
    result = audit()
    output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps({k: v for k, v in result.items() if k != "images"}, indent=2))
    for image in result["images"]:
        for address, references in image["references"].items():
            if any(references.values()):
                print(image["name"], address, references)
