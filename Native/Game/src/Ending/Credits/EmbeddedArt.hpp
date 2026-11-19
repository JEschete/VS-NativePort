#pragma once

namespace VS::Ending::Credits {

struct TimHeader;

int PrepareEmbeddedArt();
TimHeader* EmbeddedArt(unsigned index);

}  // namespace VS::Ending::Credits
