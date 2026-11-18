#pragma once

struct Ending_Credits_TimHeader;

namespace VS::Ending::Credits {

int PrepareEmbeddedArt();
Ending_Credits_TimHeader* EmbeddedArt(unsigned index);

}  // namespace VS::Ending::Credits
