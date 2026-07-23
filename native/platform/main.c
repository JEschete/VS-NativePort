/* Phase 0 entry point. The port does not run yet -- this exists to prove the
 * native target links and to give Phase 1+ somewhere to start. */
#include <stdio.h>
int main(void)
{
    printf("Vagrant Story - native target (Phase 0 scaffolding)\n");
    printf("Menu/title subset linked. No platform layer implemented yet.\n");
    printf("Calling into the game will hit an UNIMPLEMENTED stub by design.\n");
    return 0;
}
