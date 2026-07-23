/* GTE differential/spec test harness.
 *
 * SCOPE HONESTY: with no PS1 hardware or emulator available, these tests check
 * the implementation against the documented hardware behaviour and against
 * independently derived expected values. They catch transcription errors and
 * regressions; they do NOT prove agreement with real silicon. The golden-vector
 * hook at the bottom exists so captured hardware/emulator traces can be dropped
 * in later and turn this into a true differential test.
 */
#include <psx_gte.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int checks = 0, failures = 0;

static void eq(const char* what, long long got, long long want)
{
    checks++;
    if (got != want) {
        failures++;
        printf("  FAIL %-28s got %lld (0x%llX) want %lld (0x%llX)\n",
               what, got, (unsigned long long)got, want, (unsigned long long)want);
    }
}

static void identity_matrix(void)
{
    /* 4096 == 1.0 in the GTE 1.3.12 fixed-point format */
    gte_set_ctrl(GTE_C_R11R12, (0u << 16) | (unsigned short)4096);
    gte_set_ctrl(GTE_C_R13R21, 0);
    gte_set_ctrl(GTE_C_R22R23, (unsigned short)4096);
    gte_set_ctrl(GTE_C_R31R32, 0);
    gte_set_ctrl(GTE_C_R33,    (unsigned short)4096);
    gte_set_ctrl(GTE_C_TRX, 0);
    gte_set_ctrl(GTE_C_TRY, 0);
    gte_set_ctrl(GTE_C_TRZ, 0);
}

static void set_v0(int x, int y, int z)
{
    gte_set_data(GTE_D_VXY0, ((unsigned)(y & 0xFFFF) << 16) | (unsigned)(x & 0xFFFF));
    gte_set_data(GTE_D_VZ0, (unsigned)(z & 0xFFFF));
}

/* 1. identity MVMVA must be a no-op on the vector */
static void test_mvmva_identity(void)
{
    printf("test: MVMVA identity\n");
    gte_reset(); identity_matrix(); set_v0(100, 200, 300);
    gte_op_mvmva(1, 0, 0, 3, 0);
    eq("MAC1", (int)gte_get_data(GTE_D_MAC1), 100);
    eq("MAC2", (int)gte_get_data(GTE_D_MAC2), 200);
    eq("MAC3", (int)gte_get_data(GTE_D_MAC3), 300);
    eq("FLAG", gte_get_ctrl(GTE_C_FLAG), 0);
}

/* 2. MVMVA with translation (cv=0) adds TR */
static void test_mvmva_translation(void)
{
    printf("test: MVMVA with translation\n");
    gte_reset(); identity_matrix();
    gte_set_ctrl(GTE_C_TRX, 10); gte_set_ctrl(GTE_C_TRY, 20); gte_set_ctrl(GTE_C_TRZ, 30);
    set_v0(1, 2, 3);
    gte_op_mvmva(1, 0, 0, 0, 0);
    eq("MAC1", (int)gte_get_data(GTE_D_MAC1), 11);
    eq("MAC2", (int)gte_get_data(GTE_D_MAC2), 22);
    eq("MAC3", (int)gte_get_data(GTE_D_MAC3), 33);
}

