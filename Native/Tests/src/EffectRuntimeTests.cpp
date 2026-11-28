#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <iterator>
#include <map>
#include <set>
#include <span>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Battle/CombatActionSession.hpp"
#include "Content/DiscQueue.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Effects/CameraLiveState.hpp"
#include "Effects/EffectInstances.hpp"
#include "Effects/EffectInstancesLive.hpp"
#include "Effects/EffectResourcesLive.hpp"
#include "Effects/EffectRuntime.hpp"
#include "Effects/EffectRuntimeLive.hpp"
#include "Effects/PrimaryEffect.hpp"
#include "Effects/PrimaryEffectLive.hpp"
#include "Effects/Runtime.hpp"

namespace {
namespace fx = VS::Effects;
using fx::PrimaryEffectPhase;
using fx::PrimaryEffectState;
using fx::SecondaryEffectPhase;
using Instance = fx::LiveEffectInstance;
using Slot = fx::LiveRendererSlot;
using Log = std::vector<std::string>;

// A chain of instances kept outside the records, as both runtimes walk it.
class FakeChain {
 public:
  explicit FakeChain(std::span<Instance> instances) : instances_(instances) {}

  void Link(std::initializer_list<std::size_t> order) {
    chain_.clear();
    for (const std::size_t index : order) {
      chain_.push_back(&instances_[index]);
    }
  }
  [[nodiscard]] Instance* First() const {
    return chain_.empty() ? nullptr : chain_.front();
  }
  [[nodiscard]] Instance* Next(const Instance& instance) const {
    const auto found = std::ranges::find(chain_, &instance);
    return found == chain_.end() || std::next(found) == chain_.end()
               ? nullptr
               : *std::next(found);
  }
  void Remove(const Instance& instance) { std::erase(chain_, &instance); }
  [[nodiscard]] std::string Name(const Instance& instance) const {
    return "i" + std::to_string(&instance - instances_.data());
  }

 private:
  std::span<Instance> instances_;
  std::vector<Instance*> chain_;
};

// The playback both fakes share: a frame per instance and the scripts that
// have ended.
template <class Host>
class FakePlayback : public Host {
 public:
  FakePlayback(std::span<Instance> instances, Log& log)
      : chain(instances), log_(&log) {}

  Instance* Next(Instance& instance) override { return chain.Next(instance); }
  void SetCurrent(Instance& instance) override {
    Record("current " + chain.Name(instance));
  }
  bool HasCurves() override { return curves; }
  void SampleCurves(std::uint16_t frame) override {
    Record("sample " + std::to_string(frame));
  }
  void UpdateParticleForces() override { Record("forces"); }
  bool RunScript(Instance& instance) override {
    Record("script " + chain.Name(instance));
    return !ended.contains(&instance);
  }
  std::uint16_t Frame(Instance& instance) override { return frames[&instance]; }
  void SetFrame(Instance& instance, std::uint16_t frame) override {
    Record("frame " + chain.Name(instance) + " " + std::to_string(frame));
    frames[&instance] = frame;
  }
  void ReleaseParticlePool() override { Record("release pool"); }
  void SelectPackage(fx::ActivePackage package) override {
    Record("select " + std::to_string(std::to_underlying(package)));
  }

  FakeChain chain;
  bool curves = true;
  std::set<const Instance*> ended;
  std::map<const Instance*, std::uint16_t> frames;

 protected:
  void Record(std::string entry) { log_->push_back(std::move(entry)); }

 private:
  Log* log_;
};

class FakePrimary final : public FakePlayback<fx::PrimaryEffectRuntime> {
 public:
  using FakePlayback::FakePlayback;

  PrimaryEffectState Primary() override { return state; }
  void SetPrimary(const PrimaryEffectState& next) override {
    Record("primary " + std::to_string(std::to_underlying(next.phase)));
    state = next;
  }
  std::uint8_t PrimaryToken() override { return token; }
  void SetPrimaryToken(std::uint8_t next) override {
    Record("token " + std::to_string(next));
    token = next;
  }
  Instance* FirstPrimary() override { return chain.First(); }
  VS::Content::DiscQueueSlot* MenuReloadSlot() override { return reload; }
  void SetMenuReloadSlot(VS::Content::DiscQueueSlot* slot) override {
    Record(slot == nullptr ? "reload slot none" : "reload slot");
    reload = slot;
  }

  bool PollPurgeOverlay() override { return Poll("purge"); }
  bool PollTextures() override { return Poll("textures"); }
  bool PollPlugins() override { return Poll("plugins"); }
  bool PollPackage() override { return Poll("package"); }
  void WaitForOverlay() override { Record("wait"); }
  void ConfigureResources() override { Record("configure"); }
  bool ActionHasEffect() override { return has_effect; }
  bool PackageOnDisc() override { return on_disc; }
  bool PurgeOverlayHeld() override { return purge_held; }
  void ReleasePurgeOverlay() override {
    Record("release purge");
    purge_held = false;
  }
  void BindPackage() override { Record("bind"); }
  std::uint8_t PackageTrackCount() override { return track_count; }
  VS::Content::DiscQueueSlot* RequestMenuReload() override {
    Record("request reload");
    return reload_slot;
  }
  void EnqueueMenuReload(VS::Content::DiscQueueSlot& /*slot*/) override {
    Record("enqueue reload");
  }
  bool MenuReloadLoaded(VS::Content::DiscQueueSlot& /*slot*/) override {
    return reload_loaded;
  }
  void ReleaseMenuReload(VS::Content::DiscQueueSlot& /*slot*/) override {
    Record("release reload");
  }

  bool SpawnTrack(std::int32_t script_offset,
                  std::uint8_t spawn_token) override {
    Record("spawn " + std::to_string(script_offset) + " " +
           std::to_string(spawn_token));
    return has_package;
  }
  void DestroyPrimary(Instance& instance) override {
    Record("destroy " + chain.Name(instance));
    chain.Remove(instance);
  }

