// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Core/Memory.h"
#include "Core/Memory/Heap.h"

// decomp: SLUS_010.40 vs_main_freeHeapR (src/SLUS_010.40/main.c:8884)
void Core_Memory_FreeFromHighEnd(void* block)
{
    Core_HeapBlockHeader* previous_free = Core_legacyHeapList.prev;
    Core_HeapBlockHeader* target = (Core_HeapBlockHeader*)block - 1;

    for (; previous_free >= target || target >= previous_free->next; previous_free = previous_free->prev) {
        if (previous_free >= previous_free->next && ((previous_free < target) || (target < previous_free->next))) {
            break;
        }
    }

    if ((target + target->blockSize) == previous_free->next) {
        target->blockSize += previous_free->next->blockSize;
        target->next = previous_free->next->next;
        target->next->prev = target;
    } else {
        target->next = previous_free->next;
        target->next->prev = target;
    }

    if ((previous_free + previous_free->blockSize) == target) {
        previous_free->blockSize += target->blockSize;
        previous_free->next = target->next;
        previous_free->next->prev = previous_free;
        return;
    }
    previous_free->next = target;
    target->prev = previous_free;
}

// decomp: SLUS_010.40 vs_main_freeHeap (src/SLUS_010.40/main.c:8914)
void Core_Memory_FreeFromLowEnd(void* block)
{
    Core_HeapBlockHeader* previous_free = Core_legacyHeapList.next;
    Core_HeapBlockHeader* target = (Core_HeapBlockHeader*)block - 1;

    for (; previous_free >= target || target >= previous_free->next; previous_free = previous_free->next) {
        if (previous_free >= previous_free->next && ((previous_free < target) || (target < previous_free->next))) {
            break;
        }
    }

    if ((target + target->blockSize) == previous_free->next) {
        target->blockSize += previous_free->next->blockSize;
        target->next = previous_free->next->next;
        target->next->prev = target;
    } else {
        target->next = previous_free->next;
        target->next->prev = target;
    }

    if ((previous_free + previous_free->blockSize) == target) {
        previous_free->blockSize += target->blockSize;
        previous_free->next = target->next;
        previous_free->next->prev = previous_free;
        return;
    }
    previous_free->next = target;
    target->prev = previous_free;
}

// decomp: SLUS_010.40 vs_main_allocHeapR (src/SLUS_010.40/main.c:8944)
void* Core_Memory_AllocateFromHighEnd(u_int size)
{
    Core_HeapBlockHeader* allocation_block;
    Core_HeapBlockHeader* following_free;
    u_int allocation_units;

    allocation_units = ((size + sizeof(Core_HeapBlockHeader) - 1) / sizeof(Core_HeapBlockHeader)) + 1;
    following_free = &Core_legacyHeapList;
    allocation_block = Core_legacyHeapList.prev;

    while (1) {
        if (allocation_block->blockSize >= allocation_units) {
            if (allocation_block->blockSize == allocation_units) {
                following_free->prev = allocation_block->prev;
                allocation_block->prev->next = allocation_block->next;
            } else {
                allocation_block->blockSize = allocation_block->blockSize - allocation_units;
                allocation_block += allocation_block->blockSize;
                allocation_block->blockSize = allocation_units;
            }
            return (u_long*)(allocation_block + 1);
        }
        following_free = allocation_block;
        if (allocation_block == &Core_legacyHeapList) {
            return 0;
        }
        allocation_block = allocation_block->prev;
    }
}

// decomp: SLUS_010.40 vs_main_allocHeap (src/SLUS_010.40/main.c:8974)
void* Core_Memory_AllocateFromLowEnd(u_int size)
{
    u_int allocation_units;
    Core_HeapBlockHeader* allocation_block;
    Core_HeapBlockHeader* previous_free;

    previous_free = &Core_legacyHeapList;
    allocation_block = Core_legacyHeapList.next;
    allocation_units = ((size + sizeof(Core_HeapBlockHeader) - 1) / sizeof(Core_HeapBlockHeader)) + 1;

    while (1) {
        if (allocation_block->blockSize >= allocation_units) {
            if (allocation_block->blockSize == allocation_units) {
                previous_free->next = allocation_block->next;
                allocation_block->next->prev = allocation_block->prev;
            } else {
                Core_HeapBlockHeader* remainder = allocation_block + allocation_units;
                remainder->blockSize = allocation_block->blockSize - allocation_units;
                remainder->prev = allocation_block->prev;
                remainder->next = allocation_block->next;
                remainder->prev->next = remainder;
                remainder->next->prev = remainder;
                allocation_block->blockSize = allocation_units;
            }
            return (u_long*)(allocation_block + 1);
        }
        previous_free = allocation_block;
        if (allocation_block == &Core_legacyHeapList) {
            return 0;
        }
        allocation_block = allocation_block->next;
    }
}

// decomp: SLUS_010.40 vs_main_initHeap (src/SLUS_010.40/main.c:9008)
void Core_Memory_InitializeHeap(Core_HeapBlockHeader* node, u_int byte_count)
{
    Core_legacyHeapList.prev = node;
    Core_legacyHeapList.next = node;
    Core_legacyHeapList.blockSize = 0;
    node->prev = &Core_legacyHeapList;
    node->next = &Core_legacyHeapList;
    node->blockSize = byte_count / sizeof(Core_HeapBlockHeader) - 1;
    Core_legacySecondaryHeapList.prev = &Core_legacySecondaryHeapList;
    Core_legacySecondaryHeapList.next = &Core_legacySecondaryHeapList;
    Core_legacySecondaryHeapList.blockSize = 0;
}

}  // extern "C"
