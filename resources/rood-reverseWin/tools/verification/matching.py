#!/usr/bin/env python3
"""Isolated WSL-only PSX matching verification; never runs the root build."""

import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile


DECOMP = Path(__file__).resolve().parents[2]
ROOT = DECOMP.parents[1]
RELEASE = "0.17"
COMPILERS = {
    "2.7.2-psx": "500a459b3485e885a8d302cac23c2a4632f3900e03a09153f6190699fd723571",
    "2.7.2-cdk": "42bb0df96db11a9b5d2e23d78bdc962791f40046280d3d360da93fe5eef6f0bb",
    "2.8.1-psx": "f6f6e883942d4d3289d048236c672e71ed410e546aaae8ff655952f1567e1be0",
}
BINUTILS = ("make", "mipsel-linux-gnu-cpp", "mipsel-linux-gnu-as", "mipsel-linux-gnu-ld")
SYMBOL = re.compile(r"^\s*([A-Za-z_]\w*)\s*=\s*(0x[0-9A-Fa-f]+)\s*;", re.MULTILINE)
R_MIPS_26 = 4


def run(arguments, *, cwd=None, input_data=b"", timeout=60):
    result = subprocess.run(
        [str(argument) for argument in arguments], cwd=cwd, input=input_data,
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=timeout,
    )
    if result.returncode:
        raise RuntimeError(
            f"Command failed ({result.returncode}): {arguments[0]}\n"
            + result.stderr.decode("utf-8", errors="replace")[-6000:]
        )
    return result.stdout


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def require_wsl():
    if sys.platform != "linux" or "microsoft" not in platform.release().lower():
        raise RuntimeError("Run this verifier inside WSL; Windows builds do not use it.")
    missing = [name for name in BINUTILS if shutil.which(name) is None]
    if missing:
        raise RuntimeError("Missing WSL prerequisites: " + ", ".join(missing))


def initialize(cache):
    if not shutil.which("curl"):
        raise RuntimeError("curl is required inside WSL.")
    cache.mkdir(parents=True, exist_ok=True)
    for version, expected in COMPILERS.items():
        destination = cache / "old-gcc" / version
        compiler = destination / "cc1"
        archive = cache / f"gcc-{version}.tar.gz"
        if not archive.is_file() or digest(archive) != expected:
            temporary = archive.with_suffix(".download")
            try:
                run([
                    "curl", "--fail", "--location", "--connect-timeout", "15",
                    "--max-time", "120", "--output", temporary,
                    f"https://github.com/decompals/old-gcc/releases/download/{RELEASE}/gcc-{version}.tar.gz",
                ], timeout=135)
                if digest(temporary) != expected:
                    raise RuntimeError(f"SHA-256 mismatch for {version}; not extracting.")
                temporary.replace(archive)
            finally:
                temporary.unlink(missing_ok=True)
        destination.mkdir(parents=True, exist_ok=True)
        with tarfile.open(archive) as contents:
            contents.extractall(destination, filter="data")
        if not compiler.is_file():
            raise RuntimeError(f"Archive did not provide {compiler}.")
        compiler.chmod(compiler.stat().st_mode | 0o111)
        if compiler.read_bytes()[:4] != b"\x7fELF":
            raise RuntimeError(f"Not a Linux ELF compiler: {compiler}")
        run([compiler, "-version"], timeout=10)
        print(f"Verified old-gcc {version} (archive SHA-256 {expected})", flush=True)

    python = cache / "venv" / "bin" / "python"
    if not python.is_file():
        run([sys.executable, "-m", "venv", cache / "venv"], timeout=60)
    run([
        python, "-m", "pip", "install", "--disable-pip-version-check",
        "--timeout", "20", "--retries", "1", "pyelftools==0.32", "PyYAML==6.0.2",
    ], timeout=180)
    run([python, "-c", "import elftools, yaml; print(elftools.__version__, yaml.__version__)"], timeout=10)
    state = {
        "release": RELEASE, "archive_sha256": COMPILERS,
        "compiler_sha256": {version: digest(cache / "old-gcc" / version / "cc1") for version in COMPILERS},
        "python": str(python), "python_version": platform.python_version(),
        "binutils": {name: shutil.which(name) for name in BINUTILS},
    }
    (cache / "toolchain.json").write_text(json.dumps(state, indent=2) + "\n", encoding="utf-8")
    print(f"WSL verifier toolchain ready at {cache}", flush=True)


