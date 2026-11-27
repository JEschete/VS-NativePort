#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <utility>
#include <vector>

#include "Content/ActorAssets.hpp"

namespace VS::Content {

// Immutable compiled resources consumed by the existing renderer and
// playback adapters. Their format retirement remains EC-04-T-SHP/WEP/SEQ;
// no resource owns or borrows an actor record or a game-heap allocation.
class ActorSequenceResource final {
 public:
  ActorSequenceResource(std::span<const std::byte> bytes,
                        std::uint8_t animation_count)
      : bytes_(bytes.begin(), bytes.end()), animation_count_(animation_count) {}

  [[nodiscard]] std::span<const std::byte> Bytes() const { return bytes_; }
  [[nodiscard]] std::uint8_t AnimationCount() const { return animation_count_; }

 private:
  std::vector<std::byte> bytes_;
  std::uint8_t animation_count_ = 0;
};

class ActorModelResource final {
 public:
  ActorModelResource(
      ActorAssetRecord definition, std::span<const std::byte> geometry,
      std::span<const std::byte, kActorTextureLayoutBytes> layout,
      std::shared_ptr<const ActorSequenceResource> embedded_sequence)
      : definition_(definition),
        geometry_(geometry.begin(), geometry.end()),
        embedded_sequence_(std::move(embedded_sequence)) {
    definition_.model = AssetAddress::kNone;
    definition_.owned_sequence = AssetAddress::kNone;
    definition_.common_sequence = AssetAddress::kNone;
    definition_.battle_sequence = AssetAddress::kNone;
    definition_.shared_sequences.fill(AssetAddress::kNone);
    definition_.slot = 0;
    definition_.companion_slot = 0;
    definition_.companion = {};
    for (std::size_t i = 0; i < layout_.size(); ++i) layout_[i] = layout[i];
  }

  [[nodiscard]] const ActorAssetRecord& Definition() const {
    return definition_;
  }
  [[nodiscard]] std::span<const std::byte> Geometry() const {
    return geometry_;
  }
  [[nodiscard]] std::span<const std::byte> Layout() const { return layout_; }
  [[nodiscard]] const std::shared_ptr<const ActorSequenceResource>&
  EmbeddedSequence() const {
    return embedded_sequence_;
  }

 private:
  ActorAssetRecord definition_;
  std::vector<std::byte> geometry_;
  std::array<std::byte, kActorTextureLayoutBytes> layout_{};
  std::shared_ptr<const ActorSequenceResource> embedded_sequence_;
};

class WeaponModelResource final {
 public:
  WeaponModelResource(WeaponAssetRecord definition,
                      std::span<const std::byte> geometry,
                      std::span<const std::byte> bones)
      : definition_(definition),
        geometry_(geometry.begin(), geometry.end()),
        bones_(bones.begin(), bones.end()) {
    definition_.geometry = AssetAddress::kNone;
    definition_.actor_id = 0;
    definition_.attachment_slot = 0;
    definition_.attachment_bone = 0;
  }

  [[nodiscard]] const WeaponAssetRecord& Definition() const {
    return definition_;
  }
  [[nodiscard]] std::span<const std::byte> Geometry() const {
    return geometry_;
  }
  [[nodiscard]] std::span<const std::byte> Bones() const { return bones_; }

 private:
  WeaponAssetRecord definition_;
  std::vector<std::byte> geometry_;
  std::vector<std::byte> bones_;
};

enum class ActorSequenceKind : std::uint8_t {
  kEmbedded,
  kCommon,
  kBattle,
  kSpecial
};

struct ActorSequenceResources {
  std::shared_ptr<const ActorSequenceResource> common;
  std::shared_ptr<const ActorSequenceResource> battle;
  std::uint8_t battle_model = 0;
};

}  // namespace VS::Content
