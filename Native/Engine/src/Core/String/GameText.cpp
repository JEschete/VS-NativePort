// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Core/String/GameText.h"
#include "Core/Text/TextCode.h"

// decomp: MENU9.PRG _strcpy (src/MENU/MENU9.PRG/238.c:2176)
void Core_String_CopyGameText(u_char* dest, u_char* src)
{
    while (*src != Core_TextCode_Terminator) {
        *dest++ = *src++;
    }

    *dest = *src;
}

// decomp: MENU9.PRG _strcat (src/MENU/MENU9.PRG/238.c:2185)
void Core_String_AppendGameText(char* dest, char* src)
{
    while (*dest != Core_TextCode_Terminator) {
        ++dest;
    }

    while (*src != Core_TextCode_Terminator) {
        *dest++ = *src++;
    }

    *dest = *src;
}

// decomp: MENU9.PRG _toVsStringInt (src/MENU/MENU9.PRG/238.c:2348)
void Core_String_FormatGameInteger(char* buf, int value)
{
    int len;
    int i;

    buf++;
    buf--;

    sprintf(buf, "%d", value);
    len = strlen(buf);

    for (i = 0; i < len; ++i) {
        buf[i] -= '0';
    }

    buf += len;
    *buf = Core_TextCode_Terminator;
}

// decomp: MENU9.PRG _toVsStringPercent (src/MENU/MENU9.PRG/238.c:2370)
void Core_String_FormatGamePercent(char* buf, int value)
{
    typedef struct {
        char text[4];
    } Core_PercentSuffixText;
#pragma vsstring(start)
    static Core_PercentSuffixText Core_kPercentSuffix = { "%\0" };
#pragma vsstring(end)

    int len;
    int i;

    buf++;
    buf--;

    sprintf(buf, "%d", value);
    len = strlen(buf);

    for (i = 0; i < len; ++i) {
        buf[i] -= '0';
    }

    buf += len;

    *((Core_PercentSuffixText*)buf) = Core_kPercentSuffix;
}

}  // extern "C"
