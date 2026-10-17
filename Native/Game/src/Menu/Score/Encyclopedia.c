// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "ActorAssets.h"
#include "Actors/ActorRecord.h"
#include "Actors/ActorState.h"
#include "Actors/Animation.h"
#include "Actors/ModelInstance.h"
#include "Actors/Movement.h"
#include "Actors/Shadow.h"
#include "Actors/Skeleton.h"
#include "Actors/TextureAnimation.h"
#include "Audio/Seq/Channel.h"
#include "Battle/Feedback.h"
#include "Battle/StatusEffects.h"
#include "CdDrive.h"
#include "Content/DiscLayout.h"
#include "Content/StringTables.h"
#include "Core/Memory/Heap.h"
#include "DiscQueue.h"
#include "Field/Actors.h"
#include "Menu/Framework/ActorPreview.h"
#include "Menu/Framework/Framework.h"
#include "Menu/Framework/MenuItems.h"
#include "Menu/Inventory/Inventory.h"
#include "Menu/Score/Encyclopedia.h"
#include "Menu/Score/Records.h"
#include "Menu/Score/ScoreScreen.h"
#include "Render/Font.h"
#include "Render/Palette.h"
#include "Render/Render.h"
#include "Render/RoomGeometry.h"
#include "Save/MemoryCard/GameFiles.h"
#include "State/Progress.h"

// decomp: _enemyNavigationState (_enemyNavigationState@src/MENU/MENU9.PRG/238.c)
int Menu_Score_enemyNavigationState;

// decomp: _modelVariant (_modelVariant@src/MENU/MENU9.PRG/592C.c)
int Menu_Score_modelVariant;

// decomp: _modelVariants (_modelVariants@src/MENU/MENU9.PRG/592C.c)
char Menu_Score_modelVariants[] = { 30, 31, 32, 33, 172, 173 };

// decomp: _paletteFlag (_paletteFlag@src/MENU/MENU9.PRG/592C.c)
int Menu_Score_paletteFlag;

// decomp: _paletteFlags (_paletteFlags@src/MENU/MENU9.PRG/592C.c)
char Menu_Score_paletteFlags[256] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 1, 1 };

// decomp: _variantMap (_variantMap@src/MENU/MENU9.PRG/592C.c)
char Menu_Score_variantMap[] = { 27, 27, 72, 72, 80, 80, 72, 52 };

// decomp: _zudBuffer (_zudBuffer@src/MENU/MENU9.PRG/592C.c)
void* Menu_Score_zudBuffer;

// decomp: _zudCdSlot (_zudCdSlot@src/MENU/MENU9.PRG/592C.c)
Content_DiscQueueSlot* Menu_Score_zudCdSlot;

// decomp: _zudData (_zudData@src/MENU/MENU9.PRG/592C.c)
Menu_Score_ZudHeader* Menu_Score_zudData;

