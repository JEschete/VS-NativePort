/* Native reimplementation of the battle/menu text-box render subsystem.
 *
 * These are INCLUDE_ASM (MIPS-only) in 5BF94.c, so the native build stubs them.
 * They render every active text box (vs_battle_textBoxes[0..7]) into a
 * triple-buffered ordering table (D_800F4CD0 / D_800F51B8) which is then linked
 * into the main OT via AddPrims. The info box (slot 7) is what the ITEMS submenu
 * shows as "Choose items to equip."
 *
 * Translated from:
 *   build/src/BATTLE/BATTLE.PRG/nonmatchings/5BF94/func_800CB83C.s  (driver)
 *   build/src/BATTLE/BATTLE.PRG/nonmatchings/5BF94/func_800CD158.s  (box frame)
 * Native-functional (PSX build keeps the assembly).
 *
 * func_800CD3E4 (the 566-instr dialogue/menu-box frame, box states 2/7) is NOT
 * needed for the info box (state 4 -> func_800CD158) and is left as a safe
 * no-op; see docs/NATIVE_DEVIATIONS.md.
 */
#include <stdint.h>

typedef unsigned long  u_long;
typedef unsigned char  u_char;
typedef unsigned short u_short;

/* vs_battle_textBox is 0x30 bytes; access by explicit offset to mirror the asm.
 *   0x00 int   flags   (low nibble = render state, bits4-5 = easing type)
 *   0x14 short x, 0x16 short y          (target position)
 *   0x18 short centerX, 0x1A short centerY (start / collapsed position)
 *   0x1C short state                    (animation counter)
 *   0x20 u8 charsPerLine, 0x21 u8 lineCount, 0x22 u8 animDuration
 *   0x24..0x2A short  animated render rect (written here, read by CD158)
 */
extern u_char  vs_battle_textBoxes[];
extern int     vs_gametime_tickspeed;
extern u_char  D_800F4E80;            /* current box index (read by _printVariableWidthFont) */
extern u_long* D_1F800000[];
extern u_long* D_800F51B8;            /* current frame's text-box OT base */
extern u_char  D_800EB9CE;            /* text-box OT triple-buffer frame index */
extern u_char  D_800F4CD0[];          /* triple-buffered text-box OT storage */
extern u_char  vs_battle_lowerScreenUiState;
extern u_char  D_8005046C[];

extern int     rsin(int a);
extern void    AddPrims(u_long* ot, u_long* first, u_long* last);
extern u_long* ClearOTag(u_long* ot, int n);
extern void    _printVariableWidthFont(void* box);
extern void    _printFixedWidthFont(void* box, int arg1);
extern void    _renderTextImmediate(void* box, int arg1);
extern void    func_800CD158(int boxIdx);
extern void    func_800CD3E4(int boxIdx);
extern void    _renderTimer(void* arg);
extern void    func_800CB708(void);
extern int     vs_battle_keystreamBits(int a);

#define BOX(off)   (vs_battle_textBoxes + (off))
#define SH(p)      (*(short*)(p))
#define SB(p)      (*(u_char*)(p))
#define IW(p)      (*(int*)(p))

