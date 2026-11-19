#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <map>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Core/Memory/HeapLive.hpp"
#include "Effects/EffectInstances.hpp"
#include "Effects/EffectInstancesLive.hpp"
#include "Effects/Runtime.h"
#include "Effects/SpawnRequest.hpp"

namespace {
namespace fx = VS::Effects;
namespace Memory = VS::Core::Memory;
using Instance = Effects_EffectInstance;
using Slot = Effects_RendererSlot;
using Log = std::vector<std::string>;
using Create = Instance* (*)(std::int32_t, fx::EffectInstanceChain&);

template <typename Record>
std::string Entry(std::string_view what, std::span<Record> records,
                  const Record& record) {
  return std::string(what) + " " +
         std::to_string(static_cast<std::size_t>(&record - records.data()));
}

// Links `records[order...]` into a chain kept outside the records.
template <typename Record>
class FakeLinks {
 public:
  explicit FakeLinks(std::span<Record> records) : records_(records) {}

  void Link(const std::vector<std::size_t>& order) {
    Record* previous = nullptr;
    for (const std::size_t index : order) {
      Record* record = &records_[index];
      (previous == nullptr ? first : next[previous]) = record;
      next[record] = nullptr;
      previous = record;
    }
  }
  [[nodiscard]] std::vector<std::size_t> Order() const {
    std::vector<std::size_t> order;
    for (Record* record = first; record != nullptr; record = next.at(record)) {
      order.push_back(static_cast<std::size_t>(record - records_.data()));
    }
    return order;
  }

  Record* first = nullptr;
  std::map<Record*, Record*> next;

 private:
  std::span<Record> records_;
};

class FakeRenderers final : public fx::RendererChain {
 public:
  FakeRenderers(std::span<Slot> slots, Log& log)
      : links(slots), slots_(slots), log_(&log) {}

  Slot* First() override { return links.first; }
  void SetFirst(Slot* slot) override { links.first = slot; }
  Slot* Next(Slot& slot) override { return links.next[&slot]; }
  void SetNext(Slot& slot, Slot* next) override { links.next[&slot] = next; }
  Slot* NewSlot(fx::EffectRenderer /*renderer*/) override { return spare; }
  void Run(Slot& slot, fx::RendererCommand command,
           std::int32_t parameters) override {
    const bool start = command == fx::RendererCommand::kStart;
    log_->push_back(Entry(start ? "start" : "stop", slots_, slot) + " " +
                    std::to_string(parameters));
  }
  void Free(Slot& slot) override {
    log_->push_back(Entry("free", slots_, slot));
  }

  FakeLinks<Slot> links;
  Slot* spare = nullptr;

 private:
  std::span<Slot> slots_;
  Log* log_;
};

// An instance chain over `kRecords` records, logging what it releases.
class FakeInstances final : public fx::EffectInstanceChain {
  static constexpr std::size_t kRecords = 6;
  // Declared first: the links and renderers view them.
  std::vector<Instance> records_ = std::vector<Instance>(kRecords);
  std::vector<Slot> slots_ = std::vector<Slot>(kRecords);
  std::map<Instance*, std::uint8_t> priority_;

 public:
  FakeInstances() : links(records_), renderers(slots_, log) {}

  // Links records 0, 1, ... with `priorities`.
  void Link(std::initializer_list<std::uint8_t> priorities) {
    std::vector<std::size_t> order;
    for (const std::uint8_t priority : priorities) {
      priority_[&records_[order.size()]] = priority;
      order.push_back(order.size());
    }
    links.Link(order);
  }
  Instance& Record(std::size_t index) { return records_.at(index); }
  Slot& RendererSlot(std::size_t index) { return slots_.at(index); }

