// decomp: func_800D6E44 (src/BATTLE/BATTLE.PRG/6E644.s:19) live binding.
// C counterpart: rood-boi pc/src/effect_particles.c:86-368,
// revision 3be5ba38b008cad919a2b1ca196432c8d4817335 (CC0-1.0).
#include "Effects/ParticleRenderLive.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "Core/Log.hpp"
#include "Effects/CurvesLive.hpp"
#include "Effects/Package.hpp"
#include "Effects/ParticleRender.hpp"
#include "Effects/ParticleTextureAnimationLive.hpp"
#include "Effects/Particles.hpp"
#include "Effects/Runtime.h"
#include "Effects/TransformsLiveState.hpp"
#include "Legacy/Psx.h"
#include "Render/Camera.hpp"
#include "Render/Display.hpp"
#include "Render/Frame.hpp"
#include "Render/PageSpace.hpp"
#include "Render/Render.hpp"
#include "Render/RoomDrawLive.hpp"
#include "Render/UiPrimitives.hpp"
#include "nlohmann/json.hpp"

namespace VS::Effects {
namespace {
constexpr unsigned kAttached = 0x20000;
constexpr unsigned kCullBelow = 0x80000;
constexpr unsigned kFollowVelocity = 0x10000;
constexpr int kFixedShift = 12;
constexpr int kVelocityShift = 5;
constexpr unsigned kPrimitiveBytes = 0x20000;
constexpr unsigned kQuadWords = 10;
constexpr unsigned kModeWords = 3;
constexpr unsigned kLinkMask = 0xFFFFFF;
constexpr unsigned kLengthShift = 24;
constexpr unsigned kHalfShift = 16;
constexpr unsigned kHalfMask = 0xFFFF;
constexpr unsigned kSemiTransparentQuad = 0x2E000000;
constexpr unsigned kTexturedQuad = 0x2C000000;
constexpr u_long kEnableTextureWindow = 0xE1000200;
constexpr u_long kRestoreDrawMode = 0xE1000000;
constexpr std::size_t kSpatialDimensions = 3;

enum QuadWord {
  kTag,
  kColor,
  kPosition0,
  kTexture0,
  kPosition1,
  kTexture1,
  kPosition2,
  kTexture2,
  kPosition3,
  kTexture3
};

[[noreturn]] void Fail(const char* message) {
  VS::Core::Errorf("effects", "%s", message);
  std::abort();
}

void Require(bool condition, const char* message) {
  if (!condition) Fail(message);
}

std::int16_t Half(int value) {
  return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}

unsigned Packed(int x, int y) {
  return (static_cast<unsigned>(x) & kHalfMask) |
         (static_cast<unsigned>(y) << kHalfShift);
}

void Link(u_long* packet, unsigned length, u_long* ordering) {
  packet[0] = (*ordering & kLinkMask) | (length << kLengthShift);
  *ordering = (*ordering & ~kLinkMask) | LegacyGpu_Handle(packet);
}

void CheckCapacity(unsigned quads) {
  const auto base = reinterpret_cast<std::uintptr_t>(
      VS::Render::legacyPrimitiveBuffers[VS::Render::frameBuf & 1]);
  const auto cursor = reinterpret_cast<std::uintptr_t>(
      VS::Render::FrameScratchpad().primitive_cursor);
  const auto bytes = (static_cast<std::uint64_t>(quads) * kQuadWords +
                      std::uint64_t{kModeWords} * 2) *
                     sizeof(u_long);
  Require(base != 0 && cursor >= base && cursor - base <= kPrimitiveBytes &&
              bytes <= kPrimitiveBytes - (cursor - base),
          "Particle rendering exceeds the native frame packet buffer");
}

void Mode(u_long word, u_long* ordering) {
  auto* packet =
      static_cast<u_long*>(VS::Render::FrameScratchpad().primitive_cursor);
  Link(packet, 1, ordering);
  packet[1] = word;
  packet[2] = 0;
  VS::Render::FrameScratchpad().primitive_cursor = packet + kModeWords;
}

std::optional<std::uint32_t> ColorSample(unsigned curve, unsigned age) {
  if (curve == 0) return std::nullopt;
  const auto* block = Effects_package->curveBlock;
  Require(block != nullptr && curve <= block->count &&
              curve <= std::size(Effects_package->curves) &&
              block->Size(curve - 1) != 0 &&
              Effects_package->curves[curve - 1] != nullptr,
          "Particle color references an invalid effect curve");
  return static_cast<std::uint32_t>(VS::Effects::LiveSampleEffectCurve(
      static_cast<int>(curve), static_cast<int>(age)));
}

bool ProjectNode(LiveParticle& node, SVECTOR& point, Core_GteWord& xy,
                 Core_GteWord& depth, Core_GteWord& projection_flags) {
  point = {Half((node.motion.position.vx >> kFixedShift) +
                node.texture_animation.frame_offset_x),
           Half((node.motion.position.vy >> kFixedShift) +
                node.texture_animation.frame_offset_y),
           Half(node.motion.position.vz >> kFixedShift), 0};
  const MATRIX* matrix = &VS::Render::FrameScratchpad().view_matrix;
  if ((node.emit_flags & kAttached) != 0) {
    Require(Effects_currentInstance != nullptr &&
                node.transform_index <
                    std::size(Effects_currentInstance->transforms),
            "Attached particle has no valid native transform");
    const auto& transform =
        Effects_currentInstance->transforms[node.transform_index];
    if ((node.emit_flags & kCullBelow) != 0) {
      Core_Gte_SetRotMatrix(&transform.world);
      Core_Gte_SetTransMatrix(&transform.world);
      VECTOR world{};
      Core_GteWord flags;
      Core_Gte_RotTrans(&point, &world, &flags);
      if (world.vy > 0) return false;
    }
    matrix = &transform.view;
  } else if ((node.emit_flags & kCullBelow) != 0 && point.vy > 0) {
    return false;
  }
  Core_Gte_SetRotMatrix(matrix);
  Core_Gte_SetTransMatrix(matrix);
  Core_GteWord p;
  depth = Core_Gte_RotTransPers(&point, &xy, &p, &projection_flags);
  return true;
}

void TraceParticle(const LiveParticle& node, SVECTOR point, Core_GteWord xy,
                   Core_GteWord depth, Core_GteWord flags,
                   std::string_view outcome, const u_long* first_quad = nullptr,
                   unsigned quad_count = 0) {
  if (!VS::Render::GamePages().IsEffectPathTraceEnabled() ||
      !VS::Core::IsLogEnabled(VS::Core::LogLevel::Trace, "effect-path")) {
    return;
  }
  const bool attached = (node.emit_flags & kAttached) != 0;
  const auto matrix = [](const MATRIX& value) -> nlohmann::json {
    return {{"rotation",
             {{value.m[0][0], value.m[0][1], value.m[0][2]},
              {value.m[1][0], value.m[1][1], value.m[1][2]},
              {value.m[2][0], value.m[2][1], value.m[2][2]}}},
            {"translation", {value.t[0], value.t[1], value.t[2]}}};
  };
  const MATRIX* view = &VS::Render::FrameScratchpad().view_matrix;
  std::array<std::int64_t, kSpatialDimensions> world{point.vx, point.vy,
                                                     point.vz};
  nlohmann::json transform = nullptr;
  if (attached) {
    const auto& slot =
        Effects_currentInstance->transforms[node.transform_index];
    view = &slot.view;
    transform = matrix(slot.world);
    const std::array<int, kSpatialDimensions> local{point.vx, point.vy,
                                                    point.vz};
    for (std::size_t row = 0; row < world.size(); ++row) {
      std::int64_t value = 0;
      for (std::size_t axis = 0; axis < local.size(); ++axis) {
        value += std::int64_t{slot.world.m[row][axis]} * local[axis];
      }
      world[row] = (value >> kFixedShift) + slot.world.t[row];
    }
  }
  auto commands = nlohmann::json::array();
  for (unsigned index = 0; index < quad_count; ++index) {
    commands.push_back(reinterpret_cast<std::uintptr_t>(
        first_quad + std::size_t{index} * kQuadWords + 1));
  }
  const VS::Render::ScreenOffset offset =
      VS::Render::GetLiveProjectionScreenOffset();
  nlohmann::json record{
      {"kind", "particle"},
      {"pool", reinterpret_cast<std::uintptr_t>(particlePool)},
      {"slot", &node - particlePool},
      {"instance", reinterpret_cast<std::uintptr_t>(Effects_currentInstance)},
      {"effect_id",
       Effects_currentInstance
           ? nlohmann::json(Effects_currentInstance->context.effect_id)
           : nlohmann::json(nullptr)},
      {"age", node.age},
      {"lifetime", node.lifetime},
      {"emit_flags", node.emit_flags},
      {"attached", attached},
      {"transform_index", node.transform_index},
      {"position_q12",
       {node.motion.position.vx, node.motion.position.vy,
        node.motion.position.vz}},
      {"velocity_raw",
       {node.motion.velocity.vx, node.motion.velocity.vy,
        node.motion.velocity.vz}},
      {"local_point", {point.vx, point.vy, point.vz}},
      {"world_point", world},
      {"world_transform", std::move(transform)},
      {"view_transform", matrix(*view)},
      {"camera_transform", matrix(VS::Render::FrameScratchpad().view_matrix)},
      {"projection_distance", VS::Render::projectionDistance},
      {"projection_offset", {offset.x, offset.y}},
      {"screen",
       outcome == "below-ground"
           ? nlohmann::json(nullptr)
           : nlohmann::json::array({Half(xy), Half(xy >> kHalfShift)})},
      {"depth", depth},
      {"projection_flags", flags},
      {"outcome", outcome},
      {"command_addresses", std::move(commands)}};
  VS::Core::LogTrace("effect-path", "{}", record.dump());
}

std::size_t PoolSlot(const LiveParticle* node) {
  const auto base = reinterpret_cast<std::uintptr_t>(particlePool);
  const auto address = reinterpret_cast<std::uintptr_t>(node);
  Require(address >= base && (address - base) % sizeof(*node) == 0 &&
              (address - base) / sizeof(*node) <
                  static_cast<unsigned>(particleCapacity),
          "Particle render list exceeds its native pool");
  return (address - base) / sizeof(*node);
}

std::vector<std::size_t> CollectParticleSlots(const LiveParticle* head) {
  std::vector<std::size_t> slots;
  if (head == nullptr) return slots;
  Require(Effects_package != nullptr && Effects_package->textures != nullptr,
          "Particle rendering requires a loaded effect package");
  Require(particlePool != nullptr && particleCapacity > 0,
          "Particle rendering requires its native pool");
  for (const auto* node = head; node != nullptr; node = node->next) {
    Require(slots.size() < static_cast<unsigned>(particleCapacity),
            "Particle render list exceeds its native pool");
    slots.push_back(PoolSlot(node));
  }
  return slots;
}

class LiveParticleRenderer final : public ParticleRenderer {
 public:
  void AdvanceTextureAnimation(std::size_t particle) override {
    LiveAdvanceParticleTextureAnimation(&Node(particle).texture_animation);
  }

