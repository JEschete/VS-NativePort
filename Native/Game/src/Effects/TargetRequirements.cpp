#include "Effects/TargetRequirements.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <mutex>
#include <span>
#include <utility>
#include <vector>

#include "Core/Diagnostics.h"

namespace {
using Bytes = std::span<const std::uint8_t>;
constexpr std::size_t kWord = 4;
constexpr std::size_t kHalfword = 2;
constexpr std::size_t kByteBits = 8;
constexpr std::size_t kMetaKindShift = 16;
constexpr std::uint32_t kSizeMask = 0xffff;
constexpr std::size_t kBlockCapacity = 12;
constexpr std::size_t kMaxPackage = kBlockCapacity * kSizeMask + kWord;
constexpr std::size_t kParameterBytes = 26;
constexpr std::size_t kTransformStride = 0x38;
constexpr std::size_t kParticleStride = 0xcc;
constexpr std::size_t kTransformCapacity = 16;
constexpr std::size_t kCurveCapacity = 32;
constexpr std::size_t kParticleCapacity = 64;
constexpr std::size_t kTimelineCapacity = 256;
constexpr std::size_t kTrackCapacity = 31;
constexpr std::size_t kRegisterCount = 4;
constexpr std::size_t kPrimaryTrackCountOffset = 20;
constexpr std::size_t kTransformCountOffset = 21;
constexpr std::size_t kTimelineIdsOffset = 22;
constexpr std::size_t kGravityCurveOffset = 18;
constexpr std::size_t kDragCurveOffset = 19;
constexpr std::size_t kTextureStride = 24;
constexpr std::size_t kTextureCapacity = 4;
constexpr std::size_t kParticleTransformOffset = 0xc8;
constexpr std::size_t kParticleAttractionOffset = 0xc9;
constexpr std::size_t kParticleColorOffset = 0xc4;
constexpr std::size_t kParticleCurveEnd = 24;
constexpr std::size_t kParticleTickEventOffset = 3;
constexpr std::size_t kColorChannelCount = 3;
constexpr std::size_t kReservedBlock6 = 6;
constexpr std::size_t kReservedBlock7 = 7;
constexpr std::uint16_t kAttachmentEnabled = 2;
constexpr std::uint16_t kAttachmentModeMask = 0x18;
constexpr std::uint16_t kAttachmentEmitTo = 0x10;
constexpr std::uint16_t kFacingEnabled = 0x80;
constexpr std::uint16_t kFacingModeMask = 0xc00;
constexpr std::uint16_t kFacingEmitTo = 0x800;
constexpr std::size_t kTranslationCurveOffset = 4;
constexpr std::size_t kRotationCurveOffset = 6;
constexpr std::size_t kScaleCurveOffset = 7;
constexpr std::uint8_t kTargetColourTrack = 7;
constexpr std::uint8_t kSceneColourTrack = 9;
constexpr std::uint32_t kDirectionMask = 0xe000;
constexpr std::uint32_t kRadialDirection = 0x2000;
constexpr std::uint32_t kAnimationKindMask = 0xf0000000;
constexpr std::uint32_t kAnimationOffset = 0x10000000;
constexpr std::uint32_t kAnimationLoop = 0x20000000;
constexpr std::uint32_t kAnimationEnd = 0x30000000;
constexpr std::uint32_t kFrameMask = 0xffff;
constexpr std::size_t kNoOwner = std::numeric_limits<std::size_t>::max();
constexpr int kUnknownRegister = -1;
constexpr int kMaxSignedRegister = 127;

enum Block : std::size_t {
  kTextures = 1,
  kAnimations = 2,
  kTimelines = 3,
  kCurves = 4,
  kParticles = 5,
  kProgram = 8,
  kParameters = 9,
  kTransforms = 10,
  kSegments = 11
};

enum Opcode : std::uint8_t {
  kYield = 0,
  kJump = 1,
  kEnd = 3,
  kSelector = 4,
  kTickRenderers = 11,
  kUpdateTransforms = 12,
  kVerticalOffset = 13,
  kInitializeTransforms = 14,
  kSetRegister = 16,
  kBranchRegister = 17,
  kBranchParticles = 22,
  kEmitParticles = 32,
  kTickParticles = 33,
  kEnvironment = 34,
  kClearVariant = 36,
  kContinue = 40,
  kLoadTimeline = 41,
  kTickTimeline = 42,
  kBranchTimeline = 43,
  kFreeTimeline = 44,
  kCompleteChild = 47
};

bool Reject(const char* reason) {
  Core_Errorf("effects", "Absent-target package proof rejected: %s", reason);
  return false;
}

bool Contains(Bytes bytes, std::size_t offset, std::size_t length) {
  return offset <= bytes.size() && length <= bytes.size() - offset;
}

std::uint16_t Halfword(Bytes bytes, std::size_t offset) {
  return static_cast<std::uint16_t>(bytes[offset] |
                                    (bytes[offset + 1] << kByteBits));
}

std::uint32_t Word(Bytes bytes, std::size_t offset) {
  return Halfword(bytes, offset) |
         (static_cast<std::uint32_t>(Halfword(bytes, offset + kHalfword))
          << kMetaKindShift);
}

bool Claim(std::vector<bool>& occupied, std::size_t offset,
           std::size_t length) {
  if (offset > occupied.size() || length > occupied.size() - offset) {
    return Reject("truncated section or keys");
  }
  for (std::size_t index = offset; index < offset + length; ++index) {
    if (occupied[index]) return Reject("overlapping timeline storage");
    occupied[index] = true;
  }
  return true;
}

// Original layout: _parsePfileBlock* (BATTLE.PRG/6F014.c:475-543);
// timeline offsets: Content_PFileTimelineSection/TrackData, originally
// _getPFileBlock3section and func_800D5564 (BATTLE.PRG/5BF94.c:7874-7879).
struct PackageProof {
  std::array<Bytes, kBlockCapacity> blocks{};
  std::array<bool, kBlockCapacity> present{};
  std::array<std::size_t, kTextureCapacity> texture_frames{};
  std::vector<std::uint32_t> animation_frames;
  std::size_t transform_count = 0;
  std::size_t particle_count = 0;
  std::size_t timeline_count = 0;
  std::size_t curve_count = 0;
  bool needs_target = false;
  bool timeline_emits = false;

