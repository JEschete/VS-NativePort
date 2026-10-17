// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_DISCQUEUE_H_TYPES
#define VS_DISCQUEUE_H_TYPES
#include "Legacy/Psx.h"

typedef struct Content_DiscDrive Content_DiscDrive;
typedef struct Content_DiscFile Content_DiscFile;
typedef struct Content_DiscQueueSlot Content_DiscQueueSlot;
typedef struct Content_DiscRequestCounts Content_DiscRequestCounts;

// decomp: cdBufferModes_e (src/SLUS_010.40/main.c:89)
enum Content_DiscBufferMode {
    Content_DiscBufferMode_Direct = 0,
    Content_DiscBufferMode_Buffered = 1,
    Content_DiscBufferMode_Manual = 2,
};
typedef enum Content_DiscBufferMode Content_DiscBufferMode;

// decomp: diskState_e (src/SLUS_010.40/main.c:77)
enum Content_DiscDriveState {
    Content_DiscDriveState_Idle = 0,
    Content_DiscDriveState_SeekReady = 1,
    Content_DiscDriveState_ReadReady = 2,
    Content_DiscDriveState_ReadError = 4,
    Content_DiscDriveState_ReadInit = 5,
    Content_DiscDriveState_ReadXaInit = 6,
    Content_DiscDriveState_StreamXa = 7,
    Content_DiscDriveState_CommandFailed = 8,
    Content_DiscDriveState_StreamXaEnd = 9,
};
typedef enum Content_DiscDriveState Content_DiscDriveState;

// decomp: vs_main_CdQueueState (src/SLUS_010.40/main.h:26)
enum Content_DiscRequestState {
    Content_DiscRequestState_Free = 0,
    Content_DiscRequestState_Allocated = 1,
    Content_DiscRequestState_Reading = 2,
    Content_DiscRequestState_Enqueued = 3,
    Content_DiscRequestState_Loaded = 4,
    Content_DiscRequestState_Error = 6,
    Content_DiscRequestState_Dispatched = 7,
};
typedef enum Content_DiscRequestState Content_DiscRequestState;

// decomp: (unnamed at D:\Dev\NativePort\VS-NativePort\resources\rood-reverseWin\src\SLUS_010.40\main.c:7978:8) (src/SLUS_010.40/main.c:7978)
struct Content_DiscRequestCounts {
    short reading;
    short waiting;
};
typedef struct Content_DiscRequestCounts Content_DiscRequestCounts;
_Static_assert(sizeof(Content_DiscRequestCounts) == 0x4, "Content_DiscRequestCounts must keep its PS1 layout");

// decomp: vs_main_CdFile (src/SLUS_010.40/main.h:24)
typedef struct Content_DiscFile {
    int lba;
    int size;
} Content_DiscFile;
_Static_assert(sizeof(Content_DiscFile) == 0x8, "Content_DiscFile must keep its PS1 layout");

// decomp: vs_main_CdQueueSlot (src/SLUS_010.40/main.h:42)
typedef struct Content_DiscQueueSlot {
    short state;
    short priority;
    Content_DiscFile cdFile;
    int opaqueC;
    void* vram;
} Content_DiscQueueSlot;

// decomp: vs_main_disk_t (src/SLUS_010.40/main.c:117)
typedef struct Content_DiscDrive {
    char state;
    char xaModeMarker;
    char reserved2;
    char failureCount;
    u_int sectorsReceived;
    u_int sectorCount;
    int startSector;
    int byteCount;
    void* destination;
    DslFILTER xaFilter;
    DslLOC position;
    int commandId;
    int reserved24;
    int reserved28;
    enum Content_DiscBufferMode bufferMode;
    u_int destinationSector;
    int bufferedSectors;
    u_int ringSectorsWritten;
    u_int ringWriteIndex;
    u_int ringReadIndex;
    int framesSinceLastRead;
} Content_DiscDrive;

#endif // VS_DISCQUEUE_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_DISCQUEUE_H_DECLS)
#define VS_DISCQUEUE_H_DECLS
#include "Core/Memory/Heap.h"

extern Core_HeapBlockHeader* Content_legacyDiscReadBuffer;
extern DslLOC Content_legacyDiscReadLocation;
extern int Content_legacyXaReadyBcdField;
extern int Content_legacyXaReadyByteThree;
extern int Content_legacyXaReadyByteFour;
extern int Content_xaReadyMetadataWord;
extern Content_DiscQueueSlot Content_discRequestSlots[32];
extern struct Content_DiscRequestCounts Content_discRequestCounts;
extern Content_DiscDrive Content_discDriveState;
extern char Content_discControlResponseBytes[11];

#endif // VS_DISCQUEUE_H_DECLS
