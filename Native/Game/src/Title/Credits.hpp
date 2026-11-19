#pragma once
// TITLE.PRG's publisher and developer logos, loaded from the module image.
#include <cstddef>
#include <cstdint>

namespace VS::Title {
inline constexpr std::size_t kDeveloperLogoBytes = 896;
// A 16-colour palette (32 bytes), then the logo's indexed pixels.
inline constexpr std::size_t kPublisherLogoBytes = 6176;

// decomp: developer (developer@TITLE/TITLE.PRG)
extern std::uint8_t developerLogo[kDeveloperLogoBytes];
// decomp: publisher_clut (publisher_clut@TITLE/TITLE.PRG)
extern std::uint8_t publisherLogo[kPublisherLogoBytes];
}  // namespace VS::Title
