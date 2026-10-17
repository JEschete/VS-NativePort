// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_OVERLAYWORKSPACE_H_TYPES
#define VS_OVERLAYWORKSPACE_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_OVERLAYWORKSPACE_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_OVERLAYWORKSPACE_H_DECLS)
#define VS_OVERLAYWORKSPACE_H_DECLS

extern char Content_overlayWorkspaceOwner;
int Content_AcquireOverlayWorkspace(int arg0);
int Content_ReleaseOverlayWorkspace(int arg0);

#endif // VS_OVERLAYWORKSPACE_H_DECLS
