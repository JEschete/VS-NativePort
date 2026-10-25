"""Windows build operations shared by Visual Studio and the matching project."""

import argparse
import hashlib
import json
import os
import shlex
import shutil
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET
import zipfile
from pathlib import Path

import yaml

ROOT = Path(__file__).resolve().parents[1]
TOOLS = ROOT / "build" / "tools"
DECOMP = ROOT / "resources" / "rood-reverseWin"
UPSTREAM = "https://github.com/ser-pounce/rood-reverse.git"
SOURCE_FOLDERS = ("src", "include", "assets", "config")
SOURCE_TOOL_FOLDERS = ("etc", "libdata", "kaitai", "splat_ext", "data", "dev")
SOURCE_PATHS = tuple(Path(folder) for folder in SOURCE_FOLDERS) + tuple(
    Path("tools") / folder for folder in SOURCE_TOOL_FOLDERS)


def run(arguments, *, cwd=None, input_data=None):
    return subprocess.run([str(argument) for argument in arguments], cwd=cwd,
        input=input_data, capture_output=True,
        check=True, timeout=300).stdout


def download(url: str, target: Path) -> None:
    target.parent.mkdir(parents=True, exist_ok=True)
    if target.exists():
        if target.suffix == ".zip" and not zipfile.is_zipfile(target):
            target.unlink()
        else:
            return
    temporary = target.with_suffix(target.suffix + ".download")
    try:
        run(["curl.exe", "--fail", "--location", "--connect-timeout", "15",
             "--max-time", "120", "--output", temporary, url])
        if target.suffix == ".zip" and not zipfile.is_zipfile(temporary):
            raise RuntimeError(f"Invalid downloaded archive: {url}")
        temporary.replace(target)
    finally:
        if temporary.exists():
            temporary.unlink()


def executable(directory: Path, name: str) -> Path:
    matches = sorted(directory.rglob(name))
    if len(matches) != 1:
        raise RuntimeError(f"Expected one Windows {name} under {directory}, found {len(matches)}")
    return matches[0]


def require_windows_binary(path: Path) -> Path:
    with path.open("rb") as stream:
        if stream.read(2) != b"MZ":
            raise RuntimeError(f"Not a Windows executable: {path}; Linux binaries are unsupported")
    return path


def replace_upstream_sources(snapshot: Path, destination: Path) -> dict[str, str]:
    files = {}
    for relative in SOURCE_PATHS:
        original = snapshot / relative
        target = destination / relative
        if not original.is_dir():
            continue
        if target.exists():
            shutil.rmtree(target)
        shutil.copytree(original, target)
        for copied in target.rglob("*"):
            if copied.is_file():
                files[copied.relative_to(destination).as_posix()] = hashlib.sha256(copied.read_bytes()).hexdigest()
    return files


def source_digests() -> dict[str, str]:
    return {
        path.relative_to(DECOMP).as_posix(): hashlib.sha256(path.read_bytes()).hexdigest()
        for relative in SOURCE_PATHS for path in (DECOMP / relative).rglob("*")
        if path.is_file() and "__pycache__" not in path.parts and path.suffix not in (".pyc", ".pyo")
    }


def check_sync_destination() -> None:
    current = source_digests()
    manifest_path = DECOMP / "upstream.json"
    if not manifest_path.is_file():
        if current:
            raise RuntimeError("Cannot synchronize populated source without an upstream manifest. "
                               "Preserve and review existing work first. No files were changed.")
        return
    expected = json.loads(manifest_path.read_text(encoding="utf-8"))["files"]
    changes = sorted(name for name in current.keys() | expected.keys()
                     if current.get(name) != expected.get(name))
    if changes:
        raise RuntimeError("Synchronization refused: local source changes must be preserved and reviewed first: "
                           + ", ".join(changes[:10]) + ". No files were changed.")


def sync_upstream(fetch: bool = False) -> None:
    check_sync_destination()
    reference = ROOT / "resources" / "rood-reverse"
    if fetch:
        run(["git", "-C", reference, "fetch", UPSTREAM, "main"])
        revision = "FETCH_HEAD"
    else:
        revision = "refs/remotes/upstream/main"
    commit = run(["git", "-C", reference, "rev-parse", revision]).decode("ascii").strip()
    with tempfile.TemporaryDirectory(prefix="vagrant-upstream-") as directory:
        staging = Path(directory)
        archive = staging / "upstream.zip"
        run(["git", "-C", reference, "archive", "--format=zip", f"--output={archive}", commit])
        snapshot = staging / "source"
        with zipfile.ZipFile(archive) as contents:
            contents.extractall(snapshot)
        files = replace_upstream_sources(snapshot, DECOMP)
    (DECOMP / "upstream.json").write_text(json.dumps(
        {"repository": UPSTREAM, "commit": commit, "files": files}, indent=2) + "\n", encoding="utf-8")
    print(f"Authoritative ser-pounce source synchronized at {commit}")


