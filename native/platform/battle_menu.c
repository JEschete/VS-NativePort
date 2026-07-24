/* Native reimplementation of func_800C930C -- the main-menu tab input/build
 * handler (still MIPS asm in 5BF94.c, so stubbed in the native build). This is
 * the demand-driven blocker for menu navigation: without it, currentState=0
 * (the tab-navigating state) aborts.
 *
 * Translated from build/src/BATTLE/BATTLE.PRG/nonmatchings/5BF94/func_800C930C.s.
 * Native-functional (not byte-matching); the PSX build keeps the assembly.
 *
 *   arg0 == 2 : (re)build the menu items from the enabled-menu bitmask
 *               (vs_main_settings.menuFlags), start the slide-in (D_800F4CC0<0).
 *   arg0 == 0 : per frame -- while D_800F4CC0<0 animate the slide-in, else read
 *               input (confirm/cancel/up/down), move the cursor, return the
 *               selected menu (+1) on confirm, -1 on cancel, else 0.
 */
#include <stdint.h>

/* --- externs from the game (all real in the native build) --- */
extern void*         vs_battle_menuItems;        /* menuItem[]; stride 0x40 */
extern unsigned char vs_battle_submenuStates[];
extern unsigned char vs_battle_rowAnimationSteps[];
extern unsigned short vs_battle_menuStrings[];
extern unsigned char vs_battle_stringContext[];
extern unsigned char vs_battle_textBoxes[];
extern unsigned char D_800EBA78[];

extern int  D_800F4CC0;                           /* slide-in / cursor counter */
extern unsigned D_800F4EA0;                       /* per-menu enable flags */
extern unsigned vs_main_buttonRepeat;
extern unsigned vs_main_buttonsState;

/* vs_main_settings: byte1 = cursorMemory, u16@2 = menuFlags (enabled bitmask) */
extern unsigned char vs_main_settings[];
/* vs_main_buttonsPressed: .all at offset 0 */
extern unsigned vs_main_buttonsPressed;

extern void vs_battle_rMemzero(void* p, int n);
extern void* vs_battle_setMenuItem(int row, int x, int y, int w, int bgw, char* text);
extern int  vs_battle_getCurrentSceneId(void);
extern int  vs_battle_drawCursor(int animStep, int position);
extern void vs_battle_initInformationTextBox(int flags);
extern int  vs_battle_setTextBox(int a, char* b);
extern void vs_battle_playMenuSelectSfx(void);
extern void vs_battle_playMenuLeaveSfx(void);
extern void vs_battle_playMenuChangeSfx(void);

#define ITEM(i)      ((unsigned char*)vs_battle_menuItems + ((i) << 6))
#define ENABLED(m,i) (((m) >> (i)) & 1)

/* Next enabled menu index at or after `from`, wrapping mod 10. */
static int next_enabled(unsigned mask, int from)
{
    int i = from;
    while (!ENABLED(mask, i % 10)) i = (i % 10) + 1;
    return i % 10;
}

