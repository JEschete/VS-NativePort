// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_CORE_TEXT_TEXTCODE_H_TYPES
#define VS_CORE_TEXT_TEXTCODE_H_TYPES
#include "Legacy/Psx.h"


// decomp: vs_string_chars (src/include/vs_string.h:3)
enum Core_TextCode {
    Core_TextCode_NoChunking = 0,
    Core_TextCode_Zero = 0,
    Core_TextCode_FontTableZero = 6,
    Core_TextCode_UpperR = 0x1B,
    Core_TextCode_LowerJ = 0x2D,
    Core_TextCode_Space = 0x8F,
    Core_TextCode_Colon = 0x9E,
    Core_TextCode_ForwardSlash = 0xA1,
    Core_TextCode_Hyphen = 0xA7,
    Core_TextCode_Arrow = 0xBC,
    Core_TextCode_NonPrinting = 0xE5,
    Core_TextCode_Confirm = 0xE6,
    Core_TextCode_Terminator = 0xE7,
    Core_TextCode_Newline = 0xE8,
    Core_TextCode_Control = 0xEC,
    Core_TextCode_ChunkSize = 0xF8,
    Core_TextCode_Spacing = 0xFA,
    Core_TextCode_FontTable = 0xFB,
    Core_TextCode_PrintHex = 0xFD,
    Core_TextCode_PrintDecimal = 0xFE,
    Core_TextCode_PrintString = 0xFF
};
typedef enum Core_TextCode Core_TextCode;

#endif // VS_CORE_TEXT_TEXTCODE_H_TYPES
