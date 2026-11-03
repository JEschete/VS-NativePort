// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Render/Camera.h"

// decomp: vs_main_nearClip (vs_main_nearClip@SLUS_010.40)
int Render_nearClip;

// decomp: D_8005E218 (D_8005E218@SLUS_010.40)
MATRIX Render_cameraBasisMatrix;

// decomp: vs_main_projectionDistance (vs_main_projectionDistance@SLUS_010.40)
int Render_projectionDistance;

}  // extern "C"