  bool ReadBlocks(Bytes source) {
    std::size_t cursor = 0;
    while (Contains(source, cursor, kWord)) {
      const auto meta = Word(source, cursor);
      const auto kind = meta >> kMetaKindShift;
      const auto length = meta & kSizeMask;
      if (kind == 0) {
        for (const auto required : {kCurves, kTimelines, kParticles, kProgram,
                                    kParameters, kTransforms}) {
          if (!present[required]) {
            return Reject("missing required package block");
          }
        }
        const auto remainder = source.subspan(cursor);
        return std::all_of(remainder.begin(), remainder.end(),
                           [](std::uint8_t byte) { return byte == 0; }) ||
               Reject("nonzero terminator or trailing source");
      }
      if (length < kWord || !Contains(source, cursor, length)) {
        return Reject("invalid block length");
      }
      if (kind >= blocks.size() || kind == kReservedBlock6 ||
          kind == kReservedBlock7) {
        return Reject("unsupported package block");
      }
      if (present[kind]) return Reject("duplicate package block");
      present[kind] = true;
      blocks[kind] = source.subspan(cursor + kWord, length - kWord);
      cursor += length;
    }
    return Reject("missing package terminator");
  }

  bool Curves() {
    const auto bytes = blocks[kCurves];
    if (!Contains(bytes, 0, kWord)) return Reject("missing curve header");
    curve_count = Halfword(bytes, 0);
    auto cursor = static_cast<std::size_t>(Halfword(bytes, kHalfword));
    if (curve_count > kCurveCapacity || !Contains(bytes, kWord, curve_count) ||
        cursor < kWord + curve_count || cursor > bytes.size()) {
      return Reject("invalid curve table");
    }
    for (std::size_t index = 0; index < curve_count; ++index) {
      const auto length = bytes[kWord + index];
      if (length == 0 || !Contains(bytes, cursor, length)) {
        return Reject("empty or truncated curve");
      }
      cursor += length;
    }
    return true;
  }

