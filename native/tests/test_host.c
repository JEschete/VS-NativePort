#include <psx_host.h>
#include <stdint.h>
#include <stdio.h>

int main(void)
{
    uint64_t start = psx_host_now_ns();
    uint64_t finish;
    void* library;
    volatile unsigned char* mapped;

    psx_host_sleep_ns(2000000ull);
    finish = psx_host_now_ns();
    if (finish <= start) return 1;

    library = psx_host_load_library("kernel32.dll");
    if (!library || !psx_host_library_symbol(library, "GetCurrentProcessId")) return 2;
    psx_host_free_library(library);

    mapped = (volatile unsigned char*)psx_host_map_memory((void*)0x1F800000UL, 0x2000);
    if (!mapped) return 3;
    mapped[0] = 0xA5;
    mapped[0x1FFF] = 0x5A;
    if (mapped[0] != 0xA5 || mapped[0x1FFF] != 0x5A) return 4;

    printf("Windows host: clock, DLL loading and fixed-address memory passed\n");
    return 0;
}
