#pragma once

namespace VS::Core {

// The PS1's 1 KiB fast scratch memory, as ordinary memory owned by Core
// (PORTING_WATCHLIST.md §6). Code that reached the original's scratchpad
// addresses the same bytes through this block, so callers that share them
// still agree. Each use is replaced by locals or module state when its
// function is modernized (EC-05).
[[nodiscard]] void* ScratchBase() noexcept;

}  // namespace VS::Core
