// Hand-maintained (tools/port/owned.csv).
// decomp: func_800D6E44 (src/BATTLE/BATTLE.PRG/6E644.s:19).
// C counterpart: rood-boi pc/src/effect_particles.c:86-368,
// revision 3be5ba38b008cad919a2b1ca196432c8d4817335 (CC0-1.0).
#include "Effects/ParticleRender.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <limits>

#include "Legacy/Psx.h"

extern "C" {
#include "Effects/Curves.h"
#include "Effects/Package.h"
#include "Effects/Particles.h"
#include "Effects/Runtime.h"
#include "Effects/Transforms.h"
#include "Render/Camera.h"
#include "Render/Display.h"
#include "Render/Frame.h"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"
}

namespace {
constexpr unsigned kAttached = 0x20000;
constexpr unsigned kCullBelow = 0x80000;
constexpr unsigned kFollowVelocity = 0x10000;
constexpr int kFixedShift = 12;
constexpr int kProjectionShift = 10;
constexpr int kVelocityShift = 5;
constexpr unsigned kPrimitiveBytes = 0x20000;
constexpr unsigned kQuadWords = 10;
constexpr unsigned kModeWords = 3;
constexpr unsigned kLinkMask = 0xFFFFFF;
constexpr unsigned kLengthShift = 24;
constexpr unsigned kHalfShift = 16;
constexpr unsigned kHalfMask = 0xFFFF;
constexpr unsigned kByteShift = 8;
constexpr int kScaleDivisor = 4;
constexpr int kDepthStep = 8;
constexpr int kDepthLargeStep = 16;
constexpr int kOverlayBack = -15;
constexpr int kOverlayMiddle = -13;
constexpr int kOverlayFront = -11;
constexpr unsigned kSemiTransparentClut = 0x8000;
constexpr unsigned kTexturedQuad = 0x2C000000;
constexpr unsigned kSemiTransparentQuad = 0x2E000000;
constexpr u_long kEnableTextureWindow = 0xE1000200;
constexpr u_long kRestoreDrawMode = 0xE1000000;
constexpr std::size_t kQuadVertices = 4;
constexpr int kMaximumDepth = 2048;
constexpr int kForegroundSlots = 16;

enum class DepthMode : unsigned {
  kActual,
  kFar,
  kFarther,
  kNear,
  kNearer,
  kBackground,
  kOverlayBack,
  kOverlayMiddle,
  kOverlayFront
};
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
  Core_Debugf("effects", "%s", message);
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
      Render_legacyPrimitiveBuffers[Render_frameBuf & 1]);
  const auto cursor =
      reinterpret_cast<std::uintptr_t>(Render_frameScratch.primitiveCursor);
  const auto bytes = (static_cast<std::uint64_t>(quads) * kQuadWords +
                      std::uint64_t{kModeWords} * 2) *
                     sizeof(u_long);
  Require(base != 0 && cursor >= base && cursor - base <= kPrimitiveBytes &&
              bytes <= kPrimitiveBytes - (cursor - base),
          "Particle rendering exceeds the native frame packet buffer");
}

void Mode(u_long word, u_long* ordering) {
  auto* packet = static_cast<u_long*>(Render_frameScratch.primitiveCursor);
  Link(packet, 1, ordering);
  packet[1] = word;
  packet[2] = 0;
  Render_frameScratch.primitiveCursor = packet + kModeWords;
}

unsigned ColorChannel(unsigned curve, unsigned age) {
  constexpr unsigned kDefault = 128;
  if (curve == 0) return kDefault;
  const auto* block = Effects_package->curveBlock;
  Require(block != nullptr && curve <= block->count &&
              curve <= std::size(Effects_package->curves) &&
              block->curveSizes[curve - 1] != 0 &&
              Effects_package->curves[curve - 1] != nullptr,
          "Particle color references an invalid effect curve");
  const auto sample = static_cast<unsigned>(Effects_SampleEffectCurve(
      static_cast<int>(curve), static_cast<int>(age)));
  return sample == 0 ? 0 : sample * 2 - 1;
}

unsigned Color(const Effects_Particle& node) {
  const auto age = static_cast<u_char>(node.age + 1);
  return ColorChannel(node.colorCurves[0], age) |
         (ColorChannel(node.colorCurves[1], age) << kByteShift) |
         (ColorChannel(node.colorCurves[2], age) << kHalfShift);
}

