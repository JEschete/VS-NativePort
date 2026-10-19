#include <string.h>

#include "Effects/Runtime.h"
#include "Effects/Timeline.h"
#include "Legacy/Psx.h"

// 5BF94.c:8442 used PS1 header-sized pointer arithmetic instead of indexing
// native tracks.
int Tests_EffectTimelineTrackStride(void) {
  union {
    unsigned long long alignment;
    unsigned char bytes[sizeof(Effects_EffectTimeline) +
                        2 * sizeof(Effects_TimelineTrack)];
  } storage;
  Effects_EffectInstance effect = {0};
  Effects_EffectTimeline* timeline = (void*)storage.bytes;
  u_long firstKey = 10;
  u_long secondKey = 20;
  memset(&storage, 0, sizeof storage);
  effect.context.timeline = timeline;
  timeline->trackCount = 2;
  timeline->activeTracks = 3;
  timeline->tracks[0].keys = &firstKey;
  timeline->tracks[1].keys = &secondKey;
  timeline->tracks[0].header.kind = timeline->tracks[1].header.kind = 2;
  timeline->tracks[0].header.keyCount = timeline->tracks[1].header.keyCount = 1;
  // Both sound keys lie in the future: no audio side effects, but both native
  // key pointers are read.
  Effects_TickEffectTimeline(&effect, 0);
  return timeline->frame == 1 && timeline->activeTracks == 3 &&
         timeline->tracks[0].keys == &firstKey &&
         timeline->tracks[1].keys == &secondKey &&
         timeline->tracks[0].keyIndex == 0 && timeline->tracks[1].keyIndex == 0;
}
