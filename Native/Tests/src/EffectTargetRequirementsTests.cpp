#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Core/Log.hpp"
#include "Effects/TargetRequirements.h"

namespace {
using Bytes = std::vector<std::uint8_t>;

void Put16(Bytes& bytes, std::size_t offset, std::uint16_t value) {
  REQUIRE(offset + 2 <= bytes.size());
  bytes[offset] = static_cast<std::uint8_t>(value);
  bytes[offset + 1] = static_cast<std::uint8_t>(value >> 8);
}

void Put32(Bytes& bytes, std::size_t offset, std::uint32_t value) {
  Put16(bytes, offset, static_cast<std::uint16_t>(value));
  Put16(bytes, offset + 2, static_cast<std::uint16_t>(value >> 16));
}

void Append32(Bytes& bytes, std::uint32_t value) {
  const auto offset = bytes.size();
  bytes.resize(offset + 4);
  Put32(bytes, offset, value);
}

struct Script {
  Bytes bytes;

  std::size_t Offset() const { return bytes.size(); }
  void Op(std::uint8_t opcode) { bytes.push_back(opcode); }
  void Halfword(std::uint16_t value) {
    const auto offset = bytes.size();
    bytes.resize(offset + 2);
    Put16(bytes, offset, value);
  }
  void SetRegister(std::uint8_t index, std::uint16_t value) {
    Op(16);
    Op(index);
    Halfword(value);
  }
  void LoadTimeline(std::uint8_t index) {
    Op(41);
    Op(index);
  }
  void Jump(std::uint16_t offset) {
    Op(1);
    Halfword(offset);
  }
  void Yield(std::uint16_t offset) {
    Op(0);
    Halfword(offset);
  }
  void BranchParticles(std::uint8_t count, std::uint16_t offset) {
    Op(22);
    Op(count);
    Halfword(offset);
  }
};

// Semantic, synthetic original-format records, never copied disc payloads.
struct Package {
  Bytes textures;
  Bytes animations;
  Bytes timelines = Bytes(4);
  Bytes curves = {0, 0, 4, 0};
  Bytes particles = Bytes(4);
  Bytes program = {3};
  Bytes parameters = Bytes(26);
  Bytes transforms = Bytes(0x38);
  Bytes segments;

  Package() {
    parameters[20] = 1;
    parameters[21] = 1;
  }

  static void Block(Bytes& bytes, unsigned kind, const Bytes& payload) {
    REQUIRE(payload.size() + 4 <= 0xffff);
    Append32(bytes,
             (kind << 16) | static_cast<std::uint32_t>(payload.size() + 4));
    bytes.insert(bytes.end(), payload.begin(), payload.end());
  }

  Bytes Build() const {
    Bytes bytes;
    if (!textures.empty()) Block(bytes, 1, textures);
    if (!animations.empty()) Block(bytes, 2, animations);
    Block(bytes, 3, timelines);
    Block(bytes, 4, curves);
    Block(bytes, 5, particles);
    Block(bytes, 8, program);
    Block(bytes, 9, parameters);
    Block(bytes, 10, transforms);
    if (!segments.empty()) Block(bytes, 11, segments);
    Append32(bytes, 0);
    return bytes;
  }

  int Classify() const {
    const auto bytes = Build();
    return Effects_ScriptPackageAllowsAbsentTarget(bytes.data(), bytes.size());
  }

  void Particle() {
    textures.resize(12 + 24);
    Put16(textures, 0, 1);
    Put16(textures, 2, 12);
    Put32(textures, 4, 8);
    Put16(textures, 8, 1);
    animations.resize(16);
    Put32(animations, 0, 1);
    Put16(animations, 4, 8);
    Put32(animations, 8, 2 << 16);  // Frame zero, lasting two ticks.
    Put32(animations, 12, 0x30000000);
    particles.resize(4 + 0xcc);
    Put32(particles, 0, 1);
    // Positive interval/count ranges; curves and transform indices are zero.
    Put16(particles, 4 + 0xbc, 1);
    Put16(particles, 4 + 0xbe, 1);
    Put16(particles, 4 + 0xc0, 1);
    Put16(particles, 4 + 0xc2, 1);
  }

