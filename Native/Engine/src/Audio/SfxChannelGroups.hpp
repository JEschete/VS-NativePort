#pragma once
// The sound-effect channel groups' next tags, and the slot request record the
// original's sound-effect loader filled.
#include <cstddef>
#include <cstdint>

namespace VS::Audio {
// decomp: func_800172D4_t (src/SLUS_010.40/25AC.c:56)
struct SfxSlotRequest {
  std::int32_t index;
  std::int32_t reserved04;
  std::int32_t reserved08;
  std::int32_t reserved0c;
  std::int32_t reserved10;
};

namespace layout {
inline constexpr std::size_t kSfxSlotRequestBytes = 0x14;
static_assert(sizeof(SfxSlotRequest) == kSfxSlotRequestBytes);
}  // namespace layout

// decomp: D_8005FE70 (D_8005FE70@SLUS_010.40)
extern std::int32_t nextSfxChannelGroup1;
// decomp: D_8005FE74 (D_8005FE74@SLUS_010.40)
extern std::int32_t nextSfxChannelGroup7E;
// decomp: D_8005FE78 (D_8005FE78@SLUS_010.40)
extern std::int32_t nextSfxChannelGroup180;
// decomp: D_8005FE7C (D_8005FE7C@SLUS_010.40)
extern std::int32_t nextSfxChannelGroup200;
// decomp: D_8005FE80 (D_8005FE80@SLUS_010.40)
extern std::int32_t nextSfxChannelGroupFF000;
// decomp: D_8005FE84 (D_8005FE84@SLUS_010.40)
extern std::int32_t nextSfxChannelGroupF00000;
}  // namespace VS::Audio
