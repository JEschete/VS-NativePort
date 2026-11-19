// Hand-maintained (tools/port/owned.csv).
#include "Effects/BoneTrailLive.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>

#include "Actors/ModelDrawing.hpp"
#include "Actors/SkeletonLive.hpp"
#include "Core/Gte.h"
#include "Core/Log.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Core/Scratch.hpp"
#include "Effects/ActionRequest.hpp"
#include "Effects/BoneTrail.hpp"
#include "Effects/BoneTrailLiveState.hpp"
#include "Effects/EffectInstances.hpp"
#include "Effects/EffectRuntimeLive.hpp"
#include "Effects/Runtime.h"
#include "Effects/Templates/RingCore.hpp"
#include "Legacy/Psx.h"
#include "Render/Display.hpp"
#include "Render/Frame.hpp"
#include "Render/LegacyPackets.hpp"
#include "Render/Render.hpp"
#include "Render/SpritesLive.hpp"
#include "Render/UiPacketsLive.hpp"
#include "Render/UiPrimitives.hpp"

namespace VS::Effects {
// decomp: D_800EC2E8 (src/BATTLE/BATTLE.PRG/5BF94.c:398); contents: module
// image (ModuleData.cpp).
std::array<CVECTOR, kAdditiveTrailColors> trailAdditiveColors{};
// decomp: D_800EC308 (src/BATTLE/BATTLE.PRG/5BF94.c:399); contents: module
// image (ModuleData.cpp).
std::array<CVECTOR, kDistortionTrailColors> trailColors{};

namespace {
using Templates::kRingLinkMask;
using Templates::kRingPacketWords;
using Templates::RingColor;
using Templates::RingQuad;
using Templates::RingUv;

// The trail state lives in the slot's game heap block, which the original
// sized at 0xB4 bytes.
constexpr std::size_t kTrailStateBytes = 0xB4;
static_assert(sizeof(BoneTrail) <= kTrailStateBytes);
static_assert(std::is_trivially_destructible_v<BoneTrail>);
static_assert(std::is_same_v<decltype(&LiveUpdateBoneTrail), EffectRenderer>);
constexpr std::uint32_t kOrderingHighMask = ~kRingLinkMask;
constexpr unsigned kByteBits = 8;

std::uint64_t refused_calls = 0;
std::uint64_t unknown_colors = 0;
std::uint64_t dropped_quads = 0;

// True on the 1st, 2nd, 4th, 8th... occurrence, to keep repeated
// diagnostics readable.
[[nodiscard]] bool ShouldReport(std::uint64_t& count) {
  return std::has_single_bit(++count);
}

void Refuse(std::string_view call, std::string_view reason) {
  if (ShouldReport(refused_calls)) {
    VS::Core::LogWarn("effects", "bone trail: {} refused: {} ({} refusals)",
                      call, reason, refused_calls);
  }
}

[[nodiscard]] RingColor ColorOf(const CVECTOR& color) noexcept {
  return {.r = color.r, .g = color.g, .b = color.b};
}

// The module's two tables (D_800EC2E8 and D_800EC308), each at its size.
[[nodiscard]] TrailPalettes LivePalettes() noexcept {
  TrailPalettes palettes{};
  std::ranges::transform(trailAdditiveColors, palettes.additive.begin(),
                         ColorOf);
  std::ranges::transform(trailColors, palettes.distortion.begin(), ColorOf);
  return palettes;
}

[[nodiscard]] SVECTOR VectorOf(TrailPoint point) noexcept {
  return {.vx = point.x, .vy = point.y, .vz = point.z, .pad = 0};
}

class LiveTrailSegmentHost final : public TrailSegmentHost {
 public:
  // decomp: 5BF94.c:4903 (RotAverage4 of the segment's corners; the
  // interpolation and flag outputs were never read).
  TrailProjection Project(
      const std::array<TrailPoint, kTrailCorners>& corners) override {
    std::array<SVECTOR, kTrailCorners> vertices{};
    std::ranges::transform(corners, vertices.begin(), VectorOf);
    std::array<Core_GteWord, kTrailCorners> screens{};
    Core_GteWord interpolation = 0;
    Core_GteWord flags = 0;
    const Core_GteWord depth = Core_Gte_RotAverage4(
        &vertices[0], &vertices[1], &vertices[2], &vertices.back(), &screens[0],
        &screens[1], &screens[2], &screens.back(), &interpolation, &flags);
    TrailProjection projection{.depth = static_cast<std::int32_t>(depth)};
    std::ranges::transform(
        screens, projection.screens.begin(),
        [](Core_GteWord screen) { return static_cast<std::uint32_t>(screen); });
    return projection;
  }
  const TrailPalettes& Palettes() override {
    palettes_ = LivePalettes();
    return palettes_;
  }
  std::uint32_t DrawBuffer() override {
    return static_cast<std::uint32_t>(VS::Render::frameBuf) & 1U;
  }
  // decomp: 5BF94.c:4931 and 4954 (addPrim at the segment's depth). The
  // original wrote past the primitive buffer when it was full; a quad
  // without room or outside the scene table is dropped here.
  void Submit(const RingQuad& quad, std::uint8_t code,
              std::int32_t depth) override {
    u_long* const entry =
        Render::LiveFrameOrderingEntry(Render::LiveOrderingBase::kScene, depth);
    u_long* const packet =
        entry != nullptr ? Render::ReserveLivePrimitiveWords(kRingPacketWords)
                         : nullptr;
    if (packet == nullptr) {
      if (ShouldReport(dropped_quads)) {
        VS::Core::LogWarn(
            "effects",
            "bone trail: quad at depth {} has no ordering entry or "
            "buffer room; dropped ({} drops)",
            depth, dropped_quads);
      }
      return;
    }
    const std::array<std::uint32_t, kRingPacketWords> words =
        Templates::EncodeRingPacket(quad, static_cast<std::uint32_t>(*entry),
                                    code);
    std::ranges::copy(words, packet);
    *entry = (*entry & kOrderingHighMask) |
             (LegacyGpu_Handle(packet) & kRingLinkMask);
    VS::Render::FrameScratchpad().primitive_cursor = packet + kRingPacketWords;
  }