  bool HasSfxBank() override { return sfx_bank; }
  void UnbindSfxBank() override { Record("unbind sfx"); }
  bool HasCutsceneStream() override { return stream; }
  void UnbindCutsceneStream() override { Record("unbind stream"); }
  void StopVibration() override { Record("stop vibration"); }
  void ReleasePackageResources() override { Record("release package"); }
  void ReleaseToken(std::uint8_t released) override {
    Record("release token " + std::to_string(released));
  }
  void ResetCameraMotion() override { Record("reset camera"); }

  void StoreActionRequest() override { Record("store request"); }
  void SetActionVariant(std::uint8_t variant) override {
    Record("variant " + std::to_string(variant));
  }
  std::uint8_t AllocateToken() override { return allocated_token; }
  void CaptureSavedCamera() override { Record("capture saved"); }
  void CaptureEffectCamera() override { Record("capture effect"); }

  bool HasTimeline(Instance& instance) override {
    return timelines.contains(&instance);
  }
  void FreeTimeline(Instance& instance) override {
    Record("free timeline " + chain.Name(instance));
    timelines.erase(&instance);
  }

  PrimaryEffectState state;
  std::uint8_t token = 0;
  VS::Content::DiscQueueSlot* reload = nullptr;
  VS::Content::DiscQueueSlot* reload_slot = nullptr;
  std::set<std::string> done;
  bool has_effect = false;
  bool on_disc = true;
  bool purge_held = false;
  std::uint8_t track_count = 0;
  bool reload_loaded = false;
  bool has_package = true;
  bool sfx_bank = false;
  bool stream = false;
  std::uint8_t allocated_token = 0;
  std::set<const Instance*> timelines;

 private:
  bool Poll(const std::string& loader) {
    Record("poll " + loader);
    return done.contains(loader);
  }
};

class FakeSecondary final : public FakePlayback<fx::SecondaryEffectRuntime> {
 public:
  FakeSecondary(std::span<Instance> instances, std::span<Slot> slots, Log& log)
      : FakePlayback(instances, log), slots_(slots) {}

  Instance* FirstSecondary() override { return chain.First(); }
  std::int16_t PhaseOf(Instance& instance) override {
    return phases[&instance];
  }
  void SetPhase(Instance& instance, SecondaryEffectPhase phase) override {
    Record("phase " + chain.Name(instance) + " " +
           std::to_string(std::to_underlying(phase)));
    phases[&instance] = std::to_underlying(phase);
  }
  void AcquireParticlePool() override { Record("acquire pool"); }
  Slot* FirstRenderer(Instance& instance) override {
    const auto& order = renderers[&instance];
    return order.empty() ? nullptr : order.front();
  }
  Slot* NextRenderer(Slot& slot) override {
    for (const auto& [instance, order] : renderers) {
      const auto found = std::ranges::find(order, &slot);
      if (found != order.end()) {
        return std::next(found) == order.end() ? nullptr : *std::next(found);
      }
    }
    return nullptr;
  }
  bool UpdateRenderer(Slot& slot) override {
    Record("update " + SlotName(slot));
    return !finished.contains(&slot);
  }
  void DetachRenderer(Instance& instance, Slot& slot) override {
    Record("detach " + chain.Name(instance) + " " + SlotName(slot));
    std::erase(renderers[&instance], &slot);
  }
  std::uint8_t BusyBits(Instance& instance) override {
    return destroyed.contains(&instance) ? 0xFF : busy[&instance];
  }
  void DestroySecondary(Instance& instance) override {
    Record("destroy " + chain.Name(instance));
    chain.Remove(instance);
    destroyed.insert(&instance);
  }

  std::map<const Instance*, std::int16_t> phases;
  std::map<const Instance*, std::vector<Slot*>> renderers;
  std::set<const Slot*> finished;
  std::map<const Instance*, std::uint8_t> busy;
  std::set<const Instance*> destroyed;

 private:
  [[nodiscard]] std::string SlotName(const Slot& slot) const {
    return "r" + std::to_string(&slot - slots_.data());
  }

  std::span<Slot> slots_;
};

class FakeStartup final : public fx::EffectRuntimeStartup {
 public:
  explicit FakeStartup(Log& log) : log_(&log) {}
  void ResetInstances() override { log_->emplace_back("reset"); }
  void InitializeResources() override { log_->emplace_back("resources"); }
  void ResetParticlePoolReferences() override { log_->emplace_back("pool"); }

 private:
  Log* log_;
};

class FakeFrame final : public fx::EffectRuntimeFrame {
 public:
  explicit FakeFrame(Log& log) : log_(&log) {}
  std::uint8_t BufferIndex() override { return buffer; }
  void SetBufferIndex(std::uint8_t index) override { buffer = index; }
  void SelectPackage(fx::ActivePackage package) override {
    log_->push_back("select " + std::to_string(std::to_underlying(package)));
  }
  bool UpdatePrimary() override {
    log_->emplace_back("primary");
    return primary_busy;
  }
  std::uint8_t UpdateSecondary() override {
    log_->emplace_back("secondary");
    return secondary_busy;
  }
  PrimaryEffectPhase PrimaryPhase() override { return phase; }
  void UpdateCameraMotion() override { log_->emplace_back("camera"); }

  std::uint8_t buffer = 0;
  bool primary_busy = false;
  std::uint8_t secondary_busy = 0;
  PrimaryEffectPhase phase = PrimaryEffectPhase::kIdle;

 private:
  Log* log_;
};

PrimaryEffectState InPhase(PrimaryEffectPhase phase) {
  return {.phase = phase};
}
}  // namespace

namespace {
struct ResetRecords {
  std::array<Instance, 2> instances{};
  VS::Content::DiscQueueSlot slot{};
  fx::EffectRuntimeRecords records{
      .primary = {.phase = PrimaryEffectPhase::kPlayingTracks,
                  .track_count = 5,
                  .started_tracks = 3,
                  .pending_tracks = 0x18,
                  .loaded = true},
      .primary_token = 33,
      .primary_instances = &instances[0],
      .secondary_instances = &instances[1],
      .current_instance = &instances[1],
      .unused_primary_word = 7,
      .first_curve_sample = 9,
      .action_variant = 4,
      .menu_reload_slot = &slot,
      .tokens = {3, 5, 9, 17}};

