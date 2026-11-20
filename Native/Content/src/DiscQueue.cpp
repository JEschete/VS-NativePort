// The legacy disc request array, counts and drive record
// (Content/DiscQueue.hpp).
#include "Content/DiscQueue.hpp"

namespace VS::Content {

std::uint8_t* legacyDiscReadBuffer;
Legacy::DslLOC legacyDiscReadLocation;
std::int32_t legacyXaReadyBcdField;
std::int32_t legacyXaReadyByteThree;
std::int32_t legacyXaReadyByteFour;
std::int32_t xaReadyMetadataWord;
std::array<DiscQueueSlot, kDiscRequestCapacity> discRequestSlots;
DiscRequestCounts discRequestCounts;
DiscDrive discDriveState;
std::array<char, kDiscControlResponseBytes> discControlResponseBytes;

}  // namespace VS::Content
