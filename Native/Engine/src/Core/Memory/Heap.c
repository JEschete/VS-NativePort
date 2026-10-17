// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Core/Memory.h"
#include "Core/Memory/Heap.h"

// decomp: SLUS_010.40 vs_main_freeHeapR (src/SLUS_010.40/main.c:8884)
void Core_Memory_FreeFromHighEnd(void* block)
{
    Core_HeapBlockHeader* var_a1 = Core_legacyHeapList.prev;
    Core_HeapBlockHeader* target = (Core_HeapBlockHeader*)block - 1;

    for (; var_a1 >= target || target >= var_a1->next; var_a1 = var_a1->prev) {
        if (var_a1 >= var_a1->next && ((var_a1 < target) || (target < var_a1->next))) {
            break;
        }
    }

    if ((target + target->blockSize) == var_a1->next) {
        target->blockSize += var_a1->next->blockSize;
        target->next = var_a1->next->next;
        target->next->prev = target;
    } else {
        target->next = var_a1->next;
        target->next->prev = target;
    }

    if ((var_a1 + var_a1->blockSize) == target) {
        var_a1->blockSize += target->blockSize;
        var_a1->next = target->next;
        var_a1->next->prev = var_a1;
        return;
    }
    var_a1->next = target;
    target->prev = var_a1;
}

// decomp: SLUS_010.40 vs_main_freeHeap (src/SLUS_010.40/main.c:8914)
void Core_Memory_FreeFromLowEnd(void* block)
{
    Core_HeapBlockHeader* var_a1 = Core_legacyHeapList.next;
    Core_HeapBlockHeader* target = (Core_HeapBlockHeader*)block - 1;

    for (; var_a1 >= target || target >= var_a1->next; var_a1 = var_a1->next) {
        if (var_a1 >= var_a1->next && ((var_a1 < target) || (target < var_a1->next))) {
            break;
        }
    }

    if ((target + target->blockSize) == var_a1->next) {
        target->blockSize += var_a1->next->blockSize;
        target->next = var_a1->next->next;
        target->next->prev = target;
    } else {
        target->next = var_a1->next;
        target->next->prev = target;
    }

    if ((var_a1 + var_a1->blockSize) == target) {
        var_a1->blockSize += target->blockSize;
        var_a1->next = target->next;
        var_a1->next->prev = var_a1;
        return;
    }
    var_a1->next = target;
    target->prev = var_a1;
}

// decomp: SLUS_010.40 vs_main_allocHeapR (src/SLUS_010.40/main.c:8944)
void* Core_Memory_AllocateFromHighEnd(u_int size)
{
    Core_HeapBlockHeader* var_a1;
    Core_HeapBlockHeader* var_a2;
    u_int blockSz;

    blockSz = ((size + sizeof(Core_HeapBlockHeader) - 1) / sizeof(Core_HeapBlockHeader)) + 1;
    var_a2 = &Core_legacyHeapList;
    var_a1 = Core_legacyHeapList.prev;

    while (1) {
        if (var_a1->blockSize >= blockSz) {
            if (var_a1->blockSize == blockSz) {
                var_a2->prev = var_a1->prev;
                var_a1->prev->next = var_a1->next;
            } else {
                var_a1->blockSize = var_a1->blockSize - blockSz;
                var_a1 += var_a1->blockSize;
                var_a1->blockSize = blockSz;
            }
            return (u_long*)(var_a1 + 1);
        }
        var_a2 = var_a1;
        if (var_a1 == &Core_legacyHeapList) {
            return 0;
        }
        var_a1 = var_a1->prev;
    }
}

// decomp: SLUS_010.40 vs_main_allocHeap (src/SLUS_010.40/main.c:8974)
void* Core_Memory_AllocateFromLowEnd(u_int size)
{
    u_int blockSz;
    Core_HeapBlockHeader* var_a1;
    Core_HeapBlockHeader* var_v1;

    var_v1 = &Core_legacyHeapList;
    var_a1 = Core_legacyHeapList.next;
    blockSz = ((size + sizeof(Core_HeapBlockHeader) - 1) / sizeof(Core_HeapBlockHeader)) + 1;

    while (1) {
        if (var_a1->blockSize >= blockSz) {
            if (var_a1->blockSize == blockSz) {
                var_v1->next = var_a1->next;
                var_a1->next->prev = var_a1->prev;
            } else {
                Core_HeapBlockHeader* temp_v1 = var_a1 + blockSz;
                temp_v1->blockSize = var_a1->blockSize - blockSz;
                temp_v1->prev = var_a1->prev;
                temp_v1->next = var_a1->next;
                temp_v1->prev->next = temp_v1;
                temp_v1->next->prev = temp_v1;
                var_a1->blockSize = blockSz;
            }
            return (u_long*)(var_a1 + 1);
        }
        var_v1 = var_a1;
        if (var_a1 == &Core_legacyHeapList) {
            return 0;
        }
        var_a1 = var_a1->next;
    }
}

// decomp: SLUS_010.40 vs_main_initHeap (src/SLUS_010.40/main.c:9008)
void Core_Memory_InitializeHeap(Core_HeapBlockHeader* node, u_int value)
{
    Core_legacyHeapList.prev = node;
    Core_legacyHeapList.next = node;
    Core_legacyHeapList.blockSize = 0;
    node->prev = &Core_legacyHeapList;
    node->next = &Core_legacyHeapList;
    node->blockSize = value / sizeof(Core_HeapBlockHeader) - 1;
    Core_legacySecondaryHeapList.prev = &Core_legacySecondaryHeapList;
    Core_legacySecondaryHeapList.next = &Core_legacySecondaryHeapList;
    Core_legacySecondaryHeapList.blockSize = 0;
}

