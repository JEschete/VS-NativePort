#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

// NOLINTNEXTLINE(readability-identifier-naming): established subsystem API.
namespace VS::Menu::Magic {

struct SpellCandidate {
  std::uint8_t source_index{};
  std::uint8_t action_id{};
  bool unlocked{};
};

struct AvailableSpell {
  std::uint8_t source_index{};
  std::uint8_t action_id{};
};

struct AvailableSpellList {
  static constexpr std::size_t kCapacity = 18;

  std::array<AvailableSpell, kCapacity> spells{};
  std::size_t count{};
};

[[nodiscard]] std::optional<AvailableSpellList> BuildAvailableSpellList(
    std::span<const SpellCandidate> candidates) noexcept;

void ApplyGlobalSpellRestrictions(const AvailableSpellList& available,
                                  std::span<int> row_types,
                                  int action_restrictions) noexcept;

}  // namespace VS::Menu::Magic
