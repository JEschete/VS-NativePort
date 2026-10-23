#pragma once

typedef struct Battle_TargetPreview Battle_TargetPreview;

typedef struct Battle_TargetPageInfo {
  int total;
  int page;
  int pages;
  int first;
  int visible;
  int selected;
} Battle_TargetPageInfo;

#ifdef __cplusplus
extern "C" {
#endif

void Battle_SetSphereTargets(int count, Battle_TargetPreview* entries,
                             int mode);
void Battle_ResetTargetPages(void);
int Battle_AdvanceTargetPage(int direction);
int Battle_HandleTargetPageButtons(void);
int Battle_TargetPagesUseShoulders(void);
unsigned char Battle_MapTargetPageIndex(unsigned char index);
int Battle_ValidateTargetPreviewIndex(int index);
int Battle_TargetPreviewIndexIsValid(int index);
Battle_TargetPageInfo Battle_GetTargetPageInfo(void);
void Battle_DrawTargetPageHint(void);

#ifdef __cplusplus
}
#endif
