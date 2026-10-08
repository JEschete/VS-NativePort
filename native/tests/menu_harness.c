/* Render-pipeline bring-up harness.
 *
 * Proves the native render pipeline end-to-end -- per-frame render-init ->
 * ordering table -> real game primitive builder -> rasteriser -> VRAM -- using
 * the game's OWN decompiled primitive builder (vs_battle_addTile) driven with
 * controlled data. This is the milestone the plan calls "first pixels": the
 * GTE->OT->raster->VRAM path, already unit-verified in isolation, now runs as
 * one connected system.
 *
 * The per-frame render-init contract is copied from the one fully-decompiled
 * example of it, ENDING.PRG func_8006B930 + _drawScreen:
 *
 *     temp = otBuffer[frame];
 *     D_1F800000[2] = temp + 8;      // tpage-list head
 *     D_1F800000[1] = temp + 0x48;   // ordering-table base (AddPrim target)
 *     ClearOTagR(temp, 0x822);       // build the reverse-linked empty OT
 *     D_1F800000[0] = primBuffer[frame];  // bump-allocated primitive store
 *     ... renderers AddPrim into the OT ...
 *     DrawOTag(D_1F800000[1] + 0x7FF);    // walk the OT tail-first
 *
 * D_1F800000 and _camera are ALIASED onto the scratchpad at 0x1F800000 (via
 * --defsym in Makefile.native, mirroring the retail config Makefiles), so
 * D_1F800000[i], *(void**)0x1F80000{0,4,8} and _camera.unk{0,4,8} are all the
 * same memory -- which is what lets the render-init actually take.
 *
 * NOTE ON DRIVING THE REAL MENU: vs_battle_execMenu now runs far enough to generate
 * real primitives, but vs_mainMenu_exec's init points each menuItem->text at
 * still-unharvested (zeroed) string data with no 0xFF terminator, so the glyph
 * loop in vs_battle_renderMenuItem runs away. Rendering the actual menu is
 * therefore gated on the data harvest (plan Phase 2 step 1 / caveat 2), NOT on
 * the render pipeline, which this harness demonstrates is working.
 */
#include <psx_gpu.h>
#include <psx_present.h>
#include <stdio.h>
#include <stdlib.h>

extern void vs_battle_execMenu(void);
extern char vs_battle_menuState;               /* [0] = currentState (char) */
extern void* vs_battle_menuItems;              /* pointer game allocs at init */
extern void* vs_battle_characterState;
extern void* vs_battle_rowTypeBuf;
static unsigned char items_buf[64 * 64];
static unsigned char charstate_buf[16 * 1024];
static unsigned char rowtype_buf[4 * 1024];
/* characterState->unk44 (a D_800F4538_unk0*, the equipped-weapon slot). Read by
 * func_800CABE0 -> ->weaponDrawn when the menu is opened; a zeroed block is
 * enough, but it must not be NULL. */
static unsigned char equipslot_buf[0x100];

/* Load the font + UI textures the way _loadSystemDat (INITBTL) does, but read
 * SYSTEM.DAT from the extracted file instead of the disc. Drives the real game
 * loaders (LoadImage, vs_main_loadClut, func_800CA9C0) so the font lands in
 * VRAM exactly where the menu's glyph sprites sample it (tpage 0x2D -> 832,0).
 * SYSTEM.DAT header = eight u32 offsets (unk0..unk1C); each texture block is an
 * 8-byte RECT then pixel data; unk8 is a CLUT; unk1C is the font buffer. */
extern int  LoadImage(RECT* r, void* data);
extern void vs_main_loadClut(void* clut, int a, int b, int x);
extern void func_800CA9C0(void* arg0);

