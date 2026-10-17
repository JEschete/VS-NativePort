#include "Core/Scratch.h"

namespace {
alignas(16) unsigned char g_scratch[1024];
}

extern "C" void* Core_ScratchBase(void)
{
    return g_scratch;
}