  void Timeline(std::uint8_t kind, std::size_t stride,
                std::uint8_t key_count = 1) {
    timelines.assign(16 + key_count * stride, 0);
    Put32(timelines, 0, 1);
    Put16(timelines, 4, 8);
    Put32(timelines, 8, 1);
    timelines[12] = kind;
    timelines[13] = key_count;
    Put16(timelines, 14, 16);
  }
};

struct Observation {
  std::vector<std::string> errors;
  VS::Core::LogObserver previous;

  Observation()
      : previous(
            VS::Core::SetLogObserver([this](const VS::Core::LogRecord& record) {
              if (record.level == VS::Core::LogLevel::Error) {
                errors.emplace_back(record.message);
              }
            })) {
    Effects_ResetDeferredScriptTarget();
  }
  ~Observation() {
    Effects_ResetDeferredScriptTarget();
    VS::Core::SetLogObserver(std::move(previous));
  }
};

void CheckInvalid(const Bytes& bytes, Observation& observation) {
  const auto before = observation.errors.size();
  CHECK(Effects_ScriptPackageAllowsAbsentTarget(bytes.data(), bytes.size()) ==
        0);
  CHECK(observation.errors.size() > before);
}

int Validate(const Package& package, unsigned effect = 23, unsigned token = 7,
             int owner = -1) {
  const auto bytes = package.Build();
  return Effects_ValidateDeferredScriptTarget(effect, token, owner,
                                              bytes.data(), bytes.size());
}

Bytes ParticleTimelineProgram() {
  Script script;
  script.Op(14);
  script.SetRegister(0, 0);
  script.Op(32);
  script.Op(0);
  script.LoadTimeline(0);
  script.Op(42);
  script.Halfword(0xffff);
  script.Op(3);
  return script.bytes;
}

Bytes OriginalGolemPackage() {
  std::ifstream input("Native\\data\\disc\\EFFECT\\E239.P", std::ios::binary);
  REQUIRE(input.is_open());
  Bytes bytes((std::istreambuf_iterator<char>(input)),
              std::istreambuf_iterator<char>());
  REQUIRE_FALSE(input.bad());
  REQUIRE(bytes.size() == 8140);
  return bytes;
}

std::size_t BlockPayload(const Bytes& bytes, unsigned wanted) {
  std::size_t cursor = 0;
  while (cursor + 4 <= bytes.size()) {
    const unsigned kind = bytes[cursor + 2] | (bytes[cursor + 3] << 8);
    const unsigned length = bytes[cursor] | (bytes[cursor + 1] << 8);
    REQUIRE(length >= 4);
    REQUIRE(length <= bytes.size() - cursor);
    if (kind == wanted) return cursor + 4;
    cursor += length;
  }
  FAIL("required original package block missing");
  return 0;
}
}  // namespace

TEST_CASE("Absent-target package proof is read-only and repeatable",
          "[effects][target-requirements]") {
  Observation observation;
  Package package;
  auto bytes = package.Build();
  const auto original = bytes;
  for (int iteration = 0; iteration < 3; ++iteration) {
    CHECK(Effects_ScriptPackageAllowsAbsentTarget(bytes.data(), bytes.size()) ==
          1);
  }
  CHECK(bytes == original);
  CHECK(observation.errors.empty());
  package.parameters[21] = 0;
  package.transforms.clear();
  CHECK(package.Classify() == 1);
}

TEST_CASE("World CFG follows branches and yields without scanning operands",
          "[effects][target-requirements]") {
  Observation observation;
  Package package;
  package.Timeline(7, 8);
  Script script;
  script.SetRegister(0, 0);
  script.LoadTimeline(0);
  script.Op(14);
  const auto loop = static_cast<std::uint16_t>(script.Offset());
  script.Op(12);
  script.Op(42);
  script.Halfword(0xffff);  // Unused child offset: no child timeline tracks.
  script.Op(11);
  script.Op(33);
  script.Op(43);
  const auto finished = script.Offset();
  script.Halfword(0);
  script.Yield(loop);
  const auto drain = static_cast<std::uint16_t>(script.Offset());
  script.Op(47);
  script.Jump(static_cast<std::uint16_t>(script.Offset() + 5));
  script.Op(0xff);  // Unreachable padding, not an instruction.
  script.Op(0xff);
  const auto branch = script.Offset();
  script.BranchParticles(0, static_cast<std::uint16_t>(branch + 7));
  script.Yield(drain);
  script.Op(44);
  script.Op(3);
  Put16(script.bytes, finished, drain);
  package.program = script.bytes;
  CHECK(package.Classify() == 1);
  CHECK(observation.errors.empty());
}

