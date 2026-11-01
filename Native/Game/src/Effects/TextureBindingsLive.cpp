// Hand-maintained (tools/port/owned.csv).
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <span>

#include "Core/Log.hpp"
#include "Effects/TextureBindings.hpp"
#include "Legacy/Psx.h"

extern "C" {
#include "Effects/Package.h"
#include "Effects/Runtime.h"
}

static_assert(sizeof(Effects_PackageTextureInfo) ==
              VS::Effects::kTextureFrameRecordBytes);
static_assert(alignof(Effects_PackageTextureInfo) ==
              VS::Effects::kTextureFrameRecordAlignment);
static_assert(sizeof(Effects_PackageTexture) ==
              VS::Effects::kTextureDescriptorBytes);
static_assert(sizeof(Effects_EffectPackage::textureInfoTable) ==
              VS::Effects::kTextureTableCapacity *
                  sizeof(Effects_PackageTextureInfo*));

namespace {
void Require(bool condition, const char* message) {
  if (!condition) {
    VS_LOG_ERROR("effects.material", "Invalid effect texture block: {}",
                 message);
    std::abort();
  }
}
}  // namespace

void VS::Effects::BindLiveEffectTextureTables(std::span<std::byte> block) {
  Require(block.data() != nullptr && Effects_package != nullptr,
          "missing package or source block");
  Require(reinterpret_cast<std::uintptr_t>(block.data()) %
                  alignof(Effects_PackageTexture) ==
              0,
          "unaligned texture storage");
  const EffectTextureOwner owner{
      Effects_package == &Effects_actionPackage,
      static_cast<int>(Effects_actionRequest.effectId)};
  const TextureBlockLayout layout = BindEffectTextureTables(block, owner);
  for (std::size_t table = 0; table < kTextureTableCapacity; ++table) {
    Effects_package->textureInfoTable[table] =
        table < layout.table_count
            ? reinterpret_cast<Effects_PackageTextureInfo*>(
                  block.data() + layout.table_offsets.at(table))
            : nullptr;
  }
  Effects_package->textures = reinterpret_cast<Effects_PackageTexture*>(
      block.data() + layout.descriptor_offset);
  Effects_package->block1TableCount = static_cast<int>(layout.table_count);
}
