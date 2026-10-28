import ctypes
import json
import os
import shutil
import socket
import subprocess
import time
import urllib.error
from contextlib import contextmanager
from ctypes import wintypes
from pathlib import Path

from .config import PlayerPaths, runtime_manifest
from .model import LocalModel
from .runtime import digest_file, prepare_runtime, server_environment, verify_model_manifest


class ProcessJob:
    """Own only explicitly assigned child processes and their descendants."""

    def __init__(self, process: subprocess.Popen) -> None:
        if os.name != "nt":
            raise OSError("Managed native processes currently require Windows")

        class BasicLimits(ctypes.Structure):
            _fields_ = [
                ("process_time", ctypes.c_longlong), ("job_time", ctypes.c_longlong),
                ("flags", wintypes.DWORD), ("minimum_working_set", ctypes.c_size_t),
                ("maximum_working_set", ctypes.c_size_t), ("active_processes", wintypes.DWORD),
                ("affinity", ctypes.c_size_t), ("priority", wintypes.DWORD),
                ("scheduling", wintypes.DWORD),
            ]

        class IoCounters(ctypes.Structure):
            _fields_ = [(name, ctypes.c_ulonglong) for name in (
                "read_operations", "write_operations", "other_operations",
                "read_bytes", "write_bytes", "other_bytes",
            )]

        class ExtendedLimits(ctypes.Structure):
            _fields_ = [
                ("basic", BasicLimits), ("io", IoCounters),
                ("process_memory", ctypes.c_size_t), ("job_memory", ctypes.c_size_t),
                ("peak_process_memory", ctypes.c_size_t), ("peak_job_memory", ctypes.c_size_t),
            ]

        self.kernel = ctypes.WinDLL("kernel32", use_last_error=True)
        self.kernel.CreateJobObjectW.argtypes = (ctypes.c_void_p, wintypes.LPCWSTR)
        self.kernel.CreateJobObjectW.restype = wintypes.HANDLE
        self.kernel.SetInformationJobObject.argtypes = (
            wintypes.HANDLE, ctypes.c_int, ctypes.c_void_p, wintypes.DWORD,
        )
        self.kernel.SetInformationJobObject.restype = wintypes.BOOL
        self.kernel.AssignProcessToJobObject.argtypes = (wintypes.HANDLE, wintypes.HANDLE)
        self.kernel.AssignProcessToJobObject.restype = wintypes.BOOL
        self.kernel.OpenProcess.argtypes = (wintypes.DWORD, wintypes.BOOL, wintypes.DWORD)
        self.kernel.OpenProcess.restype = wintypes.HANDLE
        self.kernel.CloseHandle.argtypes = (wintypes.HANDLE,)
        self.kernel.CloseHandle.restype = wintypes.BOOL
        self.handle = self.kernel.CreateJobObjectW(None, None)
        if not self.handle:
            process.terminate()
            process.wait(timeout=10)
            raise ctypes.WinError(ctypes.get_last_error())
        limits = ExtendedLimits()
        limits.basic.flags = 0x2000
        if not self.kernel.SetInformationJobObject(
            self.handle, 9, ctypes.byref(limits), ctypes.sizeof(limits)
        ):
            error = ctypes.get_last_error()
            self.close()
            process.terminate()
            process.wait(timeout=10)
            raise ctypes.WinError(error)
        child = self.kernel.OpenProcess(0x101, False, process.pid)
        if not child:
            error = ctypes.get_last_error()
            self.close()
            process.terminate()
            process.wait(timeout=10)
            raise ctypes.WinError(error)
        try:
            if not self.kernel.AssignProcessToJobObject(self.handle, child):
                error = ctypes.get_last_error()
                self.close()
                process.terminate()
                process.wait(timeout=10)
                raise ctypes.WinError(error)
        finally:
            self.kernel.CloseHandle(child)

    def close(self) -> None:
        if self.handle:
            self.kernel.CloseHandle(self.handle)
            self.handle = None


def verify_installed_model(paths: PlayerPaths) -> Path:
    model = runtime_manifest()["model"]
    weights = paths.models / "blobs" / ("sha256-" + model["weights_sha256"])
    if not weights.is_file():
        raise FileNotFoundError("Pinned model weights are missing; run bootstrap")
    if weights.stat().st_size != model["weights_bytes"] or digest_file(weights) != model["weights_sha256"]:
        raise ValueError("Installed model weights failed integrity verification")
    return weights


