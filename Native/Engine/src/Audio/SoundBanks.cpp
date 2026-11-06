// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Audio/SoundBanks.h"
#include "DiscQueue.h"

// decomp: _sfxData (_sfxData@SLUS_010.40)
u_short Audio_menuSoundEffects[11264];

// decomp: vs_main_soundData (vs_main_soundData@SLUS_010.40)
Audio_SoundState Audio_soundState;

// decomp: _soundFileMap (_soundFileMap@SLUS_010.40)
char Audio_waveBankFileMap[] = { 0, 65, 66, 66, 68, 69, 67, 85, 68, 93, 69, 68, 0, 69, 69, 0, 69,
    69, 69, 69, 69, 68, 62, 61, 68, 68, 68, 68, 68, 57, 56, 72, 74, 77, 76, 86, 87, 80,
    88, 55, 70, 81, 70, 73, 75, 78, 89, 90, 79, 84, 10, 10, 10, 10, 11, 11, 11, 11, 12,
    12, 12, 12, 13, 13, 13, 13, 14, 14, 15, 15, 16, 16, 17, 17, 18, 18, 18, 18, 19, 19,
    19, 19, 20, 20, 21, 21, 21, 21, 22, 22, 22, 22, 23, 23, 23, 23, 24, 24, 25, 25, 25,
    25, 0, 0, 0, 0, 0, 85, 0, 0, 91, 91, 91, 91, 91, 91, 91, 0, 60, 94, 98, 95, 96, 97,
    97, 82, 83, 0, 0, 59, 58, 72, 74, 77, 76, 86, 87, 80, 88, 0, 0, 0, 0, 0, 0, 78, 89,
    90, 79, 84, 0, 99, 99, 99, 99, 92, 64, 64, 64, 54, 53, 53, 53, 53, 53, 63, 63, 63, 52,
    51, 50, 52, 49, 47, 46, 45, 45, 45, 44, 44, 43, 42, 41, 40, 39, 38, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0 };

}  // extern "C"
