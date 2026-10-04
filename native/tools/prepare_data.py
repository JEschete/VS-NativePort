"""Prepare native asset and LBA metadata from upstream configuration and retail data."""

import argparse
import csv
from pathlib import Path
import sys

import yaml


def macro_name(name: str) -> str:
    return name.upper().replace(".", "_").replace("-", "_")


def disc_entries(layout: Path, data_directory: Path):
    result = []
    with layout.open(encoding="utf-8") as report:
        for fields in csv.reader(report, delimiter="|"):
            fields = [field.strip() for field in fields]
            if len(fields) < 7 or fields[0] not in ("File", "XA"):
                continue
            name, sectors, lba, source = fields[1], fields[2], fields[3], fields[6]
            source_path = Path(source)
            if not source_path.is_absolute():
                source_path = data_directory / source_path
            if not source_path.is_file():
                raise FileNotFoundError(f"Retail data file missing: {source_path}")
            result.append((int(lba), int(sectors), source_path.relative_to(data_directory).as_posix(), name))
    return sorted(result)


def asset_ranges(configuration: dict):
    for segment in configuration["segments"]:
        if not isinstance(segment, dict):
            continue
        subsections = segment.get("subsegments", [])
        for index, subsection in enumerate(subsections):
            if isinstance(subsection, dict):
                start, kind, name = subsection.get("start"), subsection.get("type"), subsection.get("name")
            else:
                start, kind = subsection[:2]
                name = subsection[2] if len(subsection) > 2 else None
            if not name or kind not in ("indexed", "rgb555", "rgb555Sized", "rleRgb555", "rgba16", "rgba16Compressed", "vsString", "vsStringTable"):
                continue
            if index + 1 < len(subsections):
                following = subsections[index + 1]
                finish = following["start"] if isinstance(following, dict) else following[0]
            else:
                later = configuration["segments"][configuration["segments"].index(segment) + 1:]
                if not later:
                    raise ValueError(f"Asset end missing for {name}")
                finish = later[0]["start"] if isinstance(later[0], dict) else later[0][0]
            yield start, finish, name


def asset_symbols(configuration: dict, name: str) -> list[tuple[str, int]]:
    for segment in configuration["segments"]:
        if not isinstance(segment, dict):
            continue
        for subsection in segment.get("subsegments", []):
            if isinstance(subsection, dict):
                if subsection.get("name") != name:
                    continue
                kind = subsection.get("type")
            else:
                if len(subsection) < 3 or subsection[2] != name:
                    continue
                kind = subsection[1]
            if kind == "rgb555Sized":
                return [(name + "_header", 0), (name + "_data", 4)]
            if kind != "indexed":
                return [(name, 0)]
            if isinstance(subsection, dict):
                width, height = subsection["width"], subsection["height"]
                bitdepth = subsection.get("bitdepth", 4)
                palettes = subsection.get("num_cluts", 1)
                after = subsection.get("cluts_after", False)
            else:
                width, height = subsection[3:5]
                bitdepth = subsection[5] if len(subsection) > 5 else 4
                palettes = subsection[6] if len(subsection) > 6 else 1
                after = subsection[7] if len(subsection) > 7 else False
            if bitdepth not in (4, 8):
                raise ValueError(f"Unsupported indexed bit depth for {name}: {bitdepth}")
            pixels_size = (width * height * bitdepth + 7) // 8
            palette_size = (1 << bitdepth) * 2 * palettes
            if after:
                return [(name, 0), (name + "_clut", pixels_size)]
            return [(name + "_clut", 0), (name, palette_size)]
    raise ValueError(f"Asset metadata not found for {name}")