// decomp: _zudLbas (_zudLbas@src/MENU/MENU9.PRG/592C.c)
int Menu_Score_zudLbas[] = { VS_Z003U00_ZUD_LBA, VS_Z003U00_ZUD_LBA, VS_Z003U00_ZUD_LBA,
    VS_Z003U00_ZUD_LBA, VS_Z009U16_ZUD_LBA, VS_Z003U00_ZUD_LBA, VS_Z013U15_ZUD_LBA,
    VS_Z003U00_ZUD_LBA, VS_Z003U00_ZUD_LBA, VS_Z003U00_ZUD_LBA, VS_Z003U00_ZUD_LBA,
    VS_Z009U03_ZUD_LBA, VS_Z048U10_ZUD_LBA, VS_Z003U00_ZUD_LBA, VS_Z009U01_ZUD_LBA,
    VS_Z003U00_ZUD_LBA, VS_Z003U00_ZUD_LBA, VS_Z003U00_ZUD_LBA, VS_Z040U10_ZUD_LBA,
    VS_Z003U00_ZUD_LBA, VS_Z048U02_ZUD_LBA, VS_Z003U00_ZUD_LBA, VS_Z003U00_ZUD_LBA,
    VS_Z003U00_ZUD_LBA, VS_Z048U21_ZUD_LBA, VS_Z003U00_ZUD_LBA, VS_Z048U06_ZUD_LBA,
    VS_Z003U00_ZUD_LBA, VS_Z015U11_ZUD_LBA, VS_Z003U00_ZUD_LBA, VS_Z003U00_ZUD_LBA,
    VS_Z013U00_ZUD_LBA, VS_Z053U05_ZUD_LBA, VS_Z003U00_ZUD_LBA, VS_Z050U05_ZUD_LBA,
    VS_Z003U00_ZUD_LBA, VS_Z050U09_ZUD_LBA, VS_Z003U00_ZUD_LBA, VS_Z051U03_ZUD_LBA,
    VS_Z003U00_ZUD_LBA, VS_Z051U10_ZUD_LBA, VS_Z003U00_ZUD_LBA, VS_Z003U00_ZUD_LBA,
    VS_Z014U01_ZUD_LBA, VS_Z003U00_ZUD_LBA, VS_Z049U00_ZUD_LBA, VS_Z003U00_ZUD_LBA,
    VS_Z024U02_ZUD_LBA, VS_Z003U00_ZUD_LBA, VS_Z056U05_ZUD_LBA, VS_Z048U16_ZUD_LBA,
    VS_Z003U00_ZUD_LBA, VS_Z051U17_ZUD_LBA, VS_Z003U00_ZUD_LBA, VS_Z053U11_ZUD_LBA,
    VS_Z003U00_ZUD_LBA, VS_Z051U21_ZUD_LBA, VS_Z003U00_ZUD_LBA, VS_Z055U06_ZUD_LBA,
    VS_Z003U00_ZUD_LBA, VS_Z028U00_ZUD_LBA, VS_Z003U00_ZUD_LBA, VS_Z028U01_ZUD_LBA,
    VS_Z003U00_ZUD_LBA, VS_Z050U01_ZUD_LBA, VS_Z003U00_ZUD_LBA, VS_Z003U00_ZUD_LBA,
    VS_Z051U22_ZUD_LBA, VS_Z003U00_ZUD_LBA, VS_Z051U27_ZUD_LBA, VS_Z051U23_ZUD_LBA,
    VS_Z003U00_ZUD_LBA, VS_Z048U13_ZUD_LBA, VS_Z003U00_ZUD_LBA, VS_Z040U07_ZUD_LBA,
    VS_Z003U00_ZUD_LBA, VS_Z040U08_ZUD_LBA, VS_Z003U00_ZUD_LBA, VS_Z003U00_ZUD_LBA,
    VS_Z049U02_ZUD_LBA, VS_Z003U00_ZUD_LBA, VS_Z003U00_ZUD_LBA, VS_Z003U00_ZUD_LBA,
    VS_Z003U00_ZUD_LBA, VS_Z049U07_ZUD_LBA, VS_Z003U00_ZUD_LBA, VS_Z056U14_ZUD_LBA,
    VS_Z003U00_ZUD_LBA, VS_Z053U13_ZUD_LBA, VS_Z003U00_ZUD_LBA, VS_Z003U00_ZUD_LBA,
    VS_Z024U00_ZUD_LBA, VS_Z053U14_ZUD_LBA, VS_Z003U00_ZUD_LBA, VS_Z003U00_ZUD_LBA,
    VS_Z022U01_ZUD_LBA, VS_Z053U15_ZUD_LBA, VS_Z003U00_ZUD_LBA, VS_Z003U00_ZUD_LBA,
    VS_Z024U06_ZUD_LBA, VS_Z053U16_ZUD_LBA, VS_Z003U00_ZUD_LBA, VS_Z003U00_ZUD_LBA,
    VS_Z022U02_ZUD_LBA, VS_Z053U20_ZUD_LBA, VS_Z003U00_ZUD_LBA, VS_Z003U00_ZUD_LBA,
    VS_Z025U00_ZUD_LBA, VS_Z003U00_ZUD_LBA, VS_Z011U00_ZUD_LBA, VS_Z003U00_ZUD_LBA,
    VS_Z049U01_ZUD_LBA, VS_Z003U00_ZUD_LBA, VS_Z030U01_ZUD_LBA, VS_Z003U00_ZUD_LBA,
    VS_Z012U00_ZUD_LBA, VS_Z003U00_ZUD_LBA, VS_Z030U00_ZUD_LBA, VS_Z003U00_ZUD_LBA,
    VS_Z012U01_ZUD_LBA, VS_Z003U00_ZUD_LBA, VS_Z050U03_ZUD_LBA, VS_Z003U00_ZUD_LBA,
    VS_Z053U01_ZUD_LBA, VS_Z003U00_ZUD_LBA, VS_Z056U04_ZUD_LBA, VS_Z003U00_ZUD_LBA,
    VS_Z016U00_ZUD_LBA, VS_Z003U00_ZUD_LBA, VS_Z055U00_ZUD_LBA, VS_Z003U00_ZUD_LBA,
    VS_Z054U00_ZUD_LBA, VS_Z003U00_ZUD_LBA, VS_Z031U00_ZUD_LBA, VS_Z003U00_ZUD_LBA,
    VS_Z056U01_ZUD_LBA, VS_Z003U00_ZUD_LBA, VS_Z056U00_ZUD_LBA, VS_Z003U00_ZUD_LBA,
    VS_Z048U00_ZUD_LBA, VS_Z003U00_ZUD_LBA, VS_Z022U00_ZUD_LBA, VS_Z003U00_ZUD_LBA,
    VS_Z041U00_ZUD_LBA, VS_Z003U00_ZUD_LBA, VS_Z050U00_ZUD_LBA, VS_Z003U00_ZUD_LBA,
    VS_Z003U00_ZUD_LBA, VS_Z003U00_ZUD_LBA, VS_Z055U05_ZUD_LBA, VS_Z003U00_ZUD_LBA,
    VS_Z055U04_ZUD_LBA, VS_Z003U00_ZUD_LBA, VS_Z017U00_ZUD_LBA, VS_Z003U00_ZUD_LBA,
    VS_Z051U01_ZUD_LBA, VS_Z003U00_ZUD_LBA, VS_Z024U05_ZUD_LBA, VS_Z003U00_ZUD_LBA,
    VS_Z040U00_ZUD_LBA, VS_Z003U00_ZUD_LBA, VS_Z053U02_ZUD_LBA, VS_Z003U00_ZUD_LBA,
    VS_Z024U01_ZUD_LBA, VS_Z003U00_ZUD_LBA, VS_Z056U02_ZUD_LBA, VS_Z003U00_ZUD_LBA,
    VS_Z056U06_ZUD_LBA, VS_Z003U00_ZUD_LBA, VS_Z003U00_ZUD_LBA, VS_Z003U00_ZUD_LBA,
    VS_Z003U00_ZUD_LBA, VS_Z003U00_ZUD_LBA, VS_Z003U00_ZUD_LBA, VS_Z003U00_ZUD_LBA,
    VS_Z003U00_ZUD_LBA, VS_Z003U00_ZUD_LBA, VS_Z003U00_ZUD_LBA, VS_Z003U00_ZUD_LBA,
    VS_Z003U00_ZUD_LBA, VS_Z003U00_ZUD_LBA, VS_Z003U00_ZUD_LBA, VS_Z003U00_ZUD_LBA,
    VS_Z003U00_ZUD_LBA, VS_Z003U00_ZUD_LBA, VS_Z003U00_ZUD_LBA, VS_Z003U00_ZUD_LBA,
    VS_Z003U00_ZUD_LBA, VS_Z003U00_ZUD_LBA, VS_Z003U00_ZUD_LBA, VS_Z003U00_ZUD_LBA,
    VS_Z003U00_ZUD_LBA, VS_Z003U00_ZUD_LBA, VS_Z003U00_ZUD_LBA, VS_Z003U00_ZUD_LBA,
    VS_Z003U00_ZUD_LBA, VS_Z003U00_ZUD_LBA, VS_Z003U00_ZUD_LBA, VS_Z003U00_ZUD_LBA,
    VS_Z003U00_ZUD_LBA, VS_Z003U00_ZUD_LBA, VS_Z003U00_ZUD_LBA, VS_Z003U00_ZUD_LBA,
    VS_Z003U00_ZUD_LBA, VS_Z003U00_ZUD_LBA, VS_Z003U00_ZUD_LBA, VS_Z003U00_ZUD_LBA,
    VS_Z003U00_ZUD_LBA, VS_Z003U00_ZUD_LBA, VS_Z003U00_ZUD_LBA, VS_Z003U00_ZUD_LBA,
    VS_Z003U00_ZUD_LBA, VS_Z003U00_ZUD_LBA, VS_Z003U00_ZUD_LBA, VS_Z003U00_ZUD_LBA,
    VS_Z003U00_ZUD_LBA, VS_Z003U00_ZUD_LBA, VS_Z003U00_ZUD_LBA, VS_Z003U00_ZUD_LBA,
    VS_Z003U00_ZUD_LBA, VS_Z003U00_ZUD_LBA, VS_Z003U00_ZUD_LBA, VS_Z003U00_ZUD_LBA,
    VS_Z003U00_ZUD_LBA, VS_Z003U00_ZUD_LBA, VS_Z003U00_ZUD_LBA, VS_Z003U00_ZUD_LBA,
    VS_Z003U00_ZUD_LBA, VS_Z003U00_ZUD_LBA, VS_Z003U00_ZUD_LBA, VS_Z003U00_ZUD_LBA,
    VS_Z003U00_ZUD_LBA, VS_Z003U00_ZUD_LBA, VS_Z003U00_ZUD_LBA, VS_Z003U00_ZUD_LBA,
    VS_Z003U00_ZUD_LBA, VS_Z003U00_ZUD_LBA, VS_Z003U00_ZUD_LBA, VS_Z003U00_ZUD_LBA,
    VS_Z003U00_ZUD_LBA, VS_Z003U00_ZUD_LBA, VS_Z003U00_ZUD_LBA, VS_Z003U00_ZUD_LBA,
    VS_Z003U00_ZUD_LBA, VS_Z003U00_ZUD_LBA, VS_Z003U00_ZUD_LBA, VS_Z003U00_ZUD_LBA,
    VS_Z003U00_ZUD_LBA, VS_Z003U00_ZUD_LBA, VS_Z003U00_ZUD_LBA, VS_Z003U00_ZUD_LBA,
    VS_Z003U00_ZUD_LBA, VS_Z040U02_ZUD_LBA, VS_Z009U04_ZUD_LBA, VS_Z003U00_ZUD_LBA,
    VS_Z003U00_ZUD_LBA };