static void load_system_dat(void)
{
    FILE* f = fopen("data/BATTLE/SYSTEM.DAT", "rb");
    long sz;
    unsigned char* buf;
    unsigned* off;
    RECT rect;
    short* h;
    int i;
    static const int tex[] = { 0, 1, 3, 4, 5 };    /* texture blocks */

    if (!f) { printf("[harness] SYSTEM.DAT not found; glyphs will be blank\n"); return; }
    fseek(f, 0, SEEK_END); sz = ftell(f); fseek(f, 0, SEEK_SET);
    buf = (unsigned char*)malloc(sz);
    if (fread(buf, 1, sz, f) != (size_t)sz) { fclose(f); return; }
    fclose(f);
    off = (unsigned*)buf;

    for (i = 0; i < (int)(sizeof tex / sizeof tex[0]); i++) {
        h = (short*)(buf + off[tex[i]]);
        rect.x = h[0]; rect.y = h[1]; rect.w = h[2]; rect.h = h[3];
        LoadImage(&rect, (unsigned*)h + 2);
    }
    h = (short*)(buf + off[2]);                     /* unk8: CLUT */
    vs_main_loadClut((unsigned*)h + 2, 0xF, 0, h[0]);
    h = (short*)(buf + off[6]);                     /* unk18: fixed dest */
    rect.x = 0x300; rect.y = 0xF0; rect.w = h[0]; rect.h = h[1];
    LoadImage(&rect, (unsigned*)h + 1);
    func_800CA9C0(buf + off[7]);                    /* unk1C: font -> (832,0) */
    printf("[harness] loaded SYSTEM.DAT textures (%ld bytes)\n", sz);
}

/* Item name / help / menu12 text. The real submenu CD-loads these contiguously
 * (ITEMNAME.BIN|ITEMHELP.BIN|MENU12.BIN) into one buffer; the harness's CD stub
 * marks the load complete without transferring data, so the name tables would be
 * zero and vs_mainMenu_setWeaponRow -> vs_battle_printf faults. Load the real
 * files up front and publish the pointers, so the submenu (which skips its own
 * load when vs_mainMenu_itemNames != NULL) enumerates real item names. */
extern unsigned short* vs_mainMenu_itemNames;
extern unsigned short* vs_mainMenu_itemHelp;
extern unsigned short* vs_mainMenu_menu12Text;

static void load_item_text(void)
{
    /* sizes from build/src/include/lbas.h */
    enum { NAME = 12288, HELP = 28672, MENU12 = 4096 };
    static unsigned char buf[NAME + HELP + MENU12] __attribute__((aligned(16)));
    static const struct { const char* path; long off, len; } parts[] = {
        { "data/MENU/ITEMNAME.BIN", 0,           NAME },
        { "data/MENU/ITEMHELP.BIN", NAME,        HELP },
        { "data/MENU/MENU12.BIN",   NAME + HELP, MENU12 },
    };
    int i;
    for (i = 0; i < 3; i++) {
        FILE* f = fopen(parts[i].path, "rb");
        if (!f) { printf("[harness] missing %s; item names blank\n", parts[i].path); return; }
        if (fread(buf + parts[i].off, 1, parts[i].len, f) != (size_t)parts[i].len)
            printf("[harness] short read on %s\n", parts[i].path);
        fclose(f);
    }
    vs_mainMenu_itemNames  = (unsigned short*)buf;
    vs_mainMenu_itemHelp   = (unsigned short*)(buf + NAME);
    vs_mainMenu_menu12Text = (unsigned short*)(buf + NAME + HELP);
    printf("[harness] loaded item name/help text (%d bytes)\n", NAME + HELP + MENU12);
}

/* Synthetic actor state (Phase 2.5). The equipment/status panel dereferences
 * vs_battle_actors[idx]->unk3C (a vs_battle_actor2 = the stat block). Zeroed
 * stats render as zeros, which is fine for bring-up; only the POINTER CHAIN
 * must be valid: vs_battle_actors[i] -> actor -> (unk3C @0x3C) -> actor2. */
extern void* vs_battle_actors[];               /* array of vs_battle_actor* */
static unsigned char actor_buf[0x100];         /* vs_battle_actor (~0x50)   */
static unsigned char actor2_buf[0x1000];       /* vs_battle_actor2 (~0x960) */

