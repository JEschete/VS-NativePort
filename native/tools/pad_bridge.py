"""DualSense -> native port pad bridge (runs on WINDOWS, not in WSL).

WSL2 does not pass USB or Bluetooth devices through to the VM: there is no
/dev/input at all, so a controller connected to Windows is invisible to the
Linux native binary. This reads the pad on the Windows side over raw HID and
streams its state to the port's TCP listener inside WSL. (WSL2 forwards Windows
localhost connections to servers running in the VM, so the game listens and
this connects.)

Raw HID via ctypes deliberately -- no pip installs, no third-party dependency.

Usage (from a Windows shell, with Windows Python):

    python native/tools/pad_bridge.py            # connect + stream
    python native/tools/pad_bridge.py --probe    # print state, don't connect
    python native/tools/pad_bridge.py --list     # list matching HID devices

Wire format, 12 bytes little-endian, one packet per input report:
    u32 magic 'VSPD' | u16 buttons (PS1 layout, active high)
    u8 lx, ly, rx, ry (128 = centred) | u16 flags (bit0 = pad live)
"""

import ctypes
import socket
import struct
import sys
import time
from ctypes import wintypes

# ---------------------------------------------------------------- PS1 pad bits
PAD_L2, PAD_R2, PAD_L1, PAD_R1 = 1 << 0, 1 << 1, 1 << 2, 1 << 3
PAD_TRIANGLE, PAD_CIRCLE, PAD_CROSS, PAD_SQUARE = 1 << 4, 1 << 5, 1 << 6, 1 << 7
PAD_SELECT, PAD_L3, PAD_R3, PAD_START = 1 << 8, 1 << 9, 1 << 10, 1 << 11
PAD_UP, PAD_RIGHT, PAD_DOWN, PAD_LEFT = 1 << 12, 1 << 13, 1 << 14, 1 << 15

HOST, PORT, MAGIC = "127.0.0.1", 27015, 0x44505356

SONY_VID = 0x054C
DUALSENSE_PIDS = (0x0CE6, 0x0DF2)      # DualSense, DualSense Edge

# ---------------------------------------------------------------- Win32 plumbing
setupapi = ctypes.WinDLL("setupapi")
hid = ctypes.WinDLL("hid")
kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)

GENERIC_READ = 0x80000000
FILE_SHARE_READ, FILE_SHARE_WRITE = 0x1, 0x2
OPEN_EXISTING = 3
INVALID_HANDLE_VALUE = ctypes.c_void_p(-1).value
DIGCF_PRESENT, DIGCF_DEVICEINTERFACE = 0x02, 0x10


class GUID(ctypes.Structure):
    _fields_ = [("Data1", wintypes.DWORD), ("Data2", wintypes.WORD),
                ("Data3", wintypes.WORD), ("Data4", ctypes.c_ubyte * 8)]


class SP_DEVICE_INTERFACE_DATA(ctypes.Structure):
    _fields_ = [("cbSize", wintypes.DWORD), ("InterfaceClassGuid", GUID),
                ("Flags", wintypes.DWORD), ("Reserved", ctypes.POINTER(ctypes.c_ulong))]


class HIDD_ATTRIBUTES(ctypes.Structure):
    _fields_ = [("Size", wintypes.ULONG), ("VendorID", wintypes.USHORT),
                ("ProductID", wintypes.USHORT), ("VersionNumber", wintypes.USHORT)]


class HIDP_CAPS(ctypes.Structure):
    _fields_ = [("Usage", wintypes.USHORT), ("UsagePage", wintypes.USHORT),
                ("InputReportByteLength", wintypes.USHORT),
                ("OutputReportByteLength", wintypes.USHORT),
                ("FeatureReportByteLength", wintypes.USHORT),
                ("Reserved", wintypes.USHORT * 17),
                ("NumberLinkCollectionNodes", wintypes.USHORT),
                ("NumberInputButtonCaps", wintypes.USHORT),
                ("NumberInputValueCaps", wintypes.USHORT),
                ("NumberInputDataIndices", wintypes.USHORT),
                ("NumberOutputButtonCaps", wintypes.USHORT),
                ("NumberOutputValueCaps", wintypes.USHORT),
                ("NumberOutputDataIndices", wintypes.USHORT),
                ("NumberFeatureButtonCaps", wintypes.USHORT),
                ("NumberFeatureValueCaps", wintypes.USHORT),
                ("NumberFeatureDataIndices", wintypes.USHORT)]