  ResetRecords() { fx::ResetEffectInstances(records); }
};
}  // namespace

TEST_CASE(
    "F01421 the reset clears the primary state but keeps the pending "
    "tracks",
    "[effect-runtime][F01421]") {
  const ResetRecords reset;
  const fx::EffectRuntimeRecords& records = reset.records;
  CHECK(records.primary.phase == PrimaryEffectPhase::kIdle);
  CHECK(records.primary.track_count == 0);
  CHECK(records.primary.started_tracks == 0);
  CHECK(records.primary.pending_tracks == 0x18);
  CHECK_FALSE(records.primary.loaded);
  CHECK(records.primary_token == 0);
}

TEST_CASE("F01421 the reset clears the instance links, samples and tokens",
          "[effect-runtime][F01421]") {
  const ResetRecords reset;
  const fx::EffectRuntimeRecords& records = reset.records;
  CHECK(records.primary_instances == nullptr);
  CHECK(records.secondary_instances == nullptr);
  CHECK(records.current_instance == nullptr);
  CHECK(records.unused_primary_word == 0);
  CHECK(records.first_curve_sample == 0);
  CHECK(records.action_variant == 0);
  CHECK(records.menu_reload_slot == nullptr);
  CHECK(records.tokens == fx::EffectTokenPool{});
}

TEST_CASE("F01420 the start-up resets the instances, then the resources",
          "[effect-runtime][F01420]") {
  Log log;
  FakeStartup startup(log);
  fx::InitEffectRuntime(startup);
  CHECK(log == Log{"reset", "resources", "pool"});
}

TEST_CASE("F01425 an idle primary effect is not busy and unknown phases are",
          "[effect-runtime][F01425]") {
  std::array<Instance, 1> instances{};
  Log log;
  FakePrimary primary(instances, log);
  CHECK_FALSE(fx::UpdatePrimaryEffects(primary));
  const auto phase = GENERATE(1U, 5U, 7U, 10U, 0xFFFFFFFFU);
  primary.state.phase = static_cast<PrimaryEffectPhase>(phase);
  CHECK(fx::UpdatePrimaryEffects(primary));
  CHECK(log.empty());
}

namespace {
struct PurgeOverlayLoad {
  std::array<Instance, 1> instances{};
  Log log;
  FakePrimary primary{instances, log};

  PurgeOverlayLoad() {
    primary.state = {.phase = PrimaryEffectPhase::kLoadingPurgeOverlay,
                     .track_count = 4,
                     .started_tracks = 2};
    primary.purge_held = true;
  }
  void Finish() {
    primary.done.insert("purge");
    CHECK(fx::UpdatePrimaryEffects(primary));
    CHECK(log.at(1) == "wait");
    CHECK(log.at(2) == "configure");
    CHECK(log.back() == "release purge");
    CHECK_FALSE(primary.purge_held);
  }
};
}  // namespace

TEST_CASE("F01425 the purge overlay load waits for the overlay",
          "[effect-runtime][F01425]") {
  PurgeOverlayLoad load;
  CHECK(fx::UpdatePrimaryEffects(load.primary));
  CHECK(load.log == Log{"poll purge"});
  CHECK(load.primary.purge_held);
}

TEST_CASE("F01425 an effect without a package file plays once purged",
          "[effect-runtime][F01425]") {
  PurgeOverlayLoad load;
  load.primary.has_effect = true;
  load.primary.on_disc = false;
  load.Finish();
  CHECK(load.primary.state.phase == PrimaryEffectPhase::kPlayingTracks);
  CHECK(load.primary.state.track_count == 0);
  CHECK(load.primary.state.started_tracks == 0);
}

TEST_CASE("F01425 otherwise the textures load once purged",
          "[effect-runtime][F01425]") {
  PurgeOverlayLoad load;
  std::tie(load.primary.has_effect, load.primary.on_disc) = GENERATE(
      std::pair{false, false}, std::pair{false, true}, std::pair{true, true});
  load.Finish();
  CHECK(load.primary.state.phase == PrimaryEffectPhase::kLoadingTextures);
  CHECK(load.primary.state.track_count == 4);
  CHECK(load.primary.state.started_tracks == 2);
}

TEST_CASE("F01425 the texture and plugin loads advance once done",
          "[effect-runtime][F01425]") {
  std::array<Instance, 1> instances{};
  Log log;
  FakePrimary primary(instances, log);
  primary.state = InPhase(PrimaryEffectPhase::kLoadingTextures);
  CHECK(fx::UpdatePrimaryEffects(primary));
  CHECK(primary.state.phase == PrimaryEffectPhase::kLoadingTextures);
  primary.done = {"textures", "plugins"};
  CHECK(fx::UpdatePrimaryEffects(primary));
  CHECK(primary.state.phase == PrimaryEffectPhase::kLoadingPlugins);
  CHECK(fx::UpdatePrimaryEffects(primary));
  CHECK(primary.state.phase == PrimaryEffectPhase::kLoadingPackage);
  CHECK(log == Log{"poll textures", "poll textures", "primary 8",
                   "poll plugins", "primary 3", "wait"});
}

namespace {
struct PackageLoad {
  std::array<Instance, 1> instances{};
  VS::Content::DiscQueueSlot slot{};
  Log log;
  FakePrimary primary{instances, log};

  PackageLoad() {
    primary.state = {.phase = PrimaryEffectPhase::kLoadingPackage,
                     .started_tracks = 6,
                     .pending_tracks = 0x100};
    primary.done.insert("package");
    primary.reload_slot = &slot;
  }
};
}  // namespace

TEST_CASE("F01425 a loaded package queues its tracks",
          "[effect-runtime][F01425]") {
  PackageLoad load;
  const auto [count, mask] = GENERATE(
      std::pair{std::uint8_t{0}, 0x0U}, std::pair{std::uint8_t{3}, 0x7U},
      std::pair{std::uint8_t{31}, 0x7FFFFFFFU},
      std::pair{std::uint8_t{32}, 0xFFFFFFFFU},
      std::pair{std::uint8_t{200}, 0xFFFFFFFFU});
  load.primary.track_count = count;
  CHECK(fx::UpdatePrimaryEffects(load.primary));
  const PrimaryEffectState& state = load.primary.state;
  CHECK(state.phase == PrimaryEffectPhase::kAwaitingTrack);
  CHECK(state.track_count == count);
  CHECK(state.started_tracks == 0);
  CHECK(state.pending_tracks == (mask | 0x100U));
  CHECK(state.loaded);
}