TEST_CASE("Absent-target proof excludes unproved primary entry points",
          "[effects][target-requirements]") {
  Observation observation;
  Package package;
  // Entry zero ends; a second primary track would start at offset three.
  package.program = {3, 0xff, 0xff, 46};
  REQUIRE(package.Classify() == 1);
  REQUIRE(observation.errors.empty());
  SECTION("zero primary tracks are outside the allowance") {
    package.parameters[20] = 0;
  }
  SECTION("second entry consumes target through paired colour presets") {
    package.parameters[20] = 2;
  }
  SECTION("even a benign second entry has not been proved") {
    package.program[3] = 3;
    package.parameters[20] = 2;
  }
  CheckInvalid(package.Build(), observation);
}

TEST_CASE("Transform target modes distinguish direct and emitTo attachment",
          "[effects][target-requirements]") {
  Observation observation;
  Package package;
  SECTION("direct actor attachment is not emitTo substitution") {
    Put16(package.transforms, 0, 2);
    package.transforms[2] = 2;
    package.transforms[3] = 11;
    CHECK(package.Classify() == 1);
  }
  SECTION("emitTo attachment requires target") {
    Put16(package.transforms, 0, 0x12);
    CHECK(package.Classify() == 0);
  }
  SECTION("emitTo facing requires target") {
    Put16(package.transforms, 0, 0x880);
    CHECK(package.Classify() == 0);
  }
  SECTION("inactive target mode bits do not consume emitTo") {
    Put16(package.transforms, 0, 0x810);
    CHECK(package.Classify() == 1);
  }
}

TEST_CASE("Only guarded target colour is optional among timeline consumers",
          "[effects][target-requirements]") {
  Observation observation;
  Package package;
  SECTION("source attachment does not consume target") {
    Put16(package.transforms, 0, 0xa);
    CHECK(package.Classify() == 1);
  }
  SECTION("optional target colour has eight-byte keys") {
    package.Timeline(7, 8, 2);
    CHECK(package.Classify() == 1);
  }
  SECTION("scene colour has eight-byte keys") {
    package.Timeline(9, 8);
    CHECK(package.Classify() == 1);
  }
}

TEST_CASE("Timeline children renderers and action-control remain unproven",
          "[effects][target-requirements]") {
  Observation observation;
  Package package;
  SECTION("action-control timeline requires target") {
    package.Timeline(12, 4);
    CHECK(package.Classify() == 0);
  }
  SECTION("child spawning is outside the proof") {
    package.Timeline(3, 12);
    CHECK(package.Classify() == 0);
  }
  SECTION("renderer timeline is outside the proof") {
    package.Timeline(4, 8);
    CHECK(package.Classify() == 0);
  }
}

TEST_CASE("Particle definitions and four-byte timeline events are bounded",
          "[effects][target-requirements]") {
  Observation observation;
  Package package;
  package.Particle();
  package.Timeline(1, 4);
  package.program = ParticleTimelineProgram();
  REQUIRE(package.Classify() == 1);
  SECTION("radial direction is target-independent") {
    Put32(package.particles, 8, 0x2000);
    CHECK(package.Classify() == 1);
  }
  SECTION("target aim is mandatory") {
    Put32(package.particles, 8, 0x6000);
    CHECK(package.Classify() == 0);
  }
  SECTION("unproven aim is conservative") {
    Put32(package.particles, 8, 0x4000);
    CHECK(package.Classify() == 0);
  }
  SECTION("end events are conservative") {
    package.particles[6] = 1;
    CHECK(package.Classify() == 0);
  }
  SECTION("tick events are conservative") {
    package.particles[7] = 1;
    CHECK(package.Classify() == 0);
  }
}

TEST_CASE("Particle event and transform indices cannot escape definitions",
          "[effects][target-requirements]") {
  Observation observation;
  Package package;
  package.Particle();
  package.Timeline(1, 4);
  SECTION("timeline event indices are six-bit fields") {
    Put32(package.timelines, 16, 1 << 20);
    CheckInvalid(package.Build(), observation);
  }
  SECTION("transform index cannot escape its definition count") {
    package.particles[4 + 0xc8] = 1;
    CheckInvalid(package.Build(), observation);
  }
  SECTION("particle position curve remains a validated table index") {
    package.particles[4 + 8] = 1;
    CheckInvalid(package.Build(), observation);
  }
}

