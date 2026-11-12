// Native side of status_helpers.py. Original 146C.c:13801-13830,
// 14011-14016 and 14745-14765; func_80086EA4/86EF4/87578/88B8C/88CA0.
#include "Battle/StatusHelpers.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

namespace {
using Guards = std::array<std::uint32_t, 8>;
using Event = std::array<std::uint32_t, 5>;
using Trace = std::vector<Event>;
using StatsImage = std::array<std::uint32_t, (0x970 + 32) / 4>;
using Armor = std::array<VS::Items::ArmorLoadout, VS::Items::kLimbCount>;
unsigned diagnostics = 0;

template <typename T>
struct Guarded {
  std::array<std::uint32_t, 4> before{};
  T value{};
  std::array<std::uint32_t, 4> after{};
};

template <typename T>
void SetGuards(Guarded<T>& object, const Guards& guards) {
  for (std::size_t index = 0; index < 4; ++index) {
    object.before[index] = guards[index];
    object.after[index] = guards[index + 4];
  }
}

template <typename T>
bool GuardsUnchanged(const Guarded<T>& object, const Guards& guards) {
  for (std::size_t index = 0; index < 4; ++index) {
    if (object.before[index] != guards[index] ||
        object.after[index] != guards[index + 4]) return false;
  }
  return true;
}

template <typename T>
void Decode(const StatsImage& image, std::size_t offset, T& record) {
  static_assert(std::is_trivially_copyable_v<T>);
  std::memcpy(&record, reinterpret_cast<const std::byte*>(image.data()) + 16 + offset, sizeof(record));
}

template <typename T>
void Encode(StatsImage& image, std::size_t offset, const T& record) {
  static_assert(std::is_trivially_copyable_v<T>);
  std::memcpy(reinterpret_cast<std::byte*>(image.data()) + 16 + offset, &record, sizeof(record));
}

void PrintPrefix(bool refused, bool guards_ok, bool inputs_ok) {
  std::cout << refused << ' ' << diagnostics << ' ' << guards_ok << ' ' << inputs_ok;
}

void PrintTrace(const Trace& trace) {
  std::cout << ' ' << trace.size();
  for (const auto& event : trace) {
    for (const auto word : event) std::cout << ' ' << word;
  }
}

class NumberWorld final : public VS::Battle::StatNumberWorld {
 public:
  NumberWorld(int slot, std::uint8_t spawn) : slot_(slot), spawn_(spawn) {}

  std::optional<std::uint8_t> RoomSpawnIndex(int slot) const override {
    trace.push_back({0, static_cast<std::uint32_t>(slot), spawn_, 0, 0});
    return slot == slot_ ? std::optional<std::uint8_t>{spawn_} : std::nullopt;
  }

  void QueueValueLabel(const VS::Battle::StatNumberRequest& request) override {
    trace.push_back({1, static_cast<std::uint32_t>(request.actor_slot),
                     static_cast<std::uint32_t>(request.limb_tag), request.encoded_amount,
                     static_cast<std::uint32_t>(request.label_flags)});
  }

  mutable Trace trace;

 private:
  const int slot_;
  const std::uint8_t spawn_;
};

class RiskWorld final : public VS::Battle::TargetingRiskWorld {
 public:
  RiskWorld(const std::array<std::uint32_t, 10>& inputs, const Guards& guards)
      : inputs_(inputs) {
    risk.value = static_cast<std::uint16_t>(inputs_[7]);
    SetGuards(risk, guards);
  }

  std::uint32_t ControlMode() const override {
    trace.push_back({0, inputs_[0], 0, 0, 0});
    return inputs_[0];
  }

  std::uint32_t ActionFlowMode() const override {
    trace.push_back({1, inputs_[1], 0, 0, 0});
    return inputs_[1];
  }

  std::int32_t TickSpeed() const override {
    trace.push_back({2, inputs_[3], 0, 0, 0});
    return std::bit_cast<std::int32_t>(inputs_[3]);
  }

  std::optional<VS::Battle::TargetingRiskTarget> SelectedTarget() const override {
    trace.push_back({3, inputs_[4], inputs_[5], inputs_[6] != 0, 0});
    if (inputs_[8] == 0) return std::nullopt;
    return VS::Battle::TargetingRiskTarget{
        static_cast<int>(inputs_[4]), inputs_[5], inputs_[6] != 0};
  }

  std::optional<std::uint16_t> PlayerRisk() const override {
    trace.push_back({4, risk.value, 0, 0, 0});
    if (inputs_[9] == 0) return std::nullopt;
    return risk.value;
  }

  void SetPlayerRisk(std::uint16_t value) override {
    trace.push_back({5, value, 0, 0, 0});
    risk.value = value;
  }

  bool InputsUnchanged(const std::array<std::uint32_t, 10>& before) const {
    return inputs_ == before;
  }

  Guarded<std::uint16_t> risk;
  mutable Trace trace;