  bool ParametersAndTransforms() {
    const auto parameters = blocks[kParameters];
    const auto transforms = blocks[kTransforms];
    if (!Contains(parameters, 0, kParameterBytes)) {
      return Reject("missing package parameters");
    }
    // func_800CE9B0/800CEF74 (BATTLE.PRG/5BF94.c:5170/5399):
    // frameCount primary tracks enter at track_index * 3. Only entry zero
    // is proved here, so no additional primary entry may execute.
    if (parameters[kPrimaryTrackCountOffset] != 1) {
      return Reject("absent-target proof requires one primary track");
    }
    transform_count = parameters[kTransformCountOffset];
    if (transform_count > kTransformCapacity ||
        transforms.size() % kTransformStride != 0 ||
        transforms.size() / kTransformStride != transform_count ||
        parameters[kGravityCurveOffset] > curve_count ||
        parameters[kDragCurveOffset] > curve_count) {
      return Reject("invalid transform count or environment curves");
    }
    for (std::size_t offset = 0; offset < transforms.size();
         offset += kTransformStride) {
      const auto flags = Halfword(transforms, offset);
      // Effects_InitializeTransformStates, func_800CFEF0:
      // BATTLE.PRG/5BF94.c:5940; only these two modes read emitTo.
      needs_target |= ((flags & kAttachmentEnabled) != 0 &&
                       (flags & kAttachmentModeMask) == kAttachmentEmitTo) ||
                      ((flags & kFacingEnabled) != 0 &&
                       (flags & kFacingModeMask) == kFacingEmitTo);
      if (((flags & kAttachmentEnabled) != 0 &&
           (flags & kAttachmentModeMask) == kAttachmentModeMask) ||
          ((flags & kFacingEnabled) != 0 &&
           (flags & kFacingModeMask) == kFacingModeMask)) {
        return Reject("unsupported transform attachment mode");
      }
      for (const auto field :
           {kTranslationCurveOffset, kRotationCurveOffset, kScaleCurveOffset}) {
        if (transforms[offset + field] > curve_count) {
          return Reject("transform curve index outside table");
        }
      }
    }
    return true;
  }

  bool Textures() {
    const auto bytes = blocks[kTextures];
    if (!present[kTextures]) return true;
    if (!Contains(bytes, 0, kWord)) return Reject("truncated texture header");
    const auto count = Halfword(bytes, 0);
    const auto geometry = Halfword(bytes, kHalfword);
    const auto header = kWord + count * kWord;
    if (count > kTextureCapacity || !Contains(bytes, 0, header) ||
        geometry < header || geometry > bytes.size() ||
        (bytes.size() - geometry) % kTextureStride != 0) {
      return Reject("invalid texture table or descriptor span");
    }
    const auto descriptors = (bytes.size() - geometry) / kTextureStride;
    for (std::size_t index = 0; index < count; ++index) {
      const auto offset = Word(bytes, kWord + index * kWord);
      const auto end = index + 1 == count
                           ? geometry
                           : Word(bytes, kWord + (index + 1) * kWord);
      if (offset < header || end < offset || end > geometry ||
          (end - offset) % kWord != 0) {
        return Reject("invalid texture frame table");
      }
      texture_frames[index] = (end - offset) / kWord;
      for (auto frame = offset; frame < end; frame += kWord) {
        const auto first = Halfword(bytes, frame + kHalfword);
        const auto length = Halfword(bytes, frame);
        if (first > descriptors || length > descriptors - first) {
          return Reject("texture frame outside descriptors");
        }
      }
    }
    return true;
  }