  Instance* First() override { return links.first; }
  void SetFirst(Instance* instance) override { links.first = instance; }
  Instance* Next(Instance& instance) override { return links.next[&instance]; }
  void SetNext(Instance& instance, Instance* next) override {
    links.next[&instance] = next;
  }
  std::uint8_t PriorityOf(Instance& instance) override {
    return priority_.at(&instance);
  }
  Instance* StartInstance() override { return spare; }
  void ReleaseParticles(Instance& instance) override {
    log.push_back(Entry("particles", std::span(records_), instance));
  }
  fx::RendererChain& Renderers(Instance& /*instance*/) override {
    return renderers;
  }
  void ReleaseToken(Instance& instance) override {
    log.push_back(Entry("token", std::span(records_), instance));
  }
  void Free(Instance& instance) override {
    log.push_back(Entry("free instance", std::span(records_), instance));
  }

  Log log;
  FakeLinks<Instance> links;
  FakeRenderers renderers;
  Instance* spare = nullptr;
};

class RecordingStart final : public fx::EffectInstanceStart {
 public:
  void Begin(const fx::EffectInstanceIdentity& started) override {
    identity = started;
    log.emplace_back("begin");
  }
  void ClearTransform(std::size_t transform) override {
    log.push_back("clear " + std::to_string(transform));
  }
  void TakeEndpoints() override { log.emplace_back("endpoints"); }

  fx::EffectInstanceIdentity identity{};
  Log log;
};

struct RendererCall {
  const void* slot{};
  unsigned int command{};
  int parameters{};

  friend bool operator==(const RendererCall&, const RendererCall&) = default;
};

std::vector<RendererCall>& RendererCalls() {
  static std::vector<RendererCall> calls;
  return calls;
}

// Records its commands and keeps renderer state from its start.
int RecordingRenderer(Slot* slot, unsigned int command, int parameters) {
  RendererCalls().push_back({slot, command, parameters});
  if (command == std::to_underlying(fx::RendererCommand::kStart)) {
    slot->rendererState = Memory::AllocateLiveFromHighEnd(16);
  }
  return 1;
}

// Records its commands, keeps renderer state from its start and finishes on
// its first update.
int FinishingRenderer(Slot* slot, unsigned int command, int parameters) {
  RendererCalls().push_back({slot, command, parameters});
  if (command == std::to_underlying(fx::RendererCommand::kStart)) {
    slot->rendererState = Memory::AllocateLiveFromHighEnd(16);
  }
  return command == std::to_underlying(fx::RendererCommand::kUpdate) ? 0 : 1;
}

// The game heap, the effect chains and the token pool's first word, restored
// afterwards.
class LiveInstances {
 public:
  LiveInstances()
      : saved_primaries_(std::exchange(Effects_primaryInstances, nullptr)),
        saved_secondaries_(std::exchange(Effects_secondaryInstances, nullptr)),
        saved_tokens_(Effects_tokenBitmaps[0]) {
    RendererCalls().clear();
  }
  ~LiveInstances() {
    Effects_primaryInstances = saved_primaries_;
    Effects_secondaryInstances = saved_secondaries_;
    Effects_tokenBitmaps[0] = saved_tokens_;
  }
  LiveInstances(const LiveInstances&) = delete;
  LiveInstances& operator=(const LiveInstances&) = delete;
  LiveInstances(LiveInstances&&) = delete;
  LiveInstances& operator=(LiveInstances&&) = delete;

  fx::LiveEffectSpawnRequest* Spawn(int priority, std::uint8_t token = 0) {
    spawn_ = {};
    spawn_.source = &endpoint_;
    spawn_.emit_from = &endpoint_;
    spawn_.target = &endpoint_;
    spawn_.emit_to = &endpoint_;
    spawn_.priority = priority;
    spawn_.token = token;
    return &spawn_;
  }

