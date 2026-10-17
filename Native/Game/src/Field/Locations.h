// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_FIELD_LOCATIONS_H_TYPES
#define VS_FIELD_LOCATIONS_H_TYPES
#include "Legacy/Psx.h"


// decomp: src/include/lbaMacros.h:5
#define mkCdFile(file)                                                                   \
    {                                                                                    \
        VS_##file##_LBA, VS_##file##_SIZE                                                \
    }

#endif // VS_FIELD_LOCATIONS_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_FIELD_LOCATIONS_H_DECLS)
#define VS_FIELD_LOCATIONS_H_DECLS
#include "DiscQueue.h"

extern char Field_locationDestinations[1024];
extern char Field_zoneMusicIds[280];
extern Content_DiscFile Field_legacyZoneDiscFiles[];

#endif // VS_FIELD_LOCATIONS_H_DECLS
