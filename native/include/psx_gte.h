/* Software implementation of the PS1 Geometry Transformation Engine (COP2).
 *
 * Register indices and operation semantics follow the documented hardware
 * behaviour (nocash psx-spx). Fixed-point widths, saturation limits and FLAG
 * bits are part of the contract: game code depends on the exact results,
 * including overflow behaviour, so this must be bit-exact -- not merely close.
 */
#ifndef NATIVE_PSX_GTE_H
#define NATIVE_PSX_GTE_H

#include <stdint.h>

/* ---- control registers (cop2c) ---- */
enum {
    GTE_C_R11R12 = 0, GTE_C_R13R21 = 1, GTE_C_R22R23 = 2,
    GTE_C_R31R32 = 3, GTE_C_R33 = 4,
    GTE_C_TRX = 5, GTE_C_TRY = 6, GTE_C_TRZ = 7,
    GTE_C_RFC = 21, GTE_C_GFC = 22, GTE_C_BFC = 23,   /* far colour */
    GTE_C_OFX = 24, GTE_C_OFY = 25, GTE_C_H = 26,
    GTE_C_DQA = 27, GTE_C_DQB = 28,
    GTE_C_ZSF3 = 29, GTE_C_ZSF4 = 30, GTE_C_FLAG = 31
};

/* ---- data registers (cop2d) ---- */
enum {
    GTE_D_VXY0 = 0, GTE_D_VZ0 = 1,
    GTE_D_RGBC = 6,
    GTE_D_RGB0 = 20, GTE_D_RGB1 = 21, GTE_D_RGB2 = 22,   /* colour FIFO */
    GTE_D_IR0 = 8, GTE_D_IR1 = 9, GTE_D_IR2 = 10, GTE_D_IR3 = 11,
    GTE_D_SXY0 = 12, GTE_D_SXY1 = 13, GTE_D_SXY2 = 14, GTE_D_SXYP = 15,
    GTE_D_SZ0 = 16, GTE_D_SZ1 = 17, GTE_D_SZ2 = 18, GTE_D_SZ3 = 19,
    GTE_D_MAC0 = 24, GTE_D_MAC1 = 25, GTE_D_MAC2 = 26, GTE_D_MAC3 = 27
};

/* FLAG (cop2c 31) bits actually produced by MVMVA/RTPS. */
#define GTE_FLAG_IR0_SAT      (1u << 12)
#define GTE_FLAG_SY2_SAT      (1u << 13)
#define GTE_FLAG_SX2_SAT      (1u << 14)
#define GTE_FLAG_DIV_OVF      (1u << 17)
#define GTE_FLAG_SZ3_SAT      (1u << 18)
#define GTE_FLAG_RGB_B_SAT    (1u << 19)
#define GTE_FLAG_RGB_G_SAT    (1u << 20)
#define GTE_FLAG_RGB_R_SAT    (1u << 21)
#define GTE_FLAG_IR3_SAT      (1u << 22)
#define GTE_FLAG_IR2_SAT      (1u << 23)
#define GTE_FLAG_IR1_SAT      (1u << 24)
#define GTE_FLAG_MAC3_NEG     (1u << 25)
#define GTE_FLAG_MAC2_NEG     (1u << 26)
#define GTE_FLAG_MAC1_NEG     (1u << 27)
#define GTE_FLAG_MAC3_POS     (1u << 28)
#define GTE_FLAG_MAC2_POS     (1u << 29)
#define GTE_FLAG_MAC1_POS     (1u << 30)
#define GTE_FLAG_ERROR        (1u << 31)   /* sticky: set if any of bits 30..23 */

typedef struct {
    uint32_t cop2d[32];
    uint32_t cop2c[32];
} gte_state_t;

gte_state_t* gte(void);
void gte_reset(void);

/* raw register access (mirrors ctc2/cfc2/lwc2/swc2/mfc2) */
void     gte_set_ctrl(int reg, uint32_t v);
uint32_t gte_get_ctrl(int reg);
void     gte_set_data(int reg, uint32_t v);
uint32_t gte_get_data(int reg);

/* operations */
void gte_op_rtps(void);
void gte_op_mvmva(int sf, int mx, int v, int cv, int lm);
void gte_op_dpcs(void);   /* depth-cue single colour */

/* exposed for testing: the hardware reciprocal used by RTPS */
uint32_t gte_divide(uint32_t h, uint32_t sz3);

#endif /* NATIVE_PSX_GTE_H */
