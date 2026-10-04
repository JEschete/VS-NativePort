#pragma once

#include <stddef.h>
#include <stdint.h>

uint64_t psx_host_now_ns(void);
void psx_host_sleep_ns(uint64_t duration);
void* psx_host_map_memory(void* address, size_t size);
void* psx_host_load_library(const char* name);
void* psx_host_library_symbol(void* library, const char* name);
void psx_host_free_library(void* library);