/* 3. RTPS end-to-end against independently computed expectations */
static void test_rtps_basic(void)
{
    long long n, sx, sy;
    printf("test: RTPS basic projection\n");
    gte_reset(); identity_matrix(); set_v0(100, 200, 300);
    gte_set_ctrl(GTE_C_H, 400);
    gte_set_ctrl(GTE_C_OFX, 0); gte_set_ctrl(GTE_C_OFY, 0);
    gte_set_ctrl(GTE_C_DQA, 0); gte_set_ctrl(GTE_C_DQB, 0);
    gte_op_rtps();

    eq("SZ3", gte_get_data(GTE_D_SZ3), 300);
    eq("IR1", (int)gte_get_data(GTE_D_IR1), 100);
    eq("IR2", (int)gte_get_data(GTE_D_IR2), 200);
    eq("IR3", (int)gte_get_data(GTE_D_IR3), 300);

    /* independently derived: n = (H*0x20000/SZ3 + 1)/2 */
    n  = ((400LL * 0x20000 / 300) + 1) / 2;
    sx = (n * 100) >> 16;
    sy = (n * 200) >> 16;
    eq("divide(n)", gte_divide(400, 300), n);
    eq("SX2", (short)(gte_get_data(GTE_D_SXY2) & 0xFFFF), sx);
    eq("SY2", (short)(gte_get_data(GTE_D_SXY2) >> 16), sy);
    eq("FLAG (no overflow)", gte_get_ctrl(GTE_C_FLAG), 0);
}

/* 4. divide overflow: SZ3 == 0 and H >= 2*SZ3 */
static void test_divide_overflow(void)
{
    printf("test: divide overflow flags\n");
    gte_reset(); identity_matrix(); set_v0(0, 0, 0);
    gte_set_ctrl(GTE_C_H, 1000);
    gte_op_rtps();
    eq("SZ3 == 0", gte_get_data(GTE_D_SZ3), 0);
    eq("DIV_OVF set", (gte_get_ctrl(GTE_C_FLAG) & GTE_FLAG_DIV_OVF) != 0, 1);
    eq("ERROR set", (gte_get_ctrl(GTE_C_FLAG) & GTE_FLAG_ERROR) != 0, 1);
    eq("divide()==0x1FFFF", gte_divide(1000, 0), 0x1FFFF);
    eq("h>=2*sz3 -> 0x1FFFF", gte_divide(1000, 400), 0x1FFFF);
}

/* 5. IR saturation raises the right FLAG bits */
static void test_ir_saturation(void)
{
    printf("test: IR saturation\n");
    gte_reset(); identity_matrix();
    set_v0(0x7FFF, 0x7FFF, 0x7FFF);
    gte_set_ctrl(GTE_C_TRX, 0x7FFFFF);
    gte_op_mvmva(1, 0, 0, 0, 0);
    eq("IR1 saturated to 0x7FFF", (short)gte_get_data(GTE_D_IR1), 0x7FFF);
    eq("IR1_SAT flag", (gte_get_ctrl(GTE_C_FLAG) & GTE_FLAG_IR1_SAT) != 0, 1);
}

/* 6. lm=1 clamps IR low bound to 0 instead of -0x8000 */
static void test_lm_clamp(void)
{
    printf("test: lm=1 lower clamp\n");
    gte_reset(); identity_matrix(); set_v0(-100, -100, -100);
    gte_op_mvmva(1, 0, 0, 3, 1);
    eq("IR1 clamped to 0", (short)gte_get_data(GTE_D_IR1), 0);
    gte_reset(); identity_matrix(); set_v0(-100, -100, -100);
    gte_op_mvmva(1, 0, 0, 3, 0);
    eq("IR1 keeps -100", (short)gte_get_data(GTE_D_IR1), -100);
}

/* 7. UNR reciprocal vs the ideal formula across a sweep */
static void test_divide_sweep(void)
{
    unsigned sz3, exact = 0, total = 0, maxerr = 0;
    printf("test: UNR divide sweep vs ideal\n");
    for (sz3 = 1; sz3 <= 0xFFFF; sz3 += 7) {
        unsigned h = sz3;               /* keep h < 2*sz3 so no overflow */
        unsigned long long ideal = ((unsigned long long)h * 0x20000 / sz3 + 1) / 2;
        unsigned got = gte_divide(h, sz3);
        unsigned err = (unsigned)(got > ideal ? got - ideal : ideal - got);
        total++;
        if (err == 0) exact++; else if (err > maxerr) maxerr = err;
    }
    printf("  %u/%u exact, max deviation %u\n", exact, total, maxerr);
    checks++;
    if (maxerr > 1) { failures++; printf("  FAIL UNR deviates by more than 1\n"); }
}