// decomp: _zudSizes (_zudSizes@src/MENU/MENU9.PRG/592C.c)
int Menu_Score_zudSizes[] = { VS_Z003U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE,
    VS_Z003U00_ZUD_SIZE, VS_Z009U16_ZUD_SIZE, VS_Z003U00_ZUD_SIZE, VS_Z013U15_ZUD_SIZE,
    VS_Z003U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE,
    VS_Z009U03_ZUD_SIZE, VS_Z048U10_ZUD_SIZE, VS_Z003U00_ZUD_SIZE, VS_Z009U01_ZUD_SIZE,
    VS_Z003U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE, VS_Z040U10_ZUD_SIZE,
    VS_Z003U00_ZUD_SIZE, VS_Z048U02_ZUD_SIZE, VS_Z003U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE,
    VS_Z003U00_ZUD_SIZE, VS_Z048U21_ZUD_SIZE, VS_Z003U00_ZUD_SIZE, VS_Z048U06_ZUD_SIZE,
    VS_Z003U00_ZUD_SIZE, VS_Z015U11_ZUD_SIZE, VS_Z003U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE,
    VS_Z013U00_ZUD_SIZE, VS_Z053U05_ZUD_SIZE, VS_Z003U00_ZUD_SIZE, VS_Z050U05_ZUD_SIZE,
    VS_Z003U00_ZUD_SIZE, VS_Z050U09_ZUD_SIZE, VS_Z003U00_ZUD_SIZE, VS_Z051U03_ZUD_SIZE,
    VS_Z003U00_ZUD_SIZE, VS_Z051U10_ZUD_SIZE, VS_Z003U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE,
    VS_Z014U01_ZUD_SIZE, VS_Z003U00_ZUD_SIZE, VS_Z049U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE,
    VS_Z024U02_ZUD_SIZE, VS_Z003U00_ZUD_SIZE, VS_Z056U05_ZUD_SIZE, VS_Z048U16_ZUD_SIZE,
    VS_Z003U00_ZUD_SIZE, VS_Z051U17_ZUD_SIZE, VS_Z003U00_ZUD_SIZE, VS_Z053U11_ZUD_SIZE,
    VS_Z003U00_ZUD_SIZE, VS_Z051U21_ZUD_SIZE, VS_Z003U00_ZUD_SIZE, VS_Z055U06_ZUD_SIZE,
    VS_Z003U00_ZUD_SIZE, VS_Z028U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE, VS_Z028U01_ZUD_SIZE,
    VS_Z003U00_ZUD_SIZE, VS_Z050U01_ZUD_SIZE, VS_Z003U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE,
    VS_Z051U22_ZUD_SIZE, VS_Z003U00_ZUD_SIZE, VS_Z051U27_ZUD_SIZE, VS_Z051U23_ZUD_SIZE,
    VS_Z003U00_ZUD_SIZE, VS_Z048U13_ZUD_SIZE, VS_Z003U00_ZUD_SIZE, VS_Z040U07_ZUD_SIZE,
    VS_Z003U00_ZUD_SIZE, VS_Z040U08_ZUD_SIZE, VS_Z003U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE,
    VS_Z049U02_ZUD_SIZE, VS_Z003U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE,
    VS_Z003U00_ZUD_SIZE, VS_Z049U07_ZUD_SIZE, VS_Z003U00_ZUD_SIZE, VS_Z056U14_ZUD_SIZE,
    VS_Z003U00_ZUD_SIZE, VS_Z053U13_ZUD_SIZE, VS_Z003U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE,
    VS_Z024U00_ZUD_SIZE, VS_Z053U14_ZUD_SIZE, VS_Z003U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE,
    VS_Z022U01_ZUD_SIZE, VS_Z053U15_ZUD_SIZE, VS_Z003U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE,
    VS_Z024U06_ZUD_SIZE, VS_Z053U16_ZUD_SIZE, VS_Z003U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE,
    VS_Z022U02_ZUD_SIZE, VS_Z053U20_ZUD_SIZE, VS_Z003U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE,
    VS_Z025U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE, VS_Z011U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE,
    VS_Z049U01_ZUD_SIZE, VS_Z003U00_ZUD_SIZE, VS_Z030U01_ZUD_SIZE, VS_Z003U00_ZUD_SIZE,
    VS_Z012U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE, VS_Z030U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE,
    VS_Z012U01_ZUD_SIZE, VS_Z003U00_ZUD_SIZE, VS_Z050U03_ZUD_SIZE, VS_Z003U00_ZUD_SIZE,
    VS_Z053U01_ZUD_SIZE, VS_Z003U00_ZUD_SIZE, VS_Z056U04_ZUD_SIZE, VS_Z003U00_ZUD_SIZE,
    VS_Z016U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE, VS_Z055U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE,
    VS_Z054U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE, VS_Z031U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE,
    VS_Z056U01_ZUD_SIZE, VS_Z003U00_ZUD_SIZE, VS_Z056U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE,
    VS_Z048U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE, VS_Z022U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE,
    VS_Z041U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE, VS_Z050U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE,
    VS_Z003U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE, VS_Z055U05_ZUD_SIZE, VS_Z003U00_ZUD_SIZE,
    VS_Z055U04_ZUD_SIZE, VS_Z003U00_ZUD_SIZE, VS_Z017U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE,
    VS_Z051U01_ZUD_SIZE, VS_Z003U00_ZUD_SIZE, VS_Z024U05_ZUD_SIZE, VS_Z003U00_ZUD_SIZE,
    VS_Z040U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE, VS_Z053U02_ZUD_SIZE, VS_Z003U00_ZUD_SIZE,
    VS_Z024U01_ZUD_SIZE, VS_Z003U00_ZUD_SIZE, VS_Z056U02_ZUD_SIZE, VS_Z003U00_ZUD_SIZE,
    VS_Z056U06_ZUD_SIZE, VS_Z003U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE,
    VS_Z003U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE,
    VS_Z003U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE,
    VS_Z003U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE,
    VS_Z003U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE,
    VS_Z003U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE,
    VS_Z003U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE,
    VS_Z003U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE,
    VS_Z003U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE,
    VS_Z003U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE,
    VS_Z003U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE,
    VS_Z003U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE,
    VS_Z003U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE,
    VS_Z003U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE,
    VS_Z003U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE,
    VS_Z003U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE,
    VS_Z003U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE,
    VS_Z003U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE,
    VS_Z003U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE,
    VS_Z003U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE,
    VS_Z003U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE, VS_Z003U00_ZUD_SIZE,
    VS_Z003U00_ZUD_SIZE, VS_Z040U02_ZUD_SIZE, VS_Z009U04_ZUD_SIZE, VS_Z003U00_ZUD_SIZE,
    VS_Z003U00_ZUD_SIZE };

