#pragma once

#include <array>

#include "Content/CatalogTable.hpp"

namespace VS::Content {

// ItemRows.cpp uses material - 1 + WoodName, category - 1 + Dagger and
// damage_type - 1 + Blunt. StringTables.hpp preserves those source lists.
inline constexpr std::array<EnumValue, 8> kMaterials{{
    {0, "none"},
    {1, "wood"},
    {2, "leather"},
    {3, "bronze"},
    {4, "iron"},
    {5, "hagane"},
    {6, "silver"},
    {7, "damascus"},
}};
inline constexpr std::array<EnumValue, 10> kBladeCategories{{
    {0, "none"},
    {1, "dagger"},
    {2, "sword"},
    {3, "great-sword"},
    {4, "axe-mace"},
    {5, "great-axe"},
    {6, "staff"},
    {7, "heavy-mace"},
    {8, "polearm"},
    {9, "crossbow"},
}};
// ItemRows.cpp uses category - 1 + ShortGrip (ITEMHELP 641-644: "Grip for
// Daggers/Swords/Great Swords", "Grip for Axes/Maces/Staves", "Grip for
// Polearms", "Bolt for Crossbows"); the names are the decomp's asset keys.
// Assembly.hpp's kGripBladeCategories lists the blade categories each holds.
inline constexpr std::array<EnumValue, 5> kGripTypes{{
    {0, "none"},
    {1, "short-grip"},
    {2, "long-grip"},
    {3, "polearm-grip"},
    {4, "crossbow-grip"},
}};
// ItemRows.cpp uses category - 1 + Shield (ITEMHELP 645-649) for shields and
// armor pieces. Accessories (7) never combine; 6 has no ITEMHELP text.
inline constexpr std::array<EnumValue, 6> kArmorCategories{{
    {0, "none"},
    {1, "shield"},
    {2, "helm"},
    {3, "breast-armor"},
    {4, "leggings"},
    {5, "glove"},
}};
// The volume kinds the targeting code implements; 0 is no volume.
// decomp: VolumeKind (Native/Game/src/Battle/SphereContainment.cpp:28)
inline constexpr std::array<EnumValue, 8> kVolumeShapes{{
    {0, "none"},
    {1, "ellipsoid"},
    {2, "centered-cylinder"},
    {3, "cylinder"},
    {4, "cone"},
    {5, "reversed-cone"},
    {6, "rectangle"},
    {7, "diamond"},
}};
inline constexpr std::array<EnumValue, 4> kDamageTypes{{
    {0, "none"},
    {1, "blunt"},
    {2, "edged"},
    {3, "piercing"},
}};
// ActionCostsLive.cpp consumes weapon cost_type through ActionCosts.cpp's
// resource selector, not ActionDefinition::type.
inline constexpr std::array<EnumValue, 5> kCostResources{{
    {0, "none"},
    {1, "mp"},
    {2, "risk"},
    {3, "hp"},
    {4, "weapon-pp"},
}};
// Targeting.cpp selects status strings as enemy_class + Human (51).
inline constexpr std::array<EnumValue, 6> kEnemyClasses{{
    {0, "human"},
    {1, "beast"},
    {2, "undead"},
    {3, "phantom"},
    {4, "dragon"},
    {5, "evil"},
}};

}  // namespace VS::Content