def hid_interface_guid():
    g = GUID()
    hid.HidD_GetHidGuid(ctypes.byref(g))
    return g


def enum_hid_paths():
    """Yield every present HID device interface path."""
    guid = hid_interface_guid()
    setupapi.SetupDiGetClassDevsW.restype = ctypes.c_void_p
    dev_info = setupapi.SetupDiGetClassDevsW(
        ctypes.byref(guid), None, None, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE)
    if dev_info == INVALID_HANDLE_VALUE:
        return

    iface = SP_DEVICE_INTERFACE_DATA()
    iface.cbSize = ctypes.sizeof(SP_DEVICE_INTERFACE_DATA)
    index = 0
    try:
        while setupapi.SetupDiEnumDeviceInterfaces(
                ctypes.c_void_p(dev_info), None, ctypes.byref(guid), index,
                ctypes.byref(iface)):
            index += 1
            need = wintypes.DWORD(0)
            setupapi.SetupDiGetDeviceInterfaceDetailW(
                ctypes.c_void_p(dev_info), ctypes.byref(iface), None, 0,
                ctypes.byref(need), None)
            if not need.value:
                continue
            buf = ctypes.create_string_buffer(need.value)
            # cbSize is the size of the FIXED part: DWORD + WCHAR[1], which the
            # SDK pads to 8 on 64-bit and 6 on 32-bit.
            cb = 8 if ctypes.sizeof(ctypes.c_void_p) == 8 else 6
            ctypes.memmove(buf, struct.pack("<I", cb), 4)
            if setupapi.SetupDiGetDeviceInterfaceDetailW(
                    ctypes.c_void_p(dev_info), ctypes.byref(iface), buf,
                    need.value, ctypes.byref(need), None):
                yield ctypes.wstring_at(ctypes.addressof(buf) + 4)
    finally:
        setupapi.SetupDiDestroyDeviceInfoList(ctypes.c_void_p(dev_info))


def open_path(path):
    kernel32.CreateFileW.restype = ctypes.c_void_p
    h = kernel32.CreateFileW(path, GENERIC_READ,
                             FILE_SHARE_READ | FILE_SHARE_WRITE, None,
                             OPEN_EXISTING, 0, None)
    if h == INVALID_HANDLE_VALUE:
        return None
    return h


def describe(handle):
    """(vid, pid, input_report_len) for an open HID handle, or None."""
    attrs = HIDD_ATTRIBUTES()
    attrs.Size = ctypes.sizeof(HIDD_ATTRIBUTES)
    if not hid.HidD_GetAttributes(ctypes.c_void_p(handle), ctypes.byref(attrs)):
        return None
    pp = ctypes.c_void_p()
    if not hid.HidD_GetPreparsedData(ctypes.c_void_p(handle), ctypes.byref(pp)):
        return None
    try:
        caps = HIDP_CAPS()
        if hid.HidP_GetCaps(pp, ctypes.byref(caps)) != 0x00110000:  # HIDP_STATUS_SUCCESS
            return None
        return attrs.VendorID, attrs.ProductID, caps.InputReportByteLength
    finally:
        hid.HidD_FreePreparsedData(pp)


def find_dualsense(verbose=False):
    """Open the DualSense's gamepad collection. Returns (handle, report_len)."""
    for path in enum_hid_paths():
        h = open_path(path)
        if h is None:
            continue
        info = describe(h)
        if info is None:
            kernel32.CloseHandle(ctypes.c_void_p(h))
            continue
        vid, pid, rlen = info
        if verbose and vid == SONY_VID:
            print("  VID_%04X PID_%04X  input report %3d  %s" % (vid, pid, rlen, path))
        # The gamepad collection is the one with a full-size input report;
        # the controller's other HID collections (audio control) are smaller.
        if vid == SONY_VID and pid in DUALSENSE_PIDS and rlen >= 64:
            return h, rlen
        kernel32.CloseHandle(ctypes.c_void_p(h))
    return None, 0


# ---------------------------------------------------------------- report parsing
HAT_TO_BITS = {
    0: PAD_UP,
    1: PAD_UP | PAD_RIGHT,
    2: PAD_RIGHT,
    3: PAD_DOWN | PAD_RIGHT,
    4: PAD_DOWN,
    5: PAD_DOWN | PAD_LEFT,
    6: PAD_LEFT,
    7: PAD_UP | PAD_LEFT,
}