  [[nodiscard]] std::optional<ParticleProjection> Project(
      std::size_t particle) override {
    point_ = {};
    xy_ = depth_ = flags_ = 0;
    if (!ProjectNode(Node(particle), point_, xy_, depth_, flags_)) {
      return std::nullopt;
    }
    return ParticleProjection{{Half(xy_), Half(xy_ >> kHalfShift)},
                              static_cast<std::int32_t>(depth_)};
  }

  [[nodiscard]] ParticleAppearance Appearance(
      std::size_t particle) const override {
    const auto& node = Node(particle);
    return {static_cast<ParticleDepthMode>(
                node.variant != 0 ? node.variant
                                  : node.texture_animation.frame_flags),
            (static_cast<unsigned>(node.emit_flags) & kFollowVelocity) != 0,
            node.transparency, node.texture_animation.rotation};
  }

  [[nodiscard]] std::int32_t NearClip() const override {
    return VS::Render::nearClip;
  }

  [[nodiscard]] std::int32_t ProjectionDistance() const override {
    return VS::Render::projectionDistance;
  }

  [[nodiscard]] std::optional<ParticleTextureFrame> TextureFrame(
      std::size_t particle) const override {
    const auto* info = Node(particle).texture_animation.texture_info;
    if (info == nullptr) return std::nullopt;
    return ParticleTextureFrame{info->first_texture, info->texture_count};
  }