// decomp: local static of _renderEnemyMenu, at file scope so the module reset reaches it
static char Menu_Score_enemyListCursorAnimStep = 0;

// decomp: MENU9.PRG _setEnemyDescription (src/MENU/MENU9.PRG/238.c:1414)
void Menu_Score_ShowEnemyDescription(void)
{
    if (Menu_Score_monBinData[Menu_Score_selectedEnemy].unlocked != 0) {
        Menu_Framework_SetInformationMessage(
            (char*)&Menu_Score_enemyDescriptions[Menu_Score_enemyDescriptions[Menu_Score_selectedEnemy]]);
    } else {
        Menu_Framework_SetInformationMessage(
            (char*)&Menu_Score_miscInfo[Menu_Score_miscInfo[Text_Menu9_miscInfo_Index_enemyNotDefeated]]);
    }
}

// decomp: MENU9.PRG _renderEnemyNo (src/MENU/MENU9.PRG/238.c:1878)
void Menu_Score_DrawEnemyNumber(int x, int y, int enemyNo, int dimmed)
{
    char digits[2];
    int i;
    POLY_FT4* poly;

    sprintf(digits, "%03d", enemyNo);

    x += 0x18;
    poly = *(void**)Core_ScratchAddr(0);

    for (i = 2; i >= 0; --i, x -= 5) {

        setPolyFT4(poly);
        setXY4(poly, x, y - 4, x + 6, y - 4, x, y + 6, x + 6, y + 6);
        setcode(poly, (getcode(poly) | 0x02) & 0xFE);
        setUV4(poly, (digits[i] - 48) * 6, 0, ((digits[i] - 48) * 6) + 6, 0,
            (digits[i] - 48) * 6, 10, ((digits[i] - 48) * 6) + 6, 10);

        if (dimmed != 0) {
            setTPage(poly, 0, 1, 768, 0);
            setRGB0(poly, 32, 32, 32);
            setClut(poly, 880, 223);
        } else {
            setTPage(poly, 0, 0, 768, 0);
            setRGB0(poly, 128, 128, 128);
            setClut(poly, 832, 223);
        }

        AddPrim(*(void**)Core_ScratchAddr(2), poly++);
    }

    *(void**)Core_ScratchAddr(0) = poly;
}

