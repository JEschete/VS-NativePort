// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Movie/Intro.h"
#include "Movie/Intro/Playback.h"

// decomp: StCdIntrFlag (StCdIntrFlag@SLUS_010.40)
int Movie_legacyStreamInterruptFlag;

// decomp: _decodedDataBuf0 (_decodedDataBuf0@src/TITLE/TITLE.PRG/22C.c)
void* Movie_Intro_legacyDecodedStripZero;

// decomp: _decodedDataBuf1 (_decodedDataBuf1@src/TITLE/TITLE.PRG/22C.c)
void* Movie_Intro_legacyDecodedStripOne;

// decomp: _dslMode (_dslMode@src/TITLE/TITLE.PRG/22C.c)
int Movie_Intro_legacyDiscStreamingMode;

// decomp: _encodedDataBuf0 (_encodedDataBuf0@src/TITLE/TITLE.PRG/22C.c)
void* Movie_Intro_legacyEncodedFrameZero;

// decomp: _encodedDataBuf1 (_encodedDataBuf1@src/TITLE/TITLE.PRG/22C.c)
void* Movie_Intro_legacyEncodedFrameOne;

// decomp: _introMovieLoc (_introMovieLoc@src/TITLE/TITLE.PRG/22C.c)
DslLOC Movie_Intro_legacyDiscStreamLocation;

// decomp: _movieData (_movieData@src/TITLE/TITLE.PRG/22C.c)
Movie_Intro_MdecState Movie_Intro_legacyMdecState;

// decomp: _movieRingBuf (_movieRingBuf@src/TITLE/TITLE.PRG/22C.c)
u_long* Movie_Intro_legacyDiscRingBuffer;

// decomp: _vlcTable (_vlcTable@src/TITLE/TITLE.PRG/22C.c)
DECDCTTAB Movie_Intro_legacyVlcDecodeTable;