void func_800CB83C(void)
{
    int boxIdx;
    int off = 0x150;   /* byte offset of box 7 (7 * 0x30) */

    for (boxIdx = 7; boxIdx >= 0; boxIdx--, off -= 0x30) {
        u_char* box = BOX(off);
        int s3 = SH(box + 0x1C);        /* animation counter (signed short) */
        int v1, s0, s2, state;

        if (s3 == 0) continue;          /* inactive */
        v1 = IW(box + 0x0);
        if (v1 < 0) continue;           /* inactive */

        D_800F4E80 = (u_char)boxIdx;

        s0 = vs_gametime_tickspeed >> 1;
        s2 = SB(box + 0x22);            /* animation duration */

        if (s3 < 0) {
            /* closing animation */
            s3 -= s0;
            if (s3 < -s2) { SH(box + 0x1C) = 0; continue; }
            s0 = ((s2 + s3 + 1) << 16) / s2;
        } else {
            /* opening / open animation, easing selected by (flags>>4)&3 */
            int type;
            s3 += s0;
            if (s2 < s3) s3 = s2 + 1;
            type = (v1 >> 4) & 3;
            s0 = s3 - 1;                /* default easing = linear step */
            if (type == 2) {
                if (s0 < s2 - 1) {
                    s0 = (s0 << 16) / (s2 - 2);
                } else {
                    s0 = (s0 == s2) ? 0x10000 : 0x11000;
                }
            } else if (type < 3) {      /* 0 or 1: sinusoidal ease */
                s0 = rsin((s0 << 10) / s2) << 4;
            } else if (type == 3) {
                if (s2 >= 3) {
                    if (s0 < s2 - 1) {
                        s0 = ((s0 * 3) << 14) / (s2 - 2) + 0x6000;
                    } else {
                        s0 = (s0 == s2) ? 0x10000 : 0x11000;
                    }
                }
                /* s2 < 3: keep s0 = s3 - 1 */
            }
        }

        /* interpolate the animated render rect by factor s0 (.16 fixed) */
        {
            int inv = 0x10000 - s0;
            SH(box + 0x1C) = (short)s3;
            SH(box + 0x24) = (short)((SH(box + 0x14) * s0 + SH(box + 0x18) * inv) >> 16);
            SH(box + 0x26) = (short)((SH(box + 0x16) * s0 + SH(box + 0x1A) * inv) >> 16);
            SH(box + 0x28) = (short)(((12 * SB(box + 0x20) + 10) * s0) >> 16);
            SH(box + 0x2A) = (short)(((13 * SB(box + 0x21) +  4) * s0) >> 16);
        }

        /* dispatch on render state (jtbl_800699A8) */
        state = v1 & 0xF;
        {
            int drewFrame_fallthrough = 0;   /* whether we reach the text check */
            switch (state) {
            case 0:                       /* L800CBA88 */
                if (s3 >= 0) { drewFrame_fallthrough = 1; break; }
                _renderTextImmediate(box, s0);
                continue;
            case 6:                       /* L800CBAB0 -> _renderTextImmediate */
                _renderTextImmediate(box, s0);
                continue;
            case 4:                       /* L800CBA98 -> box frame (info box) */
                func_800CD158(boxIdx);
                if (s3 < 0) continue;
                drewFrame_fallthrough = 1;
                break;
            case 2:                       /* L800CBAC4 -> dialogue/menu box frame */
            case 7:
                func_800CD3E4(boxIdx);
                drewFrame_fallthrough = 1;   /* falls into L800CBACC/D0 */
                break;
            case 1:                       /* L800CBACC */
            case 5:
                drewFrame_fallthrough = 1;
                break;
            case 3:                       /* L800CBAF8 -> nothing */
            default:
                continue;
            }

            if (drewFrame_fallthrough) {
                /* L800CBAD0: text if the box is fully open, else fixed-width */
                if (s3 == s2 + 1) {
                    _printVariableWidthFont(box);
                } else if (s3 < 0) {
                    _printFixedWidthFont(box, s0);
                }
            }
        }
    }

    /* link the text-box OT into the main OT, then flip + clear for next frame */
    AddPrims(D_1F800000[1] - 6, D_800F51B8, (u_long*)((u_char*)D_800F51B8 + 0x84));

    D_800EB9CE = (D_800EB9CE == 2) ? 0 : (u_char)(D_800EB9CE + 1);
    D_800F51B8 = (u_long*)(D_800F4CD0 + 0x88 * D_800EB9CE);
    ClearOTag(D_800F51B8, 0x22);

    if (vs_battle_lowerScreenUiState == 1)
        _renderTimer(D_8005046C);
    func_800CB708();
    vs_battle_keystreamBits(0);
}

/* func_800CD3E4: dialogue/menu-box frame (box states 2 & 7). Not used by the
 * info box (state 4). Safe no-op until a dialogue/menu box screen is brought
 * up and can be diffed against DuckStation. See docs/NATIVE_DEVIATIONS.md. */
void func_800CD3E4(int boxIdx) { (void)boxIdx; }

/* func_800CD158: draws the info box's background frame (state 4) -- the shaded
 * bar behind the info text, built from per-segment edge sprites + a tile fill.
 * The info-box TEXT already renders without it (via _printVariableWidthFont);
 * this is the decorative backing. Deferred (safe no-op) because its 146-instr
 * two-sprites-per-segment GP0 build needs a DuckStation diff to verify pixel
 * placement. See docs/NATIVE_DEVIATIONS.md. */
void func_800CD158(int boxIdx) { (void)boxIdx; }
