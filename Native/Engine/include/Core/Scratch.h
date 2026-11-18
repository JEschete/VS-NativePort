#pragma once

// The PS1's 1 KiB fast scratch memory, as ordinary memory owned by Core
// (PORTING_WATCHLIST.md §6). The copy tool rewrites getScratchAddr(n) to
// Core_ScratchAddr(n) and maps the game's scratch block to Core_ScratchBase(),
// so code that reaches the same bytes both ways still agrees. Each use is
// replaced by locals or module state when its function is modernized.

void* Core_ScratchBase(void);

#define Core_ScratchAddr(n) ((unsigned long*)Core_ScratchBase() + (n))