@contextmanager
def model_server(paths: PlayerPaths, port: int = 11551,
                 log_directory: Path | None = None):
    executable = prepare_runtime(paths)
    with socket.socket() as probe:
        probe.settimeout(0.25)
        result = probe.connect_ex(("127.0.0.1", port))
        if result == 0:
            raise RuntimeError(f"Local model port {port} is already occupied; it will not be replaced")
    logs = log_directory or paths.runtime
    logs.mkdir(exist_ok=True)
    stdout_path = logs / "server.stdout.log"
    stderr_path = logs / "server.stderr.log"
    with stdout_path.open("ab") as stdout, stderr_path.open("ab") as stderr:
        process = subprocess.Popen(
            [str(executable), "serve"], env=server_environment(paths, port),
            stdout=stdout, stderr=stderr,
            creationflags=subprocess.CREATE_NO_WINDOW | subprocess.BELOW_NORMAL_PRIORITY_CLASS,
        )
        job = ProcessJob(process)
        client = LocalModel(f"http://127.0.0.1:{port}")
        try:
            deadline = time.monotonic() + 30
            while True:
                if process.poll() is not None:
                    raise RuntimeError(f"Owned model server exited; inspect {stderr_path}")
                try:
                    client.request("/api/version")
                    break
                except (urllib.error.URLError, TimeoutError):
                    if time.monotonic() >= deadline:
                        raise TimeoutError("Owned model server did not become ready")
                    time.sleep(0.1)
            yield client
        finally:
            if process.poll() is None:
                process.terminate()
                process.wait(timeout=10)
            job.close()


def prepare_model(paths: PlayerPaths) -> Path:
    executable = prepare_runtime(paths)
    model = runtime_manifest()["model"]
    weights = paths.models / "blobs" / ("sha256-" + model["weights_sha256"])
    if weights.is_file():
        return verify_installed_model(paths)
    verify_model_manifest()
    with model_server(paths):
        log_path = paths.runtime / "model-pull.log"
        with log_path.open("wb") as output:
            result = subprocess.run(
                [str(executable), "pull", model["name"]],
                env=server_environment(paths), stdout=output, stderr=subprocess.STDOUT,
                creationflags=subprocess.CREATE_NO_WINDOW,
                check=False,
            )
        if result.returncode:
            raise RuntimeError(f"Model download failed ({result.returncode}); inspect {log_path}")
    return verify_installed_model(paths)


def warm_model(client: LocalModel, maximum_vram_bytes: int = 4 * 1024**3) -> dict:
    output = subprocess.check_output(
        ["nvidia-smi", "--query-gpu=memory.free", "--format=csv,noheader,nounits"],
        text=True,
    )
    free = int(output.splitlines()[0].strip()) * 1024 * 1024
    if free < maximum_vram_bytes + 2 * 1024**3:
        raise RuntimeError("Insufficient GPU headroom for the configured conservative model budget")
    client.ready()
    started = time.monotonic()
    client.request("/api/generate", {
        "model": client.model["name"], "prompt": "Reply OK.", "stream": False,
        "think": False, "keep_alive": "5m",
        "options": {"num_ctx": client.model["context_tokens"],
                    "num_batch": client.model["batch_tokens"],
                    "num_predict": 1, "temperature": 0},
    }, timeout=120)
    models = client.request("/api/ps")["models"]
    resident = next((model for model in models if model["name"] == client.model["name"]), None)
    if resident is None:
        raise RuntimeError("The requested model did not become resident")
    memory = resident["size_vram"]
    if type(memory) is not int or memory <= 0 or memory > maximum_vram_bytes:
        client.unload()
        raise RuntimeError(f"Model GPU use is outside the configured budget: {memory}")
    for word in ("READY", "LOCAL"):
        control = client.request("/api/chat", {
            "model": client.model["name"], "stream": False, "think": False,
            "options": {"num_ctx": client.model["context_tokens"],
                        "num_batch": client.model["batch_tokens"],
                        "num_predict": 16, "temperature": 0},
            "messages": [{"role": "user", "content": f"Reply with only the word {word}."}],
        })
        if control.get("done_reason") != "stop" or control["message"]["content"].strip() != word:
            raise RuntimeError(f"Model failed the {word} instruction-following control: {control}")
    return {"model": client.model["name"], "vram_bytes": memory,
            "warm_seconds": time.monotonic() - started,
            "instruction_controls": 2}


def freeze_game(executable: Path, directory: Path) -> dict:
    executable = executable.resolve(strict=True)
    if executable.name not in ("VS.Desktop.exe", "VS.CombatLab.exe"):
        raise ValueError("Only an explicitly selected native game host can be controlled")
    symbols = executable.with_suffix(".pdb")
    if not symbols.is_file():
        raise FileNotFoundError("A matching PDB is required before a managed gameplay test")
    directory.mkdir()
    sources = [executable, symbols, *sorted(executable.parent.glob("*.dll"))]
    hashes = {}
    for source in sources:
        before = digest_file(source)
        destination = directory / source.name
        shutil.copy2(source, destination)
        if digest_file(destination) != before or digest_file(source) != before:
            raise RuntimeError(f"Build output changed while freezing it: {source}")
        hashes[source.name] = before
    (directory / "image.json").write_text(json.dumps({
        "source": str(executable), "sha256": hashes,
    }, indent=2) + "\n", encoding="utf-8")
    return hashes


def game_environment() -> dict[str, str]:
    environment = dict(os.environ)
    environment.update({
        "SDL_GAMECONTROLLER_IGNORE_DEVICES_EXCEPT": "0xF00D/0xBEEF",
        "SDL_WINDOW_ACTIVATE_WHEN_SHOWN": "0",
        "SDL_WINDOW_ACTIVATE_WHEN_RAISED": "0",
    })
    return environment
