// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Effects/Curves.h"
#include "Effects/Package.h"
#include "Effects/Particles.h"
#include "Effects/Runtime.h"
#include "Effects/Transforms.h"

// decomp: BATTLE.PRG func_800D1104 (src/BATTLE/BATTLE.PRG/5BF94.c:6314)
void Effects_SampleAllEffectCurves(int sample_distance)
{
    int i;
    Effects_CurveBlock* curve_block = Effects_package->curveBlock;

    for (i = 0; i < curve_block->count; ++i) {
        Effects_curveSamples[i + 1] = Effects_package->curves[i][sample_distance % curve_block->curveSizes[i]];
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
int Effects_SampleSegmentedEffectCurve(int curve_index, int segment_distance)
{
    u_char* buf = Effects_package->block11Data;
    Effects_PackageBlock11* table = (Effects_PackageBlock11*)buf;
    int offset;
    int v;

    if (curve_index < table->count) {
        offset = *(curve_index + table->offsets);
        offset += buf[offset];
        while (buf[offset] != 0) {
            v = segment_distance - buf[offset];
            if (v < 0) {
                break;
            }
            offset += 2;
            segment_distance = v;
        }
        if (buf[offset] != 0) {
            return (buf[offset + 3] - buf[offset + 1]) * segment_distance / buf[offset]
                 + buf[offset + 1];
        }
        if (segment_distance == 0) {
            return buf[offset + 1];
        }
        return -1;
    }
    return 0;
}

// decomp: BATTLE.PRG func_800D12D8 (src/BATTLE/BATTLE.PRG/5BF94.c:6365)
int Effects_SegmentedEffectCurveDuration(int curve_index)
{
    u_char* table = Effects_package->block11Data;
    int sum = 0;

    if (curve_index < *(int*)table) {
        int off = ((short*)table)[curve_index + 2];
        off += table[off];
        while (table[off] != 0) {
            sum += table[off];
            off += 2;
        }
        sum += 1;
    }
    return sum;
}

}  // extern "C"