def prepare(source: Path, data_directory: Path, layout: Path, output: Path):
    sys.path.insert(0, str(source))
    from tools.etc.vsString_dumpTable import vsString_dumpTable
    from tools.etc.vsString_yaml import read_yaml, write_binary_header, write_data, write_header
    output.mkdir(parents=True, exist_ok=True)
    entries = disc_entries(layout, data_directory)
    if not entries:
        raise ValueError("Disc layout contains no usable LBA entries")
    header = output / "build" / "src" / "include" / "lbas.h"
    header.parent.mkdir(parents=True, exist_ok=True)
    definitions = ["#pragma once"]
    for lba, sectors, relative, name in entries:
        key = macro_name(name)
        definitions.extend((f"#define VS_{key}_LBA {lba}", f"#define VS_{key}_SIZE {sectors * 2048}"))
    header.write_text("\n".join(definitions) + "\n", encoding="ascii")

    index = output / "disc_index.c"
    index.write_text('#include "disc_index.h"\nconst psx_disc_entry psx_disc_index[] = {\n' +
        "".join(f'    {{ {lba}, {sectors}, "{relative}" }},\n' for lba, sectors, relative, name in entries) +
        '};\nconst int psx_disc_index_count = sizeof(psx_disc_index) / sizeof(psx_disc_index[0]);\n', encoding="ascii")

    assets = []
    seen = set()
    for config_path in sorted((source / "config").rglob("splat.yaml")):
        configuration = yaml.safe_load(config_path.read_text(encoding="utf-8"))
        target = configuration["options"]["target_path"]
        binary = data_directory / Path(target).relative_to("data")
        if not binary.is_file():
            continue
        payload = binary.read_bytes()
        for start, finish, name in asset_ranges(configuration):
            if finish > len(payload) or start >= finish:
                raise ValueError(f"Invalid retail asset range for {name}")
            content = payload[start:finish]
            definition = source / "assets" / Path(target).relative_to("data") / (name + ".yaml")
            if definition.is_file():
                base = output / "build" / "assets" / Path(target).relative_to("data") / name
                table = base.with_suffix(".vsString.yaml")
                table.parent.mkdir(parents=True, exist_ok=True)
                vsString_dumpTable(content, definition, table)
                strings, offsets, enums, indices = read_yaml(table)
                write_data(base.with_suffix(".vsString"), offsets, strings)
                write_header(base.with_suffix(".h"), name, enums, indices)
            if name in seen:
                continue
            seen.add(name)
            backing = "vs_native_asset_" + name
            symbols = asset_symbols(configuration, name)
            if any(offset < 0 or offset > len(content) for symbol, offset in symbols):
                raise ValueError(f"Asset symbol offset outside retail payload for {name}")
            aliases = "".join(
                f'__asm__(".globl _{symbol}\\n.set _{symbol}, _{backing} + {offset}\\n");\n'
                for symbol, offset in symbols)
            assets.append(f"unsigned char {backing}[{len(content)}] __attribute__((aligned(16))) = {{" +
                          ",".join(str(value) for value in content) + "};\n" + aliases)
    if not assets:
        raise ValueError("No upstream asset ranges were found")
    from tools.etc.vsString_dumpFixedTable import main as dump_fixed
    for definition in sorted((source / "assets/MENU").glob("*.BIN.yaml")):
        name = definition.name.removesuffix(".yaml")
        binary = data_directory / "MENU" / name
        base = output / "build" / "assets" / "MENU" / name
        table = base.with_suffix(base.suffix + ".vsString.yaml")
        table.parent.mkdir(parents=True, exist_ok=True)
        if name == "ITEMNAME.BIN":
            dump_fixed([str(binary), str(definition), str(table), "24"])
        else:
            vsString_dumpTable(binary.read_bytes(), definition, table)
        strings, offsets, enums, indices = read_yaml(table)
        write_data(base.with_suffix(base.suffix + ".vsString"), offsets, strings)
        write_header(base.with_suffix(base.suffix + ".h"), name.replace(".", "_"), enums, indices)
    (output / "assets.c").write_text("".join(assets) +
        f"const int psx_asset_count = {len(assets)};\n", encoding="ascii")
    print(f"Prepared {len(entries)} disc entries and {len(assets)} native retail assets")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("data", type=Path)
    parser.add_argument("layout", type=Path, help="Windows mkpsxiso -lba report")
    parser.add_argument("output", type=Path)
    arguments = parser.parse_args()
    prepare(arguments.source, arguments.data, arguments.layout, arguments.output)
