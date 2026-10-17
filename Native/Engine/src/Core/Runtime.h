// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_CORE_RUNTIME_H_TYPES
#define VS_CORE_RUNTIME_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_CORE_RUNTIME_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_CORE_RUNTIME_H_DECLS)
#define VS_CORE_RUNTIME_H_DECLS

void Core_IgnoreCall(void);
int Core_ReturnZero(void);
void Core_IdleInstruction(void);
void Core_IgnoreDiagnostic(int arg0 __attribute__((unused)), int arg1 __attribute__((unused)));

#endif // VS_CORE_RUNTIME_H_DECLS
