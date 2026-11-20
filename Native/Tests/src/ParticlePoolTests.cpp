#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Core/Math/ScalarMath.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Effects/ParticlePool.hpp"
#include "Effects/ParticlePoolLive.hpp"
#include "Effects/Particles.hpp"
#include "Effects/Runtime.hpp"

using VS::Core::Gte::VECTOR;

namespace {
namespace fx = VS::Effects;
namespace Memory = VS::Core::Memory;
using Particle = VS::Effects::LiveParticle;
using Instance = fx::LiveEffectInstance;

constexpr std::size_t kRecords = 6;
constexpr std::uint8_t kUnaged = 0xFF;

// Records in a fixed array; Reserve hands out the array once.
class TestRecords final : public fx::ParticleRecords {
 public:
  std::array<Particle, kRecords> records{};
  bool room = true;
  int reserved = 0;
  int freed = 0;
  std::vector<Particle*> renewed;

  Particle* Reserve(std::int32_t count) override {
    if (!room || count > static_cast<std::int32_t>(kRecords)) {
      return nullptr;
    }
    ++reserved;
    return records.data();
  }
  void Free(Particle& block) override {
    CHECK(&block == records.data());
    ++freed;
  }
  void Renew(Particle& particle) override {
    particle.age = kUnaged;
    renewed.push_back(&particle);
  }
  Particle& At(Particle& block, std::int32_t index) override {
    CHECK(&block == records.data());
    return records.at(static_cast<std::size_t>(index));
  }
  // The links stay in the records, as in the game heap.
  Particle* Next(Particle& record) override { return record.next; }
  Particle* Previous(Particle& record) override { return record.previous; }
  void SetNext(Particle& record, Particle* link) override {
    record.next = link;
  }
  void SetPrevious(Particle& record, Particle* link) override {
    record.previous = link;
  }

  [[nodiscard]] Particle* At(std::size_t index) { return &records.at(index); }
  // Links records `order...` doubly, as an instance list.
  [[nodiscard]] Particle* Link(std::span<const std::size_t> order) {
    Particle* previous = nullptr;
    for (const std::size_t index : order) {
      Particle* particle = At(index);
      particle->previous = previous;
      particle->next = nullptr;
      if (previous != nullptr) {
        previous->next = particle;
      }
      previous = particle;
    }
    return order.empty() ? nullptr : At(order.front());
  }
};

// The records `first` links forward, as record numbers.
[[nodiscard]] std::vector<std::size_t> Forward(TestRecords& records,
                                               Particle* first) {
  std::vector<std::size_t> order;
  for (; first != nullptr && order.size() <= kRecords; first = first->next) {
    order.push_back(static_cast<std::size_t>(first - records.records.data()));
  }
  return order;
}

// The records `last` links backward, as record numbers.
[[nodiscard]] std::vector<std::size_t> Backward(TestRecords& records,
                                                Particle* last) {
  std::vector<std::size_t> order;
  for (; last != nullptr && order.size() <= kRecords; last = last->previous) {
    order.push_back(static_cast<std::size_t>(last - records.records.data()));
  }
  return order;
}
}  // namespace

namespace {
// A pool of four particles, freshly acquired.
struct AcquiredPool {
  TestRecords records;
  fx::ParticlePool pool{.used = 9};

  AcquiredPool() { REQUIRE(fx::AcquireParticlePool(pool, 4, records)); }
};
}  // namespace

TEST_CASE("Particle pool links every reserved particle free in order",
          "[particle-pool][F01505]") {
  AcquiredPool acquired;
  TestRecords& records = acquired.records;
  const fx::ParticlePool& pool = acquired.pool;
  CHECK(pool.block == records.At(0));
  CHECK(pool.free == records.At(0));
  CHECK(pool.capacity == 4);
  CHECK(pool.used == 0);
  CHECK(pool.references == 1);
  CHECK(Forward(records, pool.free) == std::vector<std::size_t>{0, 1, 2, 3});
  CHECK(Backward(records, records.At(3)) ==
        std::vector<std::size_t>{3, 2, 1, 0});
}