 private:
  TrailPalettes palettes_{};
};

void DrawSegment(const BoneTrail& trail, TrailSegmentWeights weights) {
  LiveTrailSegmentHost host;
  if (DrawTrailSegment(trail, weights, host) ==
          TrailSegmentDraw::kUnknownColor &&
      ShouldReport(unknown_colors)) {
    VS::Core::LogWarn(
        "effects",
        "bone trail: colour {} has no entry for texture {}; segment "
        "not drawn ({} segments)",
        trail.config.color, trail.config.texture, unknown_colors);
  }
}

[[nodiscard]] std::string_view CommandName(RendererCommand command) {
  switch (command) {
    case RendererCommand::kStart:
      return "start";
    case RendererCommand::kUpdate:
      return "update";
    case RendererCommand::kFade:
      return "fade";
    case RendererCommand::kStop:
      return "stop";
  }
  return "command";
}

class LiveBoneTrailHost final : public BoneTrailHost {
 public:
  explicit LiveBoneTrailHost(LiveRendererSlot& slot) : slot_(&slot) {}

  BoneTrail* Trail() override {
    return static_cast<BoneTrail*>(slot_->renderer_state);
  }
  BoneTrail* Allocate() override {
    void* const storage =
        Core::Memory::AllocateLiveFromHighEnd(kTrailStateBytes);
    if (storage == nullptr) {
      return nullptr;
    }
    BoneTrail* const trail =
        std::construct_at(static_cast<BoneTrail*>(storage));
    slot_->renderer_state = trail;
    return trail;
  }
  void Release() override {
    if (slot_->renderer_state != nullptr) {
      Core::Memory::FreeLiveFromHighEnd(slot_->renderer_state);
    }
    slot_->renderer_state = nullptr;
  }
  // decomp: 5BF94.c:4973-4974 (func_800A1AF8; a refused bone leaves the
  // sample as it was).
  void SampleBone(std::uint8_t actor, std::uint8_t bone,
                  Actors::BoneEndpoint endpoint, TrailPoint& point) override {
    SVECTOR position = VectorOf(point);
    static_cast<void>(Actors::LiveActorBonePosition(
        actor, bone, &position, std::to_underlying(endpoint)));
    point = {.x = position.vx, .y = position.vy, .z = position.vz};
  }
  void LoadView() override { Render::LoadLiveFrameViewMatrix(); }
  void DrawSegment(const BoneTrail& trail,
                   TrailSegmentWeights weights) override {
    VS::Effects::DrawSegment(trail, weights);
  }
  void Refuse(RendererCommand command, BoneTrailRefusal refusal) override {
    VS::Effects::Refuse(CommandName(command),
                        refusal == BoneTrailRefusal::kNoTrail
                            ? "the slot holds no trail"
                            : "no game heap room for a trail");
  }

