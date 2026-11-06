// Hand-maintained (tools/port/owned.csv).
#include "Field/RoomTriggerMarkersLive.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <vector>

#include "Field/CollisionMapLive.hpp"
#include "Field/RoomTriggerMarkers.hpp"
#include "Render/RoomDrawLive.hpp"

extern "C" {
#include "Audio/Seq/Channel.h"
#include "Field/CameraLiveState.h"
#include "Field/Collision.h"
#include "Field/Gimmicks.h"
#include "Field/Mpd.h"
#include "Field/TriggerMarkers.h"
#include "Render/UiPrimitives.h"
#include "State/Timers.h"
}

namespace {
constexpr std::size_t kMarkerRecordBytes = 116;
constexpr std::size_t kMarkerPolygonCountsOffset = 68;
constexpr std::size_t kMarkerQuadOffset = 76;
constexpr int kMarkerTextureTargetX = 64;
constexpr int kMarkerTextureTargetY = 384;
constexpr int kMarkerPaletteTargetY = 320;
}  // namespace

// Complete D_800E8548 definition from BATTLE.PRG:0x7FD48, loaded by Content.
extern "C" unsigned int
    Content_battleTriggerMarkerBytes[kMarkerRecordBytes / sizeof(unsigned int)];

namespace {
// One-group room geometry: the count, the group record, the polygon counts
// and the single marker quad.
struct RoomTriggerMarkerGeometry {
  unsigned group_count;
  std::array<std::byte, VS::Render::kRoomGroupRecordBytes> group;
  unsigned triangle_count;
  unsigned quad_count;
  Field_MpdQuad quad;
};

static_assert(sizeof(RoomTriggerMarkerGeometry) == kMarkerRecordBytes,
              "Complete original trigger marker definition");
static_assert(offsetof(RoomTriggerMarkerGeometry, group) == sizeof(unsigned),
              "Trigger marker group record");
static_assert(offsetof(RoomTriggerMarkerGeometry, triangle_count) ==
                  kMarkerPolygonCountsOffset,
              "Trigger marker polygon counts");
static_assert(offsetof(RoomTriggerMarkerGeometry, quad) == kMarkerQuadOffset,
              "Trigger marker quad");

class LiveTriggerMarkerRenderer final
    : public VS::Field::RoomTriggerMarkerRenderer {
 public:
  [[nodiscard]] std::uint16_t FloorHeightCode(int world_x,
                                              int world_z) override {
    return static_cast<std::uint16_t>(
        VS::Field::LiveFloorHeightAt(world_x, world_z));
  }

  void ShowAnimationFrame(
      const VS::Field::TriggerMarkerAnimationFrame& frame) override {
    Field_triggerMarkerTextureSource.x = frame.texture_x;
    Field_triggerMarkerPaletteSource.x = frame.palette_x;
    MoveImage(&Field_triggerMarkerTextureSource, kMarkerTextureTargetX,
              kMarkerTextureTargetY);
    MoveImage(&Field_triggerMarkerPaletteSource, 0, kMarkerPaletteTargetY);
  }

  // The definition is copied per draw so the resident record stays pristine.
  void DrawMarker(const VS::Field::TriggerMarkerPlacement& placement) override {
    RoomTriggerMarkerGeometry marker{};
    std::memcpy(&marker, Content_battleTriggerMarkerBytes, sizeof marker);
    const auto bytes = std::as_writable_bytes(std::span(&marker, 1));
    const auto group = VS::Render::ReadRoomGroup(bytes, 0);
    if (marker.group_count != 1 || !group ||
        group->polygon_offset !=
            offsetof(RoomTriggerMarkerGeometry, triangle_count) ||
        marker.triangle_count != 0 || marker.quad_count != 1) {
      return;
    }
    // G00039/G00040/G00041 were aliases inside this record, not native storage.
    VS::Render::WriteRoomGroupView(bytes, 0, VS::Render::LiveRoomCamera().view);
    marker.quad.vertex1X = placement.corner_x;
    marker.quad.vertex1Y = placement.corner_y;
    marker.quad.vertex1Z = placement.corner_z;
    const auto shade = static_cast<char>(placement.shade);
    marker.quad.red1 = marker.quad.green1 = marker.quad.blue1 = shade;
    marker.quad.red2 = marker.quad.green2 = marker.quad.blue2 = shade;
    marker.quad.red3 = marker.quad.green3 = marker.quad.blue3 = shade;
    marker.quad.red4 = marker.quad.green4 = marker.quad.blue4 = shade;
    VS::Render::DrawLiveRoomGeometry(&marker, sizeof marker,
                                     Field_roomGeometryDrawMode);
  }
};
}  // namespace

namespace VS::Field {
void DrawLiveRoomTriggerMarkers() {
  if (Field_roomData.floorTriggers == nullptr) return;
  const std::span records(Field_roomData.floorTriggers,
                          Field_roomData.header.floorTriggersLength /
                              sizeof *Field_roomData.floorTriggers);
  std::vector<FloorTriggerMarker> triggers;
  triggers.reserve(records.size());
  for (const auto& record : records) {
    triggers.push_back(FloorTriggerMarker{
        record.tileX, record.tileZ, record.kind, record.activation != 0,
        static_cast<std::uint8_t>(record.markerFade)});
  }
  const TriggerMarkerFrame frame{State_tickSpeed,
                                 Audio_Seq_unusedStateFlags.opaque20 != 0 ||
                                     Audio_Seq_unusedStateFlags.opaque40 != 0,
                                 Field_roomAnimationTicks};
  TriggerMarkerAnimation animation{Field_triggerMarkerFrameTicks};
  LiveTriggerMarkerRenderer renderer;
  DrawRoomTriggerMarkers(triggers, frame, animation, renderer);
  Field_triggerMarkerFrameTicks = animation.pending_ticks;
  for (std::size_t index = 0; index < records.size(); ++index) {
    records[index].markerFade = static_cast<char>(triggers[index].fade);
  }
}

void DrawLiveRoomTriggerMarker(int world_x, int world_z, std::int16_t height,
                               std::uint32_t brightness) {
  LiveTriggerMarkerRenderer renderer;
  DrawRoomTriggerMarker(world_x, world_z, height, brightness, renderer);
}
}  // namespace VS::Field
