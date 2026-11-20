// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.hpp"
#include "Render/Camera.hpp"

using VS::Core::Gte::MATRIX;

// decomp: vs_main_nearClip (vs_main_nearClip@SLUS_010.40)
int VS::Render::nearClip;

// decomp: D_8005E218 (D_8005E218@SLUS_010.40)
MATRIX VS::Render::cameraBasisMatrix;

// decomp: vs_main_projectionDistance (vs_main_projectionDistance@SLUS_010.40)
int VS::Render::projectionDistance;

