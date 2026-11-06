// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_TITLE_SAVECARD_H_TYPES
#define VS_TITLE_SAVECARD_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_TITLE_SAVECARD_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_TITLE_SAVECARD_H_DECLS)
#define VS_TITLE_SAVECARD_H_DECLS

extern struct DIRENTRY* Title_cardDirectoryEntryBuffer;;
extern char Title_saveTransferErrorOffset;;
extern long Title_cardEventDescriptors[8];;
extern struct DIRENTRY* Title_cardDirectoryEntries[15];;

#endif // VS_TITLE_SAVECARD_H_DECLS
