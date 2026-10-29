#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Ending_Credits_TimHeader Ending_Credits_TimHeader;

int Ending_Credits_PrepareEmbeddedArt(void);
Ending_Credits_TimHeader* Ending_Credits_EmbeddedArt(unsigned index);

#ifdef __cplusplus
}
#endif