bool Project(Effects_Particle& node, SVECTOR& point, Core_GteWord& xy,
             Core_GteWord& depth) {
  point = {Half((node.motion.position.vx >> kFixedShift) +
                node.textureAnimation.frameOffsetX),
           Half((node.motion.position.vy >> kFixedShift) +
                node.textureAnimation.frameOffsetY),
           Half(node.motion.position.vz >> kFixedShift), 0};
  const MATRIX* matrix = &Render_frameScratch.viewMatrix;
  if ((node.emitFlags & kAttached) != 0) {
    Require(Effects_currentInstance != nullptr &&
                node.transformIndex <
                    std::size(Effects_currentInstance->transforms),
            "Attached particle has no valid native transform");
    const auto& transform =
        Effects_currentInstance->transforms[node.transformIndex];
    if ((node.emitFlags & kCullBelow) != 0) {
      Core_Gte_SetRotMatrix(&transform.world);
      Core_Gte_SetTransMatrix(&transform.world);
      VECTOR world{};
      Core_GteWord flags;
      Core_Gte_RotTrans(&point, &world, &flags);
      if (world.vy > 0) return false;
    }
    matrix = &transform.view;
  } else if ((node.emitFlags & kCullBelow) != 0 && point.vy > 0) {
    return false;
  }
  Core_Gte_SetRotMatrix(matrix);
  Core_Gte_SetTransMatrix(matrix);
  Core_GteWord p, flags;
  depth = Core_Gte_RotTransPers(&point, &xy, &p, &flags);
  return true;
}

int Bucket(const Effects_Particle& node, Core_GteWord projected_depth) {
  const auto mode = static_cast<DepthMode>(
      node.opaque61Bit0 != 0 ? node.opaque61Bit0
                             : node.textureAnimation.reserved7);
  int depth = static_cast<int>(projected_depth);
  switch (mode) {
    case DepthMode::kFar:
      depth += kDepthStep;
      break;
    case DepthMode::kFarther:
      depth += kDepthLargeStep;
      break;
    case DepthMode::kNear:
      depth -= kDepthStep;
      break;
    case DepthMode::kNearer:
      depth -= kDepthLargeStep;
      break;
    default:
      break;
  }
  if (projected_depth <= 0 || depth >= kMaximumDepth ||
      depth <= Render_nearClip) {
    return kMaximumDepth;
  }
  switch (mode) {
    case DepthMode::kBackground:
      return kMaximumDepth - 2;
    case DepthMode::kOverlayBack:
      return kOverlayBack;
    case DepthMode::kOverlayMiddle:
      return kOverlayMiddle;
    case DepthMode::kOverlayFront:
      return kOverlayFront;
    default:
      return depth;
  }
}

void UpdateRotation(Effects_Particle& node, SVECTOR point,
                    Core_GteWord centre) {
  if ((node.emitFlags & kFollowVelocity) == 0) return;
  point.vx = Half(point.vx + (node.motion.velocity.vx >> kVelocityShift));
  point.vy = Half(point.vy + (node.motion.velocity.vy >> kVelocityShift));
  point.vz = Half(point.vz + (node.motion.velocity.vz >> kVelocityShift));
  Core_GteWord projected, p, flags;
  Core_Gte_RotTransPers(&point, &projected, &p, &flags);
  const int dx = Half(centre) - Half(projected);
  const int dy = Half(centre >> kHalfShift) - Half(projected >> kHalfShift);
  node.textureAnimation.reserved14 =
      Half(dx == 0 && dy == 0 ? 0 : Core_Gte_Atan2(dy, dx));
}

std::int16_t Scale(int projection, int trigonometric, unsigned size) {
  const auto factor = trigonometric * static_cast<int>(size) / kScaleDivisor;
  const auto product = static_cast<std::uint32_t>(projection) *
                       static_cast<std::uint32_t>(factor);
  return Half(std::bit_cast<std::int32_t>(product) >> kFixedShift);
}