TEST_CASE("Particle pool counts later references without reserving",
          "[particle-pool][F01505]") {
  AcquiredPool acquired;
  fx::ParticlePool& pool = acquired.pool;
  REQUIRE(fx::AcquireParticlePool(pool, 0, acquired.records));
  CHECK(pool.references == 2);
  CHECK(acquired.records.reserved == 1);
  REQUIRE(fx::ReleaseParticlePool(pool, acquired.records));
  CHECK(pool.references == 1);
  CHECK(acquired.records.freed == 0);
}

TEST_CASE("Particle pool frees the block with the last reference",
          "[particle-pool][F01506]") {
  AcquiredPool acquired;
  fx::ParticlePool& pool = acquired.pool;
  REQUIRE(fx::ReleaseParticlePool(pool, acquired.records));
  CHECK(pool.references == 0);
  CHECK(pool.block == nullptr);
  CHECK(acquired.records.freed == 1);
}

TEST_CASE("Particle pool reset forgets the references and keeps the block",
          "[particle-pool][F01504]") {
  AcquiredPool acquired;
  fx::ResetParticlePoolReferences(acquired.pool);
  CHECK(acquired.pool.references == 0);
  CHECK(acquired.pool.block == acquired.records.At(0));
}

TEST_CASE("Particle pool links a single particle alone",
          "[particle-pool][F01505]") {
  TestRecords records;
  records.records[1].previous = records.At(5);
  fx::ParticlePool pool;
  REQUIRE(fx::AcquireParticlePool(pool, 1, records));
  CHECK(records.records[0].previous == nullptr);
  CHECK(records.records[0].next == nullptr);
  CHECK(records.records[1].previous == records.At(5));
  CHECK(pool.capacity == 1);
}

TEST_CASE("Particle pool refuses empty counts full heaps and extra releases",
          "[particle-pool][F01505][F01506]") {
  TestRecords records;
  const fx::ParticlePool before{.capacity = 3, .used = 2};
  fx::ParticlePool pool = before;
  const std::int32_t count = GENERATE(0, -1);
  CHECK_FALSE(fx::AcquireParticlePool(pool, count, records));
  records.room = false;
  CHECK_FALSE(fx::AcquireParticlePool(pool, 4, records));
  CHECK_FALSE(fx::ReleaseParticlePool(pool, records));
  CHECK(pool.references == 0);
  CHECK(pool.capacity == before.capacity);
  CHECK(pool.used == before.used);
  CHECK(records.reserved == 0);
  CHECK(records.freed == 0);
}

TEST_CASE("Particle allocation renews the first free particle as the newest",
          "[particle-pool][F01507]") {
  TestRecords records;
  fx::ParticlePool pool;
  REQUIRE(fx::AcquireParticlePool(pool, 3, records));
  fx::ParticleList list;

  Particle* first = fx::AllocateEffectParticle(pool, list, records);
  Particle* second = fx::AllocateEffectParticle(pool, list, records);
  REQUIRE(std::vector<Particle*>{first, second} ==
          std::vector<Particle*>{records.At(0), records.At(1)});
  CHECK(list == fx::ParticleList{.first = second, .count = 2});
  CHECK(pool == fx::ParticlePool{.block = records.At(0),
                                 .free = records.At(2),
                                 .capacity = 3,
                                 .used = 2,
                                 .references = 1});
  CHECK(Forward(records, list.first) == std::vector<std::size_t>{1, 0});
  CHECK(Backward(records, first) == std::vector<std::size_t>{0, 1});
  CHECK(records.renewed == std::vector<Particle*>{first, second});
}

TEST_CASE("Particle allocation refuses when every particle is in use",
          "[particle-pool][F01507]") {
  TestRecords records;
  fx::ParticlePool pool;
  REQUIRE(fx::AcquireParticlePool(pool, 2, records));
  fx::ParticleList list{.count = 0xFF};
  REQUIRE(fx::AllocateEffectParticle(pool, list, records) == records.At(0));
  REQUIRE(fx::AllocateEffectParticle(pool, list, records) == records.At(1));
  CHECK(fx::AllocateEffectParticle(pool, list, records) == nullptr);
  CHECK(list.first == records.At(1));
  CHECK(list.count == 1);
  CHECK(pool.used == 2);
  CHECK(records.renewed.size() == 2);
}

