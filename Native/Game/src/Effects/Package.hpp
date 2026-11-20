#pragma once
// The effect package's blocks as the live bindings read them in place
// (EffectResourcesLive.cpp binds them): the timeline and particle definition
// block headers, block 9's parameters, the curve block header, and the
// texture tables' frame records and descriptors (TextureBindingsLive.cpp),
// and the package record holding the bound blocks' pointers.
// The typed readers are in Effects/EffectResources.hpp, Effects/Curves.hpp
// and Effects/TextureBindings.hpp; Content/PFile.hpp reads the timeline
// sections.

#include <array>
#include <cstddef>
#include <cstdint>

#include "Effects/EffectResources.hpp"
#include "Effects/ParticleRender.hpp"
#include "Effects/Particles.hpp"
#include "Effects/TextureBindings.hpp"

namespace VS::Effects {
// decomp: PFileBlock3 (src/BATTLE/BATTLE.PRG/battleEffect.h:198): the
// timeline block's header. The section offsets follow it; Content/PFile.hpp
// reads them within the bound block.
struct LiveTimelineTable {
  std::int32_t count;
};

// decomp: PFileBlock5 (src/BATTLE/BATTLE.PRG/battleEffect.h:193): the
// particle definition block's header; the definitions follow it.
struct LiveParticleDefinitionTable {
  // The definitions the header counts. The live readers bound them by the
  // block's recorded extent (LiveParticleDefinitionCount) instead.
  std::int32_t count;

  // The definitions after the header.
  [[nodiscard]] LiveParticleDefinition* Definitions() {
    return reinterpret_cast<LiveParticleDefinition*>(this + 1);
  }
};

// decomp: PFileBlock9 (src/BATTLE/BATTLE.PRG/battleEffect.h:45): block 9,
// read typed by ReadPackageParameters (Effects/EffectResources.hpp).
struct LivePackageParameters {
  // The gravity ranges, sampled by gravity_curve.
  std::array<std::int16_t, kGravityRangeCount> gravity_range;
  // The drag range, sampled by drag_curve.
  std::array<std::int16_t, kDragRangeCount> drag_range;
  std::uint8_t reserved_10;
  std::uint8_t reserved_11;
  std::uint8_t gravity_curve;
  std::uint8_t drag_curve;
  // The tracks the primary effect plays: binding the package sets that many
  // pending-track bits.
  std::uint8_t track_count;
  // The transforms the package animates.
  std::uint8_t transform_count;
  // The timelines a root instance's register timelines map through.
  std::array<std::uint8_t, kParameterTimelineCount> timeline_ids;
};

// decomp: PFileCurveBlock (src/BATTLE/BATTLE.PRG/battleEffect.h:78): the
// curve block's header. A size byte per curve follows it; the curves follow
// each other from data_offset.
struct LiveCurveTable {
  std::uint16_t count;
  // Where the first curve starts, from the start of the block.
  std::uint16_t data_offset;

  // Curve `curve`'s length in samples, from the sizes after the header.
  [[nodiscard]] std::uint8_t Size(std::size_t curve) const {
    return reinterpret_cast<const std::uint8_t*>(this + 1)[curve];
  }
};

// decomp: PFileTextureInfo (src/BATTLE/BATTLE.PRG/battleEffect.h:84): one
// frame of a texture table, a run of the package's texture descriptors.
struct LivePackageTextureFrame {
  std::uint16_t texture_count;
  // The run's first descriptor in the package's textures.
  std::uint16_t first_texture;
};

// One corner of a texture's sprite quad, from the sprite's centre.
struct LivePackageTextureCorner {
  std::int16_t x;
  std::int16_t y;
};

// decomp: vs_PFileTexture (src/BATTLE/BATTLE.PRG/battleEffect.h:203): one
// texture descriptor: its CLUT, texture page and rectangle, and the corners
// of the quad a particle draws it on (x0, y0) to (x3, y3).
struct LivePackageTexture {
  std::uint16_t clut;
  std::uint16_t tpage;
  std::uint8_t u;
  std::uint8_t v;
  std::uint8_t w;
  std::uint8_t h;
  std::array<LivePackageTextureCorner, kParticleQuadCorners> corners;
};

struct LiveTransformDefinition;
struct LiveActionEffectRequest;

// decomp: D_800F569C_t (src/BATTLE/BATTLE.PRG/battleEffect.h:220): the
// loaded effect package. PublishBlocks (EffectResourcesLive.cpp) stores each
// bound block's pointer; a block the package lacks keeps the previous one.
struct LiveEffectPackage {
  std::int32_t reserved0;
  std::int32_t reserved4;
  LiveParticleDefinitionTable* particle_definitions;
  // Each curve's samples; curve_block holds their sizes.
  std::array<std::uint8_t*, kEffectCurveCapacity> curves;
  LiveCurveTable* curve_block;
  // The effect scripts; a spawn's script offset is from here.
  char* programs;
  // How many of texture_tables the package bound.
  std::int32_t texture_table_count;
  std::array<LivePackageTextureFrame*, kTextureTableCapacity> texture_tables;
  LivePackageTexture* textures;
  std::int32_t animation_count;
  std::uint16_t* animation_table;
  LivePackageParameters* parameters;
  LiveTransformDefinition* transform_definitions;
  LiveTimelineTable* timelines;
  std::uint8_t* sound_sequences;
  void* sound_bank;
  void* auxiliary_sound;
  // Returned when the auxiliary sound bank is bound; its sound keys play only
  // while it is set.
  std::int32_t auxiliary_sound_handle;
  // The action effect request being presented.
  LiveActionEffectRequest* request;
};

namespace layout {
// The table headers' size, after the block's {size, type} word.
inline constexpr std::size_t kTableHeaderBytes = 4;
static_assert(sizeof(LiveTimelineTable) == kTableHeaderBytes);
static_assert(sizeof(LiveParticleDefinitionTable) == kTableHeaderBytes);
static_assert(sizeof(LiveCurveTable) == kTableHeaderBytes);
static_assert(sizeof(LivePackageParameters) == kPackageParametersBytes);
static_assert(offsetof(LivePackageParameters, timeline_ids) ==
              kPackageParametersRequiredBytes);
static_assert(sizeof(LivePackageTextureFrame) == kTextureFrameRecordBytes);
static_assert(alignof(LivePackageTextureFrame) == kTextureFrameRecordAlignment);
static_assert(sizeof(LivePackageTexture) == kTextureDescriptorBytes);
}  // namespace layout
}  // namespace VS::Effects