TEST_CASE("F01425 a loaded package requests the menu reload",
          "[effect-runtime][F01425]") {
  PackageLoad load;
  const bool queued = GENERATE(false, true);
  if (!queued) {
    load.primary.reload_slot = nullptr;
  }
  CHECK(fx::UpdatePrimaryEffects(load.primary));
  CHECK(load.primary.reload == load.primary.reload_slot);
  Log expected{"poll package", "bind",           "primary 5",
               "primary 5",    "request reload", "reload slot"};
  if (queued) {
    expected.emplace_back("enqueue reload");
  } else {
    expected.back() = "reload slot none";
  }
  CHECK(load.log == expected);
}

TEST_CASE("F01425 the playing tracks run each instance's frame",
          "[effect-runtime][F01425]") {
  std::array<Instance, 3> instances{};
  Log log;
  FakePrimary primary(instances, log);
  primary.state = {.phase = PrimaryEffectPhase::kPlayingTracks,
                   .track_count = 2,
                   .started_tracks = 1};
  primary.chain.Link({0, 1, 2});
  primary.frames[&instances[0]] = 0xFFFF;
  primary.frames[&instances[2]] = 4;
  primary.ended.insert(&instances[1]);
  primary.curves = GENERATE(false, true);
  CHECK(fx::UpdatePrimaryEffects(primary));
  Log expected;
  for (const auto& [name, frame, next] :
       {std::tuple{"i0", "65535", "frame i0 0"},
        std::tuple{"i1", "0", "destroy i1"},
        std::tuple{"i2", "4", "frame i2 5"}}) {
    expected.push_back(std::string("current ") + name);
    if (primary.curves) {
      expected.push_back(std::string("sample ") + frame);
    }
    expected.emplace_back("forces");
    expected.push_back(std::string("script ") + name);
    expected.emplace_back(next);
  }
  CHECK(log == expected);
  CHECK(primary.state.phase == PrimaryEffectPhase::kPlayingTracks);
}

namespace {
struct LastTrackEnded {
  std::array<Instance, 1> instances{};
  Log log;
  FakePrimary primary{instances, log};

  LastTrackEnded() {
    primary.chain.Link({0});
    primary.ended.insert(&instances[0]);
    primary.curves = false;
    primary.token = 70;
    primary.state = {.phase = PrimaryEffectPhase::kPlayingTracks,
                     .track_count = 3,
                     .started_tracks = 2,
                     .loaded = true};
  }
};
}  // namespace

TEST_CASE("F01425 a track not yet started is awaited",
          "[effect-runtime][F01425]") {
  LastTrackEnded ended;
  CHECK(fx::UpdatePrimaryEffects(ended.primary));
  CHECK(ended.log.back() == "primary 5");
  CHECK(ended.primary.state.loaded);
}

TEST_CASE("F01425 once every track started the action is released",
          "[effect-runtime][F01425]") {
  LastTrackEnded ended;
  FakePrimary& primary = ended.primary;
  primary.state.started_tracks = GENERATE(3, 4);
  primary.sfx_bank = true;
  primary.stream = true;
  primary.purge_held = true;
  CHECK(fx::UpdatePrimaryEffects(primary));
  CHECK(ended.log == Log{"current i0", "forces", "script i0", "destroy i0",
                         "unbind sfx", "unbind stream", "stop vibration",
                         "release package", "release pool", "release token 70",
                         "primary 4", "token 0", "release purge",
                         "reset camera", "primary 9"});
  CHECK_FALSE(primary.state.loaded);
  CHECK(primary.token == 0);
}

TEST_CASE("F01425 instances still playing keep the phase",
          "[effect-runtime][F01425]") {
  LastTrackEnded ended;
  ended.primary.ended.clear();
  CHECK(fx::UpdatePrimaryEffects(ended.primary));
  CHECK(ended.primary.state.phase == PrimaryEffectPhase::kPlayingTracks);
}

TEST_CASE("F01425 the menu reload idles the effect once loaded",
          "[effect-runtime][F01425]") {
  std::array<Instance, 1> instances{};
  VS::Content::DiscQueueSlot slot{};
  Log log;
  FakePrimary primary(instances, log);
  primary.state = InPhase(PrimaryEffectPhase::kReloadingMenu);

  SECTION("without a request the effect idles at once") {
    CHECK_FALSE(fx::UpdatePrimaryEffects(primary));
    CHECK(log == Log{"primary 0"});
  }
  SECTION("a request in flight keeps the effect busy") {
    primary.reload = &slot;
    CHECK(fx::UpdatePrimaryEffects(primary));
    CHECK(log.empty());
    primary.reload_loaded = true;
    CHECK_FALSE(fx::UpdatePrimaryEffects(primary));
    CHECK(log == Log{"release reload", "reload slot none", "primary 0"});
    CHECK(primary.reload == nullptr);
  }
}

TEST_CASE("F01426 the secondary instances start, play and end",
          "[effect-runtime][F01426]") {
  std::array<Instance, 4> instances{};
  std::array<Slot, 3> slots{};
  Log log;
  FakeSecondary secondary(instances, slots, log);
  secondary.curves = false;
  secondary.chain.Link({0, 1, 2, 3});
  secondary.phases = {{&instances[0], 2},
                      {&instances[1], 3},
                      {&instances[2], 4},
                      {&instances[3], 4}};
  secondary.busy = {{&instances[0], 1},
                    {&instances[1], 2},
                    {&instances[2], 0},
                    {&instances[3], 4}};
  secondary.frames[&instances[2]] = 9;
  secondary.renderers[&instances[3]] = {&slots[0], &slots[1], &slots[2]};
  secondary.finished = {&slots[0], &slots[2]};
  secondary.ended.insert(&instances[3]);
  CHECK(fx::UpdateSecondaryEffects(secondary) == 7);
  CHECK(log == Log{"current i0", "current i1", "acquire pool", "phase i1 4",
                   "current i2", "forces", "script i2", "frame i2 10",
                   "current i3", "forces", "update r0", "detach i3 r0",
                   "update r1", "update r2", "detach i3 r2", "script i3",
                   "destroy i3", "release pool"});
  CHECK(secondary.renderers[&instances[3]] == std::vector{&slots[1]});
}