def verify_upstream() -> None:
    """Report provenance only; local decompilation work is used as-is and never replaced."""
    manifest_path = DECOMP / "upstream.json"
    if not manifest_path.exists():
        print("WARNING: Missing upstream manifest; source provenance is unrecorded. No files were changed.")
        return
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    reference = ROOT / "resources" / "rood-reverse"
    commit = run(["git", "-C", reference, "rev-parse", "refs/remotes/upstream/main"]).decode("ascii").strip()
    if commit != manifest["commit"]:
        print(f"WARNING: Upstream reference is {commit}, but the Windows snapshot records "
              f"{manifest['commit']}. Review the update and synchronize explicitly. No files were changed.")
    current = source_digests()
    changes = sorted(name for name in current.keys() | manifest["files"].keys()
                     if current.get(name) != manifest["files"].get(name))
    if changes:
        print(f"Local decompilation changes: {len(changes)} files differ from upstream {manifest['commit']} "
              f"({', '.join(changes[:5])}). Using local source; no files were changed.")
    elif commit == manifest["commit"]:
        print(f"Authoritative upstream source verified at {commit}")
    else:
        print(f"Windows snapshot matches its recorded upstream {manifest['commit']}.")


def bootstrap() -> dict:
    if os.name != "nt":
        raise RuntimeError("The maintained toolchain requires Windows")
    archive = TOOLS / "downloads" / "mkpsxiso-2.30-win64.zip"
    download("https://github.com/Lameguy64/mkpsxiso/releases/download/v2.30/" + archive.name, archive)
    disc_tools = TOOLS / "mkpsxiso"
    if not disc_tools.exists():
        with zipfile.ZipFile(archive) as contents:
            contents.extractall(disc_tools)
    archive = TOOLS / "downloads" / "gcc-mipsel-none-elf-12.3.0-windows.zip"
    download("https://github.com/Lameguy64/PSn00bSDK/releases/download/v0.24/" + archive.name, archive)
    mips_tools = TOOLS / "mips"
    if not mips_tools.exists():
        with zipfile.ZipFile(archive) as contents:
            contents.extractall(mips_tools)
    make = TOOLS / "make.exe"
    download("https://raw.githubusercontent.com/mbuilov/gnumake-windows/"
             "2a2989816aece7c191c0015bff30d104ef31f52a/gnumake-4.4.1-x64.exe", make)
    result = {
        "make": str(require_windows_binary(make)),
        "dumpsxiso": str(require_windows_binary(executable(disc_tools, "dumpsxiso.exe"))),
        "mkpsxiso": str(require_windows_binary(executable(disc_tools, "mkpsxiso.exe"))),
        "mips_bin": str(require_windows_binary(executable(mips_tools, "mipsel-none-elf-as.exe")).parent),
    }
    (TOOLS / "toolchain.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print("Windows GNU Make, disc tools and MIPS toolchain are ready")
    return result


def toolchain() -> dict:
    manifest = TOOLS / "toolchain.json"
    if not manifest.exists():
        return bootstrap()
    return json.loads(manifest.read_text(encoding="utf-8"))


def extract_disc() -> None:
    disc = ROOT / "disks" / "SLUS-01040.bin"
    xml = ROOT / "disks" / "SLUS-01040.xml"
    if not disc.is_file():
        raise FileNotFoundError(f"Supply your own NTSC-U disc image at {disc}")
    tools = toolchain()
    if not xml.exists() or not (ROOT / "data" / "SLUS_010.40").exists():
        arguments = [tools["dumpsxiso"], "-s", xml]
        if not (ROOT / "data" / "SLUS_010.40").exists():
            arguments.extend(["-x", ROOT / "data"])
        arguments.append(disc)
        run(arguments, cwd=ROOT)
    tree = ET.parse(xml)
    for directory in tree.iter("directory_tree"):
        directory.set("source", (ROOT / "data").as_posix())
    for license_element in tree.iter("license"):
        license_element.set("file", (ROOT / "data" / "license_data.dat").as_posix())
    tree.write(xml, encoding="utf-8", xml_declaration=True)
    metadata = ROOT / "build" / "native-data"
    metadata.mkdir(parents=True, exist_ok=True)
    run([tools["mkpsxiso"], "-q", "-noisogen", "-lba", metadata / "lbas.txt",
         "-lbahead", metadata / "disc_lbas.h", xml], cwd=ROOT)
    print("Windows retail layout and LBA header are ready")


def matching_build() -> None:
    verify_upstream()
    tools = toolchain()
    versions = ("2.7.2-psx", "2.7.2-cdk", "2.8.1-psx")
    for version in versions:
        compiler = TOOLS / "old-gcc" / version / "cc1.exe"
        if not compiler.exists():
            raise FileNotFoundError(
                f"Missing Windows historical compiler: {compiler}. "
                "Provide PSYQ-compatible Windows cc1 executables; old-gcc 0.17 has no Windows release. "
                "No Linux/WSL fallback is used.")
        require_windows_binary(compiler)
    extract_disc()
    arguments = [tools["make"], "-C", DECOMP, "-f", "Makefile.windows", "-j4",
        f"PYTHON={Path(sys.executable).as_posix()}",
        f"MIPS_BIN={Path(tools['mips_bin']).as_posix()}",
        f"WINDOWS_TOOLS={TOOLS.as_posix()}"]
    subprocess.run([str(argument) for argument in arguments], check=True, timeout=1800)


def utility(command: str, paths: list[str]) -> None:
    if command == "mkdir":
        for path in paths:
            Path(path).mkdir(parents=True, exist_ok=True)
    elif command == "touch":
        for path in paths:
            Path(path).parent.mkdir(parents=True, exist_ok=True)
            Path(path).touch()
    elif command == "copy":
        shutil.copy2(paths[0], paths[1])
    elif command == "move":
        shutil.move(paths[0], paths[1])
    elif command == "compare":
        failures = []
        for generated in paths:
            reference = Path(str(generated).removeprefix("build/"))
            if hashlib.sha256(Path(generated).read_bytes()).digest() != hashlib.sha256(reference.read_bytes()).digest():
                failures.append(str(generated))
        if failures:
            raise RuntimeError("Retail byte mismatch: " + ", ".join(failures))
        print(f"All {len(paths)} targets match the retail files")
    elif command == "copy-source-data":
        shutil.copytree(paths[0], paths[1], dirs_exist_ok=True)
    elif command == "size":
        print(Path(paths[0]).stat().st_size)
    elif command == "title-bg-size":
        configuration = yaml.safe_load(Path(paths[0]).read_text(encoding="utf-8"))
        for segment in configuration["segments"]:
            if not isinstance(segment, dict):
                continue
            subsections = segment.get("subsegments", [])
            for index, subsection in enumerate(subsections[:-1]):
                name = subsection.get("name") if isinstance(subsection, dict) else subsection[2] if len(subsection) > 2 else None
                if name != "_titleScreenBg":
                    continue
                start = subsection["start"] if isinstance(subsection, dict) else subsection[0]
                following = subsections[index + 1]
                finish = following["start"] if isinstance(following, dict) else following[0]
                print(finish - start)
                return
        raise ValueError("Upstream title background boundary was not found")
    elif command == "image-object":
        binary, symbols, output = map(Path, paths)
        objcopy = Path(toolchain()["mips_bin"]) / "mipsel-none-elf-objcopy.exe"
        arguments = [objcopy, "-I", "binary", "-O", "elf32-tradlittlemips"]
        for line in symbols.read_text(encoding="utf-8").splitlines():
            name, offset = line.split()
            arguments.extend(["--add-symbol", f"{name}=.data:{offset}"])
        arguments.extend([binary, output])
        run(arguments)
    else:
        raise ValueError(f"Unsupported Windows build utility: {command}")


def compile_matching(arguments) -> None:
    mips = Path(toolchain()["mips_bin"])
    compiler = require_windows_binary(Path(arguments.compiler))
    content = run([mips / "mipsel-none-elf-cpp.exe", *shlex.split(arguments.cppflags), arguments.source])
    content = run([sys.executable, "-m", "tools.etc.vsStringTransformer"], input_data=content)
    content = run([compiler, *shlex.split(arguments.cc1flags)], input_data=content)
    content = run([sys.executable, "tools/maspsx/maspsx.py", *shlex.split(arguments.masflags)], input_data=content)
    run([mips / "mipsel-none-elf-as.exe", *shlex.split(arguments.asflags),
         "-o", arguments.output], input_data=content)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("bootstrap", "sync", "verify-upstream", "extract", "decomp", "compile", "mkdir", "touch",
        "copy", "move", "compare", "copy-source-data", "size", "title-bg-size", "image-object"))
    parser.add_argument("paths", nargs="*")
    parser.add_argument("--source")
    parser.add_argument("--output")
    parser.add_argument("--compiler")
    parser.add_argument("--cppflags", default="")
    parser.add_argument("--cc1flags", default="")
    parser.add_argument("--masflags", default="")
    parser.add_argument("--asflags", default="")
    parser.add_argument("--fetch-upstream", action="store_true")
    arguments = parser.parse_args()
    try:
        if arguments.command == "bootstrap":
            bootstrap()
        elif arguments.command == "sync":
            sync_upstream(arguments.fetch_upstream)
        elif arguments.command == "verify-upstream":
            verify_upstream()
        elif arguments.command == "extract":
            extract_disc()
        elif arguments.command == "decomp":
            matching_build()
        elif arguments.command == "compile":
            compile_matching(arguments)
        else:
            utility(arguments.command, arguments.paths)
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as error:
        print(f"Windows build failed: {error}", file=sys.stderr)
        sys.exit(1)
