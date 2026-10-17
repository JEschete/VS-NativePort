#include "Movie/Movie.hpp"

#include "Content/Disc.hpp"

namespace VS::Movie {

std::vector<MovieEntry> ListMovies(const Content::Disc& disc)
{
    std::vector<MovieEntry> out;
    for (const auto& f : disc.Files()) {
        if (!f.directory && f.path.ends_with(".STR")) {
            out.push_back({f.path, f.lba, (f.size + Content::Disc::SectorData - 1) / Content::Disc::SectorData});
        }
    }
    return out;
}

} // namespace VS::Movie
