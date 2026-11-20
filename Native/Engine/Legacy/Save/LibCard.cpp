#include "Legacy/Save/LibCard.hpp"

#include <cstdint>

#include "Core/Unported.hpp"

// Transitional shims (plan §2.2). Each calls RecordUnported until it
// translates into the modern system.

namespace VS::Legacy {

std::int32_t LibApi::open(char* path, std::uint32_t mode) {
  (void)path;
  (void)mode;
  VS::Core::RecordUnported("libapi:open");
  return {};
}

std::int32_t LibApi::close(std::int32_t fd) {
  (void)fd;
  VS::Core::RecordUnported("libapi:close");
  return {};
}

std::int32_t LibApi::read(std::int32_t fd, void* buf, std::int32_t n) {
  (void)fd;
  (void)buf;
  (void)n;
  VS::Core::RecordUnported("libapi:read");
  return {};
}

std::int32_t LibApi::write(std::int32_t fd, void* buf, std::int32_t n) {
  (void)fd;
  (void)buf;
  (void)n;
  VS::Core::RecordUnported("libapi:write");
  return {};
}

std::int32_t LibApi::erase(char* path) {
  (void)path;
  VS::Core::RecordUnported("libapi:erase");
  return {};
}

std::int32_t LibApi::rename(char* from, char* to) {
  (void)from;
  (void)to;
  VS::Core::RecordUnported("libapi:rename");
  return {};
}

std::int32_t LibApi::cd(char* path) {
  (void)path;
  VS::Core::RecordUnported("libapi:cd");
  return {};
}

DIRENTRY* LibApi::firstfile(char* pattern, DIRENTRY* dir) {
  (void)pattern;
  (void)dir;
  VS::Core::RecordUnported("libapi:firstfile");
  return {};
}

DIRENTRY* LibApi::nextfile(DIRENTRY* dir) {
  (void)dir;
  VS::Core::RecordUnported("libapi:nextfile");
  return {};
}

void InitCARD(std::int32_t val) {
  (void)val;
  VS::Core::RecordUnported("libcard:InitCARD");
}

std::int32_t StartCARD() {
  VS::Core::RecordUnported("libcard:StartCARD");
  return {};
}

std::int32_t _card_info(std::int32_t chan) {
  (void)chan;
  VS::Core::RecordUnported("libcard:_card_info");
  return {};
}

std::int32_t _card_clear(std::int32_t chan) {
  (void)chan;
  VS::Core::RecordUnported("libcard:_card_clear");
  return {};
}

std::int32_t _card_load(std::int32_t chan) {
  (void)chan;
  VS::Core::RecordUnported("libcard:_card_load");
  return {};
}

std::int32_t _card_format(std::int32_t chan) {
  (void)chan;
  VS::Core::RecordUnported("libcard:_card_format");
  return {};
}

}  // namespace VS::Legacy