def source_inventory():
    return {
        path.relative_to(DECOMP).as_posix(): (digest(path), path.stat().st_mtime_ns)
        for folder in ("src", "include", "config")
        for path in (DECOMP / folder).rglob("*") if path.is_file()
    }


def configured_flags(snapshot, source):
    overlay = source.relative_to("src").parent
    object_path = Path("build") / source.with_suffix(".c.o")
    makefile = snapshot / "verify-flags.mk"
    makefile.write_text(
        "BUILD := build\n"
        "CC1VER := 2.7.2-psx\n"
        "CC1FLAGS := -G0 -O2 -Wall -quiet -fno-builtin -funsigned-char -Wno-unused\n"
        "ASFLAGS := -I include -G0\n"
        "MASFLAGS := --aspsx-version=2.77 --macro-inc\n"
        f"include config/{overlay.as_posix()}/Makefile\n"
        f"{object_path.as_posix()}:\n"
        "\t@echo $(CC1VER)\n\t@echo $(CC1FLAGS)\n\t@echo $(ASFLAGS)\n\t@echo $(MASFLAGS)\n",
        encoding="utf-8",
    )
    output = run(["make", "--no-print-directory", "-f", makefile.name, object_path], cwd=snapshot)
    makefile.unlink()
    lines = output.decode("utf-8").splitlines()
    if len(lines) != 4 or lines[0] not in COMPILERS:
        raise RuntimeError("Could not determine this unit's historical compiler settings.")
    return lines[0], lines[1].split(), lines[2].split(), lines[3].split()


def tree_digests(root):
    return {path.relative_to(root).as_posix(): digest(path) for path in sorted(root.rglob("*")) if path.is_file()}


def aggregate_digest(files):
    return hashlib.sha256("".join(f"{name} {value}\n" for name, value in sorted(files.items())).encode()).hexdigest()


def load_splat_artifacts(configuration, configuration_path):
    """Use Windows-generated splat macros only while their recorded inputs are current."""
    overlay = configuration["options"]["basename"]
    directory = DECOMP / "build" / "splat-artifacts" / overlay
    manifest_path = directory / "manifest.json"
    if not manifest_path.is_file():
        raise RuntimeError(f"Missing splat artifacts for {overlay}; run tools/verification/splat_artifacts.py on Windows.")
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    required = {"config/splat.config.yaml", configuration_path.as_posix(), *configuration["options"]["symbol_addrs_path"]}
    stale = sorted(required - manifest["inputs"].keys())
    stale += [name for name, value in manifest["inputs"].items()
              if not (DECOMP / name).is_file() or digest(DECOMP / name) != value]
    stale += [name for name, value in manifest["outputs"].items()
              if not (directory / name).is_file() or digest(directory / name) != value]
    if manifest["retail"]["sha1"] != configuration["sha1"]:
        stale.append(manifest["retail"]["path"])
    if stale:
        raise RuntimeError("Stale splat artifacts (" + ", ".join(stale) + "); regenerate them on Windows.")
    return directory, manifest, digest(manifest_path)


def dependency_digests(snapshot, cpp_flags, source):
    rule = run(["mipsel-linux-gnu-cpp", *cpp_flags, "-M", source], cwd=snapshot).decode("utf-8")
    names = rule.replace("\\\n", " ").split()[1:]
    return {name: digest(Path(name) if Path(name).is_absolute() else snapshot / name) for name in names}