 private:
  const std::array<std::uint32_t, 10> inputs_;
};

int Enchantment(std::istringstream& input, unsigned function, const Guards& guards) {
  Guarded<VS::Battle::ElementEnchantment> change;
  std::uint32_t amount = 0;
  input >> change.value.increased_affinity >> change.value.decreased_affinity >> amount;
  change.value.amount = std::bit_cast<std::int32_t>(amount);
  StatsImage image{};
  for (auto& word : image) input >> word;
  if (!input) return 2;
  const auto original_image = image;
  const auto original_change = change.value;
  Guarded<VS::Items::WeaponLoadout> weapon;
  Guarded<VS::Items::ShieldLoadout> shield;
  Guarded<Armor> armor;
  SetGuards(change, guards);
  SetGuards(weapon, guards);
  SetGuards(shield, guards);
  SetGuards(armor, guards);
  // Original stats offsets exist only in this test's ABI projection. The calls
  // operate on the complete native records, including their untouched fields.
  Decode(image, 0x3C, weapon.value);
  Decode(image, 0x1D4, shield.value);
  for (std::size_t limb = 0; limb < armor.value.size(); ++limb) {
    Decode(image, 0x3B8 + limb * 0xDC, armor.value[limb]);
  }
  const bool accepted = function == 0
      ? VS::Battle::ApplyWeaponElementEnchantment(weapon.value, change.value)
      : VS::Battle::ApplyArmorElementEnchantment(shield.value, armor.value, change.value);
  const bool guards_ok = GuardsUnchanged(change, guards) && GuardsUnchanged(weapon, guards) &&
                         GuardsUnchanged(shield, guards) && GuardsUnchanged(armor, guards);
  const bool inputs_ok = image == original_image &&
                        change.value.increased_affinity == original_change.increased_affinity &&
                        change.value.decreased_affinity == original_change.decreased_affinity &&
                        change.value.amount == original_change.amount;
  Encode(image, 0x3C, weapon.value);
  Encode(image, 0x1D4, shield.value);
  for (std::size_t limb = 0; limb < armor.value.size(); ++limb) {
    Encode(image, 0x3B8 + limb * 0xDC, armor.value[limb]);
  }
  PrintPrefix(!accepted, guards_ok, inputs_ok);
  for (const auto word : image) std::cout << ' ' << word;
  return 0;
}

int Number(std::istringstream& input, const Guards& guards) {
  std::array<std::uint32_t, 6> args{};
  for (auto& word : args) input >> word;
  if (!input || args[0] >= 42 || args[4] > 255) return 2;
  Guarded<VS::Battle::StatNumberRequest> request;
  SetGuards(request, guards);
  request.value = {static_cast<int>(args[0]), std::bit_cast<std::int32_t>(args[1]), args[2],
                   std::bit_cast<std::int32_t>(args[3])};
  const auto before = request.value;
  NumberWorld world(request.value.actor_slot, static_cast<std::uint8_t>(args[4]));
  const auto result = VS::Battle::QueueActorStatChangeNumber(world, request.value);
  const bool inputs_ok = request.value.actor_slot == before.actor_slot &&
                        request.value.limb_tag == before.limb_tag &&
                        request.value.encoded_amount == before.encoded_amount &&
                        request.value.label_flags == before.label_flags;
  PrintPrefix(!result.has_value(), GuardsUnchanged(request, guards), inputs_ok);
  PrintTrace(world.trace);
  return 0;
}

int Risk(std::istringstream& input, unsigned function, const Guards& guards) {
  std::array<std::uint32_t, 10> args{};
  for (auto& word : args) input >> word;
  if (!input || args[4] >= 16 || args[7] > 65535) return 2;
  Guarded<std::int32_t> timer;
  SetGuards(timer, guards);
  timer.value = std::bit_cast<std::int32_t>(args[2]);
  RiskWorld world(args, guards);
  bool accepted = true;
  if (function == 3) {
    accepted = VS::Battle::UpdateTargetingRiskPenalty(timer.value, world);
  } else {
    VS::Battle::ResetTargetingRiskPenaltyTimer(timer.value);
  }
  PrintPrefix(!accepted, GuardsUnchanged(timer, guards) && GuardsUnchanged(world.risk, guards),
              world.InputsUnchanged(args));
  std::cout << ' ' << std::bit_cast<std::uint32_t>(timer.value) << ' ' << world.risk.value;
  PrintTrace(world.trace);
  return 0;
}
}  // namespace

extern "C" void Core_Debugf(const char*, const char*, ...) { ++diagnostics; }

int main() {
  std::string line;
  while (std::getline(std::cin, line)) {
    std::istringstream input(line);
    unsigned function = 0;
    Guards guards{};
    input >> function;
    for (auto& word : guards) input >> word;
    if (!input || function > 4) return 2;
    diagnostics = 0;
    const auto result = function < 2 ? Enchantment(input, function, guards)
                        : function == 2 ? Number(input, guards)
                                        : Risk(input, function, guards);
    if (result != 0) return result;
    std::cout << '\n';
  }
}
