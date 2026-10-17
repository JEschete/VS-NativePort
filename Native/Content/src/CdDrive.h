// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_CDDRIVE_H_TYPES
#define VS_CDDRIVE_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_CDDRIVE_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_CDDRIVE_H_DECLS)
#define VS_CDDRIVE_H_DECLS
#include "DiscQueue.h"

void Content_ReadSectorCallback(u_char intr, u_char result[] __attribute__((unused)),
    u_long* arg2 __attribute__((unused)));
void Content_ReadCommandCallback(u_char intr, u_char result[] __attribute__((unused)));
void Content_SeekCommandCallback(u_char intr, u_char result[] __attribute__((unused)));
void Content_XaReadyCallback(u_char intr, u_char result[]);
int Content_DiscState(void);
void Content_ClearDiscReadError(void);
int Content_BeginDiscRead(int sector, u_int bytes, void* vram);
void Content_UpdateDiscCommands(void);
int Content_ReadDiscFileBlocking(int sector, int bytes, void* vram);
void Content_ResetDiscDrive(void);
void Content_InitializeDiscQueue(void);
Content_DiscQueueSlot* Content_AllocateDiscRequest(Content_DiscFile const* file);
void Content_ReleaseDiscRequest(Content_DiscQueueSlot* slot);
void Content_EnqueueDiscRequest(Content_DiscQueueSlot* slot, void* vram);
void Content_EnqueuePriorityDiscRequest(Content_DiscQueueSlot* slot, void* vram);
void Content_UpdateDiscQueue(void);

#endif // VS_CDDRIVE_H_DECLS
