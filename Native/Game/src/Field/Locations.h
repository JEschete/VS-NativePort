// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#pragma once
#include "Legacy/Psx.h"


// decomp: src/include/lbaMacros.h:5
#define mkCdFile(file)                                                                   \
    {                                                                                    \
        VS_##file##_LBA, VS_##file##_SIZE                                                \
    }

#include "DiscQueue.h"

extern char Field_locationDestinations[1024];
extern char Field_zoneMusicIds[280];
extern Content_DiscFile Field_legacyZoneDiscFiles[];
