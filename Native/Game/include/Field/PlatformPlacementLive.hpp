#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <optional>

#include "Field/PlatformPlacement.hpp"
#include "Field/Platforms.h"

// Binding from the current room's moving platforms to the typed contact
// lookup. Contacts are returned by value; nothing is borrowed.
namespace VS::Field {
// The current room's authored platform on `support_layer`, hidden or not.
std::optional<Field_PlatformContactPoint> LiveMovingPlatformPlacement(
    int support_layer);

// The current room's visible platform on `support_layer` (func_800A4A24).
std::optional<Field_PlatformContactPoint> FindLiveMovingPlatformById(
    int support_layer);
}  // namespace VS::Field
