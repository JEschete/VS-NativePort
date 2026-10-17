// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Effects/Curves.h"
#include "Effects/Package.h"
#include "Effects/Particles.h"
#include "Effects/Runtime.h"
#include "Effects/Transforms.h"

// decomp: BATTLE.PRG func_800D1104 (src/BATTLE/BATTLE.PRG/5BF94.c:6314)
void Effects_SampleAllEffectCurves(int arg0)
{
    int i;
    Effects_CurveBlock* temp_a2 = Effects_package->curveBlock;

    for (i = 0; i < temp_a2->count; ++i) {
        Effects_curveSamples[i + 1] = Effects_package->curves[i][arg0 % temp_a2->curveSizes[i]];
    }
}

// decomp: BATTLE.PRG vs_battle_sampleCurve (src/BATTLE/BATTLE.PRG/5BF94.c:6324)
int Effects_SampleEffectCurve(int curveId, int distance)
{
    Effects_CurveBlock* block = Effects_package->curveBlock;

    if ((curveId != 0) && (block->count >= curveId)) {
        return Effects_package->curves[curveId - 1][distance % block->curveSizes[curveId - 1]];
    }

    return 0;
}

// decomp: BATTLE.PRG func_800D11F4 (src/BATTLE/BATTLE.PRG/5BF94.c:6335)
int Effects_SampleSegmentedEffectCurve(int arg0, int arg1)
{
    u_char* buf = Effects_package->block11Data;
    Effects_PackageBlock11* table = (Effects_PackageBlock11*)buf;
    int offset;
    int v;

    if (arg0 < table->count) {
        offset = *(arg0 + table->offsets);
        offset += buf[offset];
        while (buf[offset] != 0) {
            v = arg1 - buf[offset];
            if (v < 0) {
                break;
            }
            offset += 2;
            arg1 = v;
        }
        if (buf[offset] != 0) {
            return (buf[offset + 3] - buf[offset + 1]) * arg1 / buf[offset]
                 + buf[offset + 1];
        }
        if (arg1 == 0) {
            return buf[offset + 1];
        }
        return -1;
    }
    return 0;
}

// decomp: BATTLE.PRG func_800D12D8 (src/BATTLE/BATTLE.PRG/5BF94.c:6365)
int Effects_SegmentedEffectCurveDuration(int arg0)
{
    u_char* table = Effects_package->block11Data;
    int sum = 0;

    if (arg0 < *(int*)table) {
        int off = ((short*)table)[arg0 + 2];
        off += table[off];
        while (table[off] != 0) {
            sum += table[off];
            off += 2;
        }
        sum += 1;
    }
    return sum;
}