int func_800C930C(int arg0)
{
    unsigned char* items = (unsigned char*)vs_battle_menuItems;
    int cur = vs_battle_submenuStates[0];
    unsigned mask = *(unsigned short*)(vs_main_settings + 2);   /* menuFlags */
    short* infoBox = (short*)(vs_battle_textBoxes + 0x16C);
    int i;

    if (arg0 != 0) {
        /* ---- rebuild (arg0 == 1 or 2) ---- */
        if (arg0 == 2 && vs_main_settings[1] == 0)   /* cursorMemory */
            cur = 0;
        if (!ENABLED(mask, cur))
            cur = next_enabled(mask, cur + 1);

        vs_battle_rMemzero(vs_battle_menuItems, 0xB24);
        vs_battle_submenuStates[0] = (unsigned char)cur;
        {
            int id = 9;
            for (i = 0; i < 10; i++) {
                if (ENABLED(mask, i)) ITEM(i)[3] = (unsigned char)id++;
            }
            D_800F4CC0 = -id;
        }
        return 0;
    }

    /* ---- arg0 == 0: per-frame ---- */
    if (D_800F4CC0 < 0) {
        /* slide-in animation: rebuild each item at its animated x */
        int y = 0x12;
        D_800F4CC0++;
        for (i = 0; i < 10; i++) {
            if (ENABLED(mask, i)) {
                unsigned char* it = ITEM(i);
                int cnt = it[3];
                if (cnt != 0) it[3] = (unsigned char)(--cnt);
                {
                    int x = vs_battle_rowAnimationSteps[cnt & 0xFF] + 0xC2;
                    char* text = (char*)vs_battle_menuStrings
                               + vs_battle_menuStrings[i] * 2;
                    unsigned f = D_800F4EA0;
                    vs_battle_setMenuItem(i, x, y, 0x7E, 0, text);
                    it[0x07] = (f & 0xB7) != 0;
                    it[0x47] = (f & 0x15F) != 0;
                    it[0xC7] = (f & 0x07) != 0;
                    it[0x147] = vs_battle_getCurrentSceneId() < 1;
                    y += 0x10;
                }
            }
        }
        return 0;
    }

    /* input handling */
    {
        unsigned bp = vs_main_buttonsPressed;
        if (bp & 0x20) {                       /* confirm */
            unsigned char* it = ITEM(cur);
            vs_battle_playMenuSelectSfx();
            it[6] = 1; it[2] = 1;
            *infoBox = 0;
            return cur + 1;
        }
        ITEM(cur)[6] = 0;
        if (vs_main_buttonsPressed & 0x50) {   /* cancel */
            vs_battle_playMenuLeaveSfx();
            *infoBox = 0;
            return -1;
        }
        if (vs_main_buttonRepeat & 0x1000)     /* down */
            cur = next_enabled(mask, cur + 9);
        if (vs_main_buttonRepeat & 0x4000)     /* up */
            cur = (cur + 1) % 10;
        if (!ENABLED(mask, cur))
            cur = next_enabled(mask, cur + 1);

        if (cur != vs_battle_submenuStates[0]) {
            vs_battle_playMenuChangeSfx();
            vs_battle_submenuStates[0] = (unsigned char)cur;
        }
        {
            unsigned char* it = ITEM(cur);
            int y = *(unsigned short*)(it + 0x16);
            it[6] = 1;
            D_800F4CC0 = vs_battle_drawCursor(D_800F4CC0, ((y << 16) >> 20) - 1);
            if (it[7] != 0) {                  /* this tab shows an info box */
                int a0v, s0;
                unsigned f;
                if (vs_main_buttonsState & 0x5000) return 0;
                vs_battle_initInformationTextBox(1);
                *infoBox = 0xB;
                vs_battle_setTextBox(7, (char*)D_800EBA78);
                a0v = (cur == 0) ? 0xB7 : (cur == 1) ? 0x15F : (cur == 3) ? 0x7 : 0;
                f = D_800F4EA0 & a0v;
                for (s0 = 0; s0 < 9 && !((f >> s0) & 1); s0++) {}
                *(unsigned*)(vs_battle_stringContext + 0x28) =
                    (unsigned)((char*)vs_battle_menuStrings
                               + vs_battle_menuStrings[s0 + 0xC] * 2);
                return 0;
            }
            *infoBox = 0;
        }
        return 0;
    }
}

/* func_800AAD4C: queries a character/text data slot (D_800F4538[a0]).
 * Returns -1 when the slot is null -- which is always true in the synthetic
 * menu harness (no character data loaded), so this matches the real behaviour
 * for our state. Replace with the full decomp once data slots are populated. */
int func_800AAD4C(int a0, int a1, int a2, int a3)
{
    extern void* D_800F4538[];
    (void)a1; (void)a2; (void)a3;
    return D_800F4538[a0] ? 0 : -1;
}
