#pragma once

// Everything the copied decomp C needs from the PS1 SDK, in its transitional form (plan §2.2). Every copied file
// includes this; each piece shrinks as call sites are modernized and is deleted at zero uses (SDK_API_LEDGER.md).

#include "Legacy/Core/PsxTypes.h"
#include "Core/Gte.h"
#include "Core/BiosRand.h"
#include "Core/Scratch.h"
#include "Core/Unported.h"
#include "Legacy/Core/LibEtc.h"
#include "Legacy/Removed.h"
#include "Legacy/Render/LibGpu.h"
#include "Legacy/Input/LibPad.h"
#include "Legacy/Audio/LibSpu.h"
#include "Legacy/Movie/LibPress.h"
#include "Legacy/Save/LibCard.h"
#include "Legacy/LibDs.h"
