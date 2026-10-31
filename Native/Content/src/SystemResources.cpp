// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "CdDrive.h"
#include "Content/DiscLayout.h"
#include "Core/Memory/Heap.h"
#include "Menu/Framework/Framework.h"
#include "Render/Palette.h"
#include "SystemResources.h"

// decomp: INITBTL.PRG _loadSystemDat (src/BATTLE/INITBTL.PRG/18.c:258)
void Content_LoadSystemResources(void)
{
    RECT rect;
    RECT* p_hdr;
    Content_SystemDatHeader* buf;

    buf = (Content_SystemDatHeader*)Core_Memory_AllocateFromHighEnd(VS_SYSTEM_DAT_SIZE);
    Content_ReadDiscFileBlocking(VS_SYSTEM_DAT_LBA, VS_SYSTEM_DAT_SIZE, buf);

    p_hdr = (RECT *)(((u_char*)((void*)buf)) + buf->image0Offset);
    setRECT(&rect, p_hdr->x, p_hdr->y, p_hdr->w, p_hdr->h);
    LoadImage(&rect, (u_long*)p_hdr + 2);

    p_hdr = (RECT *)(((u_char*)((void*)buf)) + buf->image1Offset);
    setRECT(&rect, p_hdr->x, p_hdr->y, p_hdr->w, p_hdr->h);
    LoadImage(&rect, (u_long*)p_hdr + 2);

    p_hdr = (RECT *)(((u_char*)((void*)buf)) + buf->paletteOffset);
    Render_LoadClutColors((u_short*)p_hdr + 2, 0xF, 0, p_hdr->x);

    p_hdr = (RECT*)((signed char*)buf + buf->image2Offset);
    setRECT(&rect, p_hdr->x, p_hdr->y, p_hdr->w, p_hdr->h);
    LoadImage(&rect, (u_long*)p_hdr + 2);

    p_hdr = (RECT *)(((u_char*)((void*)buf)) + buf->image3Offset);
    setRECT(&rect, p_hdr->x, p_hdr->y, p_hdr->w, p_hdr->h);
    LoadImage(&rect, (u_long*)p_hdr + 2);

    p_hdr = (RECT *)(((u_char*)((void*)buf)) + buf->image4Offset);
    setRECT(&rect, p_hdr->x, p_hdr->y, p_hdr->w, p_hdr->h);
    LoadImage(&rect, (u_long*)p_hdr + 2);

    p_hdr = (RECT *)(((u_char*)((void*)buf)) + buf->fixedImageOffset);
    setRECT(&rect, 0x300, 0xF0, p_hdr->x, p_hdr->y);
    LoadImage(&rect, (u_long*)p_hdr + 1);

    Menu_Framework_InitGameInterface(((u_char*)((void*)buf)) + buf->interfaceDataOffset);
    Core_Memory_FreeFromHighEnd(buf);
}

}  // extern "C"
