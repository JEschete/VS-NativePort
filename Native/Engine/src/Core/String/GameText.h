// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_CORE_STRING_GAMETEXT_H_TYPES
#define VS_CORE_STRING_GAMETEXT_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_CORE_STRING_GAMETEXT_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_CORE_STRING_GAMETEXT_H_DECLS)
#define VS_CORE_STRING_GAMETEXT_H_DECLS

void Core_String_CopyGameText(u_char* dest, u_char* src);
void Core_String_AppendGameText(char* dest, char* src);
void Core_String_FormatGameInteger(char* buf, int value);
void Core_String_FormatGamePercent(char* buf, int value);

#endif // VS_CORE_STRING_GAMETEXT_H_DECLS
