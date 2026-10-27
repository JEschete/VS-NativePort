#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <span>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Content/ModuleData.h"
#include "Effects/TextureBindings.h"
#include "Legacy/Psx.h"
#include "Platform/App.hpp"
#include "Platform/CrashHandler.hpp"

extern "C" {
#include "Effects/EffectResources.h"
#include "Effects/Package.h"
#include "Effects/Particles.h"
#include "Effects/Runtime.h"
#include "Effects/Transforms.h"
#include "Render/Frame.h"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"
}

namespace {
struct TextureBindingFixture {
  Effects_EffectPackage saved_primary = Effects_actionPackage;
  Effects_EffectPackage* saved_package = Effects_package;
  decltype(Effects_actionRequest.effectId) saved_effect =
      Effects_actionRequest.effectId;
  std::array<std::uint32_t, 16> bytes{};

  TextureBindingFixture() {
    Effects_actionPackage = {};
    Effects_package = &Effects_actionPackage;
    Effects_actionRequest.effectId = 84;
    bytes[0] = 0x00100001;
    bytes[1] = 8;
    bytes[2] = 1;
    bytes[3] = 0x00010001;
    auto* textures =
        reinterpret_cast<Effects_PackageTexture*>(bytes.data() + 4);
    textures[0].u = 16;
    textures[0].v = 32;
    textures[0].w = textures[0].h = 8;
    textures[0].clut = 0x1234;
    textures[0].tpage = 0x567;
    textures[1] = textures[0];
    textures[1].u = 48;
  }

  ~TextureBindingFixture() {
    Effects_actionPackage = saved_primary;
    Effects_actionRequest.effectId = saved_effect;
    Effects_package = saved_package;
  }

  void Bind() { Effects_BindEffectTextureTables(bytes.data(), sizeof bytes); }

  static void CheckFirstFrame(const Effects_ParticleTextureAnimation& state) {
    REQUIRE(state.textureInfo != nullptr);
    CHECK(state.textureInfo == Effects_actionPackage.textureInfoTable[0] + 1);
    CHECK(state.textureInfo->dataIndex == 1);
    CHECK(state.frameOffsetX == 3);
    CHECK(state.frameOffsetY == 2);
    CHECK(state.frameTimer == 4);
    CHECK(state.cursor == 8);
  }
};

void CheckGravityBinding() {
  CHECK(Effects_actionPackage.block1TableCount == 2);
  CHECK(Effects_actionPackage.textureInfoTable[0] ==
        Effects_actionPackage.textureInfoTable[1]);
}

void CheckSourceTextureBlocks(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  REQUIRE(input.good());
  std::vector<char> bytes{std::istreambuf_iterator<char>(input), {}};
  const auto original = bytes;
  for (std::size_t offset = 0; offset + 4 <= bytes.size();) {
    std::array<std::uint16_t, 2> header{};
    std::memcpy(header.data(), bytes.data() + offset, sizeof header);
    if (header[1] == 0) break;
    REQUIRE(header[0] >= 4);
    REQUIRE(header[0] <= bytes.size() - offset);
    if (header[1] == 1) {
      Effects_actionRequest.effectId = path.filename() == "E084.P" ? 84 : 53;
      Effects_BindEffectTextureTables(bytes.data() + offset + 4, header[0] - 4);
      if (path.filename() == "E084.P") {
        CheckGravityBinding();
      }
    }
    offset += header[0];
  }
  CHECK(bytes == original);
}

std::filesystem::path SourceEffectDirectory() {
  for (auto directory = VS::Platform::ExecutableDir();
       directory != directory.parent_path();
       directory = directory.parent_path()) {
    const auto candidate = directory / "Native" / "data" / "disc" / "EFFECT";
    if (std::filesystem::is_directory(candidate)) return candidate;
  }
  return {};
}
}  // namespace

TEST_CASE("Gravity material channels resolve only to its own authored table",
          "[effects][texture-binding][gravity-material][binding-red]") {
  TextureBindingFixture fixture;
  const auto original = fixture.bytes;
  fixture.Bind();
  CHECK(Effects_actionPackage.block1TableCount == 2);
  CHECK(
      Effects_actionPackage.textureInfoTable[0] ==
      reinterpret_cast<Effects_PackageTextureInfo*>(fixture.bytes.data() + 2));
  CHECK(Effects_actionPackage.textureInfoTable[1] ==
        Effects_actionPackage.textureInfoTable[0]);
  CHECK(fixture.bytes == original);
}

