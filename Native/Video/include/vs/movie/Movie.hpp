#pragma once

#include "vs/core/Types.hpp"

#include <string>
#include <vector>

namespace VS::Content {
class Disc;
}

namespace VS::Movie {

// A streamed FMV on the disc (MOV\*.STR: MDEC video + XA audio, interleaved).
struct MovieEntry {
    std::string path;
    u32 lba = 0;
    u32 sectors = 0;
};

std::vector<MovieEntry> ListMovies(const Content::Disc& disc);

} // namespace VS::Movie