TEST_CASE("Packed particle size is a scalar rather than a sampled curve index",
          "[effects][target-requirements]") {
  Observation observation;
  Package package;
  package.Particle();
  for (const std::uint32_t size : {0, 1, 21, 45, 63}) {
    CAPTURE(size);
    Put32(package.particles, 8, size << 26);
    CHECK(package.Classify() == 1);
  }
  CHECK(observation.errors.empty());
}

TEST_CASE("Unproven world opcodes are never optional",
          "[effects][target-requirements]") {
  Observation observation;
  for (const auto opcode :
       {2, 5, 6, 7, 8, 9, 10, 15, 18, 21, 23, 31, 35, 45, 46, 48, 49, 255}) {
    CAPTURE(opcode);
    Package package;
    package.program = {static_cast<std::uint8_t>(opcode), 3};
    CHECK(package.Classify() == 0);
  }
}

TEST_CASE("Malformed package headers and required source spans log errors",
          "[effects][target-requirements]") {
  Observation observation;
  Package package;
  auto bytes = package.Build();
  SECTION("null pointer") {
    CHECK(Effects_ScriptPackageAllowsAbsentTarget(nullptr, bytes.size()) == 0);
    CHECK_FALSE(observation.errors.empty());
    return;
  }
  SECTION("unrepresentable input span is rejected before reading") {
    CHECK(Effects_ScriptPackageAllowsAbsentTarget(
              bytes.data(), std::numeric_limits<std::size_t>::max()) == 0);
    CHECK_FALSE(observation.errors.empty());
    return;
  }
  SECTION("truncated metadata") { bytes.resize(3); }
  CheckInvalid(bytes, observation);
}

TEST_CASE("Malformed package block metadata logs errors",
          "[effects][target-requirements]") {
  Observation observation;
  const Package package;
  auto bytes = package.Build();
  SECTION("missing terminator") { bytes.resize(bytes.size() - 4); }
  SECTION("invalid tiny length") { Put16(bytes, 0, 3); }
  SECTION("zero length") { Put16(bytes, 0, 0); }
  SECTION("length extends beyond supplied span") { Put16(bytes, 0, 0xffff); }
  SECTION("duplicate block") {
    bytes.resize(bytes.size() - 4);
    Package::Block(bytes, 8, {3});
    Append32(bytes, 0);
  }
  SECTION("empty optional block is not silently treated as missing") {
    bytes.resize(bytes.size() - 4);
    Package::Block(bytes, 1, {});
    Append32(bytes, 0);
  }
  SECTION("unsupported block") { Put16(bytes, 2, 6); }
  SECTION("unsupported resource block") { Put16(bytes, 2, 12); }
  SECTION("trailing bytes") { bytes.push_back(1); }
  SECTION("required block absent") { Put16(bytes, 2, 11); }
  CheckInvalid(bytes, observation);
}

TEST_CASE("Definition counts and strides cannot escape package blocks",
          "[effects][target-requirements]") {
  Observation observation;
  Package package;
  package.Timeline(7, 8);
  SECTION("truncated parameters") { package.parameters.resize(25); }
  SECTION("native transform capacity") { package.parameters[21] = 17; }
  SECTION("truncated transform stride") { package.transforms.pop_back(); }
  SECTION("missing declared transform") { package.parameters[21] = 2; }
  SECTION("truncated particle stride") {
    package.Particle();
    package.particles.pop_back();
  }
  CheckInvalid(package.Build(), observation);
}

TEST_CASE("Declared particle count excludes unreferenced trailing storage",
          "[effects][target-requirements]") {
  Observation observation;
  Package package;
  package.Particle();
  SECTION("unreferenced nonzero tail does not imply a partial definition") {
    package.particles.insert(package.particles.end(), 16, 0xff);
  }
  SECTION("unreferenced complete record does not become an event definition") {
    const auto tail = package.particles.size();
    package.particles.resize(tail + 0xcc);
    Put32(package.particles, tail + 4, 0x6000);
    package.particles[tail + 2] = 1;
  }
  SECTION("zero declared count does not parse residual definition storage") {
    Put32(package.particles, 0, 0);
    Put32(package.particles, 8, 0x6000);
  }
  CHECK(package.Classify() == 1);
  CHECK(observation.errors.empty());
}

