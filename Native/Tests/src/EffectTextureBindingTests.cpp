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

#include "Content/ModuleData.hpp"
#include "Effects/ActionRequest.hpp"
#include "Effects/EffectResourcesLive.hpp"
#include "Effects/Package.hpp"
#include "Effects/PackageExtents.hpp"
#include "Effects/ParticleTextureAnimationLive.hpp"
#include "Effects/Particles.hpp"
#include "Effects/Runtime.h"
#include "Effects/TextureBindings.hpp"
#include "Effects/TransformsLive.hpp"
#include "Effects/TransformsLiveState.hpp"
#include "Legacy/Psx.h"
#include "Platform/App.hpp"
#include "Platform/CrashHandler.hpp"
#include "Render/Frame.hpp"
#include "Render/Render.hpp"
#include "Render/UiPrimitives.hpp"

namespace {
struct TextureBindingFixture {
  VS::Effects::LiveEffectPackage saved_primary = Effects_actionPackage;
  VS::Effects::LiveEffectPackage* saved_package = Effects_package;
  decltype(Effects_actionRequest.effect_id) saved_effect =
      Effects_actionRequest.effect_id;
  std::array<std::uint32_t, 16> bytes{};

  TextureBindingFixture() {
    Effects_actionPackage = {};
    Effects_package = &Effects_actionPackage;
    Effects_actionRequest.effect_id = 84;
    bytes[0] = 0x00100001;
    bytes[1] = 8;
    bytes[2] = 1;
    bytes[3] = 0x00010001;
    auto* textures =
        reinterpret_cast<VS::Effects::LivePackageTexture*>(bytes.data() + 4);
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
    Effects_actionRequest.effect_id = saved_effect;
    Effects_package = saved_package;
  }

  void Bind() {
    VS::Effects::BindLiveEffectTextureTables(
        std::as_writable_bytes(std::span(bytes)));
  }

  static void CheckFirstFrame(
      const VS::Effects::LiveParticleTextureAnimation& state) {
    REQUIRE(state.texture_info != nullptr);
    CHECK(state.texture_info == Effects_actionPackage.texture_tables[0] + 1);
    CHECK(state.texture_info->first_texture == 1);
    CHECK(state.frame_offset_x == 3);
    CHECK(state.frame_offset_y == 2);
    CHECK(state.frame_timer == 4);
    CHECK(state.cursor == 8);
  }
};

// A test program, recorded as an animation table while in scope.
class RecordedProgram {
 public:
  explicit RecordedProgram(std::span<std::uint32_t> program)
      : program_(program.data()) {
    VS::Effects::RecordLiveAnimationTable(std::as_writable_bytes(program));
  }
  RecordedProgram(const RecordedProgram&) = delete;
  RecordedProgram& operator=(const RecordedProgram&) = delete;
  RecordedProgram(RecordedProgram&&) = delete;
  RecordedProgram& operator=(RecordedProgram&&) = delete;
  ~RecordedProgram() { VS::Effects::ForgetLiveAnimationTable(program_); }

 private:
  const void* program_;
};

void CheckGravityBinding() {
  CHECK(Effects_actionPackage.texture_table_count == 2);
  CHECK(Effects_actionPackage.texture_tables[0] ==
        Effects_actionPackage.texture_tables[1]);
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
      Effects_actionRequest.effect_id = path.filename() == "E084.P" ? 84 : 53;
      VS::Effects::BindLiveEffectTextureTables(std::as_writable_bytes(
          std::span(bytes).subspan(offset + 4, header[0] - 4)));
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
  CHECK(Effects_actionPackage.texture_table_count == 2);
  CHECK(Effects_actionPackage.texture_tables[0] ==
        reinterpret_cast<VS::Effects::LivePackageTextureFrame*>(
            fixture.bytes.data() + 2));
  CHECK(Effects_actionPackage.texture_tables[1] ==
        Effects_actionPackage.texture_tables[0]);
  CHECK(fixture.bytes == original);
}

TEST_CASE(
    "Effect material reentry clears bindings not owned by the new package",
    "[effects][texture-binding][binding-red]") {
  TextureBindingFixture fixture;
  Effects_actionRequest.effect_id = 53;
  Effects_actionPackage.texture_tables[1] =
      reinterpret_cast<VS::Effects::LivePackageTextureFrame*>(
          fixture.bytes.data() + 3);
  fixture.Bind();
  CHECK(Effects_actionPackage.texture_table_count == 1);
  CHECK(Effects_actionPackage.texture_tables[1] == nullptr);
  CHECK(Effects_actionPackage.texture_tables[2] == nullptr);
  CHECK(Effects_actionPackage.texture_tables[3] == nullptr);
}

TEST_CASE("Gravity animation keeps authored holds offsets and ended frames",
          "[effects][texture-binding][gravity-material]") {
  TextureBindingFixture fixture;
  fixture.Bind();
  std::array<std::uint32_t, 5> program{0x10008003, 0x00040001, 0x30000000, 0,
                                       0};
  const RecordedProgram recorded{program};
  VS::Effects::LiveParticleTextureAnimation animation{};
  animation.program = reinterpret_cast<u_char*>(program.data());
  animation.texture_index = 1;
  VS::Effects::LiveAdvanceParticleTextureAnimation(&animation);
  TextureBindingFixture::CheckFirstFrame(animation);
  const auto* frame = animation.texture_info;
  VS::Effects::LiveAdvanceParticleTextureAnimation(&animation);
  CHECK(animation.texture_info == frame);
  CHECK(animation.frame_timer == 2);
  VS::Effects::LiveAdvanceParticleTextureAnimation(&animation);
  CHECK(animation.texture_info == frame);
  CHECK(animation.frame_timer == 0);
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
  VS::Effects::BindLiveEffectTextureTables(
      std::as_writable_bytes(std::span(bytes)));
  CHECK(Effects_actionPackage.texture_table_count == 3);
  CHECK(Effects_actionPackage.texture_tables[0] !=
        Effects_actionPackage.texture_tables[1]);
  CHECK(Effects_actionPackage.texture_tables[1]->first_texture == 1);
  CHECK(Effects_actionPackage.texture_tables[2]->first_texture == 0);
  CHECK(Effects_actionPackage.texture_tables[3] == nullptr);
}

TEST_CASE("Gravity channel normalization never leaks into secondary resources",
          "[effects][texture-binding]") {
  TextureBindingFixture fixture;
  VS::Effects::LiveEffectPackage secondary{};
  Effects_package = &secondary;
  fixture.Bind();
  CHECK(secondary.texture_table_count == 1);
  CHECK(secondary.texture_tables[1] == nullptr);
  CHECK(Effects_actionPackage.texture_table_count == 0);
}

TEST_CASE("Ended and delayed texture programs preserve explicit frame state",
          "[effects][texture-binding]") {
  TextureBindingFixture fixture;
  fixture.Bind();
  std::array<std::uint32_t, 1> program{0x30000000};
  const RecordedProgram recorded{program};
  VS::Effects::LiveParticleTextureAnimation animation{};
  animation.program = reinterpret_cast<u_char*>(program.data());
  animation.texture_index = 1;
  VS::Effects::LiveAdvanceParticleTextureAnimation(&animation);
  CHECK(animation.texture_info == nullptr);
  CHECK(animation.cursor == 0);
  animation.texture_info = Effects_actionPackage.texture_tables[1];
  animation.frame_timer = 4;
  VS::Effects::LiveAdvanceParticleTextureAnimation(&animation);
  CHECK(animation.texture_info == Effects_actionPackage.texture_tables[0]);
  CHECK(animation.frame_timer == 2);
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
  VS::Effects::BindLiveEffectTextureTables(
      std::as_writable_bytes(std::span(fixture.bytes))
          .first(sizeof fixture.bytes - 1));
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
  // D_800F5310, the original's effect origin, is the request's focus point.
  constexpr std::uint32_t kRequestAddress = 0x800F5230;
  constexpr std::uint32_t kOriginAddress = 0x800F5310;
  STATIC_CHECK(offsetof(VS::Effects::LiveActionEffectRequest, position) ==
               kOriginAddress - kRequestAddress);
  const auto symbols = VS::Content::ModuleSymbols();
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          kOriginAddress) == nullptr);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          kRequestAddress) ==
        &Effects_actionRequest);
}

