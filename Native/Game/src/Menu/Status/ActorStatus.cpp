// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Menu/Status/ActorStatus.h"

// decomp: _ (_@src/MENU/MENU4.PRG/120.c)
char Menu_Status_layoutPadding1[24];

// decomp: _animationStep (_animationStep@src/MENU/MENU4.PRG/120.c)
int Menu_Status_animationStep = 0;

// decomp: _basicStatsAnimDelay (_basicStatsAnimDelay@src/MENU/MENU4.PRG/120.c)
char Menu_Status_basicStatsAnimDelay = 0;

// decomp: _basicStatsAnimState (_basicStatsAnimState@src/MENU/MENU4.PRG/120.c)
char Menu_Status_basicStatsAnimState = 0;

// decomp: _delayScreenUpdate (_delayScreenUpdate@src/MENU/MENU4.PRG/120.c)
char Menu_Status_delayScreenUpdate = 0;

// decomp: _drawBackgroundFirst (_drawBackgroundFirst@src/MENU/MENU4.PRG/120.c)
int Menu_Status_drawBackgroundFirst = 0;

// decomp: _fadeScreen (_fadeScreen@src/MENU/MENU4.PRG/120.c)
int Menu_Status_fadeScreen = 0;

// decomp: _initializedActor (_initializedActor@src/MENU/MENU4.PRG/120.c)
char Menu_Status_initializedActor = 0;

// decomp: _limbStatuses (_limbStatuses@src/MENU/MENU4.PRG/120.c)
char* Menu_Status_limbStatuses[] = { "DYING", "BAD", "AVERAGE", "GOOD", "EXCELLENT", NULL };

// decomp: _previousActor (_previousActor@src/MENU/MENU4.PRG/120.c)
int Menu_Status_previousActor = 0;

// decomp: _renderBasicStatsState (_renderBasicStatsState@src/MENU/MENU4.PRG/120.c)
char Menu_Status_renderBasicStatsState = 0;

// decomp: _selectedActor (_selectedActor@src/MENU/MENU4.PRG/120.c)
int Menu_Status_selectedActor = 0;

// decomp: _selectedElement (_selectedElement@src/MENU/MENU4.PRG/120.c)
u_char Menu_Status_selectedElement;

// decomp: _selectedStatRow (_selectedStatRow@src/MENU/MENU4.PRG/120.c)
char Menu_Status_selectedStatRow;

// decomp: _statusStrings (_statusStrings@src/MENU/MENU4.PRG/120.c)
u_short Menu_Status_statusStrings[1760]; /* contents: disc string table (ModuleData.cpp) */

// decomp: _xPos (_xPos@src/MENU/MENU4.PRG/120.c)
int Menu_Status_xPos = 0;

// decomp: _yPos (_yPos@src/MENU/MENU4.PRG/120.c)
int Menu_Status_yPos = 0;

// MENU4.PRG reloads restore these statics' initial values (PORTING_WATCHLIST.md §1).
void Menu_Status_ActorStatus_ResetMenu4Statics(void)
{
    {
        static const int initial = 0;
        memcpy((void*)&Menu_Status_animationStep, &initial, sizeof Menu_Status_animationStep);
    }
    {
        static const char initial = 0;
        memcpy((void*)&Menu_Status_basicStatsAnimDelay, &initial, sizeof Menu_Status_basicStatsAnimDelay);
    }
    {
        static const char initial = 0;
        memcpy((void*)&Menu_Status_basicStatsAnimState, &initial, sizeof Menu_Status_basicStatsAnimState);
    }
    {
        static const char initial = 0;
        memcpy((void*)&Menu_Status_delayScreenUpdate, &initial, sizeof Menu_Status_delayScreenUpdate);
    }
    {
        static const int initial = 0;
        memcpy((void*)&Menu_Status_drawBackgroundFirst, &initial, sizeof Menu_Status_drawBackgroundFirst);
    }
    {
        static const int initial = 0;
        memcpy((void*)&Menu_Status_fadeScreen, &initial, sizeof Menu_Status_fadeScreen);
    }
    {
        static const char initial = 0;
        memcpy((void*)&Menu_Status_initializedActor, &initial, sizeof Menu_Status_initializedActor);
    }
    {
        static const char* initial[] = { "DYING", "BAD", "AVERAGE", "GOOD", "EXCELLENT", NULL };
        memcpy((void*)&Menu_Status_limbStatuses, &initial, sizeof Menu_Status_limbStatuses);
    }
    {
        static const int initial = 0;
        memcpy((void*)&Menu_Status_previousActor, &initial, sizeof Menu_Status_previousActor);
    }
    {
        static const char initial = 0;
        memcpy((void*)&Menu_Status_renderBasicStatsState, &initial, sizeof Menu_Status_renderBasicStatsState);
    }
    {
        static const int initial = 0;
        memcpy((void*)&Menu_Status_selectedActor, &initial, sizeof Menu_Status_selectedActor);
    }
    {
        static const int initial = 0;
        memcpy((void*)&Menu_Status_xPos, &initial, sizeof Menu_Status_xPos);
    }
    {
        static const int initial = 0;
        memcpy((void*)&Menu_Status_yPos, &initial, sizeof Menu_Status_yPos);
    }
}

}  // extern "C"