TEST_CASE("Particle counts and declared definition spans must be bounded",
          "[effects][target-requirements]") {
  Observation observation;
  Package package;
  package.Particle();
  SECTION("truncated count field") { package.particles.resize(3); }
  SECTION("count exceeds native event index capacity") {
    Put32(package.particles, 0, 65);
  }
  SECTION("count multiplication cannot wrap") {
    Put32(package.particles, 0, 0xffffffff);
  }
  SECTION("declared second definition is missing") {
    Put32(package.particles, 0, 2);
  }
  SECTION("timeline cannot consume an undeclared record in trailing storage") {
    Put32(package.particles, 0, 0);
    package.Timeline(1, 4);
  }
  CheckInvalid(package.Build(), observation);
}

TEST_CASE("Relative timeline offsets and key storage cannot overlap or escape",
          "[effects][target-requirements]") {
  Observation observation;
  Package package;
  package.Timeline(7, 8);
  SECTION("overflowing timeline count") {
    Put32(package.timelines, 0, 0xffffffff);
  }
  SECTION("section offset out of bounds") {
    Put16(package.timelines, 4, 0xffff);
  }
  SECTION("section overlaps offset table") { Put16(package.timelines, 4, 0); }
  SECTION("overflowing track count") {
    Put32(package.timelines, 8, 0xffffffff);
  }
  SECTION("key offset outside block") { Put16(package.timelines, 14, 0xffff); }
  SECTION("key storage overlaps section headers") {
    Put16(package.timelines, 14, 12);
  }
  SECTION("two tracks alias the same key storage") {
    package.timelines.resize(28);
    Put32(package.timelines, 8, 2);
    Put16(package.timelines, 14, 20);
    package.timelines[16] = 7;
    package.timelines[17] = 1;
    Put16(package.timelines, 18, 20);
  }
  SECTION("key count exceeds remaining payload") {
    package.timelines[13] = 255;
  }
  SECTION("eight-byte colour key is not a four-byte particle key") {
    package.timelines.resize(20);
  }
  SECTION("no initial key") { package.timelines[13] = 0; }
  SECTION("unsupported track kind") { package.timelines[12] = 0xff; }
  CheckInvalid(package.Build(), observation);
}

TEST_CASE("Curve tables require bounded counts nonzero sizes and valid offsets",
          "[effects][target-requirements]") {
  Observation observation;
  Package package;
  SECTION("curve count exceeds native capacity") {
    Put16(package.curves, 0, 33);
  }
  SECTION("curve data offset outside block") {
    Put16(package.curves, 2, 0xffff);
  }
  SECTION("zero-length curve would divide by zero") {
    package.curves = {1, 0, 5, 0, 0};
  }
  SECTION("truncated curve span") { package.curves = {1, 0, 5, 0, 8, 0}; }
  CheckInvalid(package.Build(), observation);
}

TEST_CASE("Every truncated prefix of a synthetic package logs and refuses",
          "[effects][target-requirements]") {
  Observation observation;
  const Package package;
  const auto bytes = package.Build();
  for (std::size_t length = 0; length < bytes.size(); ++length) {
    CAPTURE(length);
    const auto before = observation.errors.size();
    CHECK(Effects_ScriptPackageAllowsAbsentTarget(bytes.data(), length) == 0);
    CHECK(observation.errors.size() > before);
  }
}

TEST_CASE("Ancillary resources are bounded without invoking asset consumers",
          "[effects][target-requirements]") {
  Observation observation;
  Package package;
  package.Particle();
  SECTION("texture table offset overflow") {
    Put32(package.textures, 4, 0xffffffff);
  }
  SECTION("texture frame references unavailable descriptor") {
    Put16(package.textures, 10, 2);
  }
  SECTION("texture descriptor stride truncation") {
    package.textures.pop_back();
  }
  SECTION("animation count overflow") {
    Put32(package.animations, 0, 0xffffffff);
  }
  SECTION("animation offset aliases header") {
    Put16(package.animations, 4, 0);
  }
  SECTION("animation terminator missing") { package.animations.resize(12); }
  SECTION("unknown animation command") {
    Put32(package.animations, 8, 0xf0000000);
  }
  SECTION("animation loop cannot spin without yielding") {
    Put32(package.animations, 8, 0x20000000);
  }
  SECTION("animation frame index outside bound texture table") {
    Put32(package.animations, 8, (2 << 16) | 3);
  }
  SECTION("particle animation index outside table") {
    package.particles[4] = 1;
  }
  SECTION("segmented table count overflow") {
    package.segments = Bytes(4);
    Put32(package.segments, 0, 0xffffffff);
  }
  CheckInvalid(package.Build(), observation);
}

