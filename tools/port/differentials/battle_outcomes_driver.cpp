// Semantic service fixture; differential.py executes the original instructions.
#include "Battle/ActionOutcomes.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {
namespace b = VS::Battle;
using Word = std::uint32_t;
using Bytes = std::vector<std::uint8_t>;
constexpr Word kMask = 0xFFFFFFFF, kActionSize = 0x84C;
constexpr Word kScore = 0x8006C004, kStreak = 0x8006C164, kLoot = 0x8006BE64;
constexpr Word kPlacement = 0x800A1108, kSound = 0x80045AA8;
constexpr Word kReaction = 0x8009EA14, kPose = 0x8009E070, kResult = 0x800875C8;
constexpr Word kRefresh = 0x800E6700, kMarker = 0x80095B7C;
constexpr Word kDeath = 0x8009F314, kLock = 0x800A09D8, kDefinitions = 0x8004B9DC;
constexpr std::array<Word, 2> kRecords{0x80600000, 0x80600100};
constexpr std::array<Word, 2> kStats{0x80601000, 0x80601100};
constexpr std::array<Word, 2> kStates{0x80500000, 0x80504000};

Word Read(const Bytes& data, std::size_t offset, std::size_t size = 4) {
  Word value{};
  for (std::size_t i = 0; i < size; ++i)
    value |= static_cast<Word>(data.at(offset + i)) << (i * 8);
  return value;
}
void Put(Bytes& data, std::size_t offset, Word value, std::size_t size = 4) {
  for (std::size_t i = 0; i < size; ++i)
    data.at(offset + i) = static_cast<std::uint8_t>(value >> (i * 8));
}
Bytes Block(Word seed, std::size_t size) {
  Bytes data(size);
  for (std::size_t i = 0; i < size / 4; ++i)
    Put(data, i * 4, seed ^ (static_cast<Word>(i) * 0x9E3779B9));
  return data;
}
std::array<Word, 3> Point(Word seed, Word actor) {
  return {(seed + actor * 193) & 0xFFFF,
          ((seed >> 9) - actor * 71) & 0xFFFF,
          ((seed >> 17) + actor * 257) & 0xFFFF};
}
b::ActionPoint NativePoint(const std::array<Word, 3>& point) {
  return {std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(point[0])),
          std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(point[1])),
          std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(point[2])), 0};
}
std::array<Word, 3> Coordinates(const Bytes& data, std::size_t offset) {
  return {Read(data, offset, 2), Read(data, offset + 2, 2), Read(data, offset + 4, 2)};
}
class World;
class Action final : public b::ActionOutcomeAction {
 public:
  Action(World& world, int owner) : world_(world), owner_(owner) {}
  std::uint16_t Count() const override;
  b::ActionTargetHit Target(std::size_t index) const override;
  b::ActionTargetHit Primary() const override;
  void DamageReaction(int actor) override;
  void ApplyPrimaryResult() override;

 private:
  World& world_;
  int owner_;
};
class Enemy final : public b::EnemyDefeatRecord {
 public:
  explicit Enemy(World& world) : world_(world) {}
  std::uint8_t Spawn() const override;
  bool Defeated() const override;
  void RecordKill() override;
  void UpdateWeaponStreak() override;
  void QueueLoot() override;
  void MarkDefeated() override;