  bool Animation(Bytes bytes, std::size_t start, std::size_t end,
                 std::uint32_t& frame_bound) {
    bool yields = false;
    for (auto cursor = start; cursor <= end && kWord <= end - cursor;
         cursor += kWord) {
      const auto command = Word(bytes, cursor);
      switch (command & kAnimationKindMask) {
        case 0:
          frame_bound = std::max(frame_bound, (command & kFrameMask) + 1);
          yields = true;
          break;
        case kAnimationOffset:
          break;
        case kAnimationLoop:
          return yields || Reject("animation loop without a yielding frame");
        case kAnimationEnd:
          return true;
        default:
          return Reject("unsupported texture animation command");
      }
    }
    return Reject("unterminated texture animation");
  }

  bool Animations() {
    const auto bytes = blocks[kAnimations];
    if (!present[kAnimations]) return true;
    if (!Contains(bytes, 0, kWord)) return Reject("truncated animation header");
    const auto count = Word(bytes, 0);
    if (count > kTimelineCapacity ||
        count > (bytes.size() - kWord) / kHalfword) {
      return Reject("invalid animation count");
    }
    const auto header = kWord + count * kHalfword;
    animation_frames.resize(count);
    for (std::size_t index = 0; index < count; ++index) {
      const auto start = Halfword(bytes, kWord + index * kHalfword);
      const auto end = index + 1 == count
                           ? bytes.size()
                           : Halfword(bytes, kWord + (index + 1) * kHalfword);
      if (start < header || end <= start || end > bytes.size() ||
          !Animation(bytes, start, end, animation_frames[index])) {
        return Reject("invalid animation offset or program");
      }
    }
    return true;
  }

  bool Particles() {
    const auto bytes = blocks[kParticles];
    if (!Contains(bytes, 0, kWord)) {
      return Reject("truncated particle definition count");
    }
    // Original pFileBlock5 (BATTLE.PRG/5BF94.h:326) declares count before
    // definitions. Unreferenced block-tail storage is not another definition.
    particle_count = Word(bytes, 0);
    if (particle_count > kParticleCapacity ||
        particle_count > (bytes.size() - kWord) / kParticleStride) {
      return Reject("invalid particle definition count or declared span");
    }
    for (std::size_t index = 0; index < particle_count; ++index) {
      const auto definition =
          bytes.subspan(kWord + index * kParticleStride, kParticleStride);
      // Effects_EmitParticleEvent, func_800D2ADC,
      // BATTLE.PRG/5BF94.c:6935: aim 0x6000 reads emitTo. Events recurse.
      const auto flags = Word(definition, kWord);
      needs_target |=
          definition[2] != 0 || definition[kParticleTickEventOffset] != 0;
      const auto direction = flags & kDirectionMask;
      needs_target |= direction != 0 && direction != kRadialDirection;
      // The upper six bits named transparencyCurve in 5BF94.h are copied
      // directly to scratch->unk180 (5BF94.c:6964-6965), then particle size.
      // They are not an index into Effects_curveSamples.
      if (definition[kParticleTransformOffset] >= transform_count ||
          definition[kParticleAttractionOffset] >= transform_count ||
          definition[0] >= animation_frames.size() ||
          definition[1] >= texture_frames.size() ||
          animation_frames[definition[0]] > texture_frames[definition[1]]) {
        return Reject("particle resource index outside package");
      }
      for (auto field = kWord * 2; field < kParticleCurveEnd; ++field) {
        if (definition[field] > curve_count) {
          return Reject("particle curve index outside table");
        }
      }
      for (auto field = kParticleColorOffset;
           field < kParticleColorOffset + kColorChannelCount; ++field) {
        if (definition[field] > curve_count) {
          return Reject("particle colour curve index outside table");
        }
      }
    }
    return true;
  }

