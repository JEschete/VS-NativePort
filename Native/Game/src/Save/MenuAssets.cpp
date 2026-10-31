// Hand-maintained (tools/port/owned.csv).
extern "C" {
#include "Save/MenuAssets.h"

#include "Battle/Feedback.h"
#include "Boot/Lifecycle.h"
#include "CdDrive.h"
#include "Content/DiscLayout.h"
#include "Core/Memory/Heap.h"
#include "DiscQueue.h"
#include "Menu/Files.h"
#include "Render/Font.h"
#include "Render/TextureAnimation.h"
#include "Render/Textures.h"
#include "Save/MemoryCard/GameFiles.h"
#include "Save/MemoryCard/Resources.h"
#include "Title/SaveCard.h"
#include "Title/SaveFiles.h"

enum AssetPhase { kAssetImage, kAssetTextQueue, kAssetText };
enum { kSaveImageCount = 3, kSaveTexturePage = 0x1D, kSaveTextureOwner = 6 };
static enum AssetPhase phase;
static Content_DiscQueueSlot* request;

// _initMemcard: MENU7.PRG/260.c:1080 and TITLE.PRG/22C.c:940.
// Retain the original font/text assets, not the card BIOS event transport.
static int InitializeAssets(int init, int menu) {
  Content_DiscFile file;
  if (init) {
    if (menu) {
      Render_ReserveTexturePageRange(kSaveTexturePage, kSaveImageCount,
                                     kSaveTextureOwner);
    }
    Title_memoryCardUiResource = reinterpret_cast<char*>(
        Core_Memory_AllocateFromLowEnd(VS_SPMCIMG_BIN_SIZE));
    Save_MemoryCard_uiResources = reinterpret_cast<Save_CardUiResources*>(
        Title_memoryCardUiResource +
        sizeof(Save_GameSaveImage) * kSaveImageCount);
    if (menu) {
      Menu_Files_textTable = Save_MemoryCard_uiResources->textTable;
    } else {
      Title_saveMenuText = Save_MemoryCard_uiResources->textTable;
    }
    Title_saveFileSummaries = Save_MemoryCard_uiResources->fileHeaders;
    Title_cardDirectoryEntryBuffer =
        &Save_MemoryCard_uiResources->directoryEntry;
    file.lba = VS_SPMCIMG_BIN_LBA;
    file.size = VS_SPMCIMG_BIN_SIZE;
    request = Content_AllocateDiscRequest(&file);
    Content_EnqueueDiscRequest(request, Title_memoryCardUiResource);
    phase = kAssetImage;
    return 0;
  }
  if (phase == kAssetImage) {
    if (request->state == Content_DiscRequestState_Loaded) {
      Content_ReleaseDiscRequest(request);
      Render_UploadPackedRectangle(
          vs_getXY(800, 256), Title_memoryCardUiResource, vs_getWH(224, 256));
      phase = kAssetTextQueue;
    }
    return 0;
  }
  if (phase == kAssetTextQueue) {
    file.lba = VS_MCDATA_BIN_LBA;
    file.size = VS_MCDATA_BIN_SIZE + VS_MCMAN_BIN_SIZE;
    request = Content_AllocateDiscRequest(&file);
    Content_EnqueueDiscRequest(request, Save_MemoryCard_uiResources);
    phase = kAssetText;
  }
  if (request->state != Content_DiscRequestState_Loaded) return 0;
  Content_ReleaseDiscRequest(request);
  Boot_SetSoftResetEnabled(0);
  return 1;
}

static void ReleaseAssets(int menu) {
  Boot_SetSoftResetEnabled(1);
  Core_Memory_FreeFromLowEnd(Title_memoryCardUiResource);
  Title_memoryCardUiResource = nullptr;
  Save_MemoryCard_uiResources = nullptr;
  if (menu) {
    Render_RetireTexturePageRange(kSaveTexturePage, kSaveImageCount,
                                  kSaveTextureOwner);
  }
}

int Save_MemoryCard_InitializeMenuResourcesStep(int init) {
  return InitializeAssets(init, 1);
}

// _shutdownMemcard: MENU7.PRG/260.c:1161, TITLE.PRG/22C.c:1017.
void Save_MemoryCard_ReleaseMenuResources(void) { ReleaseAssets(1); }

int Save_MemoryCard_InitializeResourcesStep(int init) {
  return InitializeAssets(init, 0);
}

void Save_MemoryCard_ReleaseResources(void) { ReleaseAssets(0); }
}  // extern "C"