TEST_CASE("CFG rejects invalid targets registers and inconsistent extents",
          "[effects][target-requirements]") {
  Observation observation;
  Package package;
  SECTION("truncated instruction") { package.program = {16, 0, 0}; }
  SECTION("branch outside program") { package.program = {1, 0xff, 0xff}; }
  SECTION("branch into own operands") { package.program = {1, 1, 0}; }
  SECTION("future instruction overlaps a queued successor") {
    package.program = {22, 0, 5, 0, 16, 0, 0, 0, 3};
  }
  SECTION("fallthrough outside program") { package.program = {40}; }
  SECTION("invalid register write") { package.program = {16, 4, 0, 0, 3}; }
  SECTION("invalid register compare") {
    package.program = {17, 4, 0, 0, 6, 0, 3};
  }
  SECTION("invalid register resource read") { package.program = {41, 4, 3}; }
  SECTION("timeline not loaded on entry") { package.program = {42, 0, 0, 3}; }
  SECTION("timeline read after free") {
    package.Timeline(9, 8);
    package.program = {41, 0, 44, 43, 6, 0, 3};
  }
  SECTION("transforms not initialized") { package.program = {12, 3}; }
  SECTION("resource register out of range") {
    package.Timeline(9, 8);
    package.program = {16, 0, 255, 0, 41, 0, 3};
  }
  SECTION("conflicting constants at a CFG join") {
    package.timelines.assign(40, 0);
    Put32(package.timelines, 0, 2);
    Put16(package.timelines, 4, 8);
    Put16(package.timelines, 6, 24);
    for (const auto section : {8, 24}) {
      Put32(package.timelines, section, 1);
      package.timelines[section + 4] = 9;
      package.timelines[section + 5] = 1;
      Put16(package.timelines, section + 6,
            static_cast<std::uint16_t>(section + 8));
    }
    package.program = {22, 0, 11, 0, 16, 0, 1,  0, 1,
                       15, 0, 16, 0, 0,  0, 41, 0, 3};
  }
  CheckInvalid(package.Build(), observation);
}

TEST_CASE("Deferred target records reset or replace the primary request",
          "[effects][target-requirements][deferred-target]") {
  Observation observation;
  Package package;
  SECTION("ordinary requests remain unchanged without reading source") {
    CHECK(Effects_ValidateDeferredScriptTarget(999, 999, -2, nullptr, 0) == 1);
    CHECK(observation.errors.empty());
  }
  SECTION("reset prevents stale identity on next ordinary request") {
    Effects_RecordDeferredScriptTarget(23, 101, 7);
    Effects_ResetDeferredScriptTarget();
    CHECK(Effects_ValidateDeferredScriptTarget(24, 8, -2, nullptr, 0) == 1);
    CHECK(observation.errors.empty());
  }
  SECTION("recording a new primary request replaces the old request") {
    Effects_RecordDeferredScriptTarget(22, 100, 6);
    Effects_RecordDeferredScriptTarget(23, 101, 7);
    CHECK(Validate(package) == 1);
  }
}

TEST_CASE("Deferred absent owners require a proof for the exact request",
          "[effects][target-requirements][deferred-target]") {
  Observation observation;
  Package package;
  SECTION("genuinely absent target requires package proof") {
    Effects_RecordDeferredScriptTarget(23, 101, 7);
    CHECK(Validate(package) == 1);
    CHECK(observation.errors.empty());
    CHECK(Effects_ValidateDeferredScriptTarget(23, 7, -2, nullptr, 0) == 1);
  }
  SECTION("mandatory target still refuses absent owner") {
    Put16(package.transforms, 0, 0x12);
    Effects_RecordDeferredScriptTarget(23, 101, 7);
    CHECK(Validate(package) == 0);
    CHECK_FALSE(observation.errors.empty());
  }
}

