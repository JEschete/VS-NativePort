#pragma once

namespace VS::Movie {
// The game's movies (TITLE.STR, MOV\*.STR) for copied and owned C code.
// VS.Extract converts each STR into a Matroska file (AV1 video, FLAC audio)
// under the extracted data; the player decodes it and writes the picture into
// the page space where the original's MDEC output went, and plays the
// soundtrack through the audio device.

// Opens the converted movie for a disc path ("MOV/TITLE.STR") and starts its
// soundtrack. Returns 0 if it is not available (logged); the other calls then
// do nothing.
int PlayerOpen(const char* disc_path);
// Draws the frame due `ticks` (1/60 s) after the start as 24-bit pixels at (x,
// y) in the page space. Returns 0 once the picture has ended.
int PlayerDrawFrame(int ticks, int x, int y);
// Stops picture and soundtrack.
void PlayerClose();
}  // namespace VS::Movie
