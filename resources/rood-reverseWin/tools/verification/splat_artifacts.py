#!/usr/bin/env python3
"""Generate an overlay's splat outputs in an isolated copy for matching checks (Windows)."""

import argparse
from datetime import datetime, timezone
import hashlib
from importlib.metadata import version
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

import yaml

DECOMP = Path(__file__).resolve().parents[2]
ROOT = DECOMP.parents[1]
ARTIFACTS = DECOMP / "build" / "splat-artifacts"
COPIED_FOLDERS = ("config", "src", "assets", "tools/splat_ext", "tools/etc", "tools/libdata", "tools/kaitai", "tools/dev")
MACRO_FILES = ("include/macro.inc", "include/gte_macros.inc", "include/labels.inc", "include/include_asm.h")


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def generate(config: Path, timeout: int) -> Path:
    if config.is_absolute() or ".." in config.parts or config.parts[0] != "config" or config.name != "splat.yaml":
        raise ValueError("Expected a relative config/.../splat.yaml path.")
    configuration = yaml.safe_load((DECOMP / config).read_text(encoding="utf-8"))
    options = configuration["options"]
    retail = ROOT / options["target_path"]
    retail_bytes = retail.read_bytes()
    if hashlib.sha1(retail_bytes).hexdigest() != configuration["sha1"]:
        raise ValueError("Retail binary SHA-1 does not match the overlay configuration.")
    inputs = sorted({"config/splat.config.yaml", config.as_posix(), *options["symbol_addrs_path"]})
    destination = ARTIFACTS / options["basename"]
    ARTIFACTS.mkdir(parents=True, exist_ok=True)
    command = [sys.executable, "-m", "splat", "split", "--disassemble-all", "config/splat.config.yaml", config.as_posix()]
    with tempfile.TemporaryDirectory(prefix="splat-", dir=ARTIFACTS) as directory:
        work = Path(directory)
        for folder in COPIED_FOLDERS:
            shutil.copytree(DECOMP / folder, work / folder, ignore=shutil.ignore_patterns("__pycache__", "*.pyc"))
        base_config = work / "config/splat.config.yaml"
        exported_config = yaml.safe_load(base_config.read_text(encoding="utf-8"))
        exported_config["options"]["dump_symbols"] = True
        base_config.write_text(yaml.safe_dump(exported_config, sort_keys=False), encoding="utf-8")
        target = work / options["target_path"]
        target.parent.mkdir(parents=True)
        target.write_bytes(retail_bytes)
        result = subprocess.run(command, cwd=work, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=timeout)
        (work / "splat.log").write_bytes(result.stdout)
        if result.returncode:
            raise RuntimeError("splat failed:\n" + result.stdout.decode("utf-8", errors="replace")[-4000:])
        for strings in sorted((work / "build/assets").rglob("*.vsString.yaml")):
            stem = strings.name.removesuffix(".vsString.yaml")
            subprocess.run([sys.executable, "-m", "tools.etc.vsString_yamlToData",
                str(strings), str(strings.with_name(stem + ".vsString")),
                str(strings.with_name(stem + ".h"))], cwd=work, check=True,
                stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=30)
        missing = [name for name in MACRO_FILES if not (work / name).is_file()]
        if missing:
            raise RuntimeError("splat did not generate: " + ", ".join(missing))
        if destination.exists():
            shutil.rmtree(destination)
        destination.mkdir(parents=True)
        for name in MACRO_FILES:
            (destination / name).parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(work / name, destination / name)
        shutil.copytree(work / "build", destination / "build")
        shutil.copy2(work / ".splat/splat_symbols.csv", destination / "splat_symbols.csv")
        shutil.copy2(work / "splat.log", destination / "splat.log")
    manifest = {
        "overlay": options["basename"],
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "splat64": version("splat64"),
        "python": sys.version.split()[0],
        "command": command,
        "retail": {"path": options["target_path"], "sha1": configuration["sha1"],
                   "sha256": hashlib.sha256(retail_bytes).hexdigest()},
        "inputs": {name: digest(DECOMP / name) for name in inputs},
        "outputs": {name: digest(destination / name) for name in (
            *MACRO_FILES, "splat_symbols.csv", *sorted(path.relative_to(destination).as_posix()
                for path in (destination / "build/config").rglob("undefined_*_auto.txt")),
            *sorted(path.relative_to(destination).as_posix()
                for path in (destination / "build/src").rglob("*.s")))},
    }
    (destination / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    return destination


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("config", type=Path, help="Overlay config, e.g. config/BATTLE/BATTLE.PRG/splat.yaml")
    parser.add_argument("--timeout", type=int, default=300)
    arguments = parser.parse_args()
    try:
        print(f"Splat artifacts ready: {generate(arguments.config, arguments.timeout)}")
    except (OSError, RuntimeError, ValueError, subprocess.TimeoutExpired) as error:
        print(f"Splat artifact error: {error}", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