 private:
  alignas(Memory::kHeapUnitBytes)
      std::array<std::byte, Memory::kHeapUnitBytes * 2048> arena_{};
  Memory::ScopedLiveGameHeap heap_{arena_};
  VS::Effects::LiveActionEffectTarget endpoint_{};
  fx::LiveEffectSpawnRequest spawn_{};
  Instance* saved_primaries_;
  Instance* saved_secondaries_;
  int saved_tokens_;
};

std::vector<int> Priorities(const Instance* instance) {
  std::vector<int> priorities;
  for (; instance != nullptr; instance = instance->next) {
    priorities.push_back(static_cast<std::uint8_t>(instance->priority));
  }
  return priorities;
}

template <typename Value>
bool SameBytes(const Value& left, const Value& right) {
  return std::ranges::equal(std::as_bytes(std::span(&left, 1)),
                            std::as_bytes(std::span(&right, 1)));
}

bool Cleared(const fx::LiveTransformSlot& slot, int kept) {
  const std::array cleared{slot.reserved_8,     slot.reserved_10[0],
                           slot.reserved_10[1], slot.reserved_10[2],
                           slot.reserved_2c[0], slot.reserved_2c[1]};
  const std::array halves{slot.rotation[0], slot.rotation[1], slot.rotation[2],
                          slot.reserved_c, slot.frame};
  return std::ranges::all_of(cleared, [](int word) { return word == 0; }) &&
         std::ranges::all_of(halves,
                             [](std::int16_t half) { return half == 0; }) &&
         slot.flags == 0 && slot.position.pad == 0 && slot.actor == 0 &&
         slot.bone == 0 && slot.target_actor == 0 && slot.target_bone == 0 &&
         slot.position.vx == kept && slot.position.vy == kept &&
         slot.position.vz == kept;
}
}  // namespace

TEST_CASE("Effect instances start from their spawn's low bytes",
          "[effect-instances]") {
  RecordingStart start;
  fx::InitEffectInstance(
      {.priority = 0x1234, .script_offset = 0x12345678, .token = 0xAB}, start);
  CHECK(start.identity == fx::EffectInstanceIdentity{.priority = 0x34,
                                                     .script_offset = 0x5678,
                                                     .token = 0xAB});
  Log expected{"begin"};
  for (std::size_t transform = 0; transform < fx::kEffectInstanceTransforms;
       ++transform) {
    expected.push_back("clear " + std::to_string(transform));
  }
  expected.emplace_back("endpoints");
  CHECK(start.log == expected);
}

TEST_CASE("Effect instances join after every instance of no greater priority",
          "[effect-instances]") {
  const Create create = GENERATE(&fx::CreatePrimaryEffectInstance,
                                 &fx::CreateSecondaryEffectInstance);
  // Unsigned bytes: 0x90 sorts after 3, and the spawn's full priority counts.
  const auto [priority, order] =
      GENERATE(table<std::int32_t, std::vector<std::size_t>>(
          {{3, {0, 1, 2, 4, 3}},
           {0, {4, 0, 1, 2, 3}},
           {-1, {4, 0, 1, 2, 3}},
           {0x8F, {0, 1, 2, 4, 3}},
           {0x90, {0, 1, 2, 3, 4}},
           {0x100, {0, 1, 2, 3, 4}}}));
  FakeInstances chain;
  chain.Link({1, 3, 3, 0x90});
  chain.spare = &chain.Record(4);
  CHECK(create(priority, chain) == &chain.Record(4));
  CHECK(chain.links.Order() == order);
}

TEST_CASE(
    "Effect instance creation fills an empty chain or refuses a full heap",
    "[effect-instances]") {
  FakeInstances chain;
  CHECK(fx::CreatePrimaryEffectInstance(7, chain) == nullptr);
  CHECK(chain.links.first == nullptr);
  chain.spare = &chain.Record(0);
  CHECK(fx::CreateSecondaryEffectInstance(7, chain) == &chain.Record(0));
  CHECK(chain.links.Order() == std::vector<std::size_t>{0});
}