TEST_CASE("Particle release unlinks any particle onto the free list",
          "[particle-pool][F01508]") {
  TestRecords records;
  constexpr std::array<std::size_t, 3> kOrder{4, 2, 0};
  fx::ParticleList list{.first = records.Link(kOrder), .count = 3};
  fx::ParticlePool pool{.free = records.At(5), .used = 5};
  const auto [released, remaining] =
      GENERATE(std::pair<std::size_t, std::vector<std::size_t>>{4, {2, 0}},
               std::pair<std::size_t, std::vector<std::size_t>>{2, {4, 0}},
               std::pair<std::size_t, std::vector<std::size_t>>{0, {4, 2}});
  fx::ReleaseEffectParticle(*records.At(released), pool, list, records);
  CHECK(Forward(records, list.first) == remaining);
  CHECK(Backward(records, records.At(remaining.back())) ==
        std::vector<std::size_t>{remaining.back(), remaining.front()});
  CHECK(pool.free == records.At(released));
  CHECK(records.At(released)->next == records.At(5));
  CHECK(pool.used == 4);
  CHECK(list.count == 2);
}

TEST_CASE("Particle list release returns every particle before the free list",
          "[particle-pool][F01509]") {
  TestRecords records;
  constexpr std::array<std::size_t, 3> kOrder{3, 1, 4};
  // #389: a count above 127 is an unsigned byte.
  const std::uint8_t count = GENERATE(std::uint8_t{3}, std::uint8_t{0x90});
  fx::ParticleList list{.first = records.Link(kOrder), .count = count};
  fx::ParticlePool pool{.free = records.At(5), .used = 0x100};
  fx::ReleaseAllEffectParticles(pool, list, records);
  CHECK(Forward(records, pool.free) == std::vector<std::size_t>{3, 1, 4, 5});
  CHECK(pool.used == 0x100 - count);
  CHECK(list.count == 0);
  CHECK(list.first == records.At(3));

  fx::ParticleList empty{.count = 7};
  fx::ReleaseAllEffectParticles(pool, empty, records);
  CHECK(empty.count == 7);
  CHECK(pool.used == 0x100 - count);
}

TEST_CASE("Particle forces keep the gravity and drag",
          "[particle-pool][F01552][F01553]") {
  fx::ParticleForces forces;
  fx::SetParticleGravity(forces, {.x = -3, .y = 0x7FFFFFFF, .z = 9});
  fx::SetParticleDrag(forces, -40);
  CHECK(forces ==
        fx::ParticleForces{.gravity = {.x = -3, .y = 0x7FFFFFFF, .z = 9},
                           .drag = -40});
}

namespace {
// The pool and force globals, restored on exit, over a game heap of its own.
class SavedPool {
 public:
  SavedPool()
      : pool_(VS::Effects::particlePool),
        free_(VS::Effects::freeParticles),
        capacity_(VS::Effects::particleCapacity),
        used_(VS::Effects::usedParticleCount),
        references_(VS::Effects::particlePoolReferences),
        gravity_(VS::Effects::particleGravity),
        drag_(VS::Effects::particleDrag) {
    VS::Effects::particlePoolReferences = 0;
  }
  SavedPool(const SavedPool&) = delete;
  SavedPool& operator=(const SavedPool&) = delete;
  SavedPool(SavedPool&&) = delete;
  SavedPool& operator=(SavedPool&&) = delete;
  ~SavedPool() {
    VS::Effects::particlePool = pool_;
    VS::Effects::freeParticles = free_;
    VS::Effects::particleCapacity = capacity_;
    VS::Effects::usedParticleCount = used_;
    VS::Effects::particlePoolReferences = references_;
    VS::Effects::particleGravity = gravity_;
    VS::Effects::particleDrag = drag_;
  }

 private:
  Particle* pool_;
  Particle* free_;
  int capacity_;
  int used_;
  int references_;
  VECTOR gravity_;
  int drag_;
  alignas(Memory::kHeapUnitBytes)
      std::array<std::byte, Memory::kHeapUnitBytes * 64> arena_{};
  Memory::ScopedLiveGameHeap heap_{arena_};
};

// The pool globals, as the typed pool.
[[nodiscard]] fx::ParticlePool LivePool() {
  return {.block = VS::Effects::particlePool,
          .free = VS::Effects::freeParticles,
          .capacity = VS::Effects::particleCapacity,
          .used = VS::Effects::usedParticleCount,
          .references = VS::Effects::particlePoolReferences};
}

// An instance's particles, as the typed list.
[[nodiscard]] fx::ParticleList ListOf(const Instance& instance) {
  return {.first = instance.particles,
          .count = std::bit_cast<std::uint8_t>(instance.particle_count)};
}
}  // namespace