TEST_CASE("F01426 secondary phases the runtime never sets are left",
          "[effect-runtime][F01426]") {
  std::array<Instance, 1> instances{};
  std::array<Slot, 1> slots{};
  Log log;
  FakeSecondary secondary(instances, slots, log);
  secondary.chain.Link({0});
  secondary.phases[&instances[0]] = GENERATE(std::int16_t{0}, std::int16_t{1},
                                             std::int16_t{5}, std::int16_t{-1});
  secondary.busy[&instances[0]] = 6;
  CHECK(fx::UpdateSecondaryEffects(secondary) == 6);
  CHECK(log == Log{"current i0"});
}

TEST_CASE("F01431 a pending track starts its instance",
          "[effect-runtime][F01431]") {
  std::array<Instance, 1> instances{};
  Log log;
  FakePrimary primary(instances, log);
  primary.token = 12;
  primary.state = {.phase = PrimaryEffectPhase::kAwaitingTrack,
                   .track_count = 40,
                   .started_tracks = 1,
                   .pending_tracks = 0x80000005};
  const auto [track, offset] =
      GENERATE(std::pair{0, 0}, std::pair{2, 6}, std::pair{31, 93});
  CHECK(fx::StartEffectTrack(track, primary) == fx::TrackStart::kStarted);
  CHECK(log == Log{"select 1", "spawn " + std::to_string(offset) + " 12",
                   "primary 4", "primary 4"});
  CHECK(primary.state.started_tracks == 2);
  CHECK(primary.state.pending_tracks ==
        (0x80000005U & ~(std::uint32_t{1} << track)));
}

namespace {
struct UnstartedTrack {
  std::array<Instance, 1> instances{};
  Log log;
  FakePrimary primary{instances, log};

  UnstartedTrack() {
    primary.state = {.phase = PrimaryEffectPhase::kPlayingTracks,
                     .track_count = 3,
                     .started_tracks = 1,
                     .pending_tracks = 0x5};
  }
  void CheckLeft(int started) const {
    CHECK(log.front() == "select 1");
    CHECK(primary.state.started_tracks == started);
    CHECK(primary.state.pending_tracks == 0x5);
  }
};
}  // namespace

TEST_CASE("F01431 no track starts once every track started",
          "[effect-runtime][F01431]") {
  UnstartedTrack unstarted;
  unstarted.primary.state.started_tracks = 3;
  CHECK(fx::StartEffectTrack(0, unstarted.primary) ==
        fx::TrackStart::kNotReady);
  unstarted.CheckLeft(3);
}

TEST_CASE("F01431 a track that is not pending is left",
          "[effect-runtime][F01431]") {
  UnstartedTrack unstarted;
  CHECK(fx::StartEffectTrack(1, unstarted.primary) ==
        fx::TrackStart::kNotPending);
  unstarted.CheckLeft(1);
}

TEST_CASE("F01431 a track outside the mask is refused",
          "[effect-runtime][F01431]") {
  UnstartedTrack unstarted;
  CHECK(fx::StartEffectTrack(GENERATE(-1, 32, 34), unstarted.primary) ==
        fx::TrackStart::kRefused);
  unstarted.CheckLeft(1);
}

TEST_CASE("F01431 a track without a package is refused",
          "[effect-runtime][F01431]") {
  UnstartedTrack unstarted;
  unstarted.primary.has_package = false;
  CHECK(fx::StartEffectTrack(2, unstarted.primary) == fx::TrackStart::kRefused);
  unstarted.CheckLeft(1);
}

TEST_CASE("F01432 the frame updates both runtimes and the camera",
          "[effect-runtime][F01432]") {
  Log log;
  FakeFrame frame(log);
  const auto [buffer, flipped] =
      GENERATE(std::pair{std::uint8_t{0}, std::uint8_t{1}},
               std::pair{std::uint8_t{1}, std::uint8_t{0}},
               std::pair{std::uint8_t{5}, std::uint8_t{252}});
  frame.buffer = buffer;
  frame.primary_busy = GENERATE(false, true);
  frame.secondary_busy = 6;
  frame.phase =
      GENERATE(PrimaryEffectPhase::kIdle, PrimaryEffectPhase::kPlayingTracks,
               PrimaryEffectPhase::kAwaitingTrack);
  CHECK(fx::UpdateEffectRuntime(frame) == (frame.primary_busy ? 7 : 6));
  CHECK(frame.buffer == flipped);
  Log expected{"select 1", "primary", "select 0", "secondary", "select 1"};
  if (frame.phase == PrimaryEffectPhase::kPlayingTracks) {
    expected.emplace_back("camera");
  }
  CHECK(log == expected);
}

TEST_CASE("F01433 the action effect begins loading with a fresh token",
          "[effect-runtime][F01433]") {
  std::array<Instance, 1> instances{};
  Log log;
  FakePrimary primary(instances, log);
  primary.allocated_token = 200;
  const auto [source, variant, kept] =
      GENERATE(std::tuple{std::uint16_t{4}, 0x1203, true},
               std::tuple{std::uint16_t{4}, 0, false},
               std::tuple{std::uint16_t{3}, 7, false},
               std::tuple{std::uint16_t{0x104}, 7, false});
  CHECK(fx::BeginActionEffect({.source_type = source, .variant = variant},
                              primary) == 200);
  Log expected{"store request"};
  if (kept) {
    expected.emplace_back("variant 3");
  }
  expected.insert(expected.end(), {"token 200", "select 1", "capture saved",
                                   "capture effect", "primary 6"});
  CHECK(log == expected);
}

