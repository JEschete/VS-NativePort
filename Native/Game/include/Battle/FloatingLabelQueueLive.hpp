#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>
#include <span>

struct Battle_FloatingLabel;

namespace VS::Battle {
void ResetLiveFloatingLabels();
int QueueLiveWorldLockedLabel(std::span<const std::uint16_t> position);
int QueueLiveWorldLockedLabelEntry(std::uint16_t* position);
void AdvanceLiveFloatingLabelQueue(Battle_FloatingLabel* label);
Battle_FloatingLabel* ReserveLiveFloatingLabel();
void CompactLiveFloatingLabels(int removal_count);
void RemoveLiveFloatingLabel(int label_index);
void CommitLiveFloatingLabel(const Battle_FloatingLabel* label);
std::int16_t LiveFloatingLabelCount();
}  // namespace VS::Battle
