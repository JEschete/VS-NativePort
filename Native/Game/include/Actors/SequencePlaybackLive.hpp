#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Actors/AnimationPose.hpp"
#include "Actors/SequencePlayback.hpp"

struct Actors_ActorRecord;
struct Actors_AnimationPose;
struct Actors_ShadowBlend;

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

// A registered bank and an address's offset in it.
struct SequenceBankLocation {
  std::span<const std::uint8_t> bank;
  std::size_t offset = 0;
};
// The registered bank that holds `address`, which may be the bank's end; an
// event script cursor is kept as an address into its bank.
[[nodiscard]] std::optional<SequenceBankLocation> LocateInSequenceBank(
    const void* address);
// Reports a lookup of `animation` in the bank at `bank` whose status is a
// bank problem (an unregistered bank, or a map entry or header outside it);
// other statuses are not problems. Repeats are reported once.
void ReportLiveAnimationHeader(const void* bank, int animation,
                               AnimationHeaderStatus status);

// The pose's keys as the typed PoseKeys and SkeletalPose, and their stores.
[[nodiscard]] PoseKeys LoadLivePoseKeys(const Actors_AnimationPose& pose);
void StoreLivePoseKeys(const PoseKeys& keys, Actors_AnimationPose& pose);
[[nodiscard]] SkeletalPose LoadLiveSkeletalPose(
    const Actors_AnimationPose& pose);
void StoreLiveSkeletalPose(const SkeletalPose& typed,
                           Actors_AnimationPose& pose);

void AdvanceLiveSkeletalAnimation(Actors_ActorRecord& actor, int mode);
void BlendLiveActorPose(Actors_AnimationPose& destination,
                        Actors_AnimationPose& source, int bones, int frame,
                        int frames);
// The actor record's shadow blend as the typed ShadowBlend, and its store.
[[nodiscard]] ShadowBlend LoadLiveShadowBlend(const Actors_ShadowBlend& live);
void StoreLiveShadowBlend(const ShadowBlend& shadow, Actors_ShadowBlend& live);
}  // namespace VS::Actors
