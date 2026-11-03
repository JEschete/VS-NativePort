#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstddef>

struct Effects_PackageBlock3;

// Live binding for the P-file timeline lookups (PFile.hpp) over a bound
// package's timeline block. The block is bounded by the game heap payload
// that holds it (a loaded package) or by the built-in package
// (Effects_emptyPackage). A lookup the block does not hold, a negative index
// or an unbounded block is refused with a "content" warning and returns null.
namespace VS::Content {

std::byte* PFileLiveTimelineSection(Effects_PackageBlock3* block, int section);
std::byte* PFileLiveTimelineTrackData(Effects_PackageBlock3* block, int section,
                                      int track);

}  // namespace VS::Content