static void wire_synthetic_actor(void)
{
    int i;
    *(void**)(actor_buf + 0x3C) = actor2_buf;  /* actor->unk3C = stat block */
    for (i = 0; i < 16; i++)
        vs_battle_actors[i] = actor_buf;
}

extern int  ResetGraph(int mode);
extern DISPENV* SetDefDispEnv(DISPENV* e, int x, int y, int w, int h);
extern DRAWENV* SetDefDrawEnv(DRAWENV* e, int x, int y, int w, int h);
extern DISPENV* PutDispEnv(DISPENV* e);
extern DRAWENV* PutDrawEnv(DRAWENV* e);
extern unsigned long* ClearOTagR(unsigned long* ot, int n);
extern void DrawOTag(unsigned long* ot);
extern int  DrawSync(int mode);

/* Real game primitive builder: threads a flat TILE onto the OT slot *before,
 * bump-allocating from D_1F800000[0]. rgb0 packs the primitive code (0x60) in
 * its high byte and B,G,R in the low three. */
extern void vs_battle_addTile(unsigned long* before, int rgb0, int xy, int wh);

/* D_1F800000[0..2] = {prim-ptr, OT base, tpage list}. Aliased to 0x1F800000. */
extern unsigned long* D_1F800000[];

/* Frame buffers for the render-init. Static (not stack) so their addresses fit
 * the PS1 24-bit OT address fields -- see PSX_NOTES "the STACK is still high".
 * OT must hold at least 0x822 words; prim store mirrors the retail 0x20000. */
static unsigned long ot_buf[0x900];
static unsigned char prim_buf[0x20000] __attribute__((aligned(16)));

#define PACK(lo, hi) (((lo) & 0xFFFF) | ((hi) << 16))
#define TILE(code, r, g, b) (((code) << 24) | ((b) << 16) | ((g) << 8) | (r))

/* Replicates ENDING func_8006B930: lay out this frame's OT + primitive store
 * and publish the three pointers into the scratchpad. */
static void render_init(void)
{
    /* Clear the display area each frame (the real game redraws a full-screen
     * background; the harness has none, so a moving cursor would otherwise
     * leave a trail). The font page at VRAM (832,0) is outside 320x240, so it
     * is preserved. */
    RECT clr;
    extern int ClearImage(RECT*, unsigned char, unsigned char, unsigned char);
    clr.x = 0; clr.y = 0; clr.w = 320; clr.h = 240;
    ClearImage(&clr, 0, 0, 0);

    D_1F800000[2] = ot_buf + 2;      /* +8 bytes:    tpage-list head  */
    D_1F800000[1] = ot_buf + 18;     /* +0x48 bytes: OT base          */
    ClearOTagR(ot_buf, 0x822);
    D_1F800000[0] = (unsigned long*)prim_buf;
}

/* Count of non-black pixels in the 320x240 display area, as objective proof
 * that the pipeline actually wrote to VRAM. */
static int nonzero_pixels(void)
{
    uint16_t* vram = psx_vram();
    int x, y, n = 0;
    for (y = 0; y < 240; y++)
        for (x = 0; x < 320; x++)
            if (vram[y * 1024 + x]) n++;
    return n;
}

/* text-box render subsystem (vs_battle_renderTextBoxes) + its triple-buffered OT */
extern void vs_battle_renderTextBoxes(void);
extern unsigned char vs_battle_textBoxes[];
extern int VSync(int mode);

/* _cdQueue: 32 slots x 0x14, state (short) at offset 0. The real CD reader
 * (stubbed natively) advances Enqueued->Loaded; since submenu overlays are
 * already linked in, just mark queued slots Loaded so vs_battle_loadMenuPrg
 * completes. */
extern unsigned char _cdQueue[];

static void pump_cd_queue(void)
{
    int k;
    for (k = 0; k < 32; k++) {
        short* st = (short*)(_cdQueue + k * 0x14);
        if (*st != 0 && *st != 4) *st = 4;      /* -> Loaded */
    }
}

