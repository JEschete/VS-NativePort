// Hand-maintained (tools/port/owned.csv).
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <span>

#include "Core/Log.hpp"
#include "Effects/Package.hpp"
#include "Effects/Runtime.h"
#include "Effects/TextureBindings.hpp"
#include "Legacy/Psx.h"

static_assert(sizeof(Effects_EffectPackage::textureInfoTable) ==
              VS::Effects::kTextureTableCapacity *
                  sizeof(VS::Effects::LivePackageTextureFrame*));

namespace {
void Require(bool condition, const char* message) {
  if (!condition) {
    VS::Core::LogError("effects.material", "Invalid effect texture block: {}",
                       message);
    std::abort();
  }
}
}  // namespace

void VS::Effects::BindLiveEffectTextureTables(std::span<std::byte> block) {
  Require(block.data() != nullptr && Effects_package != nullptr,
          "missing package or source block");
  Require(reinterpret_cast<std::uintptr_t>(block.data()) %
                  alignof(LivePackageTexture) ==
              0,
          "unaligned texture storage");
  const EffectTextureOwner owner{
      Effects_package == &Effects_actionPackage,
      static_cast<int>(Effects_actionRequest.effectId)};
  const TextureBlockLayout layout = BindEffectTextureTables(block, owner);
  for (std::size_t table = 0; table < kTextureTableCapacity; ++table) {
    Effects_package->textureInfoTable[table] =
        table < layout.table_count
            ? reinterpret_cast<LivePackageTextureFrame*>(
                  block.data() + layout.table_offsets.at(table))
            : nullptr;
  }
  Effects_package->textures = reinterpret_cast<LivePackageTexture*>(
      block.data() + layout.descriptor_offset);
  Effects_package->block1TableCount = static_cast<int>(layout.table_count);
}