def check_function(cache, source, function, candidate=None):
    from elftools.elf.elffile import ELFFile
    from elftools.elf.relocation import RelocationSection
    from elftools.elf.sections import SymbolTableSection
    import yaml

    source = Path(source)
    native_header_probe = os.environ.get("VAGRANT_MATCH_NATIVE_HEADERS") == "1"
    if source.is_absolute() or ".." in source.parts or source.suffix != ".c" or source.parts[0] != "src":
        raise RuntimeError("--source must be a relative src/.../*.c path; modern C++ is not a historical C input.")
    canonical = DECOMP / source
    if candidate is not None:
        candidate = candidate.expanduser().resolve()
        if native_header_probe:
            raise RuntimeError("Native-header probes cannot be combined with candidates.")
        if any(candidate.is_relative_to(DECOMP / folder) for folder in ("src", "include", "config")):
            raise RuntimeError("Keep candidates outside canonical src/include/config folders.")
    mode = "probe" if native_header_probe else "candidate" if candidate else "canonical"
    report_path = (cache / "reports" / {"canonical": "", "candidate": "candidates", "probe": "probes"}[mode]
                   / f"{source.stem}-{function}.json")
    report_path.unlink(missing_ok=True)
    report_path.with_suffix(".s").unlink(missing_ok=True)
    report_path.with_suffix(".dis").unlink(missing_ok=True)
    content = (candidate or canonical).read_text(encoding="utf-8")
    if function in re.findall(r'INCLUDE_ASM\(\s*"[^"]+"\s*,\s*(\w+)\s*\)', content):
        raise RuntimeError(f"{function} is still INCLUDE_ASM; assembling original code is not a decompilation match.")
    configuration_path = Path("config") / source.relative_to("src").parent / "splat.yaml"
    configuration = yaml.safe_load((DECOMP / configuration_path).read_text(encoding="utf-8"))
    artifacts, manifest, manifest_digest = load_splat_artifacts(configuration, configuration_path)
    retail = ROOT / configuration["options"]["target_path"]
    retail_bytes = retail.read_bytes()
    retail_sha1 = hashlib.sha1(retail_bytes).hexdigest()
    if retail_sha1 != configuration["sha1"]:
        raise RuntimeError("Retail binary SHA-1 does not match the overlay configuration.")
    symbols = {}
    ambiguous_symbols = {}
    function_addresses = []
    symbol_paths = configuration["options"]["symbol_addrs_path"]
    for index, relative in enumerate(symbol_paths):
        for line in (DECOMP / relative).read_text(encoding="utf-8").splitlines():
            match = SYMBOL.match(line)
            if match is None:
                continue
            name, address = match[1], int(match[2], 16)
            if name in symbols and symbols[name] != address:
                ambiguous_symbols.setdefault(name, {symbols[name]}).add(address)
            else:
                symbols[name] = address
            if index == 0 and ("type:func" in line or name.startswith("func_")):
                function_addresses.append(address)
    if function in ambiguous_symbols:
        raise RuntimeError(f"Conflicting retail address for target {function}.")
    symbols = {name: address for name, address in symbols.items() if name not in ambiguous_symbols}
    if function not in symbols:
        raise RuntimeError(f"No retail address for {function}.")
    address = symbols[function]
    later = sorted(set(value for value in function_addresses if value > address))
    if not later:
        raise RuntimeError("No known next function boundary; cannot establish a complete target range.")
    segment = next(segment for segment in configuration["segments"] if isinstance(segment, dict) and "vram" in segment)
    offset = segment["start"] + address - segment["vram"]
    expected_size = later[0] - address
    expected = retail_bytes[offset:offset + expected_size]
    if offset < 0 or len(expected) != expected_size:
        raise RuntimeError("Function range is outside the verified retail binary.")

    before = source_inventory()
    try:
        with tempfile.TemporaryDirectory(prefix="check-", dir=cache) as directory:
            snapshot = Path(directory)
            for folder in ("src", "include", "config", "tools/etc", "tools/libdata", "tools/maspsx"):
                shutil.copytree(DECOMP / folder, snapshot / folder, ignore=shutil.ignore_patterns("__pycache__", "*.pyc"))
            for name in manifest["outputs"]:
                shutil.copy2(artifacts / name, snapshot / name)
            if candidate:
                (snapshot / source).write_bytes(candidate.read_bytes())
            if native_header_probe:
                shutil.copytree(ROOT / "native/include", snapshot / "native/include")
            inputs = tree_digests(snapshot)
            version, compiler_flags, assembler_flags, maspsx_flags = configured_flags(snapshot, source)
            compiler = cache / "old-gcc" / version / "cc1"
            state = json.loads((cache / "toolchain.json").read_text(encoding="utf-8"))
            if digest(compiler) != state["compiler_sha256"][version]:
                raise RuntimeError("Installed compiler hash changed; rerun init.")
            print(f"Historical compile: {source.as_posix()}; GCC {version}; "
                  f"flags {' '.join(compiler_flags)}", flush=True)
            if ambiguous_symbols:
                print(f"Excluded {len(ambiguous_symbols)} ambiguous map symbols; "
                      "any compiled reference to one will fail verification.", flush=True)
            cpp_flags = [
                "-nostdinc", "-I", "include/psx", "-I", "src/include", "-I", ".",
                "-D__attribute__(x)=", "-D_LANGUAGE_C", "-DOBJDIFF",
            ]
            if native_header_probe:
                cpp_flags.remove("-nostdinc")
                cpp_flags.extend(["-I", "native/include"])
                print("Native-header compatibility probe: using copied native declarations "
                      "and MIPS system headers; not a canonical matching build.", flush=True)
            dependencies = dependency_digests(snapshot, cpp_flags, source)
            preprocessed = run(["mipsel-linux-gnu-cpp", *cpp_flags, source], cwd=snapshot)
            preprocessed = run([sys.executable, "-m", "tools.etc.vsStringTransformer"], cwd=snapshot, input_data=preprocessed)
            assembly = run([compiler, *compiler_flags], cwd=snapshot, input_data=preprocessed)
            assembly = run([sys.executable, "tools/maspsx/maspsx.py", *maspsx_flags], cwd=snapshot, input_data=assembly)
            object_path = snapshot / "unit.o"
            run(["mipsel-linux-gnu-as", *assembler_flags, "-o", object_path], cwd=snapshot, input_data=assembly)
            with object_path.open("rb") as stream:
                elf = ELFFile(stream)
                table = elf.get_section_by_name(".symtab")
                if not isinstance(table, SymbolTableSection):
                    raise RuntimeError("Compiled object has no symbol table.")
                compiled_symbols = table.get_symbol_by_name(function)
                if not compiled_symbols or len(compiled_symbols) != 1:
                    raise RuntimeError(f"Expected exactly one compiled symbol for {function}.")
                compiled = compiled_symbols[0]
                section_index = compiled["st_shndx"]
                if not isinstance(section_index, int):
                    raise RuntimeError("Target is not a defined compiled symbol.")
                section = elf.get_section(section_index)
                if not section["sh_flags"] & 4:
                    raise RuntimeError("Target is not in an executable section.")
                section_name = section.name
                start = compiled["st_value"]
                ends = [symbol["st_value"] for symbol in table.iter_symbols()
                        if symbol["st_shndx"] == section_index
                        and symbol["st_info"]["bind"] == "STB_GLOBAL"
                        and symbol["st_value"] > start]
                size = compiled["st_size"] or (min(ends) if ends else section["sh_size"]) - start
                section_data = section.data()
                target_relocations = []
                for relocation_section in elf.iter_sections():
                    if not isinstance(relocation_section, RelocationSection) or relocation_section["sh_info"] != section_index:
                        continue
                    relocation_symbols = elf.get_section(relocation_section["sh_link"])
                    if not isinstance(relocation_symbols, SymbolTableSection):
                        raise RuntimeError("Relocation section does not reference a symbol table.")
                    for relocation in relocation_section.iter_relocations():
                        if not start <= relocation["r_offset"] < start + size:
                            continue
                        symbol = relocation_symbols.get_symbol(relocation["r_info_sym"])
                        name = symbol.name
                        if (not name and symbol["st_info"]["type"] == "STT_SECTION"
                                and symbol["st_shndx"] == section_index and relocation["r_info_type"] == R_MIPS_26):
                            # A jump inside the target resolves exactly because the target is linked at its retail address.
                            word = int.from_bytes(section_data[relocation["r_offset"]:relocation["r_offset"] + 4], "little")
                            destination = (word & 0x3FFFFFF) << 2
                            if not start <= destination < start + size:
                                raise RuntimeError(f"Target jumps outside itself to section offset {destination:#x}.")
                            target_relocations.append({"offset": hex(relocation["r_offset"] - start),
                                                       "type": R_MIPS_26, "symbol": section_name,
                                                       "internal_target": hex(destination - start)})
                            continue
                        if name in ambiguous_symbols:
                            addresses = ", ".join(hex(value) for value in sorted(ambiguous_symbols[name]))
                            raise RuntimeError(f"Ambiguous target relocation {name}: {addresses}")
                        if name not in symbols:
                            raise RuntimeError(f"Target relocation has no verified retail address: {name or 'section-relative'}")
                        target_relocations.append({"offset": hex(relocation["r_offset"] - start),
                                                   "type": relocation["r_info_type"], "symbol": name,
                                                   "address": hex(symbols[name])})
            text_base = address - start
            linker_symbols = snapshot / "retail-symbols.ld"
            # SUBALIGN prevents input-section alignment padding from shifting compiled offsets.
            linker_symbols.write_text(
                "\n".join(f"{name} = 0x{value:X};" for name, value in sorted(symbols.items()))
                + f"\nSECTIONS {{ {section_name} 0x{text_base:X} : SUBALIGN(4) {{ *({section_name}) }} }}\n",
                encoding="ascii",
            )
            linked_path = snapshot / "unit.elf"
            # Unresolved symbols outside the target cannot affect compared bytes; target relocations are verified above.
            linker_arguments = ["-G0", "-nostdlib", "--build-id=none", "--unresolved-symbols=ignore-all",
                                "-e", "0", "-T", linker_symbols.name,
                                "-o", linked_path.name, object_path.name]
            run(["mipsel-linux-gnu-ld", *linker_arguments], cwd=snapshot)
            with linked_path.open("rb") as stream:
                linked = ELFFile(stream)
                text = linked.get_section_by_name(section_name)
                if text is None or text["sh_addr"] != text_base:
                    raise RuntimeError("Linked target section is not at its retail-relative address.")
                actual = text.data()[start:start + size]
            differences = [
                {"offset": index, "expected": expected[index:index + 4].hex(), "actual": actual[index:index + 4].hex()}
                for index in range(0, max(len(expected), len(actual)), 4)
                if expected[index:index + 4] != actual[index:index + 4]
            ]
            status = {"canonical": "", "candidate": "CANDIDATE_", "probe": "PROBE_"}[mode] + ("DIFF" if differences else "MATCH")
            tool_versions = {name: run([name, "--version"], timeout=10).decode("utf-8", errors="replace").splitlines()[0]
                             for name in ("make", "mipsel-linux-gnu-cpp", "mipsel-linux-gnu-as", "mipsel-linux-gnu-ld")}
            report = {
                "status": status, "mode": mode, "scope": "function-instructions-retail-relocations",
                "generated_utc": datetime.now(timezone.utc).isoformat(),
                "command": [sys.executable, *sys.argv], "report_path": str(report_path),
                "source": source.as_posix(), "source_sha256": digest(canonical),
                "candidate": str(candidate) if candidate else None,
                "candidate_sha256": digest(candidate) if candidate else None,
                "function": function, "cpu_address": hex(address), "file_offset": hex(offset),
                "comparison_bytes": expected_size,
                "retail": {"path": configuration["options"]["target_path"], "sha1": retail_sha1,
                           "sha256": hashlib.sha256(retail_bytes).hexdigest()},
                "compiler": version, "compiler_sha256": digest(compiler), "cc1_flags": compiler_flags,
                "cpp_flags": cpp_flags, "maspsx_flags": maspsx_flags, "assembler_flags": assembler_flags,
                "linker_arguments": linker_arguments, "tool_versions": tool_versions,
                "verifier_sha256": digest(Path(__file__).resolve()),
                "splat_artifacts": {"manifest_sha256": manifest_digest, "splat64": manifest["splat64"],
                                    "generated_utc": manifest["generated_utc"]},
                "snapshot_files": len(inputs), "snapshot_sha256": aggregate_digest(inputs),
                "dependencies": dependencies, "target_relocations": target_relocations,
                "expected_bytes": len(expected), "actual_bytes": len(actual), "differences": differences,
                "canonical_sources_unchanged": before == source_inventory(),
                "native_header_probe": native_header_probe,
                "limitations": ["Function-only comparison: not a source-unit or linked-overlay match or behavioral proof.",
                                "Other INCLUDE_ASM/INCLUDE_RODATA bodies are omitted via OBJDIFF.",
                                "Relocations outside the target function are not validated."]
                    + (["Noncanonical candidate outside canonical source: never acceptance evidence."] if candidate else [])
                    + (["Noncanonical native/MIPS standard headers: compatibility probe only."]
                       if native_header_probe else []),
            }
            if not report["canonical_sources_unchanged"]:
                raise RuntimeError("Canonical source hash or timestamp changed during verification.")
            report_path.parent.mkdir(parents=True, exist_ok=True)
            report_path.with_suffix(".s").write_bytes(assembly)
            report_path.with_suffix(".dis").write_bytes(run([
                "mipsel-linux-gnu-objdump", "-d", "-z", f"--start-address=0x{address:X}",
                f"--stop-address=0x{address + size:X}", linked_path.name], cwd=snapshot))
            report_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
            print(f"{status} {function}: {len(actual)} compiled / {len(expected)} retail bytes; GCC {version}")
            print(f"Canonical source hashes/timestamps unchanged. Report: {report_path}")
            for difference in differences[:12]:
                print(f"  +0x{difference['offset']:X}: retail {difference['expected']} compiled {difference['actual']}")
            if differences:
                return 1
            return 0 if mode == "canonical" else 3
    finally:
        if before != source_inventory():
            raise RuntimeError("Canonical sources changed while the verifier ran; do not trust this check.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("init", "status", "check"))
    parser.add_argument("--cache", type=Path, default=Path.home() / ".cache" / "vagrant-matching")
    parser.add_argument("--source")
    parser.add_argument("--function")
    parser.add_argument("--candidate", type=Path, help="Noncanonical candidate unit outside canonical source folders")
    arguments = parser.parse_args()
    try:
        require_wsl()
        cache = arguments.cache.expanduser().resolve()
        if cache.is_relative_to(DECOMP) or cache.is_relative_to(ROOT):
            raise RuntimeError("Keep the verifier cache outside the Windows source repository.")
        if arguments.command == "init":
            initialize(cache)
        elif arguments.command == "check":
            if not arguments.source or not arguments.function:
                raise RuntimeError("check requires --source and --function.")
            if not (cache / "toolchain.json").is_file():
                raise RuntimeError("Verifier is not initialized; run init first.")
            return check_function(cache, arguments.source, arguments.function, arguments.candidate)
        else:
            state = cache / "toolchain.json"
            print(state.read_text(encoding="utf-8") if state.is_file() else "Not initialized; run init.")
        return 0
    except (OSError, RuntimeError, ValueError, KeyError, ImportError, subprocess.TimeoutExpired) as error:
        print(f"Verifier error: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())