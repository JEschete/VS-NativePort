#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>
#include <span>

namespace VS::Battle {
struct FloatingLabel;
void ResetLiveFloatingLabels();
int QueueLiveWorldLockedLabel(std::span<const std::uint16_t> position);
int QueueLiveWorldLockedLabelEntry(std::uint16_t* position);
void AdvanceLiveFloatingLabelQueue(VS::Battle::FloatingLabel* label);
VS::Battle::FloatingLabel* ReserveLiveFloatingLabel();
void CompactLiveFloatingLabels(int removal_count);
void RemoveLiveFloatingLabel(int label_index);
void CommitLiveFloatingLabel(const VS::Battle::FloatingLabel* label);
std::int16_t LiveFloatingLabelCount();
}  // namespace VS::Battle
