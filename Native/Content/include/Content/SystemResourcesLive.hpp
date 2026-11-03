#pragma once
// Hand-maintained (tools/port/owned.csv).

// Live binding for the SYSTEM.DAT loader (SystemResources.hpp): the high-end
// game heap, the blocking disc read, VRAM uploads through LoadImage, the
// palette through LoadLiveClutColors and the battle menu's interface.
namespace VS::Content {

// _loadSystemDat (INITBTL.PRG 18.c:258). A heap with no room for the file,
// or a section that lies outside it, is reported on the "content" channel
// and skipped.
void LoadLiveSystemResources();

}  // namespace VS::Content