/* One frame of the menu: lay out the OT, run the menu driver and the text-box
 * pass, then walk the OT into VRAM. */
static void menu_frame(void)
{
    unsigned long* ot;

    psx_gpu_reset_stats();
    render_init();
    ot = D_1F800000[1];
    vs_battle_execMenu();
    /* The synthetic submenu re-sets the info message every frame, which resets
     * the info box's type-out counter (box->speed) to 1. Reveal it fully so the
     * whole message shows instead of one character. (Real gameplay sets the
     * message once per change.) vs_battle_textBoxes[7].speed is at 0x150+0x0C. */
    *(unsigned short*)(vs_battle_textBoxes + 0x150 + 0x0C) = 0xFFFF;
    vs_battle_renderTextBoxes();   /* text-box render pass (info box etc.) */
    DrawOTag(ot + 0x7FF);
    DrawSync(0);
}

/* Reopen the menu after it has been closed.
 *
 * Backing out of the top level (Cross), or Triangle from a submenu, closes the
 * menu: currentState walks 00 -> 1F -> 7F -> 3F and stays there. That is
 * correct game behaviour -- it hands control back to the game world -- but
 * there is no world yet, so the screen goes black and the window becomes
 * useless until restarted.
 *
 * AUTO-REOPEN IS NOT SAFE YET -- investigated and deliberately not shipped:
 *   - Restoring currentState alone leaves the next close stalled at 0x1F: the
 *     flags func_800CACD0 resets (shortcutInvoked, D_800F4FDB, D_800F4EA0,
 *     D_800F4E98) matter.
 *   - Calling func_800CACD0 directly faults, because closing NULLs
 *     vs_battle_menuItems and its `menuItems == 0` branch reallocates from the
 *     heap into state the harness only fakes. Re-wiring the buffers first makes
 *     it skip that branch and the reopen then succeeds (0x3F -> 0x00).
 *   - It also needed characterState->unk44 wired (func_800CABE0 reads
 *     ->weaponDrawn through it) -- that fix is kept, it is correct regardless.
 *   - But ~20 frames after a successful reopen the process dies inside SDL with
 *     a garbage jump, called from psx_present_pump. The reopened menu is
 *     scribbling over the C heap, which is where SDL's renderer lives. Root
 *     cause is in the menu's close/rebuild path against synthetic state, not in
 *     the presenter.
 * So: close the window cleanly instead, and say why. */

/* Interactive mode: a real game loop driven by a real controller.
 *
 * Rather than poking vs_main_buttonsPressed directly (what the scripted mode
 * below does), this registers the game's own pad buffers and calls the game's
 * own vs_main_processPadState() -- the native pad layer writes a genuine PS1
 * controller packet into those buffers every vblank, so held/pressed/released,
 * auto-repeat and the analog-stick-to-d-pad mapping are all computed by
 * untouched game code, exactly as on hardware. */
