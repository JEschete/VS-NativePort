// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_ENDING_CREDITS_STAFFROLL_H_TYPES
#define VS_ENDING_CREDITS_STAFFROLL_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_ENDING_CREDITS_STAFFROLL_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_ENDING_CREDITS_STAFFROLL_H_DECLS)
#define VS_ENDING_CREDITS_STAFFROLL_H_DECLS
#include "Ending/Credits.h"

extern u_short Ending_Credits_illustrationHoldFrames[16];
extern int Ending_Credits_ripplePhaseAngle;
extern int Ending_Credits_rippleScaleFrame;
extern u_char Ending_Credits_rippleDistanceGrid[4][16];
extern short Ending_Credits_illustrationBrightness;
extern Ending_Credits_Element Ending_Credits_elements[32];
extern u_char Ending_Credits_hasFinished;
extern u_char Ending_Credits_isBackgroundFading;
extern u_char Ending_Credits_wasSkipRequested;
extern int Ending_Credits_elapsedFrames;
extern int Ending_Credits_scriptDelayFrames;
extern u_char* Ending_Credits_scriptCursor;
extern short Ending_Credits_scrollSpeedFixed8;
extern u_short Ending_Credits_backgroundBrightness;
extern u_int Ending_Credits_xaResumeFrameCount;
void Ending_Credits_DrawPhantomPainRipple(Ending_Credits_Element* element);
void Ending_Credits_DrawFinCaption(Ending_Credits_Element* element);
void Ending_Credits_DrawScrollingTitleLogo(Ending_Credits_Element* element);
int Ending_Credits_DistanceFromScreenCenter(int x, int y);
void Ending_Credits_ApplyRadialVertexFade(Ending_Credits_TexturedGradientQuadView* quad, short fade_step, short fade_out);
void Ending_Credits_DrawIllustration(Ending_Credits_Element* element);
int Ending_Credits_MeasureTextWidth(u_char* str);
void Ending_Credits_DrawScrollingText(Ending_Credits_Element* element);
void Ending_Credits_DrawScrollingCopyright(Ending_Credits_Element* element);
void Ending_Credits_DrawBackground(Ending_Credits_Element* element __attribute__((unused)));
void Ending_Credits_FadeInBackground(Ending_Credits_Element* element);
void Ending_Credits_FadeOutBackground(Ending_Credits_Element* element);
void Ending_Credits_FadeOutForSkip(Ending_Credits_Element* element);
void Ending_Credits_RunStaffRoll(void);
void Ending_Credits_ConfigureDrawEnvironment(DRAWENV* drawenv);
void Ending_Credits_RunFrames(void);
void Ending_Credits_UploadTexture(Ending_Credits_TimHeader* tim);
void Ending_Credits_ResetElements(void);
int Ending_Credits_DrawElements(void);
Ending_Credits_Element* Ending_Credits_AddElement(void (*renderer)(Ending_Credits_Element*));
Ending_Credits_Element* Ending_Credits_ReserveMainElement(void (*renderer)(Ending_Credits_Element*));
void Ending_Credits_ExecuteScript(void);
void Ending_Credits_QueueIllustrationDecode(Ending_Credits_TimHeader* tim_destination, void* compressed_image);
int Ending_Credits_DecodeIllustrationStep(void);
void Ending_Credits_DrawSmallText(short x, short y, int palette_row_offset, u_char* text);
void Ending_Credits_InitializeDisplay(int screen_width, int screen_height, int projection_distance, int background_red, int background_green, int background_blue);
int Ending_Credits_PresentFrame(u_long* ordering_table);
void Ending_Credits_AllocateRenderBuffers(void);
void Ending_Credits_BeginFrame(void);
void Ending_Credits_StartMusic(void);
void Ending_Credits_WaitForMusicEnd(void);

#endif // VS_ENDING_CREDITS_STAFFROLL_H_DECLS
