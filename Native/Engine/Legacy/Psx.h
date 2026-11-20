#pragma once

// Everything the copied decomp C needs from the PS1 SDK, in its transitional
// form (plan §2.2). Every copied file includes this; each piece shrinks as call
// sites are modernized and is deleted at zero uses (SDK_API_LEDGER.md).

#include "Core/Arena.hpp"
#include "Core/Diagnostics.hpp"
#include "Core/Gte.h"
#include "Core/RandomLive.hpp"
#include "Core/Scratch.hpp"
#include "Legacy/Audio/LibSpu.hpp"
#include "Legacy/Core/LibEtc.hpp"
#include "Legacy/Core/PsxTypes.h"
#include "Legacy/Input/LibPad.hpp"
#include "Legacy/LibDs.h"
#include "Legacy/OnDisc.h"
#include "Legacy/Render/LibGpu.h"
#include "Legacy/Save/LibCard.hpp"