// decomp: MENU9.PRG _initEnemyList (src/MENU/MENU9.PRG/238.c:1916)
void Menu_Score_InitializeEnemyList(void)
{
    int i;
    char index;
    Menu_Score_EnemyEntry* enemy;

    for (i = 0, enemy = Menu_Score_monBinData; i < 78; ++i, ++enemy) {
        int j;
        enemy->unlocked = 0;

        for (j = enemy->killFlagsOffset;
            j < (enemy->killFlagsOffset + enemy->killFlagsCount); ++j) {

            int flag = Audio_Seq_unusedScoredata.enemyKillFlags[j / 32] & (1 << (j % 32));

            if (flag) {
                enemy->unlocked = 1;
                break;
            }
        }
    }

    for (i = 0, enemy = Menu_Score_monBinData, index = -1; i < 78; ++i, ++enemy) {
        if (enemy->unlocked != 0) {
            enemy->prev = index;
            index = i;
        }
    }

    for (i = 0, enemy = Menu_Score_monBinData; i < 78; ++i, ++enemy) {
        if (enemy->unlocked != 0) {
            enemy->prev = index;
            break;
        }
    }

    for (i = 77, enemy = &Menu_Score_monBinData[77], index = -1; i >= 0; --i, --enemy) {
        if (enemy->unlocked != 0) {
            enemy->next = index;
            index = i;
        }
    }

    for (i = 77, enemy = &Menu_Score_monBinData[77]; i >= 0; --i, --enemy) {
        if (enemy->unlocked != 0) {
            enemy->next = index;
            break;
        }
    }
}