TEST_CASE("Primary destruction stops and detaches each renderer, newest first",
          "[effect-instances]") {
  FakeInstances chain;
  chain.Link({0, 0, 0});
  chain.renderers.links.Link({0, 1, 2});
  fx::DestroyPrimaryEffectInstance(chain.Record(1), chain);
  CHECK(chain.links.Order() == std::vector<std::size_t>{0, 2});
  CHECK(chain.log == Log{"particles 1", "stop 0 0", "free 0", "stop 1 0",
                         "free 1", "stop 2 0", "free 2", "free instance 1"});
  CHECK(chain.renderers.links.first == nullptr);
}

TEST_CASE("Secondary destruction releases the token and keeps the renderers",
          "[effect-instances]") {
  FakeInstances chain;
  chain.Link({0, 0});
  chain.renderers.links.Link({0});
  fx::DestroySecondaryEffectInstance(chain.Record(0), chain);
  CHECK(chain.links.Order() == std::vector<std::size_t>{1});
  CHECK(chain.log == Log{"particles 0", "token 0", "free instance 0"});
  CHECK(chain.renderers.links.Order() == std::vector<std::size_t>{0});
}

TEST_CASE("Destruction still ends an instance missing from its chain",
          "[effect-instances]") {
  FakeInstances chain;
  chain.Link({0, 0});
  fx::DestroySecondaryEffectInstance(chain.Record(4), chain);
  fx::DestroyPrimaryEffectInstance(chain.Record(5), chain);
  CHECK(chain.links.Order() == std::vector<std::size_t>{0, 1});
  CHECK(chain.log == Log{"particles 4", "token 4", "free instance 4",
                         "particles 5", "free instance 5"});
}

TEST_CASE("Renderer attach starts the newest slot with its parameters",
          "[effect-instances]") {
  Log log;
  std::array<Slot, 3> slots{};
  FakeRenderers renderers(slots, log);
  renderers.links.Link({0});
  CHECK(fx::AttachEffectRenderer(renderers, &RecordingRenderer, 77) == nullptr);
  renderers.spare = &slots[1];
  CHECK(fx::AttachEffectRenderer(renderers, &RecordingRenderer, 77) ==
        &slots[1]);
  CHECK(renderers.links.Order() == std::vector<std::size_t>{1, 0});
  CHECK(log == Log{"start 1 77"});
}

TEST_CASE("Renderer detach frees a held slot and refuses another",
          "[effect-instances]") {
  Log log;
  std::array<Slot, 3> slots{};
  FakeRenderers renderers(slots, log);
  renderers.links.Link({0, 1, 2});
  CHECK(fx::DetachEffectRenderer(renderers, slots[1]));
  CHECK_FALSE(fx::DetachEffectRenderer(renderers, slots[1]));
  CHECK(fx::DetachEffectRenderer(renderers, slots[0]));
  CHECK(renderers.links.Order() == std::vector<std::size_t>{2});
  CHECK(log == Log{"free 1", "free 0"});
}

TEST_CASE("Secondary phases are the three the runtime sets",
          "[effect-instances]") {
  CHECK(fx::ToSecondaryEffectPhase(2) == fx::SecondaryEffectPhase::kHeld);
  CHECK(fx::ToSecondaryEffectPhase(3) == fx::SecondaryEffectPhase::kStarting);
  CHECK(fx::ToSecondaryEffectPhase(4) == fx::SecondaryEffectPhase::kPlaying);
  const std::int32_t unknown = GENERATE(-1, 0, 1, 5, 0x10003);
  CHECK_FALSE(fx::ToSecondaryEffectPhase(unknown).has_value());
  std::int16_t phase = 0x7777;
  fx::SetSecondaryEffectPhase(phase, fx::SecondaryEffectPhase::kStarting);
  CHECK(phase == 3);
}