void Quad(const Effects_PackageTexture& texture, Core_GteWord centre,
          std::int16_t cosine, std::int16_t sine, unsigned color,
          u_long* ordering) {
  auto* packet = static_cast<u_long*>(Render_frameScratch.primitiveCursor);
  Link(packet, kQuadWords - 1, ordering);
  const auto clut = static_cast<unsigned>(static_cast<u_short>(texture.clut));
  packet[kColor] =
      color | ((clut & kSemiTransparentClut) != 0 ? kSemiTransparentQuad
                                                  : kTexturedQuad);
  std::array<std::int16_t, kQuadVertices * 2> vertices;
  std::memcpy(vertices.data(), &texture.reserved8, sizeof(vertices));
  for (std::size_t vertex = 0; vertex < kQuadVertices; ++vertex) {
    const auto x = vertices[vertex * 2], y = vertices[vertex * 2 + 1];
    const auto dx =
        (std::int64_t{cosine} * x - std::int64_t{sine} * y) >> kFixedShift;
    const auto dy =
        (std::int64_t{sine} * x + std::int64_t{cosine} * y) >> kFixedShift;
    const auto screen_x =
        std::clamp<std::int64_t>(Half(centre) + dx, INT16_MIN, INT16_MAX);
    const auto screen_y = std::clamp<std::int64_t>(
        Half(centre >> kHalfShift) + dy, INT16_MIN, INT16_MAX);
    const auto offset = static_cast<std::size_t>(vertex) * 2 + 2;
    packet[offset] =
        Packed(static_cast<int>(screen_x), static_cast<int>(screen_y));
  }
  const unsigned uv =
      texture.u | (static_cast<unsigned>(texture.v) << kByteShift);
  packet[kTexture0] = uv | (clut << kHalfShift);
  packet[kTexture1] =
      (uv + texture.w) |
      (static_cast<unsigned>(static_cast<u_short>(texture.tpage))
       << kHalfShift);
  packet[kTexture2] = static_cast<u_short>(
      uv + (static_cast<unsigned>(texture.h) << kByteShift));
  packet[kTexture3] = static_cast<u_short>(
      uv + texture.w + (static_cast<unsigned>(texture.h) << kByteShift));
  Render_frameScratch.primitiveCursor = packet + kQuadWords;
}
}  // namespace

// decomp: func_800D6E24 (src/BATTLE/BATTLE.PRG/5BF94.c:8554).
void Effects_DrawEffectParticleList(Effects_Particle* head) {
  Effects_DrawAnimatedSpriteParticles(head);
}

void Effects_DrawAnimatedSpriteParticles(Effects_Particle* head) {
  if (head == nullptr) return;
  Require(Effects_package != nullptr && Effects_package->textures != nullptr,
          "Particle rendering requires a loaded effect package");
  Require(Effects_particlePool != nullptr && Effects_particleCapacity > 0,
          "Particle rendering requires its native pool");
  unsigned visited = 0;
  unsigned quads = 0;
  for (auto* node = head; node != nullptr; node = node->next) {
    const auto base = reinterpret_cast<std::uintptr_t>(Effects_particlePool);
    const auto address = reinterpret_cast<std::uintptr_t>(node);
    Require(address >= base && (address - base) % sizeof(*node) == 0 &&
                (address - base) / sizeof(*node) <
                    static_cast<unsigned>(Effects_particleCapacity) &&
                visited < static_cast<unsigned>(Effects_particleCapacity),
            "Particle render list exceeds its native pool");
    ++visited;
    Effects_AdvanceParticleTextureAnimation(&node->textureAnimation);
    SVECTOR point{};
    Core_GteWord xy, depth;
    if (!Project(*node, point, xy, depth)) continue;
    const int bucket = Bucket(*node, depth);
    if (bucket >= kMaximumDepth) continue;
    Require(bucket >= -kForegroundSlots &&
                node->textureAnimation.textureInfo != nullptr,
            "Particle rendering has no texture frame or ordering bucket");
    UpdateRotation(*node, point, xy);
    const auto& info = *node->textureAnimation.textureInfo;
    CheckCapacity(info.reserved0);
    auto* ordering =
        static_cast<u_long*>(Render_frameScratch.orderingTable) + bucket;
    Mode(kEnableTextureWindow, ordering);
    const auto projection =
        static_cast<std::int64_t>(Render_projectionDistance) *
        (1 << kProjectionShift) / depth;
    const auto cosine = Scale(static_cast<int>(projection),
                              Core_Gte_Cos(node->textureAnimation.reserved14),
                              node->transparency);
    const auto sine = Scale(static_cast<int>(projection),
                            Core_Gte_Sin(node->textureAnimation.reserved14),
                            node->transparency);
    const auto color = Color(*node);
    for (unsigned index = 0; index < info.reserved0; ++index) {
      Quad(Effects_package->textures[info.dataIndex + index], xy, cosine, sine,
           color, ordering);
      ++quads;
    }
    Mode(kRestoreDrawMode, ordering);
  }
  Core_Debugf("particles", "rendered_nodes=%u quads=%u", visited, quads);
}
