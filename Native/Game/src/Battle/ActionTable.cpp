// The action table's storage and the bridge that fills it from the catalog.
#include "Battle/ActionTable.hpp"

#include <algorithm>
#include <bit>
#include <format>
#include <optional>
#include <string>

#include "Content/ActionCatalog.hpp"

namespace VS::Battle {
namespace {
static_assert(kActionTableCount == Content::kActionCount);
constexpr unsigned kAngleShift = 3;

ActionHitParams HitParams(const std::optional<Content::ActionHit>& hit) {
  ActionHitParams params{};
  if (!hit) return params;
  params.effect_id = hit->effect;
  params.prerequisite_function_id = hit->prerequisite;
  params.hit_function_id = hit->hit_function;
  params.stat_calculator_id = hit->stat_calculator;
  params.stat_factor = hit->stat_factor;
  params.damage_type = hit->damage_type;
  params.affinity = hit->affinity;
  return params;
}
}  // namespace

// decomp: vs_main_actions (src/SLUS_010.40/actions.h:64)
ActionDefinitionTable actionDefinitions;

std::expected<void, std::string> BuildActionTable(
    const Content::ActionCatalog& catalog, ActionDefinitionTable& table) {
  ActionDefinitionTable built{};
  for (std::size_t index = 0; index < built.size(); ++index) {
    const auto& rule = catalog.actions[index];
    const auto name = Content::EncodeActionName(rule.name);
    if (!name) {
      return std::unexpected(std::format("action {}: {}", index, name.error()));
    }
    auto& action = built[index];
    action.id = static_cast<std::uint8_t>(index);
    action.effect_id = rule.effect_id;
    action.returns_to_menu_on_cancel =
        (rule.behavior & Content::kBehaviorMenuAction) != 0;
    action.type = rule.kind;
    action.category = rule.category;
    action.cost = rule.cost;
    action.range_x = rule.range.extent[0];
    action.range_y = rule.range.extent[1];
    action.range_z = rule.range.extent[2];
    action.shape = rule.range.shape;
    action.angle = rule.range.angle;
    action.area_range_x = rule.area.extent[0];
    action.area_range_y = rule.area.extent[1];
    action.area_range_z = rule.area.extent[2];
    action.area_shape = static_cast<std::uint8_t>(
        rule.area.shape | (rule.area.angle << kAngleShift));
    action.recovery_delay = rule.recovery_delay;
    action.camera_zoom_mode = rule.camera_zoom;
    action.hits_first_obstruction =
        (rule.behavior & Content::kBehaviorHitsFirstObstruction) != 0;
    action.unlocked = (rule.behavior & Content::kBehaviorStartsUnlocked) != 0;
    action.ai_flags = std::bit_cast<ActionAiFlags>(
        static_cast<std::uint16_t>(rule.behavior & Content::kBehaviorAiMask));
    for (std::size_t slot = 0; slot < rule.hits.size(); ++slot) {
      action.hits[slot] = HitParams(rule.hits[slot]);
    }
    std::ranges::transform(*name, action.name.begin(), [](std::byte value) {
      return static_cast<char>(value);
    });
  }
  table = built;
  return {};
}

}  // namespace VS::Battle