// decomp: MENU9.PRG _renderEnemyMenu (src/MENU/MENU9.PRG/238.c:1972)
void Menu_Score_RenderEnemyList(int animState)
{

    int i;
    int sp18;
    int selectedRow = Battle_unusedMenu9CursorMemory.encyclopaediaPage
                    + Battle_unusedMenu9CursorMemory.encyclopaediaRow;
    Menu_Score_EnemyEntry* enemy = Menu_Score_monBinData;

    for (i = 0; i < 78; ++i) {

        if (i == selectedRow) {

            if (enemy->selected == 0) {
                if (enemy->unlocked != 0) {
                    Menu_Framework_SetInformationMessage(
                        (char*)&Menu_Score_enemyDescriptions[Menu_Score_enemyDescriptions[i]]);
                } else {
                    Menu_Framework_SetInformationMessage(
                        (char*)&Menu_Score_miscInfo[Menu_Score_miscInfo[Text_Menu9_miscInfo_Index_enemyNotDefeated]]);
                }
            }

            enemy->selected = 1;

            if (enemy->unlocked != 0) {
                enemy->animationState = 8;
            }
        } else {

            enemy->selected = 0;

            if (enemy->animationState > 0) {
                --enemy->animationState;
            }
        }

        ++enemy;
    }

    i = 0;
    sp18 = animState * 23;
    enemy = &Menu_Score_monBinData[Battle_unusedMenu9CursorMemory.encyclopaediaPage];

    for (; i < 8; ++i, ++enemy) {
        int x;
        Menu_Framework_MenuRow* menuItem;

        Menu_Score_CursorMemory* cursorMem = &Battle_unusedMenu9CursorMemory;
        int y = sp18 - 16;

        if ((56 + i * 16) < y) {
            y = 56 + i * 16;
        }

        x = 60;

        if ((enemy->selected != 0) && (animState == 8)) {
            Menu_Score_enemyListCursorAnimStep =
                Menu_Framework_DrawCursor(Menu_Score_enemyListCursorAnimStep, ((y - 8) << 0x10) | 48);
        }

        if (enemy->unlocked != 0) {
            menuItem = Menu_Framework_SetMenuItem(0, x, y, 200, 0, enemy->name);
        } else {
            menuItem = Menu_Framework_SetMenuItem(0, x, y, 0xC8, 0,
                (char*)&Menu_Score_miscInfo[Menu_Score_miscInfo[Text_Menu9_miscInfo_Index_enemyNameNone]]);
            menuItem->fontColor = 3;
        }

        menuItem->gradientState = enemy->animationState;

        if ((i == 0) && (cursorMem->encyclopaediaPage != 0)) {
            menuItem->isScrollable = Menu_Framework_MenuItemScroll_Up;
        }

        if ((i == 7) && (cursorMem->encyclopaediaPage != 70)) {
            menuItem->isScrollable = Menu_Framework_MenuItemScroll_Down;
        }

        Menu_Score_DrawEnemyNumber(
            x - 14, y, cursorMem->encyclopaediaPage + i + 1, menuItem->isScrollable & 1);

        if ((menuItem->isScrollable != 0) && (animState == 8)) {
            Menu_Score_RenderScrollableRow(menuItem);
        } else {
            Menu_Framework_DrawMenuItem(menuItem);
        }
    }

    Menu_Framework_MenuItem(0)->state = 0;
}

// decomp: MENU9.PRG _renderEnemyDetailScreen (src/MENU/MENU9.PRG/238.c:2069)
void Menu_Score_RenderEnemyViewer(int animState, int state)
{
    char const* enemyClassName[] = { "HUMAN", "BEAST", "UNDEAD", "PHANTOM", "DRAGON",
        "EVIL" };
    MATRIX sp30;
    Render_ScreenOffset sp50;
    Render_ScreenOffset sp58;
    char sp60[12];
    int temp_s0;
    int temp_s2;
    int temp_s4;
    int temp_s6;
    void** scratch;

    if (animState < 8) {
        Menu_Score_DrawViewerEdgeFade();
    }

    if (state != 3) {

        Render_GetProjectionScreenOffset(&sp50);

        if (state == 2) {
            sp58.x = ((8 - animState) << 5) + 160;
        } else {
            sp58.x = 160 - ((8 - animState) << 5);
        }

        sp58.y = 0x68;

        Render_SetProjectionScreenOffset(&sp58);
        Menu_Score_BuildViewerCamera(&sp30);
        Menu_Framework_DrawActorPreview(&sp30);
        Render_SetProjectionScreenOffset(&sp50);
    }

    if (state > 0) {
        animState = 8;
    }

    if (Menu_Score_hideUi == 0) {
        temp_s2 = (8 - animState) * 16;

        Menu_Framework_DrawMenuItem(Menu_Framework_SetMenuItem(
            0, temp_s2 + 184, 166, 256, 0, &Menu_Score_monBinData[Menu_Score_selectedEnemy].name[2]));
        Menu_Framework_MenuItem(0)->state = 0;

        temp_s4 = animState * 16;
        scratch = (void**)Core_ScratchAddr(0);

        Menu_Framework_DrawButtonIcon(Menu_Framework_ButtonId_L2, temp_s4 - 120, 15, scratch[1] + 24);

        temp_s0 = (temp_s4 - 100) & 0xFFFF;

        Render_DrawCompactTextColor(
            "ROTATION", temp_s0 | (18 << 16), Menu_Score_rotationColor, scratch[1] + 24);

        temp_s6 = temp_s4 - 112;

        Menu_Score_DrawEnemyBanner(temp_s6, 18, 96, 12, 3);
        Menu_Framework_DrawButtonIcon(Menu_Framework_ButtonId_R2, temp_s4 - 120, 34, scratch[1] + 24);
        Render_DrawCompactTextColor(
            "ZOOM", temp_s0 | (36 << 16), Menu_Score_zoomColor, scratch[1] + 24);
        Menu_Score_DrawEnemyBanner(temp_s6, 36, 78, 12, 3);
        sprintf(sp60, "NO.   %03d/%03d", Menu_Score_selectedEnemy + 1, 78);
        Render_DrawCompactTextColor(
            sp60, ((temp_s4 - 120) & 0xFFFF) | 0xA00000, 0x808080, scratch[1] + 0x1C);
        Menu_Score_DrawGradientPanel(temp_s4 - 128, 160, 100, 12, 2);
        Render_DrawCompactTextColor(enemyClassName[Menu_Score_monBinData[Menu_Score_selectedEnemy].classId],
            ((temp_s2 + 188) & 0xFFFF) | 0x9B0000, 0x808080, NULL);
        Menu_Score_DrawGradientPanel(temp_s2 + 180, 155, 140, 26, 1);
    }
}

