#pragma once

typedef struct Ending_Credits_TimHeader Ending_Credits_TimHeader;

int Ending_Credits_PrepareEmbeddedArt();
Ending_Credits_TimHeader* Ending_Credits_EmbeddedArt(unsigned index);
