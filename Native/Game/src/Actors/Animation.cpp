// The battle program's shared sequence and weapon animation tables
// (Animation.hpp).
#include "Actors/Animation.hpp"

namespace VS::Actors {

// Contents: module image (ModuleData.cpp) for every table down to
// animationEventPoints.
ByteTable<kSharedSequenceIdsByKindBytes> sharedSequenceIdsByKind;
std::uint8_t sharedSequenceBankIndex;
char forcedModelOverrides;
ByteTable<kDrawnAnimationBasesBytes> drawnWeaponAnimationBases;
ByteTable<kSheathedAnimationBasesBytes> sheathedWeaponAnimationBases;
SecondarySlotTimelines secondarySlotSequences;
ByteTable<kAnimationEventLengthsBytes> animationEventLengths;
ByteTable<kDrawnWeaponSoundIdsBytes> drawnWeaponSoundIds;
AnimationEventPoints animationEventPoints;

ByteTable<kTexturePagesOneBasedBytes> sequenceTexturePagesOneBased;
ByteTable<kBankTexturePagesBytes> sharedSequenceBankTexturePages;
std::array<void*, kSharedSequenceBankSlots> sharedSequenceData;
SharedSequenceTable<std::int16_t> sharedSequenceAnimationIds;
void* sequenceWorkBuffer;
SharedSequenceTable<char*> sharedSequenceAnimationData;
void* primarySequenceBuffer;
SharedSequenceTable<std::int16_t> sharedSequenceEntryParams;
char lastResolvedSequenceIndex;

}  // namespace VS::Actors
