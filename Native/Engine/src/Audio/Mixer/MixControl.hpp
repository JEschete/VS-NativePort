#pragma once
// The parameters a sound command carries, and the handler type the command
// table dispatches to.
#include <cstdint>

namespace VS::Audio {
// decomp: FSoundCommandParams (src/include/sound.h:501)
struct CommandParams {
  std::uintptr_t param1;
  std::uint32_t param2;
  std::uint32_t param3;  // Seems to be usually flags
  std::uint32_t param4;
  std::uint32_t ext_param1;
  std::uint32_t ext_param2;
};

// decomp: FSoundCommandHandler (src/SLUS_010.40/25AC.c:77)
using CommandHandler = void (*)(CommandParams*);
}  // namespace VS::Audio
