// Hand-maintained (tools/port/owned.csv).
#include "Actors/ModelDrawing.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>

namespace VS::Actors {
namespace {
constexpr int kByteRange = 256;
constexpr int kByteSignBit = 128;
constexpr std::int32_t kDepthLimit = std::numeric_limits<std::uint16_t>::max();
constexpr std::uint8_t kFirstFadeState = 4;
constexpr std::uint8_t kFadeStates = 5;
constexpr std::uint8_t kDelayState = 6;
constexpr std::uint8_t kForcedOverrideState = 15;
}  // namespace

bool UsesModelOverrides(bool forced, ModelAppearance geometry) {
  // The original compares state - 4 unsigned, so states below 4 wrap high.
  const bool fading = static_cast<std::uint8_t>(
                          geometry.state - kFirstFadeState) < kFadeStates &&
                      geometry.state != kDelayState;
  return forced || geometry.pulse_kind != 0 || fading ||
         geometry.state == kForcedOverrideState;
}

ModelDrawRole ClassifyModelDraw(int companion) {
  // The original compares the companion argument as an unsigned byte.
  const auto slot = static_cast<std::uint8_t>(companion);
  if (slot == 0) return ModelDrawRole::kSelf;
  if (slot == 1) return ModelDrawRole::kCompanionOverlay;
  return ModelDrawRole::kCompanionGeometry;
}

// decomp: BATTLE.PRG func_800B28A8 (src/BATTLE/BATTLE.PRG/49EB8.c:41), material
// dispatch at 0x800B3650..0x800B38F8.
ModelDrawResult DrawActorModel(ActorModelDrawHost& host, int companion) {
  if (!host.BuildPose() || !host.IsVisible()) return ModelDrawResult::kNoPose;
  if (host.Height() >= kHiddenActorHeight &&
      (host.IsLeader() || !host.IsEventPlaying())) {
    return ModelDrawResult::kHidden;
  }
  const ModelDrawRole role = ClassifyModelDraw(companion);
  const int geometry_slot = role == ModelDrawRole::kCompanionGeometry
                                ? static_cast<std::uint8_t>(companion)
                                : 0;
  if (!host.SelectGeometry(geometry_slot)) {
    return ModelDrawResult::kMissingGeometry;
  }
  if (!host.BindGeometry()) return ModelDrawResult::kInvalidGeometry;
  ModelDrawResult result = ModelDrawResult::kCulled;
  if (!host.ProjectVertices()) {
    host.SetCulled(true);
  } else {
    host.SetCulled(false);
    const std::optional<ModelAppearance> overrides =
        UsesModelOverrides(host.ModelOverridesForced(),
                           host.GeometryAppearance())
            ? std::optional(host.ActorAppearance())
            : std::nullopt;
    host.UpdateColors(role);
    if (host.ActorHasUvLayout()) {
      host.DrawPolygons(ModelPolygons::kGouraud, overrides);
    } else if (role == ModelDrawRole::kCompanionOverlay) {
      host.DrawPolygons(ModelPolygons::kCompanionFlat, std::nullopt);
    } else {
      host.DrawPolygons(ModelPolygons::kFlat, overrides);
    }
    result = ModelDrawResult::kDrawn;
  }
  if (role == ModelDrawRole::kSelf && host.CastsShadow()) host.DrawShadow();
  return result;
}

std::uint8_t AdjustModelColorChannel(std::uint8_t base,
                                     std::uint8_t adjustment) {
  const int signed_adjustment =
      adjustment < kByteSignBit ? adjustment : adjustment - kByteRange;
  const int value = base + (signed_adjustment * kColorAdjustmentScale);
  return static_cast<std::uint8_t>(
      std::clamp(value, 0, static_cast<int>(kByteRange - 1)));
}

std::uint8_t GroundShadowChannel(std::uint8_t ground) {
  return static_cast<std::uint8_t>((ground >> 1) + (kNeutralModelColor / 2));
}

BoneEndpoint ClassifyBoneEndpoint(int endpoint) {
  if (endpoint == 0) return BoneEndpoint::kBase;
  if (endpoint == 1) return BoneEndpoint::kTip;
  return BoneEndpoint::kMiddle;
}

// 38C1C.c:39 func_800A141C: the tip negates the low halfword of the length; the
// midpoint halves the negated length with C division.
std::int16_t BoneEndpointOffset(std::int32_t length, BoneEndpoint endpoint) {
  switch (endpoint) {
    case BoneEndpoint::kBase:
      return 0;
    case BoneEndpoint::kTip:
      return static_cast<std::int16_t>(
          -static_cast<std::int32_t>(static_cast<std::uint16_t>(length)));
    case BoneEndpoint::kMiddle:
      break;
  }
  return static_cast<std::int16_t>(-static_cast<std::int64_t>(length) / 2);
}

std::int32_t ClampProjectedDepth(std::int32_t depth) {
  return std::clamp<std::int32_t>(depth, 0, kDepthLimit);
}

// decomp: BATTLE.PRG func_800A141C (src/BATTLE/BATTLE.PRG/38C1C.c:39).
int ProjectActorBoneEndpoint(BoneProjectionHost& host, int slot, int bone,
                             int endpoint, std::span<std::int32_t, 2> output) {
  const BoneSelection selection = host.SelectBone(slot, bone);
  if (selection.status == BoneLookup::kMissing) return kBoneProjectionMissing;
  if (selection.status == BoneLookup::kInvalidBone) {
    return kBoneProjectionInvalidBone;
  }
  const ProjectedBonePoint point = host.Project(
      BoneEndpointOffset(selection.length, ClassifyBoneEndpoint(endpoint)));
  output[0] = point.screen;
  output[1] = ClampProjectedDepth(point.depth);
  return 0;
}
}  // namespace VS::Actors
