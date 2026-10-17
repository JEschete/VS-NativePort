// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_LEGACYFILES_H_TYPES
#define VS_LEGACYFILES_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_LEGACYFILES_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_LEGACYFILES_H_DECLS)
#define VS_LEGACYFILES_H_DECLS

extern u_short Content_legacyShpDiscOffsets[256];
extern u_char Content_legacyShpSectorCounts[848];
extern u_short Content_legacySequenceDiscOffsets[168];
extern u_char Content_legacySequenceSectorCounts[112];
extern u_short Content_legacyEtmDiscOffsets[112];
extern u_char Content_legacyEtmSectorCounts[368];
extern u_short Content_legacyWepDiscOffsets[128];
extern u_char Content_legacyWepSectorCounts[296];
extern u_short Content_legacyMaterialSequenceDiscOffsets[77];
extern u_char Content_legacyMaterialSequenceSectorCounts[133];
extern u_short Content_legacyMaterialModelSequenceDiscOffsets[16][4];
extern u_char Content_legacyMaterialModelSequenceSectorCounts[11][4];

#endif // VS_LEGACYFILES_H_DECLS
