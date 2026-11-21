#pragma once
// The location and zone tables in SLUS_010.40's data: the zone and room each
// location id names, the music each zone expects and each zone file's
// original disc extent. Locations.cpp defines them; the module image fills
// the first two at start-up.

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "Content/CdDrive.hpp"
#include "Field/RoomLoading.hpp"
#include "Field/RoomState.hpp"

namespace VS::Field {
inline constexpr auto kZoneTableEntries = static_cast<std::size_t>(kZoneCount);

// The module image copies the original byte pairs straight into the table.
static_assert(sizeof(LocationDestination) == 2 * sizeof(std::uint8_t));
static_assert(std::is_trivially_copyable_v<LocationDestination>);

// The zone and room each location id names.
// decomp: vs_main_locationZoneMapIds (src/SLUS_010.40/main.c:618,
// vs_main_locationZoneMapIds@SLUS_010.40)
extern std::array<LocationDestination, kLocationCount> locationDestinations;
// The music a zone expects, compared with the playing track to decide whether
// it must fade.
// decomp: vs_main_zndMusicMap (src/SLUS_010.40/main.c:714,
// D_8004FCCC@SLUS_010.40)
extern std::array<std::uint8_t, kZoneTableEntries> zoneMusicIds;
// Each zone file's original sector and byte count, until zones load by asset
// id (#47). The executable's vs_main_CdFile records: 8 bytes each, the stride
// the module symbol map (ModulePointers.cpp, 0x8004F4CC) relocates.
struct ZoneDiscFile {
  std::int32_t sector = 0;
  std::int32_t bytes = 0;
};
static_assert(sizeof(ZoneDiscFile) == 2 * sizeof(std::int32_t));
using ZoneDiscFiles = std::array<ZoneDiscFile, kZoneTableEntries>;
// decomp: vs_main_zndFiles (src/SLUS_010.40/main.c:670,
// vs_main_zndFiles@SLUS_010.40)
extern ZoneDiscFiles legacyZoneDiscFiles;
}  // namespace VS::Field