  bool Track(Bytes bytes, std::size_t header, std::vector<bool>& occupied) {
    const auto kind = bytes[header];
    const auto count = bytes[header + 1];
    const auto offset = Halfword(bytes, header + kHalfword);
    std::size_t stride = 0;
    switch (kind) {
      case 1:
        stride = kWord;
        timeline_emits = true;
        break;
      case kTargetColourTrack:
      case kSceneColourTrack:
        // func_800D6048/800D6298, BATTLE.PRG/5BF94.c:8144/8189.
        // Kind 7 checks Actors_ActorStateFlags before optional actor colour.
        // No kind-3 child tracks means its "all targets" flag is always clear.
        stride = kWord * 2;
        break;
      default:
        return Reject("unsupported or target-dependent timeline track");
    }
    if (count == 0 || !Claim(occupied, offset, count * stride)) {
      return Reject("empty, truncated or overlapping timeline keys");
    }
    if (kind == 1) {
      for (std::size_t key = 0; key < count; ++key) {
        constexpr std::size_t kEventShift = 20;
        constexpr std::uint32_t kEventMask = 63;
        const auto event =
            (Word(bytes, offset + key * stride) >> kEventShift) & kEventMask;
        if (event >= particle_count) {
          return Reject("timeline particle index outside definitions");
        }
      }
    }
    return true;
  }

  bool Timelines() {
    const auto bytes = blocks[kTimelines];
    if (!Contains(bytes, 0, kWord)) return Reject("missing timeline table");
    timeline_count = Word(bytes, 0);
    if (timeline_count > kTimelineCapacity ||
        timeline_count > (bytes.size() - kWord) / kHalfword) {
      return Reject("invalid timeline count");
    }
    std::vector<bool> occupied(bytes.size());
    if (!Claim(occupied, 0, kWord + timeline_count * kHalfword)) return false;
    for (std::size_t index = 0; index < timeline_count; ++index) {
      const auto section = Halfword(bytes, kWord + index * kHalfword);
      if (!Contains(bytes, section, kWord)) {
        return Reject("timeline section outside block");
      }
      const auto count = Word(bytes, section);
      if (count > kTrackCapacity ||
          !Claim(occupied, section, kWord + count * kWord)) {
        return Reject("invalid timeline track table");
      }
      for (std::size_t track = 0; track < count; ++track) {
        if (!Track(bytes, section + kWord + track * kWord, occupied)) {
          return false;
        }
      }
    }
    return true;
  }

  bool Segments() {
    const auto bytes = blocks[kSegments];
    if (!present[kSegments]) return true;
    if (!Contains(bytes, 0, kWord)) return Reject("truncated segmented table");
    const auto count = Word(bytes, 0);
    if (count > (bytes.size() - kWord) / kHalfword) {
      return Reject("invalid segmented table count");
    }
    const auto header = kWord + count * kHalfword;
    for (std::size_t index = 0; index < count; ++index) {
      const auto offset = Halfword(bytes, kWord + index * kHalfword);
      if (offset < header || offset >= bytes.size()) {
        return Reject("segmented table offset outside block");
      }
    }
    // No accepted track/opcode samples segmented curves.
    return true;
  }
};

std::size_t InstructionSize(std::uint8_t opcode) {
  switch (opcode) {
    case kYield:
    case kJump:
    case kTickTimeline:
    case kBranchTimeline:
      return 1 + kHalfword;
    case kSelector:
    case kEmitParticles:
    case kLoadTimeline:
      return 2;
    case kSetRegister:
    case kBranchParticles:
      return kWord;
    case kBranchRegister:
      return 2 + kHalfword * 2;
    case kEnd:
    case kTickRenderers:
    case kUpdateTransforms:
    case kVerticalOffset:
    case kInitializeTransforms:
    case kTickParticles:
    case kEnvironment:
    case kClearVariant:
    case kContinue:
    case kFreeTimeline:
    case kCompleteChild:
      return 1;
    default:
      return 0;
  }
}

struct ScriptState {
  std::array<int, kRegisterCount> registers{};
  bool timeline = false;
  bool transforms = false;

