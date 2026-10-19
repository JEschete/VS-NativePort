#pragma once

// SDK calls with no native meaning (strategy D: GPU/CD/SPU synchronisation, IRQ
// and DMA setup, kernel events, critical sections, the debug font). The copy
// tool deletes calls that are whole statements; the ones used as a value stay
// here as no-ops returning 0 until their callers are modernized. Each is
// counted by tools\ledger\scan_native.py, so this file shrinks to nothing.

#include "Legacy/Core/PsxTypes.h"

typedef void (*LegacyRemovedCallback)(void);

static inline int DrawSync(int mode) {
  (void)mode;
  return 0;
}
static inline u_long DrawSyncCallback(void (*func)(void)) {
  (void)func;
  return 0;
}
static inline int ResetGraph(int mode) {
  (void)mode;
  return 0;
}
static inline int SetGraphDebug(int level) {
  (void)level;
  return 0;
}
static inline void FntLoad(int tx, int ty) {
  (void)tx;
  (void)ty;
}
static inline int FntOpen(int x, int y, int w, int h, int isbg, int n) {
  (void)x;
  (void)y;
  (void)w;
  (void)h;
  (void)isbg;
  (void)n;
  return 0;
}
static inline void SetDumpFnt(int id) { (void)id; }
static inline u_long* FntFlush(int id) {
  (void)id;
  return NULL;
}
static inline void InitGeom(void) {}
static inline void __main(void) {
}  // GCC runtime start-up (static constructors): nothing to run natively
static inline int ResetCallback(void) { return 0; }
static inline int EnterCriticalSection(void) { return 0; }
static inline void ExitCriticalSection(void) {}
static inline long TestEvent(long ev) {
  (void)ev;
  return 0;
}
static inline void UnDeliverEvent(unsigned long desc, unsigned long spec) {
  (void)desc;
  (void)spec;
}
static inline void SystemError(char c, long n) {
  (void)c;
  (void)n;
}
static inline void _bu_init(void) {}
static inline long SpuInitMalloc(long num, char* top) {
  (void)num;
  (void)top;
  return 0;
}
static inline void SpuQuit(void) {}
static inline long SpuSetIRQ(long onOff) {
  (void)onOff;
  return 0;
}
static inline unsigned long SpuSetIRQAddr(unsigned long addr) {
  (void)addr;
  return 0;
}
static inline LegacyRemovedCallback SpuSetIRQCallback(LegacyRemovedCallback f) {
  (void)f;
  return NULL;
}
static inline long SpuSetTransferMode(long mode) {
  (void)mode;
  return 0;
}
static inline void DecDCTReset(int mode) { (void)mode; }
static inline void StCdInterrupt(void) {}
static inline int CdMix(void* vol) {
  (void)vol;
  return 0;
}
static inline int DsMix(void* vol) {
  (void)vol;
  return 0;
}
static inline void DsFlush(void) {}
static inline int DsSystemStatus(void) { return 0; }
static inline void* DsReadyCallback(void* func) {
  (void)func;
  return NULL;
}
static inline int DsStartReadySystem(void* func, int count) {
  (void)func;
  (void)count;
  return 0;
}
static inline void DsEndReadySystem(void) {}