namespace {
// One transform attached to the request origin (actor 0x12) at unit scale:
// block 9 (padded to a word) and block 10, then the terminator.
std::vector<std::byte> GravityOriginPackage() {
  VS::Effects::LivePackageParameters parameters{};
  parameters.transform_count = 1;
  VS::Effects::LiveTransformDefinition definition{};
  definition.flags = 7;
  definition.attach_actor = 0x12;
  definition.scale_range.fill(4096);
  constexpr std::size_t kParametersBlock = 4 + 0x1C;
  constexpr std::size_t kDefinitionsBlock = 4 + sizeof definition;
  static_assert(sizeof parameters <= kParametersBlock - 4);
  std::vector<std::byte> package(kParametersBlock + kDefinitionsBlock + 4);
  const auto put_word = [&package](std::size_t at, std::uint32_t word) {
    std::memcpy(package.data() + at, &word, sizeof word);
  };
  put_word(0, static_cast<std::uint32_t>(kParametersBlock) | (9U << 16));
  std::memcpy(package.data() + 4, &parameters, sizeof parameters);
  put_word(kParametersBlock,
           static_cast<std::uint32_t>(kDefinitionsBlock) | (0xAU << 16));
  std::memcpy(package.data() + kParametersBlock + 4, &definition,
              sizeof definition);
  return package;
}

// An identity rotation with the given translation.
void SetTranslatedViewMatrix(const std::array<std::int32_t, 3>& translation) {
  VS::Render::FrameScratchpad().view_matrix = {};
  for (std::size_t axis = 0; axis < translation.size(); ++axis) {
    VS::Render::FrameScratchpad().view_matrix.m[axis][axis] = 4096;
    VS::Render::FrameScratchpad().view_matrix.t[axis] = translation[axis];
  }
}
}  // namespace

TEST_CASE(
    "Authored Gravity origin attachment projects the actual request target",
    "[effects][gravity-placement][placement-red]") {
  TextureBindingFixture fixture;
  const auto saved_request = Effects_actionRequest;
  const auto saved_view = VS::Render::FrameScratchpad().view_matrix;
  auto package = GravityOriginPackage();
  REQUIRE(VS::Effects::BindLiveEffectPackage(package));
  Effects_EffectInstance instance{};
  Effects_actionRequest.position = {1200, -384, 900, 0};
  SetTranslatedViewMatrix({-100, 200, 600});
  VS::Effects::LiveInitializeTransformStates(&instance);
  VS::Effects::LiveUpdateEffectTransforms(&instance);
  CHECK(instance.transforms[0].world.t[0] == 1200);
  CHECK(instance.transforms[0].world.t[1] == -384);
  CHECK(instance.transforms[0].world.t[2] == 900);
  CHECK(instance.transforms[0].view.t[0] == 1100);
  CHECK(instance.transforms[0].view.t[1] == -184);
  CHECK(instance.transforms[0].view.t[2] == 1500);
  Effects_actionRequest = saved_request;
  VS::Render::FrameScratchpad().view_matrix = saved_view;
}
