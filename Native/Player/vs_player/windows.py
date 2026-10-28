import ctypes
import os
from ctypes import wintypes
from pathlib import Path

BUTTON_KEYS = {
    "Up": (4096, 0x26), "Down": (16384, 0x28),
    "Left": (32768, 0x25), "Right": (8192, 0x27),
    "Cross": (64, 0x58), "Circle": (32, 0x43),
    "Triangle": (16, 0x53), "Square": (128, 0x5A),
    "L1": (4, 0x51), "R1": (8, 0x45),
}


class WindowInput:
    """Keyboard messages to one verified process; never changes foreground focus."""

    def __init__(self, process_id: int, executable: Path) -> None:
        if os.name != "nt":
            raise OSError("Native player input currently requires Windows")
        self.process_id = process_id
        self.executable = executable.resolve()
        self.user = ctypes.WinDLL("user32", use_last_error=True)
        self.kernel = ctypes.WinDLL("kernel32", use_last_error=True)
        self.user.PostMessageW.argtypes = (wintypes.HWND, wintypes.UINT,
                                          ctypes.c_size_t, ctypes.c_ssize_t)
        self.user.PostMessageW.restype = wintypes.BOOL
        self.user.MapVirtualKeyW.argtypes = (wintypes.UINT, wintypes.UINT)
        self.user.MapVirtualKeyW.restype = wintypes.UINT
        self.user.GetWindowThreadProcessId.argtypes = (
            wintypes.HWND, ctypes.POINTER(wintypes.DWORD),
        )
        self.user.GetWindowThreadProcessId.restype = wintypes.DWORD
        self.user.IsWindowVisible.argtypes = (wintypes.HWND,)
        self.user.IsWindowVisible.restype = wintypes.BOOL
        self.kernel.OpenProcess.argtypes = (wintypes.DWORD, wintypes.BOOL, wintypes.DWORD)
        self.kernel.OpenProcess.restype = wintypes.HANDLE
        self.kernel.QueryFullProcessImageNameW.argtypes = (
            wintypes.HANDLE, wintypes.DWORD, wintypes.LPWSTR, ctypes.POINTER(wintypes.DWORD),
        )
        self.kernel.QueryFullProcessImageNameW.restype = wintypes.BOOL
        self.kernel.CloseHandle.argtypes = (wintypes.HANDLE,)
        self.kernel.CloseHandle.restype = wintypes.BOOL
        self.window: int | None = None
        self.held: frozenset[str] = frozenset()
        self.verify_process()

    def verify_process(self) -> None:
        handle = self.kernel.OpenProcess(0x1000, False, self.process_id)
        if not handle:
            raise ctypes.WinError(ctypes.get_last_error())
        try:
            size = wintypes.DWORD(32768)
            path = ctypes.create_unicode_buffer(size.value)
            if not self.kernel.QueryFullProcessImageNameW(handle, 0, path, ctypes.byref(size)):
                raise ctypes.WinError(ctypes.get_last_error())
            if Path(path.value).resolve() != self.executable:
                raise ValueError("Process identity no longer matches the managed executable")
        finally:
            self.kernel.CloseHandle(handle)

    def find_window(self) -> bool:
        callback_type = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, ctypes.c_ssize_t)
        windows: list[int] = []

        def visit(window, unused):
            owner = wintypes.DWORD()
            self.user.GetWindowThreadProcessId(window, ctypes.byref(owner))
            if owner.value == self.process_id and self.user.IsWindowVisible(window):
                windows.append(window)
            return True

        callback = callback_type(visit)
        self.user.EnumWindows.argtypes = (callback_type, ctypes.c_ssize_t)
        self.user.EnumWindows.restype = wintypes.BOOL
        if not self.user.EnumWindows(callback, 0):
            raise ctypes.WinError(ctypes.get_last_error())
        if len(windows) > 1:
            raise ValueError("Managed process has ambiguous visible windows")
        self.window = windows[0] if windows else None
        return self.window is not None

    def _post(self, button: str, released: bool) -> None:
        if self.window is None:
            raise RuntimeError("Managed game window is not ready")
        owner = wintypes.DWORD()
        self.user.GetWindowThreadProcessId(self.window, ctypes.byref(owner))
        if owner.value != self.process_id:
            raise RuntimeError("Window no longer belongs to the managed game process")
        key = BUTTON_KEYS[button][1]
        detail = 1 | (self.user.MapVirtualKeyW(key, 0) << 16)
        if 0x25 <= key <= 0x28:
            detail |= 0x01000000
        if released:
            detail |= 0xC0000000
        if not self.user.PostMessageW(self.window, 0x101 if released else 0x100, key, detail):
            raise ctypes.WinError(ctypes.get_last_error())

    def set_buttons(self, buttons: frozenset[str]) -> None:
        if not buttons <= BUTTON_KEYS.keys():
            raise ValueError("Input contains a button outside the controller allowlist")
        for button in sorted(self.held - buttons):
            self._post(button, True)
            self.held = self.held - {button}
        for button in sorted(buttons - self.held):
            self._post(button, False)
            self.held = self.held | {button}

    def close_window(self) -> None:
        self.verify_process()
        if self.window is not None:
            owner = wintypes.DWORD()
            self.user.GetWindowThreadProcessId(self.window, ctypes.byref(owner))
            if owner.value != self.process_id:
                raise RuntimeError("Refusing to close a window with changed ownership")
            if not self.user.PostMessageW(self.window, 0x10, 0, 0):
                raise ctypes.WinError(ctypes.get_last_error())
