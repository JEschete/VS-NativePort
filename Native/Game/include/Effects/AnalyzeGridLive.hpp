#pragma once

struct Effects_RendererSlot;

namespace VS::Effects {
// Each entry binds its independently verified original overlay. State remains
// owned by the renderer slot: stopping returns zero; the runtime frees it.
// Unknown legacy commands leave a valid slot untouched and return one.
int UpdateLivePlg005AnalyzeGrid(Effects_RendererSlot* slot,
                                unsigned int command, int packed);
int UpdateLivePlg054AnalyzeGrid(Effects_RendererSlot* slot,
                                unsigned int command, int packed);
int UpdateLivePlg055AnalyzeGrid(Effects_RendererSlot* slot,
                                unsigned int command, int packed);
int UpdateLivePlg074AnalyzeGrid(Effects_RendererSlot* slot,
                                unsigned int command, int packed);
}  // namespace VS::Effects
