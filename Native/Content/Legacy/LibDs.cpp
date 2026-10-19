#include "Legacy/LibDs.h"

#include "Core/Unported.h"

// Transitional shims (plan §2.2). Each records VS_UNPORTED until it translates
// into the modern system.

extern "C" {
void StSetRing(u_long* ring, u_long size) {
  (void)ring;
  (void)size;
  VS_UNPORTED("libds:StSetRing");
}

void StUnSetRing(void) { VS_UNPORTED("libds:StUnSetRing"); }

void StSetStream(u_long mode, u_long startFrame, u_long endFrame,
                 void (*func1)(void), void (*func2)(void)) {
  (void)mode;
  (void)startFrame;
  (void)endFrame;
  (void)func1;
  (void)func2;
  VS_UNPORTED("libds:StSetStream");
}

u_long StFreeRing(u_long* base) {
  (void)base;
  VS_UNPORTED("libds:StFreeRing");
  return {};
}

u_long StGetNext(u_long** addr, u_long** header) {
  (void)addr;
  (void)header;
  VS_UNPORTED("libds:StGetNext");
  return {};
}

int StGetBackloc(CdlLOC* loc) {
  (void)loc;
  VS_UNPORTED("libds:StGetBackloc");
  return {};
}

int DsInit(void) {
  VS_UNPORTED("libds:DsInit");
  return {};
}

int DsCommand(u_char com, u_char* param, DslCB cbsync, int count) {
  (void)com;
  (void)param;
  (void)cbsync;
  (void)count;
  VS_UNPORTED("libds:DsCommand");
  return {};
}

int DsPacket(u_char mode, DslLOC* pos, u_char com, DslCB func, int count) {
  (void)mode;
  (void)pos;
  (void)com;
  (void)func;
  (void)count;
  VS_UNPORTED("libds:DsPacket");
  return {};
}

int DsGetSector(void* madr, int size) {
  (void)madr;
  (void)size;
  VS_UNPORTED("libds:DsGetSector");
  return {};
}

DslLOC* DsIntToPos(int i, DslLOC* p) {
  // sector number to minute/second/sector (BCD), 150-sector lead-in
  const int s = i + 150;
  const auto bcd = [](int v) {
    return static_cast<u_char>(((v / 10) << 4) | (v % 10));
  };
  p->minute = bcd(s / (75 * 60));
  p->second = bcd((s / 75) % 60);
  p->sector = bcd(s % 75);
  p->track = 0;
  return p;
}

int DsControl(u_char com, u_char* param, u_char* result) {
  (void)com;
  (void)param;
  (void)result;
  VS_UNPORTED("libds:DsControl");
  return {};
}

int DsControlB(u_char com, u_char* param, u_char* result) {
  (void)com;
  (void)param;
  (void)result;
  VS_UNPORTED("libds:DsControlB");
  return {};
}

int DsRead2(DslLOC* pos, int mode) {
  (void)pos;
  (void)mode;
  VS_UNPORTED("libds:DsRead2");
  return {};
}

}  // extern "C"