  [[nodiscard]] ParticleScreenPoint ProjectVelocityLead(
      std::size_t particle) override {
    const auto& node = Node(particle);
    SVECTOR lead{Half(point_.vx + (node.motion.velocity.vx >> kVelocityShift)),
                 Half(point_.vy + (node.motion.velocity.vy >> kVelocityShift)),
                 Half(point_.vz + (node.motion.velocity.vz >> kVelocityShift)),
                 0};
    Core_GteWord projected = 0, p = 0, flags = 0;
    Core_Gte_RotTransPers(&lead, &projected, &p, &flags);
    return {Half(projected), Half(projected >> kHalfShift)};
  }

  void SetRotation(std::size_t particle, std::int16_t rotation) override {
    Node(particle).texture_animation.rotation = rotation;
  }

  [[nodiscard]] ParticleColorSamples ColorSamples(
      std::size_t particle) override {
    const auto& node = Node(particle);
    const auto age = static_cast<u_char>(node.age + 1);
    return {ColorSample(node.color_curves[0], age),
            ColorSample(node.color_curves[1], age),
            ColorSample(node.color_curves[2], age)};
  }

  [[nodiscard]] ParticleTexture Texture(std::uint32_t index) const override {
    const LivePackageTexture& texture = Effects_package->textures[index];
    ParticleTexture typed{};
    for (std::size_t corner = 0; corner < kParticleQuadCorners; ++corner) {
      typed.corners.at(corner) = {texture.corners.at(corner).x,
                                  texture.corners.at(corner).y};
    }
    typed.u = texture.u;
    typed.v = texture.v;
    typed.w = texture.w;
    typed.h = texture.h;
    typed.clut = texture.clut;
    typed.tpage = texture.tpage;
    return typed;
  }