TEST_CASE("Live particle pool serves an instance from the game heap",
          "[particle-pool][F01505][F01507]") {
  const SavedPool saved;
  auto instance = std::make_unique<Instance>();
  fx::LiveAcquireParticlePool(2);
  Particle* block = VS::Effects::particlePool;
  REQUIRE(block != nullptr);

  Particle* first = fx::LiveAllocateEffectParticle(instance.get());
  Particle* second = fx::LiveAllocateEffectParticle(instance.get());
  CHECK(fx::LiveAllocateEffectParticle(instance.get()) == nullptr);
  CHECK(std::vector<Particle*>{first, second} ==
        std::vector<Particle*>{block, block + 1});
  CHECK(LivePool() ==
        fx::ParticlePool{
            .block = block, .capacity = 2, .used = 2, .references = 1});
  CHECK(ListOf(*instance) == fx::ParticleList{.first = second, .count = 2});
  CHECK(second->age == kUnaged);
  fx::LiveReleaseParticlePool();
  CHECK(Memory::GameHeap().Empty());
}

TEST_CASE("Live particle pool returns an instance's particles to the heap",
          "[particle-pool][F01506][F01508][F01509]") {
  const SavedPool saved;
  auto instance = std::make_unique<Instance>();
  fx::LiveAcquireParticlePool(2);
  Particle* first = fx::LiveAllocateEffectParticle(instance.get());
  Particle* second = fx::LiveAllocateEffectParticle(instance.get());
  REQUIRE((first != nullptr && second != nullptr));

  fx::LiveReleaseEffectParticle(second, instance.get());
  CHECK(ListOf(*instance) == fx::ParticleList{.first = first, .count = 1});
  CHECK(VS::Effects::freeParticles == second);
  fx::LiveReleaseAllEffectParticles(instance.get());
  CHECK(LivePool() ==
        fx::ParticlePool{
            .block = first, .free = first, .capacity = 2, .references = 1});

  fx::LiveReleaseParticlePool();
  CHECK((VS::Effects::particlePool == nullptr &&
         VS::Effects::particlePoolReferences == 0));
  CHECK(Memory::GameHeap().Empty());
  fx::LiveReleaseParticlePool();
  CHECK(VS::Effects::particlePoolReferences == 0);
}

TEST_CASE("Live particle list release reads counts above 127 unsigned",
          "[particle-pool][F01509]") {
  const SavedPool saved;
  std::array<Particle, 1> particle{};
  auto instance = std::make_unique<Instance>();
  instance->particles = particle.data();
  instance->particle_count = std::bit_cast<char>(std::uint8_t{0x90});
  VS::Effects::freeParticles = nullptr;
  VS::Effects::usedParticleCount = 0x90;
  fx::LiveReleaseAllEffectParticles(instance.get());
  CHECK(VS::Effects::usedParticleCount == 0);
  CHECK(instance->particle_count == 0);
  CHECK(VS::Effects::freeParticles == particle.data());
}

TEST_CASE("Live particle forces keep the whole gravity record",
          "[particle-pool][F01552][F01553]") {
  const SavedPool saved;
  VECTOR gravity{.vx = 1, .vy = -2, .vz = 3, .pad = 4};
  int drag = 0x55;
  fx::LiveSetParticleGravity(&gravity);
  fx::LiveSetParticleDrag(&drag);
  CHECK(VS::Effects::particleGravity.vx == 1);
  CHECK(VS::Effects::particleGravity.vy == -2);
  CHECK(VS::Effects::particleGravity.vz == 3);
  CHECK(VS::Effects::particleGravity.pad == 4);
  CHECK(VS::Effects::particleDrag == 0x55);
  fx::LiveSetParticleGravity(nullptr);
  fx::LiveSetParticleDrag(nullptr);
  CHECK(VS::Effects::particleDrag == 0x55);
}
