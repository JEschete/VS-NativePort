// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_ENDING_CREDITS_H_TYPES
#define VS_ENDING_CREDITS_H_TYPES
#include "Legacy/Psx.h"

typedef struct Ending_Credits_Element Ending_Credits_Element;
typedef struct Ending_Credits_TexturedGradientQuadView Ending_Credits_TexturedGradientQuadView;
typedef struct Ending_Credits_TimHeader Ending_Credits_TimHeader;
typedef struct Ending_Credits_TimImageBlockHeader Ending_Credits_TimImageBlockHeader;

// decomp: easeMode (src/ENDING/ENDING.PRG/D4.c:81)
enum Ending_Credits_EasingMode {
    Ending_Credits_EasingMode_Linear,
    Ending_Credits_EasingMode_EaseOut,
    Ending_Credits_EasingMode_EaseInOut,
    Ending_Credits_EasingMode_EaseIn,
};
typedef enum Ending_Credits_EasingMode Ending_Credits_EasingMode;

// decomp: _creditsElement (src/ENDING/ENDING.PRG/D4.c:30)
typedef struct Ending_Credits_Element {
    void (*render)(struct Ending_Credits_Element*);
    u_char* textBytes;
    short phase;
    u_short animationFrame;
    u_short illustrationIndex;
    u_short width;
    int xFixed8;
    int yFixed8;
    int reserved18;
    int reserved1C;
    int reserved20;
    int reserved24;
    int reserved28;
    int reserved2C;
} Ending_Credits_Element;

// decomp: func_80069388_t (src/ENDING/ENDING.PRG/D4.c:64)
typedef struct Ending_Credits_TexturedGradientQuadView {
    int dmaTag;
    u_char vertexZeroRed;
    u_char vertexZeroGreen;
    u_char vertexZeroBlue;
    u_char command;
    short vertexZeroX;
    short vertexZeroY;
    short vertexZeroUv;
    short palette;
    u_char vertexOneRed;
    u_char vertexOneGreen;
    u_char vertexOneBlue;
    u_char pad13;
    short vertexOneX;
    short vertexOneY;
    short vertexOneUv;
    short texturePage;
    u_char vertexTwoRed;
    u_char vertexTwoGreen;
    u_char vertexTwoBlue;
    u_char pad1F;
    short vertexTwoX;
    short vertexTwoY;
    short vertexTwoUv;
    short pad26;
    u_char vertexThreeRed;
    u_char vertexThreeGreen;
    u_char vertexThreeBlue;
    u_char pad2B;
    short vertexThreeX;
    short vertexThreeY;
} Ending_Credits_TexturedGradientQuadView;
_Static_assert(sizeof(Ending_Credits_TexturedGradientQuadView) == 0x30, "Ending_Credits_TexturedGradientQuadView must keep its PS1 layout");

// decomp: func_8006A9C0_t (src/ENDING/ENDING.PRG/D4.c:70)
typedef struct Ending_Credits_TimImageBlockHeader {
    int blockSizeBytes;
    RECT uploadRect;
    char pixelBytes[0];
} Ending_Credits_TimImageBlockHeader;
_Static_assert(sizeof(Ending_Credits_TimImageBlockHeader) == 0xC, "Ending_Credits_TimImageBlockHeader must keep its PS1 layout");

// decomp: func_8006A9C0_t2 (src/ENDING/ENDING.PRG/D4.c:79)
typedef struct Ending_Credits_TimHeader {
    u_char identifier;
    u_char reserved01;
    u_char reserved02;
    u_char reserved03;
    int mode;
    Ending_Credits_TimImageBlockHeader firstBlock;
} Ending_Credits_TimHeader;
_Static_assert(sizeof(Ending_Credits_TimHeader) == 0x14, "Ending_Credits_TimHeader must keep its PS1 layout");

#endif // VS_ENDING_CREDITS_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_ENDING_CREDITS_H_DECLS)
#define VS_ENDING_CREDITS_H_DECLS

extern u_char Ending_Credits_scriptBytes[7040];
extern u_char Ending_Credits_glyphWidths[440244];
extern Ending_Credits_TimHeader Ending_Credits_embeddedTextureOne;
extern Ending_Credits_TimHeader Ending_Credits_embeddedTextureEight;
extern Ending_Credits_TimHeader Ending_Credits_embeddedTextureNine;
extern Ending_Credits_TimHeader Ending_Credits_embeddedTextureTen;
extern Ending_Credits_TimHeader Ending_Credits_embeddedTextureEleven;
extern Ending_Credits_TimHeader Ending_Credits_embeddedTextureTwelve;
extern Ending_Credits_TimHeader Ending_Credits_embeddedTextureTwo;
extern Ending_Credits_TimHeader Ending_Credits_embeddedTextureThree;
extern Ending_Credits_TimHeader Ending_Credits_embeddedTextureFive;
extern Ending_Credits_TimHeader Ending_Credits_embeddedTextureFour;
extern Ending_Credits_TimHeader Ending_Credits_embeddedTextureSix;
extern Ending_Credits_TimHeader Ending_Credits_embeddedTextureSeven;
extern u_char Ending_Credits_illustrationDecodePhase;
extern Ending_Credits_TimHeader* Ending_Credits_illustrationTimWorkspace;
extern short Ending_Credits_illustrationTexturePageX;
extern u_short Ending_Credits_illustrationPaletteY;
extern u_char* Ending_Credits_decodeBackreferenceCursor;
extern u_char* Ending_Credits_decodeInputCursor;
extern u_char* Ending_Credits_decodeOutputCursor;
extern u_char Ending_Credits_decodeFillByteZero;
extern u_char Ending_Credits_decodeFillByteOne;
extern u_short Ending_Credits_decodeRunLength;
extern u_short Ending_Credits_decodeBackreferenceOffset;
extern short Ending_Credits_decodeBudgetStartTick;
extern void* Ending_Credits_queuedCompressedImage;
extern Ending_Credits_TimHeader* Ending_Credits_queuedTimDestination;
extern void* Ending_Credits_illustrationResources[16];
extern int Ending_Credits_illustrationDiscLocations[];
extern int Ending_Credits_illustrationDiscSizes[];

#endif // VS_ENDING_CREDITS_H_DECLS