TEST_CASE("F01434 the clear frees each primary instance's timeline",
          "[effect-runtime][F01434]") {
  std::array<Instance, 3> instances{};
  Log log;
  FakePrimary primary(instances, log);
  primary.chain.Link({2, 0, 1});
  primary.timelines = {&instances[0], &instances[2]};
  fx::ClearPrimaryEffects(primary);
  CHECK(log == Log{"stop vibration", "free timeline i2", "destroy i2",
                   "free timeline i0", "destroy i0", "destroy i1"});
  CHECK(primary.FirstPrimary() == nullptr);
}

TEST_CASE(
    "F01421 the Live reset clears the globals but keeps the pending "
    "tracks",
    "[effect-runtime][F01421]") {
  const std::uint32_t pending = fx::pendingTracks;
  const std::array<int, 4> tokens{fx::tokenBitmaps[0], fx::tokenBitmaps[1],
                                  fx::tokenBitmaps[2], fx::tokenBitmaps[3]};
  fx::pendingTracks = 0x30;
  fx::trackCount = 6;
  fx::primaryToken = 9;
  fx::tokenBitmaps[2] = 0x10;
  fx::curveSamples[0] = 5;
  fx::actionVariant = 3;
  fx::LiveResetEffectInstances();
  CHECK(fx::primaryPhase == 0);
  CHECK(fx::pendingTracks == 0x30);
  CHECK(fx::trackCount == 0);
  CHECK(fx::primaryToken == 0);
  CHECK(fx::tokenBitmaps[2] == 0);
  CHECK(fx::curveSamples[0] == 0);
  CHECK(fx::actionVariant == 0);
  CHECK(fx::primaryInstances == nullptr);
  fx::pendingTracks = pending;
  std::ranges::copy(tokens, std::begin(fx::tokenBitmaps));
}

TEST_CASE("F01433 the Live action effect refuses a missing request",
          "[effect-runtime][F01433]") {
  const std::uint32_t phase = fx::primaryPhase;
  fx::primaryPhase = 0;
  CHECK(fx::LiveBeginActionEffect(nullptr, 1, 2) == 0);
  CHECK(fx::primaryPhase == 0);
  fx::primaryPhase = phase;
}

namespace {
class FakeSpawner final : public fx::SecondaryEffectSpawner {
 public:
  FakeSpawner(std::span<Instance> instances, Log& log)
      : instances_(instances), log_(&log) {}

  bool HasPackage() override { return has_package; }
  std::uint8_t AllocateToken() override {
    log_->emplace_back("token");
    return token;
  }
  void StoreRequest() override { log_->emplace_back("store request"); }
  std::uint16_t TargetType(std::size_t target) override {
    return static_cast<std::uint16_t>(kFirstType + target);
  }
  Instance* CreateSecondary(const fx::SecondarySpawn& spawn) override {
    spawns.push_back(spawn);
    log_->push_back("create t" + std::to_string(spawn.target));
    return refused.contains(spawn.target) ? nullptr : &instances_[spawn.target];
  }
  void StartSecondary(Instance& instance, std::int32_t parameter,
                      std::uint16_t effect_id,
                      std::uint8_t busy_bits) override {
    log_->push_back(
        "start " + Name(instance) + " " + std::to_string(parameter) + " " +
        std::to_string(effect_id) + " " + std::to_string(busy_bits));
  }
  void SetPhase(Instance& instance, SecondaryEffectPhase phase) override {
    log_->push_back("phase " + Name(instance) + " " +
                    std::to_string(std::to_underlying(phase)));
  }
  void SelectPackage(fx::ActivePackage package) override {
    log_->push_back("select " + std::to_string(std::to_underlying(package)));
  }
  void CaptureSavedCamera() override { log_->emplace_back("capture saved"); }
  void CaptureEffectCamera() override { log_->emplace_back("capture effect"); }

  static constexpr std::uint16_t kFirstType = 10;
  bool has_package = true;
  std::uint8_t token = 9;
  std::set<std::size_t> refused;
  std::vector<fx::SecondarySpawn> spawns;

 private:
  [[nodiscard]] std::string Name(const Instance& instance) const {
    return "i" + std::to_string(&instance - instances_.data());
  }

  std::span<Instance> instances_;
  Log* log_;
};
}  // namespace

TEST_CASE("F01435 each target starts a secondary instance with the token",
          "[effect-runtime][F01435]") {
  std::vector<Instance> instances(fx::kActionEffectTargets);
  Log log;
  FakeSpawner spawner(instances, log);
  CHECK(fx::SpawnSecondaryEffects({.effect_id = 7, .target_count = 2}, -5,
                                  spawner) == 9);
  CHECK(log == Log{"token", "store request", "create t0", "start i0 -5 7 2",
                   "phase i0 3", "create t1", "start i1 -5 7 2", "phase i1 3"});
  REQUIRE(spawner.spawns.size() == 2);
  for (std::size_t target = 0; target < 2; ++target) {
    const fx::SecondarySpawn& spawn = spawner.spawns[target];
    CHECK(spawn.target == target);
    CHECK_FALSE(spawn.toward_source);
    CHECK(spawn.priority == FakeSpawner::kFirstType + target);
    CHECK(spawn.script_offset == 21);
    CHECK(spawn.token == 9);
  }
}

TEST_CASE("F01435 the effect sets the emit direction and busy bits",
          "[effect-runtime][F01435]") {
  const auto [effect, toward, busy] =
      GENERATE(std::tuple{std::uint16_t{22}, false, 2}, std::tuple{23, true, 2},
               std::tuple{24, false, 2}, std::tuple{25, true, 2},
               std::tuple{39, false, 2}, std::tuple{40, true, 2},
               std::tuple{41, true, 2}, std::tuple{42, false, 2},
               std::tuple{43, false, 2}, std::tuple{44, true, 2},
               std::tuple{53, true, 2}, std::tuple{54, false, 4},
               std::tuple{0x8017, false, 2});
  CHECK(fx::EmitsTowardSource(effect) == toward);
  CHECK(fx::SecondaryBusyBits(effect) == busy);
  std::vector<Instance> instances(1);
  Log log;
  FakeSpawner spawner(instances, log);
  static_cast<void>(fx::SpawnSecondaryEffects(
      {.effect_id = effect, .target_count = 1}, 0, spawner));
  REQUIRE(spawner.spawns.size() == 1);
  CHECK(spawner.spawns.front().toward_source == toward);
  CHECK(spawner.spawns.front().script_offset == effect * 3);
  CHECK(log.at(3) ==
        "start i0 0 " + std::to_string(effect) + " " + std::to_string(busy));
}

