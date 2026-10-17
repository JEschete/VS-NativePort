#pragma once

// C access to the extracted game data (Content/DataStore.hpp) for copied and owned C code.

#ifdef __cplusplus
extern "C" {
#endif

// Reads `bytes` of the disc file that starts at, or contains, sector `lba`, from that sector on. Returns 1 on
// success, 0 if no extracted file covers the range (logged).
int Content_ReadDiscBytes(unsigned lba, unsigned bytes, void* dest);

// Fills every global listed in Content_moduleData (Content/ModuleData.h) from the extracted module files. Returns the
// number of entries that could not be filled (logged).
int Content_LoadModuleData(void);

#ifdef __cplusplus
}
#endif
