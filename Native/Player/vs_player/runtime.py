import hashlib
import json
import os
import shutil
import stat
import urllib.request
import uuid
import zipfile
from pathlib import Path, PurePosixPath

from .config import PlayerPaths, runtime_manifest


def digest_file(path: Path) -> str:
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def download(url: str, destination: Path, expected_digest: str) -> None:
    if not url.startswith("https://"):
        raise ValueError("Runtime downloads require HTTPS")
    if destination.exists():
        if digest_file(destination) != expected_digest:
            raise ValueError(f"Existing download has an unexpected digest: {destination}")
        return
    partial = destination.with_name(destination.name + ".partial")
    if partial.exists():
        raise ValueError(f"Interrupted download needs explicit removal before retry: {partial}")
    request = urllib.request.Request(url, headers={"User-Agent": "VS.Player/0.1"})
    total = 0
    announced = 0
    with urllib.request.urlopen(request, timeout=60) as response, partial.open("xb") as output:
        while chunk := response.read(1024 * 1024):
            output.write(chunk)
            total += len(chunk)
            if total - announced >= 128 * 1024 * 1024:
                print(f"Downloaded {total // (1024 * 1024)} MiB", flush=True)
                announced = total
    if digest_file(partial) != expected_digest:
        raise ValueError(f"Downloaded runtime failed SHA-256 verification: {partial}")
    partial.replace(destination)


def validate_archive(archive: zipfile.ZipFile) -> None:
    for member in archive.infolist():
        path = PurePosixPath(member.filename)
        if (path.is_absolute() or ".." in path.parts or "\\" in member.filename
                or any(":" in part for part in path.parts)):
            raise ValueError(f"Unsafe runtime archive path: {member.filename}")
        if stat.S_ISLNK(member.external_attr >> 16):
            raise ValueError(f"Runtime archive contains a link: {member.filename}")


def prepare_runtime(paths: PlayerPaths) -> Path:
    paths.initialize()
    manifest = runtime_manifest()
    runtime = manifest["runtime"]
    directory = paths.runtime / ("ollama-" + runtime["version"])
    executable = directory / runtime["executable"]
    receipt = directory / "installation.json"
    if directory.exists():
        if not executable.is_file() or not receipt.is_file():
            raise ValueError(f"Incomplete runtime installation: {directory}")
        installed = json.loads(receipt.read_text(encoding="utf-8"))
        if (installed["archive_sha256"] != runtime["sha256"]
                or installed["executable_sha256"] != digest_file(executable)):
            raise ValueError("Installed runtime does not match its verified receipt")
        return executable
    required = 12 * 1024 * 1024 * 1024
    if shutil.disk_usage(paths.root).free < required:
        raise OSError("At least 12 GiB free space is required for runtime/model setup")
    archive_path = paths.runtime / ("ollama-" + runtime["version"] + ".zip")
    download(runtime["url"], archive_path, runtime["sha256"])
    staging = paths.runtime / ("install-" + uuid.uuid4().hex)
    staging.mkdir()
    with zipfile.ZipFile(archive_path) as archive:
        validate_archive(archive)
        archive.extractall(staging)
    staged_executable = staging / runtime["executable"]
    if not staged_executable.is_file():
        raise ValueError(f"Verified archive lacks {runtime['executable']}")
    (staging / "installation.json").write_text(json.dumps({
        "version": runtime["version"],
        "archive_sha256": runtime["sha256"],
        "executable_sha256": digest_file(staged_executable),
    }, indent=2) + "\n", encoding="utf-8")
    staging.rename(directory)
    return executable


def server_environment(paths: PlayerPaths, port: int = 11551) -> dict[str, str]:
    environment = dict(os.environ)
    environment.update({
        "OLLAMA_HOST": f"127.0.0.1:{port}",
        "OLLAMA_MODELS": str(paths.models),
        "OLLAMA_NUM_PARALLEL": "1",
        "OLLAMA_MAX_LOADED_MODELS": "1",
        "OLLAMA_CONTEXT_LENGTH": str(runtime_manifest()["model"]["context_tokens"]),
        "OLLAMA_FLASH_ATTENTION": "1",
        "OLLAMA_KV_CACHE_TYPE": "q8_0",
    })
    return environment


def verify_model_manifest() -> dict:
    model = runtime_manifest()["model"]
    with urllib.request.urlopen(model["manifest_url"], timeout=30) as response:
        actual = json.load(response)
    weights = [layer for layer in actual["layers"]
               if layer["mediaType"] == "application/vnd.ollama.image.model"]
    if len(weights) != 1 or weights[0]["digest"] != "sha256:" + model["weights_sha256"]:
        raise ValueError("Published model tag no longer matches the pinned weights")
    return actual
