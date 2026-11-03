#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Audio/SeqControlCommands.hpp"
#include "Audio/SeqMusicSequence.hpp"
#include "Audio/SeqVoiceCommands.hpp"
#include "Audio/VoiceOutput.hpp"

struct Audio_SeqChannel;
struct Audio_MusicConfig;
struct Audio_VoiceScheduler;

// Typed copies of the sequencer's legacy records (FSoundChannel,
// FSoundChannelConfig and FSoundVoiceSchedulerState), and the memory a
// channel's program counter may point into. Each Load/Store pair copies the
// same fields at the same widths, so storing an unchanged state leaves the
// record unchanged.
namespace VS::Audio::Seq {
Driver::ChannelKind LiveChannelKind(const Audio_SeqChannel& legacy);
ChannelState LoadChannelState(const Audio_SeqChannel& legacy);
void StoreChannelState(Audio_SeqChannel& legacy, const ChannelState& state);

MusicLayerState LoadMusicLayer(const Audio_MusicConfig& legacy);
void StoreMusicLayer(Audio_MusicConfig& legacy, const MusicLayerState& state);

VoiceSchedulerState LoadVoiceScheduler(const Audio_VoiceScheduler& legacy);
void StoreVoiceScheduler(Audio_VoiceScheduler& legacy,
                         const VoiceSchedulerState& state);

// Instrument table entry `index`; empty outside the table.
std::optional<Samples::InstrumentRecord> LiveInstrument(std::int32_t index);

// A block of memory sequence programs run from. `base` is the legacy pointer
// the block starts at, so offsets into it become channel pointers again.
struct LiveProgramMemory {
  char* base = nullptr;
  std::size_t size = 0;

  [[nodiscard]] ProgramData View() const;
  // The memory from `start`, which lies in this block, to its end.
  [[nodiscard]] LiveProgramMemory From(char* start) const;
};

// The block of memory holding `address` that sequence programs live in: a
// game heap block, the menu sound effects, the idle program or the overlay
// arena. Empty for any other address.
std::optional<LiveProgramMemory> LiveProgramRegion(char* address);
// The program a finished or unused channel runs.
char* LiveIdleProgram();

// While a channel step runs, the memory the stepped channel's program lies in.
// Its commands then read no further than that memory even when the program
// counter has reached its end.
class ScopedChannelProgram {
 public:
  ScopedChannelProgram(const Audio_SeqChannel& channel,
                       const LiveProgramMemory& memory);
  ~ScopedChannelProgram();
  ScopedChannelProgram(const ScopedChannelProgram&) = delete;
  ScopedChannelProgram& operator=(const ScopedChannelProgram&) = delete;

 private:
  const Audio_SeqChannel* previous_channel_;
  LiveProgramMemory previous_memory_;
};

// The memory the channel's program counter lies in or ends at: the stepped
// channel's (ScopedChannelProgram) when the counter lies in it or at its end,
// else LiveProgramRegion's for the counter or, when the counter is just past
// a region, for the byte before it.
std::optional<LiveProgramMemory> LiveChannelProgram(
    const Audio_SeqChannel& channel);

// The channel's program positions as offsets into `memory`. Pointers outside
// it load as kOutsideProgram, and kOutsideProgram stores the pointer back
// unchanged.
ProgramState LoadProgram(const Audio_SeqChannel& legacy,
                         const LiveProgramMemory& memory);
void StoreProgram(Audio_SeqChannel& legacy, const ProgramState& program,
                  const LiveProgramMemory& memory);

// A whole channel record; `idle` reads and writes the idle program.
SequenceChannel LoadSequenceChannel(const Audio_SeqChannel& legacy,
                                    const LiveProgramMemory& memory);
void StoreSequenceChannel(Audio_SeqChannel& legacy,
                          const SequenceChannel& record,
                          const LiveProgramMemory& memory);

// Reports a refused sequencer operation: the first time, then every power of
// two.
void ReportRefusal(const char* operation, const char* reason);
// How many sequencer operations have been refused.
[[nodiscard]] std::uint64_t LiveRefusalCount() noexcept;
}  // namespace VS::Audio::Seq
