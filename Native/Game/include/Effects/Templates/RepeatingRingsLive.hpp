#pragma once

#include "Effects/Templates/RingCore.hpp"

namespace VS::Effects {
struct LiveRendererSlot;
}  // namespace VS::Effects

namespace VS::Effects::Templates {
using LiveRingInstance = bool (*)(RingCommand, std::int32_t,
                                  std::span<const RingPaletteEntry>, RingHost&);

// Shared RingsLive host entry. It preserves current package, transform,
// animation, primitive-buffer and slot-runtime ownership bindings.
int RunLiveRing(LiveRendererSlot* slot, unsigned int command, int packed,
                std::span<const RingPaletteEntry> palette,
                LiveRingInstance instance);

// Checked bindings for the independent closed-ring overlays. Missing current
// definition/curve/transform state refuses the tick, never supplies a zero
// curve. A create requires an empty slot; stop/expiry leave freeing to the
// renderer-chain owner. Texture refusal retains the existing no-quad tick
// contract, including child emission and lifetime advancement.
int UpdateLivePlg052RepeatingRing(LiveRendererSlot* slot, unsigned int command,
                                  int packed);
int UpdateLivePlg077RepeatingRing(LiveRendererSlot* slot, unsigned int command,
                                  int packed);
int UpdateLivePlg162TransposedRing(LiveRendererSlot* slot, unsigned int command,
                                   int packed);
int UpdateLivePlg236TransposedRing(LiveRendererSlot* slot, unsigned int command,
                                   int packed);
int UpdateLivePlg263TransposedRing(LiveRendererSlot* slot, unsigned int command,
                                   int packed);
int UpdateLivePlg300RepeatingRing(LiveRendererSlot* slot, unsigned int command,
                                  int packed);
int UpdateLivePlg300ForegroundRepeatingRing(LiveRendererSlot* slot,
                                            unsigned int command, int packed);
}  // namespace VS::Effects::Templates