static void interactive_loop(void)
{
    extern int  vs_main_processPadState(void);
    extern char vs_main_padBuffer[2][34];
    extern void PadInitDirect(unsigned char*, unsigned char*);
    extern unsigned vs_main_buttonsPressed;
    unsigned long frames = 0;
    int shots = 0;
    char last_state = -1;
    /* Frame cap for automated runs (VS_MAX_FRAMES=N); 0 = run until quit. */
    const char* cap_env = getenv("VS_MAX_FRAMES");
    unsigned long cap = cap_env ? (unsigned long)atol(cap_env) : 0;

    PadInitDirect((unsigned char*)vs_main_padBuffer[0],
                  (unsigned char*)vs_main_padBuffer[1]);

    printf("[harness] interactive. D-pad/left stick or arrow keys navigate, "
           "Circle/Z confirms, Cross/X cancels,\n"
           "[harness] L1-R1 (Q/E) switch item category, ESC quits, "
           "F2 writes a screenshot.\n");
    fflush(stdout);

    /* This harness owns its loop and dumps a PPM after it, so it handles the
     * quit request itself rather than letting the pump exit the process. */
    psx_present_cooperative_quit(1);

    /* The display starts blanked, as on hardware, and the boot sequence is what
     * normally enables it. This harness skips the boot, so it has to say so. */
    SetDispMask(1);

    while (!psx_present_quit_requested()) {
        pump_cd_queue();
        vs_main_processPadState();
        if (vs_battle_menuState != last_state) {
            printf("[state] frame %lu: currentState %02X -> %02X\n",
                   frames, last_state & 0xFF, vs_battle_menuState & 0xFF);
            fflush(stdout);
            last_state = vs_battle_menuState;
        }
        if (vs_battle_menuState == 0x3F) {
            printf("[harness] menu closed (Triangle, or Cross at the top level)"
                   " -- the game would hand back to the world here, and there\n"
                   "[harness] isn't one yet. Re-run `make -f Makefile.native "
                   "window` to go again. See the reopen note above.\n");
            break;
        }
        /* Echo what the GAME decoded, not what we injected -- this is the
         * whole input chain (host -> PS1 packet -> vs_main_updatePadState ->
         * edge/repeat logic) reporting for itself. */
        if (vs_main_buttonsPressed) {
            printf("[input] frame %lu: buttonsPressed=%04X\n",
                   frames, vs_main_buttonsPressed & 0xFFFF);
            fflush(stdout);
        }
        menu_frame();
        if (psx_present_screenshot_requested()) {
            char path[64];
            snprintf(path, sizeof path, "native/build/shot%02d.ppm", ++shots);
            psx_vram_write_ppm(path, 0, 0, 320, 240);
            printf("[harness] wrote %s\n", path);
        }
        /* Paces to 60Hz, presents the frame we just drew, samples the host
         * input and republishes it into the pad buffer. */
        VSync(0);
        frames++;
        if (cap && frames >= cap) break;
    }
    psx_vram_write_ppm("native/build/window.ppm", 0, 0, 320, 240);
    printf("[harness] %lu frames; input source: %s; wrote native/build/window.ppm\n",
           frames, psx_pad_host_source());
}

/* Headless mode: the original fixed 130-frame script, kept verbatim so batch
 * runs and PPM regression comparisons behave exactly as before. */
static void scripted_loop(void)
{
    extern unsigned vs_main_buttonRepeat;
    extern unsigned vs_main_buttonsPressed;
    extern unsigned char D_800F4EE8[];   /* +0x3A = currentItemCategory */
    int frame;
    /* currentItemCategory to show (env VS_ITEM_CAT, default 1=weapons):
     * 1=weapon 4=shield 5=armor 6=gem 7=misc. In-list cursor steps (env
     * VS_ITEM_DOWN) issue N down-presses once the list is up. */
    const char* cat_env = getenv("VS_ITEM_CAT");
    const char* dn_env  = getenv("VS_ITEM_DOWN");
    int item_cat  = cat_env ? atoi(cat_env) : 1;
    int downsteps = dn_env  ? atoi(dn_env)  : 0;

    for (frame = 0; frame < 130; frame++) {
        pump_cd_queue();
        /* Slide-in settles ~frame 44. Walk down to ITEMS (tab 3) then
         * confirm to open the submenu (its category list + Ashley view). */
        vs_main_buttonRepeat = (frame == 45 || frame == 47 || frame == 49)
                             ? 0x4000u : 0u;                  /* down */
        vs_main_buttonsPressed = (frame == 55) ? 0x20u : 0u;  /* confirm */
        /* Once inside the ITEMS submenu, select the target category so the
         * item list enumerates the real loaded items. Default weapons. */
        if (frame >= 64) D_800F4EE8[0x3A] = (unsigned char)item_cat;
        /* After the list settles into its interactive state (~frame 100),
         * step the in-list cursor down N times with the D-pad (PADLdown
         * 0x4000 via buttonRepeat) to show row highlighting. (Face-button X
         * 0x40 is cancel/back, not cursor.) */
        if (downsteps > 0 && frame >= 100 && frame < 100 + downsteps * 4
            && ((frame - 100) % 4) == 0)
            vs_main_buttonRepeat = 0x4000u;
        fflush(stdout);
        menu_frame();
        printf("[harness]   frame %d: %d primitives, %d non-black pixels\n",
               frame, psx_gpu_prims_drawn(), nonzero_pixels());
    }

    {
        char path[64];
        snprintf(path, sizeof path, "native/build/menu_cat%s.ppm",
                 cat_env ? cat_env : "1");
        psx_vram_write_ppm(path, 0, 0, 320, 240);
        psx_vram_write_ppm("native/build/menu.ppm", 0, 0, 320, 240);
        printf("[harness] wrote %s\n", path);
    }
}

