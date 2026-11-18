#include "Legacy/Save/LibCard.h"

#include "Core/Unported.hpp"

// Transitional shims (plan §2.2). Each calls RecordUnported until it
// translates into the modern system.

long LegacyApi_open(char* path, unsigned long mode) {
  (void)path;
  (void)mode;
  VS::Core::RecordUnported("libcard:LegacyApi_open");
  return {};
}

long LegacyApi_close(long fd) {
  (void)fd;
  VS::Core::RecordUnported("libcard:LegacyApi_close");
  return {};
}

long LegacyApi_read(long fd, void* buf, long n) {
  (void)fd;
  (void)buf;
  (void)n;
  VS::Core::RecordUnported("libcard:LegacyApi_read");
  return {};
}

long LegacyApi_write(long fd, void* buf, long n) {
  (void)fd;
  (void)buf;
  (void)n;
  VS::Core::RecordUnported("libcard:LegacyApi_write");
  return {};
}

long LegacyApi_erase(char* path) {
  (void)path;
  VS::Core::RecordUnported("libcard:LegacyApi_erase");
  return {};
}

long LegacyApi_rename(char* from, char* to) {
  (void)from;
  (void)to;
  VS::Core::RecordUnported("libcard:LegacyApi_rename");
  return {};
}

long LegacyApi_cd(char* path) {
  (void)path;
  VS::Core::RecordUnported("libcard:LegacyApi_cd");
  return {};
}

struct DIRENTRY* LegacyApi_firstfile(char* pattern, struct DIRENTRY* dir) {
  (void)pattern;
  (void)dir;
  VS::Core::RecordUnported("libcard:LegacyApi_firstfile");
  return {};
}

struct DIRENTRY* LegacyApi_nextfile(struct DIRENTRY* dir) {
  (void)dir;
  VS::Core::RecordUnported("libcard:LegacyApi_nextfile");
  return {};
}

void InitCARD(long val) {
  (void)val;
  VS::Core::RecordUnported("libcard:InitCARD");
}

long StartCARD(void) {
  VS::Core::RecordUnported("libcard:StartCARD");
  return {};
}

long _card_info(long chan) {
  (void)chan;
  VS::Core::RecordUnported("libcard:_card_info");
  return {};
}

long _card_clear(long chan) {
  (void)chan;
  VS::Core::RecordUnported("libcard:_card_clear");
  return {};
}

long _card_load(long chan) {
  (void)chan;
  VS::Core::RecordUnported("libcard:_card_load");
  return {};
}

long _card_format(long chan) {
  (void)chan;
  VS::Core::RecordUnported("libcard:_card_format");
  return {};
}
