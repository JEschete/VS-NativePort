// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_RENDER_UPLOAD_H_TYPES
#define VS_RENDER_UPLOAD_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_RENDER_UPLOAD_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_RENDER_UPLOAD_H_DECLS)
#define VS_RENDER_UPLOAD_H_DECLS

extern int Render_legacyUploadRectCursor;
extern RECT Render_legacyUploadRects[64 / sizeof(RECT)];

#endif // VS_RENDER_UPLOAD_H_DECLS