  void Culled(std::size_t particle, ParticleCullReason reason) override {
    TraceParticle(Node(particle), point_, xy_, depth_, flags_,
                  reason == ParticleCullReason::kBelowGround ? "below-ground"
                                                             : "depth-clip");
  }

  void BeginParticle(std::size_t particle, int bucket,
                     std::uint32_t quad_count) override {
    CheckCapacity(quad_count);
    Mode(kEnableTextureWindow, Ordering(bucket));
    TraceParticle(Node(particle), point_, xy_, depth_, flags_, "draw",
                  static_cast<const u_long*>(
                      VS::Render::FrameScratchpad().primitive_cursor),
                  quad_count);
  }

  void DrawQuad(int bucket, const ParticleQuad& quad) override {
    auto* packet =
        static_cast<u_long*>(VS::Render::FrameScratchpad().primitive_cursor);
    Link(packet, kQuadWords - 1, Ordering(bucket));
    packet[kColor] = quad.color | (quad.semi_transparent ? kSemiTransparentQuad
                                                         : kTexturedQuad);
    constexpr std::array<QuadWord, kParticleQuadCorners> kPositions{
        kPosition0, kPosition1, kPosition2, kPosition3};
    for (std::size_t corner = 0; corner < kParticleQuadCorners; ++corner) {
      packet[kPositions.at(corner)] =
          Packed(quad.vertices.at(corner).x, quad.vertices.at(corner).y);
    }
    packet[kTexture0] =
        quad.uv[0] | (static_cast<unsigned>(quad.clut) << kHalfShift);
    packet[kTexture1] =
        quad.uv[1] | (static_cast<unsigned>(quad.tpage) << kHalfShift);
    packet[kTexture2] = quad.uv[2];
    packet[kTexture3] = quad.uv.back();
    VS::Render::FrameScratchpad().primitive_cursor = packet + kQuadWords;
  }

  void EndParticle(int bucket) override {
    Mode(kRestoreDrawMode, Ordering(bucket));
  }

  void Finish(std::size_t particles, std::size_t quads) override {
    VS::Core::Debugf("particles", "rendered_nodes=%zu quads=%zu", particles,
                     quads);
  }

 private:
  static LiveParticle& Node(std::size_t particle) {
    return particlePool[particle];
  }

  static u_long* Ordering(int bucket) {
    return static_cast<u_long*>(VS::Render::FrameScratchpad().ordering_table) +
           bucket;
  }

  SVECTOR point_{};
  Core_GteWord xy_ = 0;
  Core_GteWord depth_ = 0;
  Core_GteWord flags_ = 0;
};
}  // namespace

void DrawLiveEffectParticleList(LiveParticle* head) {
  DrawLiveAnimatedSpriteParticles(head);
}

void DrawLiveAnimatedSpriteParticles(LiveParticle* head) {
  if (head == nullptr) return;
  const auto slots = CollectParticleSlots(head);
  LiveParticleRenderer renderer;
  DrawAnimatedSpriteParticles(slots, renderer);
}

}  // namespace VS::Effects
