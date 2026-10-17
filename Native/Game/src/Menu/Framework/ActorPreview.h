// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_MENU_FRAMEWORK_ACTORPREVIEW_H_TYPES
#define VS_MENU_FRAMEWORK_ACTORPREVIEW_H_TYPES
#include "Legacy/Psx.h"

typedef struct Menu_Framework_PreviewPoseBytes Menu_Framework_PreviewPoseBytes;
typedef struct Menu_Framework_PreviewPoseState Menu_Framework_PreviewPoseState;

// decomp: D_800F4538_unk58_3 (src/MENU/MAINMENU.PRG/224.c:98)
typedef struct Menu_Framework_PreviewPoseBytes {
    u_int opaque0Bit0 : 8;
    u_int opaque1Bit0 : 8;
    u_int opaque2Bit0 : 8;
    u_int opaque3Bit0 : 8;
} Menu_Framework_PreviewPoseBytes;

// decomp: D_800F4590_t (src/BATTLE/BATTLE.PRG/30DB0.h:18)
typedef struct Menu_Framework_PreviewPoseState {
    int reserved0;
    int reserved4;
    int opaque8;
    int reservedC;
    char reserved10;
    char opaque11;
} Menu_Framework_PreviewPoseState;

#endif // VS_MENU_FRAMEWORK_ACTORPREVIEW_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_MENU_FRAMEWORK_ACTORPREVIEW_H_DECLS)
#define VS_MENU_FRAMEWORK_ACTORPREVIEW_H_DECLS

void Menu_Framework_LoadActorPreview(int arg0);
void Menu_Framework_CloneActorPreview(int arg0);
void Menu_Framework_ResetActorPreviewPose(void);
void Menu_Framework_ReleaseActorPreview(void);
int Menu_Framework_DrawActorPreview(MATRIX* arg0);
void Menu_Framework_ResetAshleyEquipmentPose(void);
void Menu_Framework_QueueAshleyWeaponAnimations(int arg0);

#endif // VS_MENU_FRAMEWORK_ACTORPREVIEW_H_DECLS