 private:
  LiveRendererSlot* slot_;
};

class LiveBoneTrailChains final : public BoneTrailChains {
 public:
  LiveEffectInstance* FirstInstance() override {
    return Effects_secondaryInstances;
  }
  LiveEffectInstance* NextInstance(LiveEffectInstance& instance) override {
    return instance.next;
  }
  LiveRendererSlot* FirstRenderer(LiveEffectInstance& instance) override {
    return instance.context.renderer;
  }
  LiveRendererSlot* NextRenderer(LiveRendererSlot& slot) override {
    return slot.next;
  }
  bool RunsBoneTrail(LiveRendererSlot& slot) override {
    return slot.renderer == &LiveUpdateBoneTrail;
  }
  void Fade(LiveRendererSlot& slot) override {
    static_cast<void>(
        slot.renderer(&slot, std::to_underlying(RendererCommand::kFade), 0));
  }
};

// decomp: 5BF94.c:5670-5678 (the request on the stack, then func_800CF3F8).
void Begin(const BoneTrailRequest& trail) {
  LiveActionEffectRequest request{};
  request.effect_id = trail.effect_id;
  request.count = trail.target_count;
  request.other_count = trail.other_count;
  request.source.type = trail.source_type;
  request.source.position = {.vx = 0, .vy = 0, .vz = 0, .pad = 0};
  LiveActionEffectTarget& target = std::span{request.targets}.front();
  target.type = trail.target_type;
  target.position = {.vx = 0, .vy = 0, .vz = 0, .pad = 0};
  static_cast<void>(
      LiveBeginSecondaryEffects(&request, static_cast<int>(trail.parameter)));
}
}  // namespace

// decomp: BATTLE.PRG func_800CDCBC (src/BATTLE/BATTLE.PRG/5BF94.c:4898).
void LiveDrawTrailSegment(const BoneTrail* trail, int base_weight,
                          int tip_weight) {
  if (trail == nullptr) {
    Refuse("draw", "no trail");
    return;
  }
  DrawSegment(*trail, {.base = base_weight, .tip = tip_weight});
}

// decomp: BATTLE.PRG func_800CE174 (src/BATTLE/BATTLE.PRG/5BF94.c:4958).
int LiveUpdateBoneTrail(LiveRendererSlot* slot, unsigned int command,
                        int parameter) {
  if (slot == nullptr) {
    Refuse("renderer command", "no slot");
    return 0;
  }
  LiveBoneTrailHost host(*slot);
  return UpdateBoneTrail(static_cast<RendererCommand>(command),
                         static_cast<std::uint32_t>(parameter), host)
             ? 1
             : 0;
}

// decomp: BATTLE.PRG func_800CF7A8 (src/BATTLE/BATTLE.PRG/5BF94.c:5666).
void LiveSpawnBoneTrail(int actor, int bone, int color, int texture) {
  Begin(MakeBoneTrailRequest(actor, bone, color, texture));
}

// decomp: BATTLE.PRG func_800CF830 (src/BATTLE/BATTLE.PRG/5BF94.c:5681).
void LiveFadeBoneTrails() {
  LiveBoneTrailChains chains;
  FadeBoneTrails(chains);
}

// decomp: BATTLE.PRG func_800CF92C (src/BATTLE/BATTLE.PRG/5BF94.c:5714).
void LiveClampTrailTexturePoint(int u_offset, int v_offset, int screen,
                                std::uint16_t* uv) {
  if (uv == nullptr) {
    Refuse("texel clamp", "no texture coordinate");
    return;
  }
  const RingUv texel = ClampTrailTexturePoint(
      u_offset, v_offset, static_cast<std::uint32_t>(screen));
  *uv = static_cast<std::uint16_t>(texel.u | (texel.v << kByteBits));
}

// decomp: BATTLE.PRG func_800CF988 (src/BATTLE/BATTLE.PRG/5BF94.c:5738).
void LiveMapTrailFramebufferTexture(VS::Render::TrailPacket* packet,
                                    int u_offset, int v_offset, int flags) {
  if (packet == nullptr) {
    Refuse("framebuffer map", "no packet");
    return;
  }
  RingQuad quad{.screens = {static_cast<std::uint32_t>(packet->xy0),
                            static_cast<std::uint32_t>(packet->xy1),
                            static_cast<std::uint32_t>(packet->xy2),
                            static_cast<std::uint32_t>(packet->xy3)}};
  MapTrailFramebufferTexture(
      quad, u_offset, v_offset, static_cast<std::uint16_t>(flags),
      static_cast<std::uint32_t>(VS::Render::frameBuf) & 1U);
  const auto packed = [](RingUv uv) {
    return static_cast<u_short>(uv.u | (uv.v << kByteBits));
  };
  packet->texture_page = quad.tpage;
  packet->uv0 = packed(quad.uvs[0]);
  packet->uv1 = packed(quad.uvs[1]);
  packet->uv2 = packed(quad.uvs[2]);
  packet->uv3 = packed(quad.uvs.back());
}

// decomp: BATTLE.PRG func_8006C480 (src/BATTLE/BATTLE.PRG/146C.c:1923).
void LiveStartWeaponTrail(int actor, int bone) {
  Begin(MakeWeaponTrailRequest(actor, bone));
}

// decomp: BATTLE.PRG func_8006C4A4 (src/BATTLE/BATTLE.PRG/146C.c:1925).
void LiveFadeWeaponTrails() {
  LiveBoneTrailChains chains;
  FadeWeaponTrails(chains);
}
}  // namespace VS::Effects