 private:
  World& world_;
};
class World final : public b::ActionOutcomeWorld, public b::ActorDefeatWorld,
                    public b::LearnedActionWorld {
 public:
  Word function{}, lane{}, mutation{}, seed{}, source{}, target{}, kill{};
  Word present{}, index{}, count{}, global = kStates[0], actor_pointer{};
  Word result{}, diagnostics{}, placements{}, sounds{};
  std::array<std::array<Word, 6>, 2> fields{};
  std::array<Word, 2> primary{}, indices{};
  std::array<std::array<Word, 2>, 30> hits{};
  std::array<Word, 8> flag_words{};
  std::array<Bytes, 2> actors, stats, hp, actions, origins;
  Bytes definitions, flags;
  std::array<Action, 2> bound{Action(*this, 0), Action(*this, 1)};
  std::vector<Word> calls, writes;

  Word QueueAddress(int owner) const {
    return kStates[owner] + 0x854 + ((index + static_cast<Word>(owner)) & 3) * kActionSize;
  }
  void Initialize() {
    actor_pointer = present || function != 3 ? kRecords[0] : 0;
    for (int owner = 0; owner < 2; ++owner) {
      actors[owner] = Block(seed ^ (static_cast<Word>(owner) * 0xA1110000), 0x50);
      const auto& f = fields[owner];
      for (const auto& pair : std::array<std::array<Word, 2>, 5>{
               {{8, f[5]}, {0x25, f[2]}, {0x26, f[3]}, {0x27, f[0]}, {0x28, f[1]}}})
        Put(actors[owner], pair[0], pair[1], 1);
      Put(actors[owner], 0x3C, kStats[owner]);
      stats[owner] = Block(seed ^ (0xB0000000 + static_cast<Word>(owner)), 8);
      hp[owner] = Bytes(4);
      Put(hp[owner], 0, f[4], 2);
      Put(hp[owner], 2, seed >> 16, 2);
      indices[owner] = index + static_cast<Word>(owner);
      actions[owner] = Block(seed ^ (0xC0000000 + static_cast<Word>(owner)), kActionSize);
      Put(actions[owner], 0x4A, owner == 0 ? count : 0, 2);
      Put(actions[owner], 4, primary[0] ^ (static_cast<Word>(owner) * 0x01000001));
      Put(actions[owner], 0x44, primary[1]);
      for (std::size_t hit = 0; hit < hits.size(); ++hit) {
        Put(actions[owner], 0x4C + hit * 0x44,
            hits[hit][0] ^ (static_cast<Word>(owner) * 0x01000001));
        Put(actions[owner], 0x8C + hit * 0x44, hits[hit][1]);
      }
      origins[owner] = Block(seed ^ (0xD0000000 + static_cast<Word>(owner)), 8);
    }
    definitions = Block(seed ^ 0xE0000000, 256 * 0x34);
    flags = Bytes(32);
    for (std::size_t i = 0; i < flag_words.size(); ++i)
      Put(flags, i * 4, flag_words[i]);
  }
  int Actor() {
    if (actor_pointer == kRecords[0]) return 0;
    if (actor_pointer == kRecords[1]) return 1;
    ++diagnostics;
    return 0;
  }
  void ChangePrimary() {
    global = kStates[1];
    Put(actions[0], 4, 0xFF010009);
    Put(actions[0], 8, 0x80007FFF);
    Put(actions[0], 0x44, 1);
  }
  void Mutate(Word address) {
    if (function == 0) {
      if (((mutation == 1 || mutation == 4) && address == kScore) ||
          (mutation == 2 && address == kStreak) ||
          ((mutation == 3 || mutation == 5) && address == kLoot)) {
        actor_pointer = kRecords[1];
        Put(actors[0], 0x27, 0x80, 1);
        Put(actors[0], 0x28, address == kLoot ? 0 : 255, 1);
      }
    } else if (function == 1) {
      if (address == kPlacement && placements == 1) {
        if (mutation == 1) Put(actions[0], 0x4F, 3, 1);
        if (mutation == 2 || mutation == 6) {
          global = kStates[1];
          indices[0] = index ^ 3;
          Put(actions[0], 0x4F, mutation == 2 ? 0 : 3, 1);
          Put(actions[0], 0x8C, 1);
          actor_pointer = kRecords[1];
        }
      }
      if (address == kSound && sounds == 1) {
        if (mutation == 3 || mutation == 4 || mutation == 5 || mutation == 7 || mutation == 8) {
          actor_pointer = kRecords[1];
          Put(actors[1], 0x25, 6, 1);
          Put(actions[0], 0x4C, 255, 1);
          global = kStates[1];
        }
        if (mutation == 4 || mutation == 7 || mutation == 8) {
          Put(actions[0], 0x90, 0x040000FF);
          Put(actions[0], 0xD0, 0);
          Put(actions[0], 0x4A, 3, 2);
        }
        if (mutation == 5) Put(actions[0], 0x4A, 1, 2);
      }
    } else if (function == 2) {
      if ((address == kReaction && (mutation == 1 || mutation == 3)) ||
          (address == kPose && mutation == 2))
        ChangePrimary();
      if (address == kResult && mutation == 4) ChangePrimary();
    } else if (function == 3) {
      if (address == kRefresh) {
        if (mutation == 1) actor_pointer = kRecords[0];
        else if (mutation == 2) actor_pointer = 0;
        else if (mutation == 3) actor_pointer = kRecords[1];
      }
      if (address == kMarker && (mutation == 4 || mutation == 5)) {
        actor_pointer = kRecords[1];
        Put(actors[1], 0x26, mutation == 4 ? 1 : 0, 1);
        global = kStates[1];
      }
      if (address == kDeath && mutation == 6) {
        actor_pointer = kRecords[1];
        Put(actors[1], 0x27, 0x80, 1);
      }
      if (address == kLock && (mutation == 7 || mutation == 8 || mutation == 9)) {
        actor_pointer = kRecords[1];
        Put(actors[1], 0x27, mutation == 7 ? 0x80 : 1, 1);
      }
    }
  }
  void Event(Word address, std::array<Word, 3> args = {},
             std::array<Word, 3> point = {}) {
    calls.insert(calls.end(), {address, args[0], args[1], args[2], point[0], point[1],
                              point[2], global, actor_pointer, Read(actors[0], 8, 1),
                              Read(actors[1], 8, 1), Read(actions[0], 4),
                              Read(actions[0], 8), Read(actions[0], 0x44),
                              Read(actions[0], 0x4A, 2)});
    Mutate(address);
  }
  b::ActionOutcomeAction& Queued() override {
    if (global == kStates[0]) return bound[0];
    if (global == kStates[1]) return bound[1];
    ++diagnostics;
    return bound[0];
  }
  b::ActionPoint Placement(int actor) override {
    const auto point = Point(seed, static_cast<Word>(actor));
    ++placements;
    Event(kPlacement, {static_cast<Word>(actor), 0, 0}, point);
    return NativePoint(point);
  }
  std::uint8_t WeaponCategory(int actor) override {
    if (actor < 0 || actor >= 42) ++diagnostics;
    return static_cast<std::uint8_t>(Read(actors[Actor()], 0x25, 1));
  }
  void PlaySound(std::uint8_t sound, b::ActionPoint point) override {
    ++sounds;
    Event(kSound, {0x7E, sound, 0},
          {static_cast<std::uint16_t>(point.x), static_cast<std::uint16_t>(point.y),
           static_cast<std::uint16_t>(point.z)});
  }
  void RecoveryPose() override { Event(kPose, {0, 0, 6}); }
  void RefreshReservations(int actor) override {
    Event(kRefresh, {static_cast<Word>(actor), 0, 0});
  }
  bool HasActor(int) override { return actor_pointer != 0; }
  void SetCommandState(int, std::uint8_t state) override {
    const int owner = Actor();
    Put(actors[owner], 8, state, 1);
    writes.insert(writes.end(), {kRecords[owner] + 8, 1});
  }
  std::int16_t Hp(int) override {
    return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(Read(hp[Actor()], 0, 2)));
  }
  void HideMarker(int actor) override {
    Event(kMarker, {static_cast<Word>(actor), 0, 0});
  }
  bool Retained(int) override { return Read(actors[Actor()], 0x26, 1) != 0; }
  void BeginDeath(int actor) override {
    const int owner = global == kStates[1] ? 1 : 0;
    if (global != kStates[owner]) ++diagnostics;
    Event(kDeath, {static_cast<Word>(actor), global + 0x29A8, 1},
          Coordinates(origins[owner], 0));
  }
  void LockAnimation(int actor) override {
    Event(kLock, {static_cast<Word>(actor), 1, 0});
  }
  std::uint8_t Spawn(int) override {
    return static_cast<std::uint8_t>(Read(actors[Actor()], 0x27, 1));
  }
  std::uint8_t LearnedByte(std::size_t offset) const override { return flags.at(offset); }
  void SetUnlocked(std::size_t action, bool unlocked) override {
    const auto offset = action * 0x34 + 13;
    definitions.at(offset) =
        static_cast<std::uint8_t>((definitions.at(offset) & 0x7F) | (unlocked ? 0x80 : 0));
    writes.insert(writes.end(), {kDefinitions + static_cast<Word>(action) * 0x34 + 12, 4});
  }
};
std::uint16_t Action::Count() const {
  const auto count = Read(world_.actions[owner_], 0x4A, 2);
  if (count > 30) ++world_.diagnostics;
  return static_cast<std::uint16_t>(count);
}
b::ActionTargetHit ReadHit(const Bytes& action, std::size_t at) {
  return {Read(action, at + 0x40) != 0, static_cast<int>(Read(action, at, 1)),
          static_cast<std::uint8_t>(Read(action, at + 3, 1))};
}
b::ActionTargetHit Action::Target(std::size_t index) const {
  return ReadHit(world_.actions[owner_], 0x4C + index * 0x44);
}
b::ActionTargetHit Action::Primary() const { return ReadHit(world_.actions[owner_], 4); }
void Action::DamageReaction(int actor) {
  world_.Event(kReaction, {static_cast<Word>(actor), world_.QueueAddress(owner_) + 0x844, 0},
               Coordinates(world_.actions[owner_], 0x844));
}
void Action::ApplyPrimaryResult() {
  world_.Event(kResult, {world_.QueueAddress(owner_) + 4, 0, 0});
}
std::uint8_t Enemy::Spawn() const {
  return static_cast<std::uint8_t>(Read(world_.actors[0], 0x27, 1));
}
bool Enemy::Defeated() const { return Read(world_.actors[0], 0x28, 1) != 0; }
void Enemy::RecordKill() { world_.Event(kScore, {kRecords[0], 0, 0}); }
void Enemy::UpdateWeaponStreak() { world_.Event(kStreak); }
void Enemy::QueueLoot() { world_.Event(kLoot, {kRecords[0], 0, 0}); }
void Enemy::MarkDefeated() {
  Put(world_.actors[0], 0x28, 1, 1);
  world_.writes.insert(world_.writes.end(), {kRecords[0] + 0x28, 1});
}
void Append(std::vector<Word>& values, const Bytes& data) {
  for (std::size_t at = 0; at < data.size(); at += 4) values.push_back(Read(data, at));
}
void Print(const World& world) {
  std::vector<Word> values{world.diagnostics, world.result, world.global,
                           world.actor_pointer, world.indices[0], world.indices[1]};
  for (const auto& origin : world.origins) Append(values, origin);
  if (world.function == 4) {
    Append(values, world.definitions);
    Append(values, world.flags);
  } else {
    for (const auto& actor : world.actors) Append(values, actor);
    for (const auto& stats : world.stats) Append(values, stats);
    for (const auto& hp : world.hp) Append(values, hp);
    if (world.function == 1 || world.function == 2)
      for (const auto& action : world.actions) Append(values, action);
  }
  values.push_back(static_cast<Word>(world.writes.size()));
  values.insert(values.end(), world.writes.begin(), world.writes.end());
  values.push_back(static_cast<Word>(world.calls.size()));
  values.insert(values.end(), world.calls.begin(), world.calls.end());
  for (const auto value : values) std::cout << value << ' ';
  std::cout << '\n';
}
void Execute(World& world) {
  const int source = std::bit_cast<std::int32_t>(world.source);
  switch (world.function) {
    case 0: {
      Enemy enemy(world);
      b::ResolveEnemyDefeat(source, std::bit_cast<std::int32_t>(world.kill), enemy);
      break;
    }
    case 1:
      world.result = static_cast<Word>(b::PlayWeaponHitSounds(source, world));
      break;
    case 2:
      b::ApplyActionSourceEffects(source, world);
      break;
    case 3:
      world.result = static_cast<Word>(b::BeginActorDefeatAnimation(source, world));
      break;
    case 4:
      b::ApplyLearnedActionUnlocks(world);
      if (world.mutation) {
        for (std::size_t i = 0; i < world.flag_words.size(); ++i)
          Put(world.flags, i * 4, world.mutation == 1 ? 0 : world.flag_words[i] ^ kMask);
        b::ApplyLearnedActionUnlocks(world);
      }
      break;
  }
}
}  // namespace

int main() {
  std::ios::sync_with_stdio(false);
  std::string line;
  while (std::getline(std::cin, line)) {
    World world;
    std::istringstream input(line);
    input >> world.function >> world.lane >> world.mutation >> world.seed >> world.source
          >> world.target >> world.kill >> world.present >> world.index >> world.count;
    for (auto& fields : world.fields)
      for (auto& value : fields) input >> value;
    input >> world.primary[0] >> world.primary[1];
    for (auto& hit : world.hits) input >> hit[0] >> hit[1];
    for (auto& flags : world.flag_words) input >> flags;
    if (!input) return 2;
    world.Initialize();
    Execute(world);
    Print(world);
  }
}