TEST_CASE("Live effect instances start from their spawn request",
          "[effect-instances]") {
  constexpr int kKept = static_cast<int>(0xA5A5A5A5U);
  auto instance = std::make_unique<Instance>();
  std::ranges::fill(std::as_writable_bytes(std::span(instance.get(), 1)),
                    std::byte{0xA5});
  std::array<VS::Effects::LiveActionEffectTarget, 4> endpoints{};
  for (std::size_t index = 0; index < endpoints.size(); ++index) {
    std::ranges::fill(std::as_writable_bytes(std::span(&endpoints[index], 1)),
                      static_cast<std::byte>(0x11 * (index + 1)));
  }
  Instance parent{};
  std::array<char, 4> script{};
  fx::LiveEffectSpawnRequest spawn{};
  spawn.source = &endpoints[0];
  spawn.emit_from = &endpoints[1];
  spawn.target = &endpoints[2];
  spawn.emit_to = &endpoints[3];
  spawn.priority = 0x1281;
  spawn.script = script.data();
  spawn.script_offset = 0x12345678;
  spawn.parent = &parent;
  spawn.token = 0x6C;
  fx::LiveInitEffectInstance(&spawn, instance.get());
  const Instance& started = *instance;
  CHECK((started.parent == &parent && started.particleCount == 0 &&
         static_cast<std::uint8_t>(started.priority) == 0x81 &&
         started.scriptOffset == 0x5678 && started.script == script.data()));
  CHECK(
      (std::ranges::all_of(started.registers, [](char r) { return r == 0; }) &&
       started.token == 0x6C && started.busyBits == 5 &&
       started.childCount == 0 && started.frame == 0 &&
       started.particles == nullptr));
  CHECK(std::ranges::all_of(
      started.transforms,
      [](const fx::LiveTransformSlot& slot) { return Cleared(slot, kKept); }));
  const fx::LiveEffectInstanceTargets& context = started.context;
  CHECK((SameBytes(context.source, endpoints[0]) &&
         SameBytes(context.emit_from, endpoints[1]) &&
         SameBytes(context.target, endpoints[2]) &&
         SameBytes(context.emit_to, endpoints[3])));
  CHECK((context.timeline == nullptr && context.renderer == nullptr &&
         context.parameter == 0 && context.effect_id == -0x5A5B &&
         context.phase == -0x5A5B));
}

TEST_CASE("Live effect instances refuse missing spawn data",
          "[effect-instances]") {
  LiveInstances live;
  fx::LiveEffectSpawnRequest* spawn = live.Spawn(1);
  spawn->target = nullptr;
  CHECK(fx::LiveCreatePrimaryEffectInstance(spawn) == nullptr);
  CHECK(fx::LiveCreateSecondaryEffectInstance(nullptr) == nullptr);
  auto instance = std::make_unique<Instance>();
  instance->priority = 9;
  fx::LiveInitEffectInstance(spawn, instance.get());
  CHECK(instance->priority == 9);
  CHECK(Memory::GameHeap().Empty());
}

TEST_CASE("Live effect instances join the primary chain by unsigned priority",
          "[effect-instances]") {
  LiveInstances live;
  for (const int priority : {5, 1, 0x80, 0x7F}) {
    REQUIRE(fx::LiveCreatePrimaryEffectInstance(live.Spawn(priority)) !=
            nullptr);
  }
  CHECK(Priorities(Effects_primaryInstances) ==
        std::vector<int>{1, 5, 0x7F, 0x80});
  const Instance& first = *Effects_primaryInstances;
  CHECK((first.busyBits == 0 && first.context.effect_id == 0 &&
         first.context.phase == 0));
  while (Effects_primaryInstances != nullptr) {
    fx::LiveDestroyPrimaryEffectInstance(Effects_primaryInstances);
  }
  CHECK(Memory::GameHeap().Empty());
}