  bool Merge(const ScriptState& other) {
    const auto previous = *this;
    for (std::size_t index = 0; index < registers.size(); ++index) {
      if (registers[index] != other.registers[index]) {
        registers[index] = kUnknownRegister;
      }
    }
    timeline &= other.timeline;
    transforms &= other.transforms;
    return registers != previous.registers || timeline != previous.timeline ||
           transforms != previous.transforms;
  }
};

// Bounded CFG proof, not an interpreter. Dispatch provenance:
// BATTLE.PRG world table at module offset 0x83bf4 and
// func_800D4720..800D5048 (BATTLE.PRG/5BF94.c:7515-7776).
// Every conditional edge is visited. Conflicting register constants become
// unknown; only constants can select a particle definition or timeline.
class ScriptProof {
 public:
  explicit ScriptProof(const PackageProof& package)
      : package_(package),
        bytes_(package.blocks[kProgram]),
        nodes_(bytes_.size()),
        owners_(bytes_.size(), kNoOwner) {}

  bool Run() {
    if (bytes_.empty() || !Queue(0, {})) return Reject("missing world program");
    while (!pending_.empty()) {
      const auto cursor = pending_.back();
      pending_.pop_back();
      auto state = nodes_[cursor].state;
      const auto opcode = bytes_[cursor];
      const auto length = InstructionSize(opcode);
      if (length == 0) return Reject("unsupported world instruction");
      if (!Contains(bytes_, cursor, length)) {
        return Reject("truncated world instruction");
      }
      for (auto offset = cursor; offset < cursor + length; ++offset) {
        if (owners_[offset] != kNoOwner && owners_[offset] != cursor) {
          return Reject("overlapping world instructions");
        }
        owners_[offset] = cursor;
      }
      if (!Execute(opcode, cursor, state)) return false;
      if (opcode == kEnd) continue;
      if (opcode == kYield || opcode == kJump) {
        if (!Queue(Halfword(bytes_, cursor + 1), state)) return false;
        continue;
      }
      if (opcode == kBranchParticles || opcode == kBranchTimeline ||
          opcode == kBranchRegister) {
        if (!Queue(Halfword(bytes_, cursor + length - kHalfword), state)) {
          return false;
        }
      }
      if (!Queue(cursor + length, state)) return false;
    }
    return true;
  }

 private:
  struct Node {
    ScriptState state;
    bool reached = false;
  };

  bool Queue(std::size_t cursor, const ScriptState& state) {
    if (cursor >= bytes_.size() ||
        (owners_[cursor] != kNoOwner && owners_[cursor] != cursor)) {
      return Reject("world successor outside program or inside operands");
    }
    owners_[cursor] = cursor;
    auto& node = nodes_[cursor];
    if (!node.reached) {
      node.state = state;
      node.reached = true;
      pending_.push_back(cursor);
    } else if (node.state.Merge(state)) {
      pending_.push_back(cursor);
    }
    return true;
  }

  bool RegisterInstruction(std::uint8_t opcode, std::size_t cursor,
                           ScriptState& state) {
    const auto index = bytes_[cursor + 1];
    if (index >= kRegisterCount) return Reject("script register out of range");
    if (opcode == kSetRegister) {
      state.registers[index] = bytes_[cursor + 2];
      return true;
    }
    if (opcode == kBranchRegister) return true;
    const auto value = state.registers[index];
    if (value == kUnknownRegister) {
      return Reject("resource register not constant on every incoming path");
    }
    if (opcode == kEmitParticles) {
      return (value <= kMaxSignedRegister &&
              static_cast<std::size_t>(value) < package_.particle_count &&
              state.transforms) ||
             Reject("invalid particle register or uninitialized transforms");
    }
    auto timeline = static_cast<std::size_t>(value);
    if (timeline < kRegisterCount) {
      const auto id =
          package_.blocks[kParameters][kTimelineIdsOffset + timeline];
      // Preserve func_800D4DB0's strict id < count and fallback behavior.
      if (id != 0 && id < package_.timeline_count) timeline = id - 1;
    }
    if (timeline >= package_.timeline_count) {
      return Reject("timeline register outside table");
    }
    state.timeline = true;
    return true;
  }

