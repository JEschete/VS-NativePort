// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Core/Runtime.h"

// decomp: SLUS_010.40 _gpuSyncVoidCallback (src/SLUS_010.40/main.c:8227)
void Core_IgnoreCall(void) { }

// decomp: SLUS_010.40 nop5 (src/SLUS_010.40/main.c:9731)
int Core_ReturnZero(void) { return 0; }

// decomp: SLUS_010.40 vs_main_nop9 (src/SLUS_010.40/main.c:11663)
void Core_IgnoreDiagnostic(int arg0 __attribute__((unused)), int arg1 __attribute__((unused))) { }

