#pragma once

namespace VS::Content {
// C access to the extracted game data (Content/DataStore.hpp) for copied and
// owned C code.

// Reads `bytes` of the disc file that starts at, or contains, sector `lba`,
// from that sector on. Returns 1 on success, 0 if no extracted file covers the
// range (logged).
int ReadDiscBytes(unsigned lba, unsigned bytes, void* dest);
// Fills every global listed in VS::Content::ModuleDataEntries()
// (ModuleData.hpp) from the extracted module files. Returns the number of
// entries that could not be filled (logged).
int LoadModuleData();
}  // namespace VS::Content
