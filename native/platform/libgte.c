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
