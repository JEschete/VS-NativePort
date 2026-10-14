"""Compile imported matching C through the maintained Windows toolchain."""

import argparse
import importlib.util
from pathlib import Path
import shlex
import sys

from psyq_assembly import preprocess_psyq


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--compiler", required=True)
    parser.add_argument("--cppflags", default="")
    parser.add_argument("--cc1flags", default="")
    parser.add_argument("--masflags", default="")
    parser.add_argument("--asflags", default="")
    parser.add_argument("--preprocess", choices=("maspsx", "psyq-reorder"), default="maspsx")
    arguments = parser.parse_args()
    root = Path(__file__).resolve().parents[4]
    specification = importlib.util.spec_from_file_location("windows_operations", root / "tools/windows.py")
    windows = importlib.util.module_from_spec(specification)
    specification.loader.exec_module(windows)
    if arguments.preprocess == "maspsx":
        windows.compile_matching(arguments)
        return
    compiler = windows.require_windows_binary(Path(arguments.compiler))
    mips = Path(windows.toolchain()["mips_bin"])
    content = windows.run([mips / "mipsel-none-elf-cpp.exe",
        *shlex.split(arguments.cppflags), arguments.source])
    content = windows.run([sys.executable, "-m", "tools.etc.vsStringTransformer"], input_data=content)
    content = windows.run([compiler, *shlex.split(arguments.cc1flags)], input_data=content)
    content = preprocess_psyq(content)
    windows.run([mips / "mipsel-none-elf-as.exe", *shlex.split(arguments.asflags),
        "-o", arguments.output], input_data=content)


if __name__ == "__main__":
    main()