// decomp: MENU9.PRG _renderEnemyNavigation (src/MENU/MENU9.PRG/238.c:2782)
void Menu_Score_UpdateEnemyNavigation(int direction)
{
    int x;
    int clut;
    POLY_FT4* poly;
    void** scratch;
    int new_var;

    if (Menu_Score_hideUi == 0) {

        clut = getClut(896, 223);

        if (direction >= 2) {
            direction -= 2;
            clut = getClut(912, 223);
        }

        if (direction != 0) {
            Render_DrawCompactTextColor(
                "#R1", vs_getXY(286, 98), vs_getRGB888(128, 128, 128), NULL);

            if (Menu_Score_enemyNavigationState < 9) {
                x = (Menu_Score_enemyNavigationState >> 1) + 288;
            } else {
                x = -(Menu_Score_enemyNavigationState >> 2) + 294;
            }
        } else {
            Render_DrawCompactTextColor(
                "L1", vs_getXY(34, 98), vs_getRGB888(128, 128, 128), NULL);

            if (Menu_Score_enemyNavigationState < 9) {
                new_var = 32;
                x = new_var - (Menu_Score_enemyNavigationState >> 1);
            } else {
                x = (Menu_Score_enemyNavigationState >> 2) + 26;
            }
        }

        x -= 8;
        scratch = (void**)Core_ScratchAddr(0);
        poly = scratch[0];

        setPolyFT4(poly);
        setXY4(poly, x, 96, x + 16, 96, x, 112, x + 16, 112);
        setUV4(poly, direction * 16, 48, (direction * 16) + 16, 48, direction * 16, 64,
            (direction * 16) + 16, 64);
        setTPage(poly, 0, 0, 768, 0);
        poly->clut = clut;
        setShadeTex(poly, 1);

        AddPrim(scratch[1], poly++);

        scratch[0] = poly;
    }
}

// decomp: MENU9.PRG vs_menu9_loadZud (src/MENU/MENU9.PRG/57BC.c:10)
int Menu_Score_LoadEnemyPreview(int zudId)
{
    int state = Menu_Inventory_equipmentModelLoadStep;

    switch (state) {
    case 0:
        Menu_Score_QueueEnemyModelRead(zudId);

        ++Menu_Inventory_equipmentModelLoadStep;
        break;

    case 1:
        if (Menu_Score_LoadEnemyModelParts() != 1) {
            ++Menu_Inventory_equipmentModelLoadStep;
        }

        break;

    case 2:
        Menu_Framework_ResetActorPreviewPose();

        Actors_actorRecords[1]->instance.weaponDrawn = 1;

        Actors_PlayActorAnimation(1, 1, 0, 0);

        Menu_Inventory_equipmentModelLoadStep = 0;
        return 0;
    }

    return 1;
}

// decomp: MENU9.PRG vs_menu9_freeZud (src/MENU/MENU9.PRG/57BC.c:42)
void Menu_Score_FreeEnemyPreview(void)
{
    Content_ReleaseActorModel(1);
    Content_ReleaseActorModel(16);
    ((void (*)())Menu_Score_FreeEnemyModelBuffer)();
}

// decomp: MENU9.PRG func_801080C8 (src/MENU/MENU9.PRG/57BC.c:49)
void Menu_Score_InitializeAshleyPreviewPose(void)
{
    Actors_ActorRecord* p = Actors_actorRecords[1];

    p->animationId = 33;
    p->instance.weaponDrawn = 0;

    Actors_InitializeActorAnimationPose(p, &p->blendPose);
    Actors_SetActorDisplayFlags(1, 3);
    Actors_SetActorCompanion(1, 0, NULL, NULL);
}

// decomp: MENU9.PRG vs_menu9_loadZudFile (src/MENU/MENU9.PRG/592C.c:171)
void Menu_Score_QueueEnemyModelRead(int id)
{
    static int Menu_Score_queueEnemyModelReadZudId;

    Content_DiscFile cdFile;
    int i;

    cdFile.lba = Menu_Score_zudLbas[id];
    Menu_Score_queueEnemyModelReadZudId = id;
    cdFile.size = Menu_Score_zudSizes[id];
    Menu_Score_zudCdSlot = Content_AllocateDiscRequest(&cdFile);
    Menu_Score_zudData = Core_Memory_AllocateFromHighEnd(cdFile.size);

    Content_EnqueuePriorityDiscRequest(Menu_Score_zudCdSlot, Menu_Score_zudData);

    Menu_Score_paletteFlag = Menu_Score_paletteFlags[id];
    Menu_Score_modelVariant = 0;

    for (i = 0; i < 6; ++i) {
        if (id == Menu_Score_modelVariants[i]) {
            Menu_Score_modelVariant = i;
            break;
        }
    }

    if (Menu_Score_modelVariant != 0) {
        Menu_Score_zudBuffer = Core_Memory_AllocateFromLowEnd(0x3200);
    } else {
        Menu_Score_zudBuffer = Core_Memory_AllocateFromLowEnd(0x24D0);
    }
}

