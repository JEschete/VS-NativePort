#pragma once

// Memory card surface (libapi file calls + libcard, plan §6.7, strategy T),
// translated into Engine\Save. The open-mode bits the card code passes to
// LibApi::open are VS::Save::MemoryCard::kOpenRead and its siblings.

#include <cstdint>

namespace VS::Legacy {

// libapi's directory entry, filled by LibApi::firstfile and LibApi::nextfile.
struct DIRENTRY {
  char name[20];
  std::int32_t attr;
  std::int32_t size;
  DIRENTRY* next;
  std::int32_t head;
  char system[4];
};

// The libapi file calls keep their SDK names, which are also the C runtime's
// (open, read, rename...); the nested namespace keeps them apart, so callers
// always write LibApi::open and never bring them in with a using-declaration.
namespace LibApi {
std::int32_t open(char* path, std::uint32_t mode);
std::int32_t close(std::int32_t fd);
std::int32_t read(std::int32_t fd, void* buf, std::int32_t n);
std::int32_t write(std::int32_t fd, void* buf, std::int32_t n);
std::int32_t erase(char* path);
std::int32_t rename(char* from, char* to);
std::int32_t cd(char* path);
DIRENTRY* firstfile(char* pattern, DIRENTRY* dir);
DIRENTRY* nextfile(DIRENTRY* dir);
}  // namespace LibApi

void InitCARD(std::int32_t val);
std::int32_t StartCARD();
std::int32_t _card_info(std::int32_t chan);
std::int32_t _card_clear(std::int32_t chan);
std::int32_t _card_load(std::int32_t chan);
std::int32_t _card_format(std::int32_t chan);

}  // namespace VS::Legacy