int main(void)
{
    static DISPENV disp;
    static DRAWENV draw;
    int windowed;

    printf("[harness] init graphics\n");
    ResetGraph(0);
    /* Open the host window first so it is up (and the pad bridge is accepting
     * connections) while the assets below load. */
    windowed = (psx_present_init("Vagrant Story (native)", 3) == 0);
    SetDefDispEnv(&disp, 0, 0, 320, 240);
    PutDispEnv(&disp);
    SetDefDrawEnv(&draw, 0, 0, 320, 240);
    PutDrawEnv(&draw);

    /* The game inits its heap at a hardcoded PS1 address (0x8010C000) that is
     * not mapped natively; give it a real native buffer instead. Must run
     * before anything calls vs_main_allocHeapR (e.g. the item-text loader). */
    {
        extern void vs_main_initHeap(void* node, unsigned size);
        static unsigned char heap_buf[0xF2000] __attribute__((aligned(16)));
        vs_main_initHeap(heap_buf, sizeof heap_buf);
        printf("[harness] heap initialised (%u bytes)\n", (unsigned)sizeof heap_buf);
    }

    printf("[harness] wiring synthetic game-state buffers\n");
    printf("[harness] loading font + UI textures from SYSTEM.DAT\n");
    load_system_dat();   /* func_800CA9C0 resets menu state + nulls menuItems */

    /* Wire synthetic state AFTER the reset above, or it gets clobbered. */
    vs_battle_menuItems = items_buf;
    vs_battle_characterState = charstate_buf;
    /* characterState->unk3C is the stat block (statuses @+0x948); the ITEMS
     * submenu reads it. Point it at the same synthetic actor2 stat block. */
    *(void**)(charstate_buf + 0x3C) = actor2_buf;
    *(void**)(charstate_buf + 0x44) = equipslot_buf;
    vs_battle_rowTypeBuf = rowtype_buf;
    wire_synthetic_actor();
    load_item_text();   /* real item name/help tables (for the item list) */

    /* Load the player's REAL inventory from their save card via VS's own
     * read+decode+apply (vs_native_loadSaveFile -> _applyLoadedSaveFile). The
     * card is native/saves/test_card.mcd (a copy of the user's DuckStation
     * card); slot 1, file 1 is BASLUS-01040VAG1. Falls back to a synthetic
     * inventory if the load fails, so the harness still runs without a card. */
    {
        extern int vs_native_loadSaveFile(int port, int file);
        extern unsigned char vs_main_inventory;
        unsigned char* inv = &vs_main_inventory;
        int rc = vs_native_loadSaveFile(0, 1);
        if (rc == 0) {
            /* count each category (present iff the id/blade field is nonzero) */
            int i, w = 0, sh = 0, ar = 0, gm = 0, ms = 0;
            for (i = 0; i < 8;  i++) if (inv[0x000 + i * 0x20 + 1]) w++;
            for (i = 0; i < 8;  i++) if (inv[0x100 + i * 0x20 + 1]) sh++;
            for (i = 0; i < 16; i++) if (inv[0x640 + i * 0x10])     ar++;
            for (i = 0; i < 48; i++) if (*(unsigned short*)&inv[0x8C0 + i * 8]) gm++;
            for (i = 0; i < 64; i++) if (*(unsigned short*)&inv[0xE00 + i * 8]) ms++;
            printf("[harness] loaded REAL save (BASLUS-01040VAG1): "
                   "%d weapons, %d shields, %d armor, %d gems, %d misc\n",
                   w, sh, ar, gm, ms);

            /* Save write-back round-trip (VS_SAVE_TEST=1). Change the first
             * misc item's quantity, save via VS's encode, corrupt the in-memory
             * copy, reload from the card, verify the change persisted, then
             * restore. Writes only the virtual card image (a copy). */
            if (getenv("VS_SAVE_TEST")) {
                extern int vs_native_saveSaveFile(int, int);
                unsigned char* qty = &inv[0xE00 + 2];   /* misc[0] quantity */
                unsigned char orig = *qty, want = (unsigned char)(orig + 7);
                int wr, rl;
                printf("[savetest] misc[0] qty %d -> %d, saving...\n", orig, want);
                *qty = want;
                wr = vs_native_saveSaveFile(0, 1);
                *qty = 0;                                 /* corrupt in-memory */
                rl = vs_native_loadSaveFile(0, 1);
                printf("[savetest] save rc=%d, reload rc=%d, misc[0] qty now %d "
                       "(expected %d) -> %s\n", wr, rl, *qty, want,
                       (*qty == want) ? "ROUND-TRIP OK" : "MISMATCH");
                *qty = orig;                              /* restore + persist */
                vs_native_saveSaveFile(0, 1);
                printf("[savetest] restored original (qty %d)\n", orig);
            }
        } else {
            int i;
            printf("[harness] save load failed (rc=%d); using synthetic inventory\n", rc);
            inv[0 * 0x20 + 1] = 0x11;
            inv[1 * 0x20 + 1] = 0x21;
            for (i = 0; i < 10; i++)
                *(unsigned short*)&inv[0xE00 + i * 8] = (unsigned short)(0x82 + i);
        }
    }

    /* vs_battle_getCurrentSceneId reads vs_battle_roomData.sectionB(+0x8C)
     * ->sceneId(+0x50); wire sectionB to a zeroed buffer so it reads 0. */
    {
        extern unsigned char vs_battle_roomData[];
        static unsigned char sectionB_buf[256];
        *(void**)(vs_battle_roomData + 0x8C) = sectionB_buf;
    }

    /* Enable menu tabs and build the tab list. menuFlags (settings+2) is the
     * enabled-menu bitmask; func_800C930C(2) rebuilds the items + starts the
     * slide-in. currentState=0 is the tab-navigating state. */
    {
        extern unsigned char vs_main_settings[];
        extern int func_800C930C(int);
        *(unsigned short*)(vs_main_settings + 2) = 0x03FF;   /* enable 10 tabs */
        vs_main_settings[8] = 1;   /* .information = on (else the submenu
                                    * dismisses its INFORMATION box) */
        func_800C930C(2);
    }

    extern void psx_gpu_set_ot_dump(int);
    psx_gpu_set_ot_dump(0);
    printf("[harness] opening equipment menu (currentState = 0, navigating)\n");
    (&vs_battle_menuState)[0] = 0;

    /* Drive the box open/close animation, and prime the text-box OT before the
     * first driver call (the driver clears it at the END of each frame for the
     * NEXT frame, so frame 0 needs a valid buffer already). */
    {
        extern unsigned long* D_800F51B8;
        extern unsigned char D_800EB9CE;
        extern unsigned char D_800F4CD0[];
        extern int vs_gametime_tickspeed;
        vs_gametime_tickspeed = 2;
        D_800EB9CE = 0;
        D_800F51B8 = (unsigned long*)D_800F4CD0;
        ClearOTagR(D_800F51B8, 0x22);
    }

    /* A window means a person is driving: run the real game loop against real
     * input. No window (no SDL / no display / VS_HEADLESS) falls back to the
     * scripted batch run that dumps a PPM. */
    if (windowed)
        interactive_loop();
    else
        scripted_loop();

    psx_present_shutdown();
    return 0;
}