TEST_CASE(
    "Effect material reentry clears bindings not owned by the new package",
    "[effects][texture-binding][binding-red]") {
  TextureBindingFixture fixture;
  Effects_actionRequest.effectId = 53;
  Effects_actionPackage.textureInfoTable[1] =
      reinterpret_cast<Effects_PackageTextureInfo*>(fixture.bytes.data() + 3);
  fixture.Bind();
  CHECK(Effects_actionPackage.block1TableCount == 1);
  CHECK(Effects_actionPackage.textureInfoTable[1] == nullptr);
  CHECK(Effects_actionPackage.textureInfoTable[2] == nullptr);
  CHECK(Effects_actionPackage.textureInfoTable[3] == nullptr);
}

TEST_CASE("Gravity animation keeps authored holds offsets and ended frames",
          "[effects][texture-binding][gravity-material]") {
  TextureBindingFixture fixture;
  fixture.Bind();
  std::array<std::uint32_t, 5> program{0x10008003, 0x00040001, 0x30000000, 0,
                                       0};
  Effects_ParticleTextureAnimation animation{};
  animation.program = reinterpret_cast<u_char*>(program.data());
  animation.textureIndex = 1;
  Effects_AdvanceParticleTextureAnimation(&animation);
  TextureBindingFixture::CheckFirstFrame(animation);
  const auto* frame = animation.textureInfo;
  Effects_AdvanceParticleTextureAnimation(&animation);
  CHECK(animation.textureInfo == frame);
  CHECK(animation.frameTimer == 2);
  Effects_AdvanceParticleTextureAnimation(&animation);
  CHECK(animation.textureInfo == frame);
  CHECK(animation.frameTimer == 0);
  CHECK(animation.cursor == 8);
}

TEST_CASE("Material channels preserve distinct authored multi-table bindings",
          "[effects][texture-binding]") {
  TextureBindingFixture fixture;
  std::array<std::uint32_t, 19> bytes{};
  bytes[0] = 0x001C0003;
  bytes[1] = 16;
  bytes[2] = 20;
  bytes[3] = 24;
  bytes[4] = bytes[6] = 1;
  bytes[5] = 0x00010001;
  Effects_BindEffectTextureTables(bytes.data(), sizeof bytes);
  CHECK(Effects_actionPackage.block1TableCount == 3);
  CHECK(Effects_actionPackage.textureInfoTable[0] !=
        Effects_actionPackage.textureInfoTable[1]);
  CHECK(Effects_actionPackage.textureInfoTable[1]->dataIndex == 1);
  CHECK(Effects_actionPackage.textureInfoTable[2]->dataIndex == 0);
  CHECK(Effects_actionPackage.textureInfoTable[3] == nullptr);
}

TEST_CASE("Gravity channel normalization never leaks into secondary resources",
          "[effects][texture-binding]") {
  TextureBindingFixture fixture;
  Effects_EffectPackage secondary{};
  Effects_package = &secondary;
  fixture.Bind();
  CHECK(secondary.block1TableCount == 1);
  CHECK(secondary.textureInfoTable[1] == nullptr);
  CHECK(Effects_actionPackage.block1TableCount == 0);
}

TEST_CASE("Ended and delayed texture programs preserve explicit frame state",
          "[effects][texture-binding]") {
  TextureBindingFixture fixture;
  fixture.Bind();
  std::array<std::uint32_t, 1> program{0x30000000};
  Effects_ParticleTextureAnimation animation{};
  animation.program = reinterpret_cast<u_char*>(program.data());
  animation.textureIndex = 1;
  Effects_AdvanceParticleTextureAnimation(&animation);
  CHECK(animation.textureInfo == nullptr);
  CHECK(animation.cursor == 0);
  animation.textureInfo = Effects_actionPackage.textureInfoTable[1];
  animation.frameTimer = 4;
  Effects_AdvanceParticleTextureAnimation(&animation);
  CHECK(animation.textureInfo == Effects_actionPackage.textureInfoTable[0]);
  CHECK(animation.frameTimer == 2);
  CHECK(animation.cursor == 0);
}

