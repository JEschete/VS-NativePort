#include "Legacy/Input/LibPad.h"

#include "Input/LegacyPad.hpp"

// libpad shim (plan §6.5). The game registered two receive buffers with
// PadInitDirect and decodes them itself; this writes them in the original
// format (psx-spx "Controllers"): [0] 0 = data valid / 0xFF = no pad, [1]
// type<<4 | halfwords, [2..3] active-low buttons, [4..7] right X, right Y, left
// X, left Y for an analog pad.

namespace {

struct Port {
  unsigned char* buffer = nullptr;
  bool connected = false;
  bool analog = false;
};

Port g_ports[2];

Port* PortFor(int port) {
  // libpad port ids: 0x00 = port 1, 0x10 = port 2
  const int index = port >> 4;
  return index >= 0 && index < 2 ? &g_ports[index] : nullptr;
}

}  // namespace

namespace VS::Legacy {

void FeedPad(int port, const Input::PadState& state, bool connected) {
  if (port < 0 || port > 1) {
    return;
  }
  Port& p = g_ports[port];
  p.connected = connected;
  p.analog = state.analog;
  if (p.buffer == nullptr) {
    return;
  }
  if (!connected) {
    p.buffer[0] = 0xFF;
    return;
  }
  const u32 held = state.held & 0xFFFF;
  p.buffer[0] = 0;
  p.buffer[1] = p.analog ? 0x73 : 0x41;
  p.buffer[2] = static_cast<unsigned char>(~(held >> 8));
  p.buffer[3] = static_cast<unsigned char>(~held);
  p.buffer[4] = state.sticks.rx;
  p.buffer[5] = state.sticks.ry;
  p.buffer[6] = state.sticks.lx;
  p.buffer[7] = state.sticks.ly;
}

}  // namespace VS::Legacy

extern "C" {

void PadInitDirect(unsigned char* port1, unsigned char* port2) {
  g_ports[0].buffer = port1;
  g_ports[1].buffer = port2;
  for (Port& p : g_ports) {
    if (p.buffer != nullptr) {
      p.buffer[0] = 0xFF;
    }
  }
}

void PadStartCom(void) {}

int PadGetState(int port) {
  const Port* p = PortFor(port);
  return p != nullptr && p->connected ? PadStateStable : 0;
}

int PadInfoMode(int port, int term, int offs) {
  (void)offs;
  const Port* p = PortFor(port);
  if (p == nullptr || !p->connected) {
    return 0;
  }
  switch (term) {
    case InfoModeCurID:
      return p->analog ? 7 : 4;  // 7 = analog (DualShock), 4 = digital
    case InfoModeCurExID:
      return 7;  // a DualShock: can switch modes and has actuators
    case InfoModeCurExOffs:
      return p->analog ? 1 : 0;  // mode table index currently selected
    default:
      return 0;
  }
}

int PadSetActAlign(int port, unsigned char* table) {
  (void)port;
  (void)table;
  return 1;
}

int PadSetMainMode(int port, int offs, int lock) {
  (void)port;
  (void)offs;
  (void)lock;
  return 1;
}

void PadSetAct(int port, unsigned char* table, int len) {
  // actuator levels (vibration) go to Engine\Input in the vibration milestone
  (void)port;
  (void)table;
  (void)len;
}

}  // extern "C"
