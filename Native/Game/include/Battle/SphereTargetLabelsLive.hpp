#pragma once

namespace VS::Battle {
struct SphereTargetLabelRecord;
}  // namespace VS::Battle
namespace VS::Battle {
struct TargetLabelLine;
}  // namespace VS::Battle

// The ABI-compatible entries of the sphere's target labels
// (SphereTargetLabels.hpp) over the live target sphere, its label text, the
// frame's overlay ordering table, the GTE and the actors' palette pulses.
namespace VS::Battle {
// Points are packed x | y << 16 halfwords, as the original passes them.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void DrawRightTargetLabelLinkEntry(int anchor_xy, int ring_xy, int pulse_head);
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void DrawLeftTargetLabelLinkEntry(int anchor_xy, int ring_xy, int pulse_head);
[[nodiscard]] int AreTargetLabelsSettledEntry();
void DrawSphereTargetLabelEntry(VS::Battle::SphereTargetLabelRecord* label);
void UpdateSphereTargetLabelsEntry();
void UpdateSphereTargetHighlightsEntry(VS::Battle::TargetLabelLine* targets,
                                       int count);
}  // namespace VS::Battle