TEST_CASE("Every imported effect texture block validates without changing data",
          "[effects][texture-binding][disc]") {
  const auto root = SourceEffectDirectory();
  if (root.empty()) {
    SKIP("Extracted effect packages are unavailable");
  }
  TextureBindingFixture fixture;
  std::size_t count = 0;
  for (const auto& entry : std::filesystem::directory_iterator(root)) {
    if (entry.path().extension() == ".P") {
      INFO(entry.path().filename().string());
      CheckSourceTextureBlocks(entry.path());
      ++count;
    }
  }
  CHECK(count == 290);
}

TEST_CASE("Truncated effect texture blocks are diagnosed before publication",
          "[.texture-block-short]") {
  VS::Platform::InstallCrashHandler();
  TextureBindingFixture fixture;
  Effects_BindEffectTextureTables(fixture.bytes.data(),
                                  sizeof fixture.bytes - 1);
  FAIL("Truncated texture descriptor was accepted");
}

TEST_CASE("Effect texture tables cannot point outside their block",
          "[.texture-block-offset]") {
  VS::Platform::InstallCrashHandler();
  TextureBindingFixture fixture;
  fixture.bytes[1] = 0xFFFFFFFF;
  fixture.Bind();
  FAIL("Unbounded texture table offset was accepted");
}

TEST_CASE(
    "Unavailable effect texture frames are diagnosed rather than defaulted",
    "[.texture-block-frame]") {
  VS::Platform::InstallCrashHandler();
  TextureBindingFixture fixture;
  fixture.bytes[2] = 0xFFFF0001;
  fixture.Bind();
  FAIL("An unavailable texture descriptor was replaced or accepted");
}

TEST_CASE("Effect origin is the canonical primary request focus position",
          "[effects][gravity-placement][placement-red]") {
  const auto saved = Effects_actionRequest;
  Effects_actionRequest.position = {1200, -384, 900, 0x1234};
  CHECK(Effects_effectOrigin == &Effects_actionRequest.position);
  CHECK(Effects_effectOrigin[0].vx == 1200);
  CHECK(Effects_effectOrigin[0].vy == -384);
  CHECK(Effects_effectOrigin[0].vz == 900);
  const std::span symbols(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x800F5310) == nullptr);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x800F5230) ==
        &Effects_actionRequest);
  Effects_actionRequest = saved;
}

TEST_CASE(
    "Authored Gravity origin attachment projects the actual request target",
    "[effects][gravity-placement][placement-red]") {
  TextureBindingFixture fixture;
  const auto saved_request = Effects_actionRequest;
  const auto saved_view = Render_frameScratch.viewMatrix;
  Effects_PackageBlock9 parameters{};
  parameters.transformCount = 1;
  Effects_TransformDefinition definition{};
  definition.flags = 7;
  definition.attachActor = 0x12;
  for (auto& value : definition.scaleRange) value = 4096;
  Effects_actionPackage.block9Data = &parameters;
  Effects_actionPackage.block10Data = &definition;
  Effects_EffectInstance instance{};
  Effects_actionRequest.position = {1200, -384, 900, 0};
  Render_frameScratch.viewMatrix = {};
  for (int axis = 0; axis < 3; ++axis) {
    Render_frameScratch.viewMatrix.m[axis][axis] = 4096;
  }
  Render_frameScratch.viewMatrix.t[0] = -100;
  Render_frameScratch.viewMatrix.t[1] = 200;
  Render_frameScratch.viewMatrix.t[2] = 600;
  Effects_InitializeTransformStates(&instance);
  Effects_UpdateEffectTransforms(&instance);
  CHECK(instance.transforms[0].world.t[0] == 1200);
  CHECK(instance.transforms[0].world.t[1] == -384);
  CHECK(instance.transforms[0].world.t[2] == 900);
  CHECK(instance.transforms[0].view.t[0] == 1100);
  CHECK(instance.transforms[0].view.t[1] == -184);
  CHECK(instance.transforms[0].view.t[2] == 1500);
  Effects_actionRequest = saved_request;
  Render_frameScratch.viewMatrix = saved_view;
}
