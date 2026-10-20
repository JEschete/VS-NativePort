#include "Legacy/Input/LibPad.h"

#include "Core/Log.hpp"
#include "Input/LegacyPad.hpp"
#include "Legacy/Psx.h"

extern "C" {
#include "Input/Controller.h"
}

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
  unsigned char* actuators = nullptr;
  int actuator_count = 0;
  bool aligned = false;
};

Port g_ports[2];

Port* PortFor(int port) {
  // libpad port ids: 0x00 = port 1, 0x10 = port 2
  const int index = port >> 4;
  return index >= 0 && index < 2 ? &g_ports[index] : nullptr;
}

}  // namespace

namespace VS::Legacy {

ActuatorState ReadActuators(int port) {
  if (port < 0 || port > 1) {
    VS_LOG_WARN("input", "invalid actuator port index {}", port);
    return {};
  }
  const Port& p = g_ports[port];
  if (!p.connected || !p.analog || !p.aligned || !p.actuators ||
      p.actuator_count == 0 || Input_settings.vibrationOn == 0) {
    return {};
  }
  // _padResetDefaults (main.c:8468) registers bytes subsequently mutated by
  // vs_main_setVibrateParams (main.c:8698); copying them here would lose
  // pulses.
  const auto large = p.actuator_count > 1 ? p.actuators[1] : 0;
  return {static_cast<u16>(large * 257u),
          static_cast<u16>((p.actuators[0] & 1u) ? 65535u : 0u)};
}

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
  g_ports[0] = Port{};
  g_ports[1] = Port{};
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
  Port* p = PortFor(port);
  if (p == nullptr || table == nullptr) {
    VS_LOG_WARN("input", "invalid actuator alignment for port {}", port);
    return 0;
  }
  p->aligned = table[0] == 0 && table[1] == 1;
  if (!p->aligned) {
    VS_LOG_WARN("input", "unsupported actuator alignment for port {}", port);
    return 0;
  }
  return 1;
}

int PadSetMainMode(int port, int offs, int lock) {
  (void)port;
  (void)offs;
  (void)lock;
  return 1;
}

void PadSetAct(int port, unsigned char* table, int len) {
  Port* p = PortFor(port);
  if (p == nullptr) {
    VS_LOG_WARN("input", "invalid actuator registration port {}", port);
    return;
  }
  if (len < 0 || len > 2 || (len != 0 && table == nullptr)) {
    p->actuators = nullptr;
    p->actuator_count = 0;
    VS_LOG_WARN("input", "invalid actuator buffer length {} for port {}", len,
                port);
    return;
  }
  p->actuators = len == 0 ? nullptr : table;
  p->actuator_count = len;
}

}  // extern "C"
