/* Demand-driven menu bring-up harness.
 *
 * Drives the real per-frame equipment-menu function (func_800C97BC) so the
 * stub layer's UNIMPLEMENTED aborts reveal exactly which functions the render
 * path touches, in call order. Each abort is the next decomp target.
 *
 * Not a test with a pass/fail; it is a probe. It prints how far it got and
 * dumps VRAM to native/build/menu.ppm.
 */
#include <psx_gpu.h>
#include <stdio.h>

extern void func_800C97BC(void);
extern int  ResetGraph(int mode);
extern DISPENV* SetDefDispEnv(DISPENV* e, int x, int y, int w, int h);
extern DRAWENV* SetDefDrawEnv(DRAWENV* e, int x, int y, int w, int h);
extern DISPENV* PutDispEnv(DISPENV* e);
extern DRAWENV* PutDrawEnv(DRAWENV* e);

/* First byte of the menu-state struct is currentState (char). */
extern char vs_battle_menuState;

/* Pointer-globals the game normally allocates during battle init. The stub
 * layer backs each as a zeroed array (null pointer), so the harness supplies
 * real buffers -- the "synthetic game state" the plan calls for. Declared
 * void* so the assignment writes a pointer into the symbol's storage, which is
 * exactly what the game code loads and dereferences. */
extern void* vs_battle_menuItems;
extern void* vs_battle_characterState;
extern void* vs_battle_rowTypeBuf;

static unsigned char items_buf[64 * 64];       /* generous: menuItem_t is 64B */
static unsigned char charstate_buf[16 * 1024];
static unsigned char rowtype_buf[4 * 1024];

int main(void)
{
    static DISPENV disp;
    static DRAWENV draw;
    int frame;

    printf("[harness] init graphics\n");
    ResetGraph(0);
    SetDefDispEnv(&disp, 0, 0, 320, 240);
    PutDispEnv(&disp);
    SetDefDrawEnv(&draw, 0, 0, 320, 240);
    PutDrawEnv(&draw);

    printf("[harness] wiring synthetic game-state buffers\n");
    vs_battle_menuItems = items_buf;
    vs_battle_characterState = charstate_buf;
    vs_battle_rowTypeBuf = rowtype_buf;

    printf("[harness] opening equipment menu (currentState = 1)\n");
    (&vs_battle_menuState)[0] = 1;

    for (frame = 0; frame < 3; frame++) {
        printf("[harness] frame %d -> func_800C97BC()\n", frame);
        fflush(stdout);
        func_800C97BC();
    }

    printf("[harness] survived %d frames without aborting\n", frame);
    psx_vram_write_ppm("native/build/menu.ppm", 0, 0, 320, 240);
    printf("[harness] wrote native/build/menu.ppm\n");
    return 0;
}