TEST_CASE("F01435 a spawn without a package takes no token",
          "[effect-runtime][F01435]") {
  std::vector<Instance> instances(1);
  Log log;
  FakeSpawner spawner(instances, log);
  spawner.has_package = false;
  CHECK(fx::SpawnSecondaryEffects({.effect_id = 7, .target_count = 1}, 0,
                                  spawner) == 0);
  CHECK(log.empty());
}

TEST_CASE("F01435 the spawn stops at the request's targets",
          "[effect-runtime][F01435]") {
  std::vector<Instance> instances(fx::kActionEffectTargets);
  Log log;
  FakeSpawner spawner(instances, log);
  const auto count = GENERATE(std::uint8_t{0}, std::uint8_t{16},
                              std::uint8_t{17}, std::uint8_t{255});
  CHECK(fx::SpawnSecondaryEffects({.effect_id = 1, .target_count = count}, 0,
                                  spawner) == 9);
  CHECK(spawner.spawns.size() ==
        std::min<std::size_t>(count, fx::kActionEffectTargets));
}

TEST_CASE("F01435 a refused instance is skipped", "[effect-runtime][F01435]") {
  std::vector<Instance> instances(3);
  Log log;
  FakeSpawner spawner(instances, log);
  spawner.refused = {1};
  CHECK(fx::SpawnSecondaryEffects({.effect_id = 2, .target_count = 3}, 4,
                                  spawner) == 9);
  CHECK(log == Log{"token", "store request", "create t0", "start i0 4 2 2",
                   "phase i0 3", "create t1", "create t2", "start i2 4 2 2",
                   "phase i2 3"});
}

TEST_CASE("F01436 the secondary package and both cameras precede the spawn",
          "[effect-runtime][F01436]") {
  std::vector<Instance> instances(1);
  Log log;
  FakeSpawner spawner(instances, log);
  CHECK(fx::BeginSecondaryEffects({.effect_id = 54, .target_count = 1}, 6,
                                  spawner) == 9);
  CHECK(log == Log{"select 0", "capture saved", "capture effect", "token",
                   "store request", "create t0", "start i0 6 54 4",
                   "phase i0 3"});
}

TEST_CASE("F01449 the source actor effect targets the origin",
          "[effect-runtime][F01449]") {
  const fx::SourceActorEffect effect = fx::MakeSourceActorEffect(0xC7);
  CHECK(effect.effect_id == 0x38);
  CHECK(effect.target_count == 1);
  CHECK(effect.other_count == 0);
  CHECK(effect.source_type == fx::kActorBoneEndpoint);
  CHECK(effect.actor == 0xC7);
  CHECK(effect.part == 0);
  CHECK(effect.target_type == fx::kPositionEndpoint);
  CHECK((effect.target_x == 0 && effect.target_y == 0 && effect.target_z == 0));
}

TEST_CASE("F01450 the action effect mode clear resets the variant",
          "[effect-runtime][F01450]") {
  std::uint8_t variant = 3;
  fx::ClearActionEffectMode(variant);
  CHECK(variant == 0);
  const auto saved = fx::actionVariant;
  fx::actionVariant = 5;
  fx::LiveClearActionEffectMode();
  CHECK(fx::actionVariant == 0);
  fx::actionVariant = saved;
}

namespace {
// Saves the globals a Live secondary spawn touches and gives it a script
// package and an empty game heap; the destructor restores them.
class LiveSecondaryScope {
 public:
  LiveSecondaryScope()
      : saved_package_(std::exchange(fx::package, &package_)),
        saved_secondaries_(std::exchange(fx::secondaryInstances, nullptr)),
        saved_request_(fx::secondaryRequest) {
    package_.programs = script_.data();
    std::ranges::copy(fx::tokenBitmaps, saved_tokens_.begin());
  }
  LiveSecondaryScope(const LiveSecondaryScope&) = delete;
  LiveSecondaryScope& operator=(const LiveSecondaryScope&) = delete;
  LiveSecondaryScope(LiveSecondaryScope&&) = delete;
  LiveSecondaryScope& operator=(LiveSecondaryScope&&) = delete;
  ~LiveSecondaryScope() {
    fx::secondaryRequest = saved_request_;
    std::ranges::copy(saved_tokens_, std::begin(fx::tokenBitmaps));
    fx::secondaryInstances = saved_secondaries_;
    fx::package = saved_package_;
  }

  [[nodiscard]] const char* Script() const { return script_.data(); }

 private:
  alignas(VS::Core::Memory::kHeapUnitBytes)
      std::array<std::byte, VS::Core::Memory::kHeapUnitBytes * 2048> arena_{};
  VS::Core::Memory::ScopedLiveGameHeap heap_{arena_};
  std::array<char, 128> script_{};
  fx::LiveEffectPackage package_{};
  fx::LiveEffectPackage* saved_package_;
  Instance* saved_secondaries_;
  VS::Effects::LiveActionEffectRequest saved_request_;
  std::array<int, std::size(fx::tokenBitmaps)> saved_tokens_{};
};

struct LivePrimaryScope {
  LiveSecondaryScope heap;
  fx::EffectResourceContext context = fx::LoadLiveEffectResourceContext();
  fx::LiveEffectPackage package = fx::actionPackage;
  fx::PrimaryEffectState state = fx::LivePrimaryEffectState();
  decltype(fx::primaryToken) token = fx::primaryToken;
  fx::LiveActionEffectRequest request = fx::actionRequest;
  fx::LiveEffectCamera saved_camera = fx::savedActionCamera;
  fx::LiveEffectCamera camera = fx::effectCamera;
  Instance* instances = std::exchange(fx::primaryInstances, nullptr);
  std::array<char, 128> script{};