  bool Execute(std::uint8_t opcode, std::size_t cursor, ScriptState& state) {
    switch (opcode) {
      case kSetRegister:
      case kBranchRegister:
      case kEmitParticles:
      case kLoadTimeline:
        return RegisterInstruction(opcode, cursor, state);
      case kInitializeTransforms:
        state.transforms = true;
        break;
      case kUpdateTransforms:
        if (package_.transform_count != 0 && !state.transforms) {
          return Reject("transform update before initialization");
        }
        break;
      case kTickTimeline:
      case kBranchTimeline:
        if (!state.timeline || (opcode == kTickTimeline &&
                                package_.timeline_emits && !state.transforms)) {
          return Reject("timeline use before initialization");
        }
        // Its child-script operand is unused: kind-3 tracks are rejected.
        break;
      case kFreeTimeline:
        state.timeline = false;
        break;
      default:
        break;
    }
    return true;
  }

  const PackageProof& package_;
  Bytes bytes_;
  std::vector<Node> nodes_;
  std::vector<std::size_t> owners_;
  std::vector<std::size_t> pending_;
};

struct DeferredScriptTarget {
  bool pending = false;
  unsigned effect_id = 0;
  unsigned selector = 0;
  unsigned token = 0;
};

std::mutex deferred_mutex;
DeferredScriptTarget deferred_target;
}  // namespace

int Effects_ScriptPackageAllowsAbsentTarget(const void* bytes,
                                            size_t byte_count) {
  if (bytes == nullptr || byte_count < kWord || byte_count > kMaxPackage) {
    return Reject("missing or oversized original source");
  }
  PackageProof package;
  if (!package.ReadBlocks(
          Bytes(static_cast<const std::uint8_t*>(bytes), byte_count)) ||
      !package.Curves() || !package.ParametersAndTransforms() ||
      !package.Textures() || !package.Animations() || !package.Particles() ||
      !package.Timelines() || !package.Segments() ||
      !ScriptProof(package).Run()) {
    return 0;
  }
  return package.needs_target ? 0 : 1;
}

void Effects_ResetDeferredScriptTarget(void) {
  const std::lock_guard lock(deferred_mutex);
  deferred_target = {};
}

void Effects_RecordDeferredScriptTarget(unsigned effect_id, unsigned selector,
                                        unsigned token) {
  const std::lock_guard lock(deferred_mutex);
  deferred_target = {true, effect_id, selector, token};
}

int Effects_ValidateDeferredScriptTarget(unsigned effect_id, unsigned token,
                                         int actual_owner_model,
                                         const void* bytes, size_t byte_count) {
  DeferredScriptTarget request;
  {
    const std::lock_guard lock(deferred_mutex);
    request = std::exchange(deferred_target, {});
  }
  if (!request.pending) return 1;
  if (request.effect_id != effect_id || request.token != token) {
    return Reject("deferred effect or token mismatch");
  }
  if (actual_owner_model >= 0) {
    return static_cast<unsigned>(actual_owner_model) == request.selector ||
           Reject("deferred target acquired a different model owner");
  }
  constexpr int kAbsentOwner = -1;
  if (actual_owner_model != kAbsentOwner) {
    return Reject("deferred target has an unprovable existing owner");
  }
  const auto allows_absent =
      Effects_ScriptPackageAllowsAbsentTarget(bytes, byte_count);
  return allows_absent || Reject("deferred package requires a real target");
}
