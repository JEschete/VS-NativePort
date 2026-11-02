#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstddef>
#include <cstdint>
#include <span>

#include "Actors/SequencePlayback.hpp"

struct Actors_ActorRecord;
struct Actors_AnimationPose;

// Bindings from actor records to the typed skeletal playback
// (SequencePlayback.cpp), and the extents of the SEQ banks they play.
namespace VS::Actors {
// Records that `bytes` bytes of SEQ bank start at `bank`. The loader calls
// this for every bank it copies, clones or binds in place; registrations it
// overlaps are dropped, since that memory was reused.
void RegisterSequenceBank(const void* bank, std::size_t bytes);
// Records `destination` as a copy of the registered bank at `source`, limited
// to the `capacity` bytes the copy holds.
void RegisterSequenceBankCopy(const void* destination, const void* source,
                              std::size_t capacity);
// Records the bank of a SEQ file held in place: the size word at file+4 is
// the extent of the bank that starts at file+8.
void RegisterEmbeddedSequenceBank(const void* file);
// Drops every registration overlapping the `bytes` bytes at `memory`, for
// owners releasing memory that held a bank.
void ForgetSequenceBanks(const void* memory, std::size_t bytes);
// The registered bytes of the bank at `bank`, or an empty span.
std::span<const std::uint8_t> SequenceBankBytes(const void* bank);

void AdvanceLiveSkeletalAnimation(Actors_ActorRecord& actor, int mode);
void BlendLiveActorPose(Actors_AnimationPose& destination,
                        Actors_AnimationPose& source, int bones, int frame,
                        int frames);
}  // namespace VS::Actors