TEST_CASE("Deferred unknown packages and mismatched identities remain errors",
          "[effects][target-requirements][deferred-target]") {
  Observation observation;
  Package package;
  SECTION("unknown mandatory instruction cannot use an absent owner") {
    package.program = {5};
    Effects_RecordDeferredScriptTarget(23, 101, 7);
    CHECK(Validate(package) == 0);
    CHECK_FALSE(observation.errors.empty());
  }
  SECTION("effect mismatch is logged and consumed") {
    Effects_RecordDeferredScriptTarget(23, 101, 7);
    CHECK(Validate(package, 24) == 0);
    CHECK_FALSE(observation.errors.empty());
    CHECK(Effects_ValidateDeferredScriptTarget(23, 7, -2, nullptr, 0) == 1);
  }
  SECTION("token mismatch is logged and consumed") {
    Effects_RecordDeferredScriptTarget(23, 101, 7);
    CHECK(Validate(package, 23, 8) == 0);
    CHECK_FALSE(observation.errors.empty());
    CHECK(Effects_ValidateDeferredScriptTarget(23, 7, -2, nullptr, 0) == 1);
  }
}

TEST_CASE("Deferred execution never substitutes a newly mismatched real owner",
          "[effects][target-requirements][deferred-target]") {
  Observation observation;
  Package package;
  Effects_RecordDeferredScriptTarget(23, 101, 7);
  SECTION("exact newly available owner needs no absent-target exemption") {
    CHECK(Effects_ValidateDeferredScriptTarget(23, 7, 101, nullptr, 0) == 1);
    CHECK(observation.errors.empty());
  }
  SECTION("a different newly available model is rejected") {
    CHECK(Validate(package, 23, 7, 100) == 0);
    CHECK_FALSE(observation.errors.empty());
  }
  SECTION("an existing unprovable owner is rejected") {
    CHECK(Validate(package, 23, 7, -2) == 0);
    CHECK_FALSE(observation.errors.empty());
  }
  CHECK(Effects_ValidateDeferredScriptTarget(999, 999, -2, nullptr, 0) == 1);
}

TEST_CASE("Deferred target validation consumes records before reentry",
          "[effects][target-requirements][deferred-target]") {
  Observation observation;
  const Package package;
  Effects_RecordDeferredScriptTarget(23, 101, 7);
  SECTION("model one is not an absent sentinel allowance") {
    CHECK(Validate(package, 23, 7, 1) == 0);
    CHECK_FALSE(observation.errors.empty());
  }
  SECTION("failed identity is consumed before logger reentry") {
    bool reentered = false;
    const auto previous =
        VS::Core::SetLogObserver([&reentered](const VS::Core::LogRecord&) {
          reentered = true;
          CHECK(Effects_ValidateDeferredScriptTarget(23, 7, -2, nullptr, 0) ==
                1);
        });
    CHECK(Validate(package, 24) == 0);
    VS::Core::SetLogObserver(previous);
    CHECK(reentered);
  }
  CHECK(Effects_ValidateDeferredScriptTarget(999, 999, -2, nullptr, 0) == 1);
}

TEST_CASE("Original Golem package proves only its optional target dependency",
          "[.assets][effects][target-requirements-source]") {
  Observation observation;
  const auto bytes = OriginalGolemPackage();
  CHECK(Effects_ScriptPackageAllowsAbsentTarget(bytes.data(), bytes.size()) ==
        1);
  CHECK(observation.errors.empty());

  auto mandatory = bytes;
  Put16(mandatory, BlockPayload(mandatory, 10), 0x12);
  CHECK(Effects_ScriptPackageAllowsAbsentTarget(mandatory.data(),
                                                mandatory.size()) == 0);
  Effects_RecordDeferredScriptTarget(239, 101, 42);
  CHECK(Effects_ValidateDeferredScriptTarget(239, 42, -1, mandatory.data(),
                                             mandatory.size()) == 0);
  CHECK_FALSE(observation.errors.empty());
  Effects_RecordDeferredScriptTarget(239, 101, 42);
  CHECK(Effects_ValidateDeferredScriptTarget(239, 42, -1, bytes.data(),
                                             bytes.size()) == 1);
}
