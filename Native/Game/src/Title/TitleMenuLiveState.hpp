#pragma once
// Hand-maintained (tools/port/owned.csv).
// The title's legacy visit counter, which the module pointer table maps for
// the copied code. TitleMenuLive.cpp defines it; the typed title reads and
// writes it only through its host (Title/TitleMenu.hpp Host::VisitCount).

namespace VS::Title {

// decomp: vs_main_titleScreenCount (vs_main_titleScreenCount@SLUS_010.40):
// the title screens shown since start-up; zero shows the publisher logos.
extern int titleScreenVisitCount;

}  // namespace VS::Title