// decomp: MENU9.PRG vs_menu9_parseZudFile (src/MENU/MENU9.PRG/592C.c:203)
int Menu_Score_LoadEnemyModelParts(void)
{
    Content_ActorAssetRequest slot;

    if (Menu_Score_zudCdSlot != NULL) {
        if (Menu_Score_zudCdSlot->state != Content_DiscRequestState_Loaded) {
            return 1;
        }

        Content_ReleaseDiscRequest(Menu_Score_zudCdSlot);
        Menu_Score_zudCdSlot = NULL;

        slot.assetKind = 2;
        slot.targetSlot = 1;
        slot.dataAddress = Menu_Score_zudData->shpOffset + (uintptr_t)Menu_Score_zudData;
        slot.modelId = Menu_Score_zudData->characterId;
        slot.destination = Menu_Score_zudBuffer;
        slot.subKind = 0xFF;
        slot.variant = Menu_Score_paletteFlag & 1;
        slot.material = Menu_Score_zudData->defaultMaterial;
        Content_QueueActorAssetRequest(&slot);

        if (Menu_Score_modelVariant != 0) {
            slot.assetKind = 1;
            slot.targetSlot = 16;
            slot.modelId = Menu_Score_variantMap[Menu_Score_modelVariant];
            slot.destination = Menu_Score_zudBuffer + 0x1900;
            slot.subKind = 1;
            slot.textureSlot = 0xFC;
            slot.variant = 0;
            Content_QueueActorAssetRequest(&slot);
        } else {

            if (Menu_Score_zudData->weapon != 0) {
                slot.assetKind = 4;
                slot.targetSlot = 2;
                slot.dataAddress = Menu_Score_zudData->weaponWepOffset + (uintptr_t)Menu_Score_zudData;
                slot.modelId = Menu_Score_zudData->weapon;
                slot.destination = Menu_Score_zudBuffer + 0x1900;
                slot.subKind = 1;
                slot.textureSlot = 0xF0;
                slot.material = Menu_Score_zudData->weaponMaterial;
                Content_QueueActorAssetRequest(&slot);
            }

            if (Menu_Score_zudData->shield != 0) {
                slot.assetKind = 4;
                slot.targetSlot = 3;
                slot.dataAddress = Menu_Score_zudData->shieldWepOffset + (uintptr_t)Menu_Score_zudData;
                slot.modelId = Menu_Score_zudData->shield;
                slot.destination = Menu_Score_zudBuffer + 0x1EE8;
                slot.subKind = 1;
                slot.textureSlot = 0xF1;
                slot.material = Menu_Score_zudData->shieldMaterial;
                Content_QueueActorAssetRequest(&slot);
            }
        }

        if (Menu_Score_zudData->battleSeqLen != 0) {
            slot.assetKind = 8;
            slot.targetSlot = 1;
            slot.dataAddress = Menu_Score_zudData->battleSeqOffset + (uintptr_t)Menu_Score_zudData;
            slot.subKind = 0;
            slot.modelId = Menu_Score_zudData->opaque2;
            Content_QueueActorAssetRequest(&slot);
        }

        if (Menu_Score_zudData->commonSeqLen != 0) {
            slot.assetKind = 8;
            slot.targetSlot = 1;
            slot.dataAddress = Menu_Score_zudData->commonSeqOffset + (uintptr_t)Menu_Score_zudData;
            slot.subKind = 0;
            slot.modelId = 0;
            Content_QueueActorAssetRequest(&slot);
        }
        return 1;
    }

    if (Menu_Score_zudData == NULL) {
        return 0;
    }

    Content_UpdateActorAssetQueue();

    if (Content_FindFreeActorAssetRequest() == 0) {
        Core_Memory_FreeFromHighEnd(Menu_Score_zudData);
        Menu_Score_zudData = NULL;
    }

    return 1;
}

// decomp: MENU9.PRG vs_menu9_freeZudFile (src/MENU/MENU9.PRG/592C.c:295)
void Menu_Score_FreeEnemyModelBuffer(void)
{
    Core_Memory_FreeFromHighEnd(Menu_Score_zudBuffer);
    Menu_Score_zudBuffer = NULL;
}

// MENU9.PRG reloads restore these statics' initial values (PORTING_WATCHLIST.md §1).
void Menu_Score_Encyclopedia_ResetMenu9Statics(void)
{
    {
        static const char initial[] = { 30, 31, 32, 33, 172, 173 };
        memcpy((void*)&Menu_Score_modelVariants, &initial, sizeof Menu_Score_modelVariants);
    }
    {
        static const char initial[] = { 27, 27, 72, 72, 80, 80, 72, 52 };
        memcpy((void*)&Menu_Score_variantMap, &initial, sizeof Menu_Score_variantMap);
    }
    {
        static const char initial = 0;
        memcpy((void*)&Menu_Score_enemyListCursorAnimStep, &initial, sizeof Menu_Score_enemyListCursorAnimStep);
    }
}

