#pragma once

namespace VS::Effects {
struct LiveRendererSlot;
// Each entry binds its independently verified original overlay. State remains
// owned by the renderer slot: stopping returns zero; the runtime frees it.
// Unknown legacy commands leave a valid slot untouched and return one.
int UpdateLivePlg005AnalyzeGrid(LiveRendererSlot* slot, unsigned int command,
                                int packed);
int UpdateLivePlg054AnalyzeGrid(LiveRendererSlot* slot, unsigned int command,
                                int packed);
int UpdateLivePlg055AnalyzeGrid(LiveRendererSlot* slot, unsigned int command,
                                int packed);
int UpdateLivePlg074AnalyzeGrid(LiveRendererSlot* slot, unsigned int command,
                                int packed);
}  // namespace VS::Effects
