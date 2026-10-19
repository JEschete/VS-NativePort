#pragma once
// Hand-maintained (tools/port/owned.csv).
#ifndef VS_OWNED_DISCREADER_H
#define VS_OWNED_DISCREADER_H
#include "DiscQueue.h"
#include "Legacy/Psx.h"

int Content_BeginDiscRead(int sector, u_int bytes, void* vram);

void Content_UpdateDiscCommands(void);

void Content_ResetDiscDrive(void);

#endif  // VS_OWNED_DISCREADER_H