static const char* OUTNAME[11] = {
    "IR0","IR1","IR2","IR3","SXY2","SZ3","MAC0","MAC1","MAC2","MAC3","FLAG"
};

/* True differential test: replay hardware-captured inputs through our C GTE and
 * require every output word to match exactly. Vectors come from running
 * native/tests/psx/gte_capture.exe on emulated COP2 (DuckStation + real BIOS). */
static void test_golden_vectors(const char* path)
{
    FILE* f = fopen(path, "r");
    char line[2048];
    int ncase = 0, badcase = 0;

    if (!f) {
        printf("test: golden vectors -- none present (%s), skipped\n", path);
        return;
    }
    printf("test: golden vectors (differential vs real COP2)\n");

    while (fgets(line, sizeof line, f)) {
        unsigned w[27];
        int n = 0, i, bad = 0;
        uint32_t got[11];
        char* tok;

        if (line[0] == 35 || line[0] == 10) continue;   /* skip # and blank */
        for (tok = strtok(line, " \t\r\n"); tok && n < 27; tok = strtok(NULL, " \t\r\n"))
            w[n++] = (unsigned)strtoul(tok, NULL, 16);
        if (n != 27) continue;
        ncase++;

        for (i = 0; i < 5; i++) gte_set_ctrl(GTE_C_R11R12 + i, w[i]);
        gte_set_ctrl(GTE_C_TRX, w[5]);
        gte_set_ctrl(GTE_C_TRY, w[6]);
        gte_set_ctrl(GTE_C_TRZ, w[7]);
        gte_set_ctrl(GTE_C_OFX, w[8]);
        gte_set_ctrl(GTE_C_OFY, w[9]);
        gte_set_ctrl(GTE_C_H,   w[10]);
        gte_set_ctrl(GTE_C_DQA, w[11]);
        gte_set_ctrl(GTE_C_DQB, w[12]);
        gte_set_data(GTE_D_VXY0, w[13]);
        gte_set_data(GTE_D_VZ0,  w[14]);

        if (w[15] == 0) gte_op_rtps();
        else            gte_op_mvmva(1, 0, 0, 3, 0);

        got[0]  = gte_get_data(GTE_D_IR0);
        got[1]  = gte_get_data(GTE_D_IR1);
        got[2]  = gte_get_data(GTE_D_IR2);
        got[3]  = gte_get_data(GTE_D_IR3);
        got[4]  = gte_get_data(GTE_D_SXY2);
        got[5]  = gte_get_data(GTE_D_SZ3);
        got[6]  = gte_get_data(GTE_D_MAC0);
        got[7]  = gte_get_data(GTE_D_MAC1);
        got[8]  = gte_get_data(GTE_D_MAC2);
        got[9]  = gte_get_data(GTE_D_MAC3);
        got[10] = gte_get_ctrl(GTE_C_FLAG);

        for (i = 0; i < 11; i++) {
            checks++;
            if (got[i] != w[16 + i]) {
                if (!bad) printf("  case %d (%s):\n", ncase, w[15] ? "MVMVA" : "RTPS");
                bad = 1; failures++;
                printf("    %-5s got %08X  want %08X\n", OUTNAME[i], got[i], w[16 + i]);
            }
        }
        if (bad) badcase++;
    }
    fclose(f);
    printf("  %d cases, %d with mismatches\n", ncase, badcase);
}

int main(int argc, char** argv)
{
    printf("=== GTE spec tests ===\n");
    test_mvmva_identity();
    test_mvmva_translation();
    test_rtps_basic();
    test_divide_overflow();
    test_ir_saturation();
    test_lm_clamp();
    test_divide_sweep();
    test_golden_vectors(argc > 1 ? argv[1] : "native/tests/gte_golden.txt");
    printf("=== %d checks, %d failures ===\n", checks, failures);
    return failures ? 1 : 0;
}
