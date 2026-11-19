#pragma once
// The projection state the GTE setup shares: the near clip, the projection
// distance and the camera basis matrix.
#include "Core/Gte.h"

namespace VS::Render {
// decomp: vs_main_nearClip (vs_main_nearClip@SLUS_010.40)
extern int nearClip;
// decomp: D_8005E218 (D_8005E218@SLUS_010.40)
extern MATRIX cameraBasisMatrix;
// decomp: vs_main_projectionDistance (vs_main_projectionDistance@SLUS_010.40)
extern int projectionDistance;
}  // namespace VS::Render