TEST_CASE("Live renderers start on attach and stop newest first at the end",
          "[effect-instances]") {
  LiveInstances live;
  Instance* instance = fx::LiveCreatePrimaryEffectInstance(live.Spawn(0));
  REQUIRE(instance != nullptr);
  Slot* first = fx::LiveAttachEffectRenderer(instance, &RecordingRenderer, 11);
  Slot* second = fx::LiveAttachEffectRenderer(instance, &RecordingRenderer, 22);
  REQUIRE(first != nullptr);
  REQUIRE(second != nullptr);
  CHECK((instance->context.renderer == second && second->next == first));
  fx::LiveDestroyPrimaryEffectInstance(instance);
  CHECK(RendererCalls() ==
        std::vector<RendererCall>{
            {first, 1, 11}, {second, 1, 22}, {second, 4, 0}, {first, 4, 0}});
  CHECK(Effects_primaryInstances == nullptr);
  CHECK(Memory::GameHeap().Empty());
}

TEST_CASE("Live renderer updates detach each finished renderer, newest first",
          "[effect-instances][F01526]") {
  LiveInstances live;
  Instance* instance = fx::LiveCreatePrimaryEffectInstance(live.Spawn(0));
  REQUIRE(instance != nullptr);
  Slot* oldest = fx::LiveAttachEffectRenderer(instance, &RecordingRenderer, 1);
  Slot* finishing =
      fx::LiveAttachEffectRenderer(instance, &FinishingRenderer, 2);
  Slot* newest = fx::LiveAttachEffectRenderer(instance, &RecordingRenderer, 3);
  REQUIRE((oldest != nullptr && finishing != nullptr && newest != nullptr));
  RendererCalls().clear();
  fx::LiveUpdateEffectRenderers(instance);
  CHECK(RendererCalls() == std::vector<RendererCall>{{newest, 2, 0},
                                                     {finishing, 2, 0},
                                                     {oldest, 2, 0}});
  CHECK((instance->context.renderer == newest && newest->next == oldest &&
         oldest->next == nullptr));
  fx::LiveUpdateEffectRenderers(nullptr);
  fx::LiveDestroyPrimaryEffectInstance(instance);
  CHECK(Memory::GameHeap().Empty());
}

TEST_CASE("Live renderer detach refuses a slot its instance does not hold",
          "[effect-instances]") {
  LiveInstances live;
  Instance* holder = fx::LiveCreatePrimaryEffectInstance(live.Spawn(0));
  Instance* other = fx::LiveCreatePrimaryEffectInstance(live.Spawn(0));
  REQUIRE((holder != nullptr && other != nullptr));
  Slot* slot = fx::LiveAttachEffectRenderer(holder, &RecordingRenderer, 0);
  CHECK(fx::LiveAttachEffectRenderer(other, nullptr, 0) == nullptr);
  fx::LiveDetachEffectRenderer(other, slot);
  CHECK(holder->context.renderer == slot);
  fx::LiveDetachEffectRenderer(holder, slot);
  CHECK(holder->context.renderer == nullptr);
  fx::LiveDestroyPrimaryEffectInstance(holder);
  fx::LiveDestroyPrimaryEffectInstance(other);
  CHECK(Memory::GameHeap().Empty());
}

TEST_CASE("Live secondary instances take known phases and return their token",
          "[effect-instances]") {
  LiveInstances live;
  constexpr std::uint8_t kToken = 5;
  Effects_tokenBitmaps[0] |= 1 << kToken;
  Instance* instance =
      fx::LiveCreateSecondaryEffectInstance(live.Spawn(0, kToken));
  REQUIRE(instance != nullptr);
  fx::LiveSetSecondaryEffectPhase(3, instance);
  fx::LiveSetSecondaryEffectPhase(7, instance);
  CHECK(instance->context.phase == 3);
  fx::LiveDestroySecondaryEffectInstance(instance);
  CHECK(Effects_secondaryInstances == nullptr);
  CHECK((Effects_tokenBitmaps[0] & (1 << kToken)) == 0);
  CHECK(Memory::GameHeap().Empty());
}
