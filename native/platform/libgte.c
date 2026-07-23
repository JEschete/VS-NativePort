/* Native implementations of the PSYQ libgte entry points the port needs.
 *
 * Each mirrors the original hand-written MIPS exactly (see
 * build/src/SLUS_010.40/nonmatchings/libgte/). Offsets below were read off that
 * assembly, not guessed:
 *   SetRotMatrix   : ctc2 words 0..4 of MATRIX      (rotation)
 *   SetTransMatrix : ctc2 words 5..7 (MATRIX+0x14)  (translation)
 *   ApplyMatrix    : mvmva 1,0,0,3,0 -> MAC1..3
 *   RotTransPers   : rtps -> SXY2, IR0, FLAG; returns SZ3>>2
 */
#include <psx_gte.h>

/* PSYQ ABI types. Declared locally so this TU needs neither -nostdinc nor the
 * PSYQ header set; static asserts pin the layout the assembly assumes. */
typedef struct { short m[3][3]; long t[3]; } MATRIX;
typedef struct { short vx, vy, vz, pad; }    SVECTOR;
typedef struct { long  vx, vy, vz, pad; }    VECTOR;
typedef struct { short vx, vy; }             DVECTOR;

typedef char _gte_matrix_layout_check[
    (sizeof(MATRIX) == 0x20 && __builtin_offsetof(MATRIX, t) == 0x14) ? 1 : -1];

void SetRotMatrix(MATRIX* m)
{
    const unsigned long* w = (const unsigned long*)m;
    int i;
    for (i = 0; i < 5; i++) gte_set_ctrl(GTE_C_R11R12 + i, w[i]);
}

void SetTransMatrix(MATRIX* m)
{
    const unsigned long* w = (const unsigned long*)m;
    gte_set_ctrl(GTE_C_TRX, w[5]);
    gte_set_ctrl(GTE_C_TRY, w[6]);
    gte_set_ctrl(GTE_C_TRZ, w[7]);
}

VECTOR* ApplyMatrix(MATRIX* m, SVECTOR* v0, VECTOR* v1)
{
    const unsigned long* w = (const unsigned long*)m;
    const unsigned long* vv = (const unsigned long*)v0;
    int i;
    for (i = 0; i < 5; i++) gte_set_ctrl(GTE_C_R11R12 + i, w[i]);
    gte_set_data(GTE_D_VXY0, vv[0]);
    gte_set_data(GTE_D_VZ0,  vv[1]);
    gte_op_mvmva(1, 0, 0, 3, 0);
    v1->vx = (long)gte_get_data(GTE_D_MAC1);
    v1->vy = (long)gte_get_data(GTE_D_MAC2);
    v1->vz = (long)gte_get_data(GTE_D_MAC3);
    return v1;
}

long RotTransPers(SVECTOR* v0, DVECTOR* sxy, long* p, long* flag)
{
    const unsigned long* vv = (const unsigned long*)v0;
    gte_set_data(GTE_D_VXY0, vv[0]);
    gte_set_data(GTE_D_VZ0,  vv[1]);
    gte_op_rtps();
    *(unsigned long*)sxy = gte_get_data(GTE_D_SXY2);
    *p    = (long)gte_get_data(GTE_D_IR0);
    *flag = (long)gte_get_ctrl(GTE_C_FLAG);
    return (long)(gte_get_data(GTE_D_SZ3) >> 2);
}

/* ---- remaining libgte entry points ----
 * Semantics read from the original assembly:
 *   SetFarColor    : args <<4  -> ctc2 21,22,23 (RFC/GFC/BFC)
 *   SetGeomOffset  : args <<16 -> ctc2 24,25    (OFX/OFY, 1.15.16)
 *   SetGeomScreen  : arg       -> ctc2 26       (H)
 *   SetFogNear     : DQA = -(a*320)/h, DQB = 0x1400000
 *   rcos(a)        : falls through to rsin(a + 0x400)  (0x1000 = full circle)
 *   rsin(a)        : quadrant-folded lookup into _trig_table, 513 (sin,cos)
 *                    pairs covering 0..0x200 (0..45 degrees)
 */

extern const unsigned short _trig_table[];

void SetFarColor(long r, long g, long b)
{
    gte_set_ctrl(GTE_C_RFC, (unsigned long)(r << 4));
    gte_set_ctrl(GTE_C_GFC, (unsigned long)(g << 4));
    gte_set_ctrl(GTE_C_BFC, (unsigned long)(b << 4));
}

void SetGeomOffset(long ofx, long ofy)
{
    gte_set_ctrl(GTE_C_OFX, (unsigned long)(ofx << 16));
    gte_set_ctrl(GTE_C_OFY, (unsigned long)(ofy << 16));
}

void SetGeomScreen(long h)
{
    gte_set_ctrl(GTE_C_H, (unsigned long)h);
}

void SetFogNear(long a, long h)
{
    gte_set_ctrl(GTE_C_DQB, 0x1400000);
    gte_set_ctrl(GTE_C_DQA, (unsigned long)(-(a * 320) / h));
}

int rsin(int a)
{
    int v;

    if (a & 0x400) {
        if ((a & 0x3FF) == 0)
            return (a & 0x800) ? -0x1000 : 0x1000;
        a = 0x800 - a;
    }

    if (a & 0x200)
        v = _trig_table[(0x200 - (a & 0x1FF)) * 2 + 1];   /* cos column */
    else
        v = _trig_table[(a & 0x1FF) * 2];                 /* sin column */

    return (a & 0x800) ? -v : v;
}

int rcos(int a)
{
    return rsin(a + 0x400);
}

typedef struct { unsigned char r, g, b, cd; } CVECTOR;

/* DpqColor: load RGBC + IR0, run dpcs, read the colour FIFO head. */
void DpqColor(CVECTOR* v0, long p, CVECTOR* v1)
{
    gte_set_data(GTE_D_RGBC, *(const unsigned long*)v0);
    gte_set_data(GTE_D_IR0, (unsigned long)p);
    gte_op_dpcs();
    *(unsigned long*)v1 = gte_get_data(GTE_D_RGB2);
}
