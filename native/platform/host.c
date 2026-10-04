#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <mmsystem.h>
#include <psx_host.h>
#include <stdio.h>
#include <stdlib.h>

static INIT_ONCE clock_once = INIT_ONCE_STATIC_INIT;
static uint64_t clock_frequency;

static void release_timer(void)
{
    timeEndPeriod(1);
}

static BOOL CALLBACK initialize_clock(PINIT_ONCE once, PVOID argument, PVOID* context)
{
    LARGE_INTEGER frequency;
    (void)once;
    (void)argument;
    (void)context;
    if (!QueryPerformanceFrequency(&frequency) || frequency.QuadPart <= 0) {
        fprintf(stderr, "[host] QueryPerformanceFrequency failed\n");
        abort();
    }
    clock_frequency = (uint64_t)frequency.QuadPart;
    if (timeBeginPeriod(1) == TIMERR_NOERROR) atexit(release_timer);
    return TRUE;
}

uint64_t psx_host_now_ns(void)
{
    LARGE_INTEGER counter;
    uint64_t ticks;
    InitOnceExecuteOnce(&clock_once, initialize_clock, NULL, NULL);
    if (!QueryPerformanceCounter(&counter)) abort();
    ticks = (uint64_t)counter.QuadPart;
    return (ticks / clock_frequency) * 1000000000ull
        + (ticks % clock_frequency) * 1000000000ull / clock_frequency;
}

void psx_host_sleep_ns(uint64_t duration)
{
    if (duration >= 1000000ull) Sleep((DWORD)(duration / 1000000ull));
    else SwitchToThread();
}

void* psx_host_map_memory(void* address, size_t size)
{
    void* mapped = VirtualAlloc(address, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (mapped != address) {
        if (mapped) VirtualFree(mapped, 0, MEM_RELEASE);
        return NULL;
    }
    return mapped;
}

void* psx_host_load_library(const char* name)
{
    return (void*)LoadLibraryA(name);
}

void* psx_host_library_symbol(void* library, const char* name)
{
    return (void*)GetProcAddress((HMODULE)library, name);
}

void psx_host_free_library(void* library)
{
    if (library) FreeLibrary((HMODULE)library);
}
