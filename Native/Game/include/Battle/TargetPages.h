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

void Battle_ResetTargetPages();
int Battle_AdvanceTargetPage(int direction);
int Battle_HandleTargetPageButtons();
int Battle_TargetPagesUseShoulders();
unsigned char Battle_MapTargetPageIndex(unsigned char index);
int Battle_ValidateTargetPreviewIndex(int index);
int Battle_TargetPreviewIndexIsValid(int index);
Battle_TargetPageInfo Battle_GetTargetPageInfo();
void Battle_DrawTargetPageHint();