def parse_report(data):
    """DualSense input report -> (buttons, lx, ly, rx, ry). None if unknown.

    USB report 0x01 (64B):  sticks at 1..4, buttons at 8,9,10
    BT  report 0x31 (78B):  same payload shifted one byte (seq/flags at [1])
    BT  report 0x01 (10B):  sticks at 1..4, buttons at 5,6,7
    """
    rid = data[0]
    if rid == 0x01 and len(data) >= 11:
        base, bbase = 1, 8
    elif rid == 0x31 and len(data) >= 12:
        base, bbase = 2, 9
    elif rid == 0x01 and len(data) >= 10:
        base, bbase = 1, 5
    else:
        return None

    lx, ly, rx, ry = data[base], data[base + 1], data[base + 2], data[base + 3]
    b0, b1, b2 = data[bbase], data[bbase + 1], data[bbase + 2]

    buttons = HAT_TO_BITS.get(b0 & 0x0F, 0)
    if b0 & 0x10: buttons |= PAD_SQUARE
    if b0 & 0x20: buttons |= PAD_CROSS
    if b0 & 0x40: buttons |= PAD_CIRCLE
    if b0 & 0x80: buttons |= PAD_TRIANGLE
    if b1 & 0x01: buttons |= PAD_L1
    if b1 & 0x02: buttons |= PAD_R1
    if b1 & 0x04: buttons |= PAD_L2
    if b1 & 0x08: buttons |= PAD_R2
    if b1 & 0x10: buttons |= PAD_SELECT      # Create
    if b1 & 0x20: buttons |= PAD_START       # Options
    if b1 & 0x40: buttons |= PAD_L3
    if b1 & 0x80: buttons |= PAD_R3
    _ = b2                                    # PS / touchpad / mute: unused
    return buttons, lx, ly, rx, ry


BUTTON_NAMES = [
    (PAD_UP, "Up"), (PAD_DOWN, "Down"), (PAD_LEFT, "Left"), (PAD_RIGHT, "Right"),
    (PAD_TRIANGLE, "Triangle"), (PAD_CIRCLE, "Circle"), (PAD_CROSS, "Cross"),
    (PAD_SQUARE, "Square"), (PAD_L1, "L1"), (PAD_R1, "R1"), (PAD_L2, "L2"),
    (PAD_R2, "R2"), (PAD_SELECT, "Create"), (PAD_START, "Options"),
    (PAD_L3, "L3"), (PAD_R3, "R3"),
]


def button_names(mask):
    return " ".join(n for b, n in BUTTON_NAMES if mask & b) or "-"


# ---------------------------------------------------------------- main loop
def read_report(handle, rlen):
    buf = ctypes.create_string_buffer(rlen)
    got = wintypes.DWORD(0)
    if not kernel32.ReadFile(ctypes.c_void_p(handle), buf, rlen,
                             ctypes.byref(got), None):
        return None
    return bytearray(buf.raw[:got.value])


def main():
    probe = "--probe" in sys.argv
    listing = "--list" in sys.argv
    port = PORT
    for a in sys.argv[1:]:
        if a.startswith("--port="):
            port = int(a.split("=", 1)[1])

    if listing:
        print("Sony HID collections present:")
        find_dualsense(verbose=True)
        return 0

    handle, rlen = find_dualsense()
    if handle is None:
        print("No DualSense found. Is it connected (USB or Bluetooth)?")
        print("Run with --list to see the Sony HID collections Windows exposes.")
        return 1
    print("DualSense opened (input report %d bytes)" % rlen)

    sock = None
    last = None
    tick = 0
    while True:
        data = read_report(handle, rlen)
        if data is None:
            print("read failed; controller disconnected?")
            return 1
        state = parse_report(data)
        if state is None:
            continue
        buttons, lx, ly, rx, ry = state

        if probe:
            tick += 1
            if state != last or tick % 60 == 0:
                print("\rbuttons %04X [%-28s] L(%3d,%3d) R(%3d,%3d)"
                      % (buttons, button_names(buttons), lx, ly, rx, ry), end="")
                last = state
            continue

        if sock is None:
            try:
                sock = socket.create_connection((HOST, port), timeout=2)
                sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
                print("connected to the native port on %s:%d" % (HOST, port))
            except OSError:
                time.sleep(0.5)
                continue
        try:
            sock.sendall(struct.pack("<IHBBBBH", MAGIC, buttons,
                                     lx, ly, rx, ry, 1))
        except OSError:
            print("link dropped; waiting for the game again")
            sock.close()
            sock = None


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        print("\nbridge stopped")
