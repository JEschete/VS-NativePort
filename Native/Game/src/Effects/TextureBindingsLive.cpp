// Hand-maintained (tools/port/owned.csv).
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <span>

#include "Core/Log.hpp"
#include "Effects/Package.hpp"
#include "Effects/Runtime.hpp"
#include "Effects/TextureBindings.hpp"
#include "Legacy/Psx.hpp"

static_assert(sizeof(VS::Effects::LiveEffectPackage::texture_tables) ==
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
  Require(block.data() != nullptr && VS::Effects::package != nullptr,
          "missing package or source block");
  Require(reinterpret_cast<std::uintptr_t>(block.data()) %
                  alignof(LivePackageTexture) ==
              0,
          "unaligned texture storage");
  const EffectTextureOwner owner{
      VS::Effects::package == &VS::Effects::actionPackage,
      static_cast<int>(VS::Effects::actionRequest.effect_id)};
  const TextureBlockLayout layout = BindEffectTextureTables(block, owner);
  for (std::size_t table = 0; table < kTextureTableCapacity; ++table) {
    VS::Effects::package->texture_tables[table] =
        table < layout.table_count
            ? reinterpret_cast<LivePackageTextureFrame*>(
                  block.data() + layout.table_offsets.at(table))
            : nullptr;
  }
  VS::Effects::package->textures = reinterpret_cast<LivePackageTexture*>(
      block.data() + layout.descriptor_offset);
  VS::Effects::package->texture_table_count =
      static_cast<int>(layout.table_count);
}