  LivePrimaryScope() {
    fx::actionPackage = {};
    fx::actionPackage.programs = script.data();
    fx::StoreLivePrimaryEffectState({});
  }
  ~LivePrimaryScope() {
    fx::LiveClearPrimaryEffects();
    fx::primaryInstances = instances;
    fx::actionPackage = package;
    fx::StoreLiveEffectResourceContext(context);
    fx::StoreLivePrimaryEffectState(state);
    fx::primaryToken = token;
    fx::actionRequest = request;
    fx::savedActionCamera = saved_camera;
    fx::effectCamera = camera;
  }
};

VS::Effects::LiveActionEffectRequest TwoTargetRequest() {
  VS::Effects::LiveActionEffectRequest request{};
  request.effect_id = 23;
  request.count = 2;
  request.source.type = fx::kActorBoneEndpoint;
  request.source.SetActor(
      {.actor = 3, .event_actor_slot = 0, .part = 0, .reserved3 = 0});
  const std::span targets{request.targets};
  targets[0].type = fx::kPositionEndpoint;
  targets[0].position = {.vx = 1, .vy = 2, .vz = 3, .pad = 0};
  targets[1].type = 7;
  targets[1].position = {.vx = 4, .vy = 5, .vz = 6, .pad = 0};
  return request;
}

std::vector<Instance*> SecondaryInstances() {
  std::vector<Instance*> instances;
  for (Instance* instance = fx::secondaryInstances; instance != nullptr;
       instance = instance->next) {
    instances.push_back(instance);
  }
  return instances;
}

// Effect 23 emits toward the source, so each instance's emitter is its target.
void CheckTowardSourceEndpoints(const Instance& instance) {
  const auto type = instance.context.target.type;
  CHECK((type == fx::kPositionEndpoint || type == 7));
  CHECK(instance.priority == type);
  CHECK(instance.context.emit_from.type == type);
  CHECK(instance.context.emit_to.type == fx::kActorBoneEndpoint);
  CHECK(instance.context.source.type == fx::kActorBoneEndpoint);
}

void CheckStartedInstance(const Instance& instance, const char* script,
                          int token) {
  CHECK(instance.script == script);
  CHECK(instance.script_offset == 69);
  CHECK(instance.token == static_cast<unsigned>(token));
  CHECK(instance.busy_bits == 2);
  CHECK(instance.context.parameter == 0x1234);
  CHECK(instance.context.effect_id == 23);
  CHECK(instance.context.phase ==
        std::to_underlying(SecondaryEffectPhase::kStarting));
}
}  // namespace

TEST_CASE("F01437 the Live spawn starts each target's secondary instance",
          "[effect-runtime][F01435][F01437]") {
  const LiveSecondaryScope scope;
  VS::Effects::LiveActionEffectRequest request = TwoTargetRequest();
  const int token = fx::LiveSpawnSecondaryEffectRequest(&request, 0, 0x1234);
  CHECK(token != 0);
  CHECK(fx::secondaryRequest.effect_id == 23);

  const std::vector<Instance*> spawned = SecondaryInstances();
  REQUIRE(spawned.size() == 2);
  for (Instance* instance : spawned) {
    CheckTowardSourceEndpoints(*instance);
    CheckStartedInstance(*instance, scope.Script(), token);
    fx::LiveDestroySecondaryEffectInstance(instance);
  }
  CHECK(VS::Core::Memory::GameHeap().Empty());
}

TEST_CASE("F01435 the Live secondary spawns refuse a missing request",
          "[effect-runtime][F01435][F01436][F01437]") {
  const int tokens = fx::tokenBitmaps[0];
  CHECK(fx::LiveSpawnSecondaryEffects(nullptr, 0, 1) == 0);
  CHECK(fx::LiveBeginSecondaryEffects(nullptr, 1) == 0);
  CHECK(fx::LiveSpawnSecondaryEffectRequest(nullptr, 0, 1) == 0);
  CHECK(fx::tokenBitmaps[0] == tokens);
}

TEST_CASE("Secondary combat effects carry their action into every target",
          "[effect-runtime][combat-live][issue-56]") {
  const LiveSecondaryScope scope;
  auto request = TwoTargetRequest();
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  const auto id = static_cast<VS::Battle::CombatActionId>(27);
  CHECK(fx::LiveBeginCombatSecondaryEffects(&request, 0x1234, id) != 0);
  const auto spawned = SecondaryInstances();
  REQUIRE(spawned.size() == 2);
  request = {};
  for (auto* instance : spawned) {
    CHECK(instance->context.combat_action == id);
    CHECK(instance->context.effect_id == 23);
    fx::LiveDestroySecondaryEffectInstance(instance);
  }
  CHECK(VS::Core::Memory::GameHeap().Empty());
}

TEST_CASE("Primary tracks capture combat identity at request submission",
          "[effect-runtime][combat-live][issue-56]") {
  LivePrimaryScope scope;
  auto request = TwoTargetRequest();
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  const auto id = static_cast<VS::Battle::CombatActionId>(31);
  CHECK(fx::LiveBeginCombatActionEffect(&request, 0, id) != 0);
  request = {};
  CHECK(fx::LiveStartEffectTrack(0) == 0);
  fx::StoreLivePrimaryEffectState({.phase = PrimaryEffectPhase::kAwaitingTrack,
                                   .track_count = 1,
                                   .pending_tracks = 1,
                                   .loaded = true});
  CHECK(fx::LiveStartEffectTrack(0) == 1);
  REQUIRE(fx::primaryInstances != nullptr);
  CHECK(fx::primaryInstances->context.combat_action == id);
  fx::LiveClearPrimaryEffects();
  CHECK(fx::LiveBeginActionEffect(&request, 0, 0) != 0);
  fx::StoreLivePrimaryEffectState({.phase = PrimaryEffectPhase::kAwaitingTrack,
                                   .track_count = 1,
                                   .pending_tracks = 1,
                                   .loaded = true});
  CHECK(fx::LiveStartEffectTrack(0) == 1);
  REQUIRE(fx::primaryInstances != nullptr);
  CHECK(fx::primaryInstances->context.combat_action ==
        VS::Battle::CombatActionId::kNone);
}
