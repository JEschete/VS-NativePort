#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstddef>
#include <cstdint>

struct Save_GameSaveImage;

// Binds the memory-card save cores (GameFiles.hpp) to the libapi card calls,
// the card globals both save screens share and the game state, in the shapes
// the copied MENU7.PRG and TITLE.PRG callers use. The save workspace is the
// two images at the start of Title_memoryCardUiResource and the slot
// summaries are Title_saveFileSummaries; a missing buffer is reported and
// fails the call.
namespace VS::Save::MemoryCard {
// A NUL-terminated card path. Each builder reuses its own buffer, so one of
// each can be live at once (a rename passes both).
char* BuildLiveSaveFilename(int channel, int file_number);
char* BuildLiveTemporaryFilename(int channel, int file_number);
// NewestSaveSlot over Title_saveFileSummaries; 0 without summaries.
int NewestLiveSaveSlot();
// The matching slot plus one, or 0.
int FindLiveMatchingSaveSlot();
// n for save file n, -n for its temporary file, 0 for any other name. Reads
// at most a directory name's 20 characters.
int ParseLiveSaveFileNumber(const char* name);
// Nonzero when a checksum fails or `block_count` is outside 0..92. Reads
// max(block_count, 2) blocks of `image`.
int HasLiveChecksumError(const Save_GameSaveImage* image, int block_count);
void DecodeLiveSaveBytes(std::uint32_t key, void* bytes, std::size_t count);
// Over the directory in Title_cardDirectoryEntries: the mask of the
// temporary files erased, or 0x80 when an erase failed.
int DeleteLiveRedundantTemporaryFiles(int channel);
// Lists `port`'s directory into Title_cardDirectoryEntries and fills the
// slot summaries; nonzero when a redundant temporary file could not be
// erased.
int ScanLiveSaveDirectory(int port);
// 0, or -1 when the file could not be created.
int CreateLiveSaveFile(int port, int file_number);
// `begin_port` nonzero restarts for that port; a Save_CardReadinessResult.
int AdvanceLiveCardReadiness(int begin_port);
// 0 when the incoming image is valid (and, when `apply`, applied); else 1.
int ValidateAndApplyLiveLoadedSave(int apply);
// `target_file` is zero-based.
void PackageLiveGameSave(int target_file);
// 0 while running, 1 when done, -1 when given up.
int ReadLiveSaveStep(int request);
int WriteLiveSaveStep(int request);
// 0 while the snapshot loads; 1 once the step has finished.
int LoadLiveIntroSnapshotStep(int action);
// The title's keystream, which also encodes the clear-game save.
std::uint32_t NextLiveTitleCipherBits(int bits);
void PackageLiveClearSave(int target_file);
}  // namespace VS::Save::MemoryCard
