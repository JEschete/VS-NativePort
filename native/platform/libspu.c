/* libspu / libsnd: silent SPU.
 *
 * Phase 3a (booting to the title menu) needs these to EXIST because _sysInit()
 * and the sound driver init call them, but it does not need them to make sound.
 * They are deliberately inert rather than aborting, so the boot sequence runs;
 * Phase 3b replaces this file with a real SPU (ADPCM decode + voice mixing +
 * reverb) behind an SDL audio backend.
 *
 * Tracked in docs/NATIVE_DEVIATIONS.md section B. Anything here that would
 * report state returns "idle/quiet" rather than 0-as-error, so the game's
 * wait-for-SPU loops terminate instead of spinning forever. */
#include <string.h>

/* SPU local memory is 512KB on hardware. Transfers are accepted and dropped;
 * keeping a real buffer would only matter once voices actually play. */
static unsigned spu_transfer_addr;

void SpuInit(void) { }
void SpuQuit(void) { }
int  SpuInitMalloc(int num, char* top) { (void)num; (void)top; return 0; }
long SpuMalloc(long size) { (void)size; return 0; }
void SpuFree(unsigned addr) { (void)addr; }

unsigned SpuSetTransferStartAddr(unsigned addr) { spu_transfer_addr = addr; return addr; }
unsigned SpuGetTransferStartAddr(void) { return spu_transfer_addr; }
int      SpuSetTransferMode(int mode) { (void)mode; return 0; }

/* Transfer-completion callback. On hardware the DMA finishes a moment later and
 * raises an interrupt; here the "transfer" is instantaneous, so the callback
 * fires at the end of the write.
 *
 * This is load-bearing, not bookkeeping: the sound driver does
 *     _isSpuTransfer = 1; SpuSetTransferCallback(_spuWriteComplete);
 * and then _waitTransferAvailable() spins `while (_isSpuTransfer == 1)`. With
 * no callback the boot hangs there forever, before the first frame. */
static void (*spu_transfer_cb)(void);

void* SpuSetTransferCallback(void (*func)(void))
{
    void* old = (void*)spu_transfer_cb;
    spu_transfer_cb = func;
    return old;
}

unsigned SpuWrite(unsigned char* addr, unsigned size)
{
    (void)addr;                       /* sample data is discarded until Phase 3b */
    if (spu_transfer_cb) spu_transfer_cb();
    return size;
}

unsigned SpuRead(unsigned char* addr, unsigned size)
{
    if (addr) memset(addr, 0, size);
    if (spu_transfer_cb) spu_transfer_cb();
    return size;
}

/* "Transfer finished" -- always, immediately. A never-completing transfer would
 * hang the loaders that poll it. */
int SpuIsTransferCompleted(int mode) { (void)mode; return 1; }

void SpuSetKey(int on_off, unsigned voice_bit) { (void)on_off; (void)voice_bit; }
int  SpuGetKeyStatus(unsigned voice_bit) { (void)voice_bit; return 0; }
void SpuSetCommonMasterVolume(short left, short right) { (void)left; (void)right; }
void SpuSetCommonCDVolume(short left, short right) { (void)left; (void)right; }
void SpuSetCommonCDMix(int on_off) { (void)on_off; }

int  SpuSetReverb(int on_off) { (void)on_off; return 0; }
int  SpuSetReverbModeType(int mode) { (void)mode; return 0; }
int  SpuGetReverbModeType(void) { return 0; }
void SpuSetReverbDepth(void* r_attr) { (void)r_attr; }
int  SpuReserveReverbWorkArea(int on_off) { (void)on_off; return 1; }
int  SpuClearReverbWorkArea(int mode) { (void)mode; return 1; }

unsigned SpuSetIRQ(unsigned on_off) { (void)on_off; return 0; }
unsigned SpuSetIRQAddr(unsigned addr) { (void)addr; return addr; }
void*    SpuSetIRQCallback(void (*func)(void)) { (void)func; return 0; }

void SpuStart(void) { }
void SpuSetVoiceAttr(void* attr) { (void)attr; }

/* libsnd (Ss*) -- the sequencer layer sitting on top of the SPU. */
void SsUtReverbOn(void) { }
void SsUtReverbOff(void) { }
int  SsUtSetReverbType(int type) { (void)type; return 0; }
void SsUtSetReverbDepth(short l, short r) { (void)l; (void)r; }
void SsSetMVol(short l, short r) { (void)l; (void)r; }
void SsInit(void) { }
void SsEnd(void) { }
void SsStart(void) { }
void SsQuit(void) { }
