#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "ActorAssets.h"
#include "Actors/ActorRecord.h"
#include "Actors/Movement.h"
#include "Actors/SkeletonLiveState.hpp"
#include "Battle/TargetingState.h"
#include "Boot/SessionLive.hpp"
#include "Content/ActorAssets.hpp"
#include "Content/ActorAssetsLive.hpp"
#include "Content/CdDriveLive.hpp"
#include "Content/DiscLayout.h"
#include "Core/Memory/HeapLive.hpp"
#include "DiscQueue.h"
#include "Event/ScriptVm.h"
#include "Legacy/Psx.h"
#include "LegacyFiles.h"
#include "Menu/GameClearLifecycleLiveFixture.hpp"
#include "Menu/GazetteTimesLiveFixture.hpp"
#include "Menu/OverlayLoadFixture.hpp"
#include "Menu/Score/Encyclopedia.hpp"
#include "Menu/Score/EncyclopediaPreview.hpp"
#include "Menu/Score/EncyclopediaPreviewLive.hpp"
#include "Menu/TextBox.h"
#include "ModelLoading.h"

namespace {
namespace score = VS::Menu::Score;
using VS::Content::ActorAssetRequest;
using VS::Content::AssetAddress;

constexpr auto At(std::uintptr_t value) {
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  return static_cast<AssetAddress>(value);
}

constexpr AssetAddress kRead = At(0x1000);
constexpr AssetAddress kRefusedRead = At(0x2000);
constexpr AssetAddress kFile = At(0x10000);
constexpr AssetAddress kEquippedBuffer = At(0x40000);
constexpr AssetAddress kCompanionBuffer = At(0x50000);

void PutWord(std::vector<std::byte>& bytes, std::size_t offset,
             std::uint32_t value) {
  for (std::size_t index = 0; index < 4; ++index) {
    bytes[offset + index] = static_cast<std::byte>(value >> (8 * index));
  }
}

// A ZUD file: the header bytes of _zudData_t and its sections.
std::vector<std::byte> ZudFile(std::size_t size = 0x200) {
  std::vector<std::byte> bytes(size);
  const std::array<std::uint8_t, 7> ids{13, 17, 9, 4, 27, 5, 6};
  for (std::size_t index = 0; index < ids.size(); ++index) {
    bytes[index] = static_cast<std::byte>(ids[index]);
  }
  PutWord(bytes, 0x08, 0x30);  // SHP
  PutWord(bytes, 0x0C, 0x40);
  PutWord(bytes, 0x10, 0x70);  // weapon WEP
  PutWord(bytes, 0x14, 0x20);
  PutWord(bytes, 0x18, 0x90);  // shield WEP
  PutWord(bytes, 0x1C, 0x20);
  PutWord(bytes, 0x20, 0xB0);  // common SEQ
  PutWord(bytes, 0x24, 0x10);
  PutWord(bytes, 0x28, 0xC0);  // battle SEQ
  PutWord(bytes, 0x2C, 0x10);
  return bytes;
}

struct FakeHost final : score::EnemyPreviewHost {
  std::uint8_t step = 0;
  score::EnemyPreviewFile file{
      .sector = 900, .bytes = 0x200, .palette_flag = 3};
  bool disc_queue_full = false;
  bool heap_full = false;
  bool model_heap_full = false;
  bool loaded = false;
  int first_free = 0;
  std::vector<std::byte> bytes = ZudFile();
  std::vector<ActorAssetRequest> queued;
  std::vector<std::string> calls;
  std::vector<std::string> refusals;

  [[nodiscard]] std::uint8_t LoadStep() const override { return step; }
  void SetLoadStep(std::uint8_t value) override { step = value; }
  [[nodiscard]] score::EnemyPreviewFile File(int zud_id) const override {
    CHECK(zud_id >= 0);
    return file;
  }
  [[nodiscard]] AssetAddress AllocateRead(
      const score::EnemyPreviewFile& /*file*/) override {
    calls.emplace_back("AllocateRead");
    return disc_queue_full ? AssetAddress::kNone : kRead;
  }
  void EnqueueRead(AssetAddress read, AssetAddress destination) override {
    CHECK(read == kRead);
    CHECK(destination == kFile);
    calls.emplace_back("EnqueueRead");
  }
  [[nodiscard]] AssetAddress ReadRefused(
      const score::EnemyPreviewFile& /*file*/,
      AssetAddress destination) override {
    CHECK(destination == kFile);
    calls.emplace_back("ReadRefused");
    return kRefusedRead;
  }
  [[nodiscard]] bool ReadLoaded(AssetAddress /*read*/) const override {
    return loaded;
  }
  void ReleaseRead(AssetAddress read) override {
    calls.push_back(read == kRefusedRead ? "ReleaseRefusedRead"
                                         : "ReleaseRead");
  }
  [[nodiscard]] AssetAddress AllocateFile(std::uint32_t size) override {
    calls.push_back("AllocateFile:" + std::to_string(size));
    return heap_full ? AssetAddress::kNone : kFile;
  }
  void FreeFile(AssetAddress block) override {
    CHECK(block == kFile);
    calls.emplace_back("FreeFile");
  }
  [[nodiscard]] AssetAddress AllocateModelBuffer(
      score::EnemyPreviewLayout layout) override {
    const bool companion = layout == score::EnemyPreviewLayout::kCompanion;
    calls.push_back(companion ? "AllocateCompanionBuffer"
                              : "AllocateEquippedBuffer");
    if (model_heap_full) return AssetAddress::kNone;
    return companion ? kCompanionBuffer : kEquippedBuffer;
  }
  void FreeModelBuffer(AssetAddress /*buffer*/) override {
    calls.emplace_back("FreeModelBuffer");
  }
  [[nodiscard]] AssetAddress ModelPart(
      AssetAddress buffer, score::EnemyPreviewPart part) const override {
    return VS::Content::AdvanceAddress(
        buffer, 0x100 * static_cast<std::ptrdiff_t>(part));
  }
  [[nodiscard]] std::span<const std::byte> FileBytes(
      AssetAddress block, std::uint32_t size) const override {
    if (block != kFile || size > bytes.size()) return {};
    return {bytes.data(), size};
  }
  void QueueAsset(const ActorAssetRequest& request) override {
    queued.push_back(request);
  }
  void UpdateAssetQueue() override { calls.emplace_back("UpdateAssetQueue"); }
  [[nodiscard]] int FirstFreeAssetRequest() const override {
    return first_free;
  }
  void ReleaseModel(int slot) override {
    calls.push_back("ReleaseModel:" + std::to_string(slot));
  }
  void ResetPreviewPose() override { calls.emplace_back("ResetPreviewPose"); }
  void SetWeaponDrawn(int slot, bool drawn) override {
    calls.push_back("WeaponDrawn:" + std::to_string(slot) + ":" +
                    std::to_string(static_cast<int>(drawn)));
  }
  void SetAnimation(int slot, std::uint8_t animation) override {
    calls.push_back("Animation:" + std::to_string(slot) + ":" +
                    std::to_string(animation));
  }
  void PlayAnimation(int slot, int animation, int blend_frames) override {
    calls.push_back("Play:" + std::to_string(slot) + ":" +
                    std::to_string(animation) + ":" +
                    std::to_string(blend_frames));
  }
  void InitializeAnimationPose(int slot) override {
    calls.push_back("InitializePose:" + std::to_string(slot));
  }
  void SetDisplayFlags(int slot, int flags) override {
    calls.push_back("Display:" + std::to_string(slot) + ":" +
                    std::to_string(flags));
  }
  void ClearCompanion(int slot) override {
    calls.push_back("ClearCompanion:" + std::to_string(slot));
  }
  void Refuse(std::string_view what) override { refusals.emplace_back(what); }

  [[nodiscard]] bool Called(std::string_view call) const {
    return std::ranges::find(calls, call) != calls.end();
  }
};

AssetAddress Part(AssetAddress buffer, score::EnemyPreviewPart part) {
  return VS::Content::AdvanceAddress(buffer,
                                     0x100 * static_cast<std::ptrdiff_t>(part));
}

// A state whose ZUD read has completed.
score::EnemyPreviewState LoadedState(FakeHost& host, int zud_id) {
  score::EnemyPreviewState state;
  score::QueueEnemyModelRead(host, state, zud_id);
  host.loaded = true;
  host.calls.clear();
  return state;
}
}  // namespace

TEST_CASE("Enemy preview steps queue the read and advance the shared step",
          "[encyclopedia-preview]") {
  FakeHost host;
  score::EnemyPreviewState state;
  REQUIRE(score::LoadEnemyPreview(host, state, 7) == 1);
  CHECK(host.step == 1);
  CHECK(state.zud_id == 7);
  CHECK(state.read == kRead);
  CHECK(state.file == kFile);
  CHECK(state.file_bytes == 0x200);
  CHECK(state.palette_flag == 3);
  CHECK(state.model_buffer == kEquippedBuffer);
  CHECK(host.calls ==
        std::vector<std::string>{"AllocateRead", "AllocateFile:512",
                                 "EnqueueRead", "AllocateEquippedBuffer"});
}

TEST_CASE("Enemy preview parts wait for the read", "[encyclopedia-preview]") {
  FakeHost host;
  score::EnemyPreviewState state;
  host.step = 1;
  score::QueueEnemyModelRead(host, state, 7);
  REQUIRE(score::LoadEnemyPreview(host, state, 7) == 1);
  CHECK(host.queued.empty());
  host.loaded = true;
  REQUIRE(score::LoadEnemyPreview(host, state, 7) == 1);
  CHECK(host.queued.size() == 5);
  CHECK(state.read == AssetAddress::kNone);
}

TEST_CASE("Enemy preview releases the file after the queue drains",
          "[encyclopedia-preview]") {
  FakeHost host;
  score::EnemyPreviewState state;
  host.step = 1;
  score::QueueEnemyModelRead(host, state, 7);
  host.loaded = true;
  REQUIRE(score::LoadEnemyPreview(host, state, 7) == 1);
  REQUIRE(state.read == AssetAddress::kNone);
  host.first_free = 2;
  REQUIRE(score::LoadEnemyPreview(host, state, 7) == 1);
  CHECK(state.file == kFile);
  host.first_free = 0;
  REQUIRE(score::LoadEnemyPreview(host, state, 7) == 1);
  CHECK(state.file == AssetAddress::kNone);
  CHECK(host.step == 1);
  REQUIRE(score::LoadEnemyPreview(host, state, 7) == 1);
  CHECK(host.step == 2);
}

TEST_CASE("Enemy preview pose step draws the weapon and resets the step",
          "[encyclopedia-preview]") {
  FakeHost host;
  score::EnemyPreviewState state;
  host.step = 2;
  CHECK(score::LoadEnemyPreview(host, state, 7) == 0);
  CHECK(host.step == 0);
  CHECK(host.calls == std::vector<std::string>{
                          "ResetPreviewPose", "WeaponDrawn:1:1", "Play:1:1:0"});
  const std::uint8_t other = GENERATE(3, 0x80, 0xFF);
  host.step = other;
  host.calls.clear();
  CHECK(score::LoadEnemyPreview(host, state, 7) == 1);
  CHECK(host.step == other);
  CHECK(host.calls.empty());
}

TEST_CASE("Equipped enemy parts carry fields over in the original order",
          "[encyclopedia-preview]") {
  FakeHost host;
  score::EnemyPreviewState state = LoadedState(host, 7);
  REQUIRE(score::LoadEnemyModelParts(host, state) == 1);
  REQUIRE(host.queued.size() == 5);
  const ActorAssetRequest& body = host.queued[0];
  CHECK(body == ActorAssetRequest{.kind = 2,
                                  .target_slot = 1,
                                  .model_id = 13,
                                  .destination = kEquippedBuffer,
                                  .data = At(0x10030),
                                  .sub_kind = 0xFF,
                                  .material = 6,
                                  .variant = 1});
  CHECK(host.queued[1] ==
        ActorAssetRequest{.kind = 4,
                          .target_slot = 2,
                          .model_id = 17,
                          .destination = Part(kEquippedBuffer,
                                              score::EnemyPreviewPart::kWeapon),
                          .data = At(0x10070),
                          .sub_kind = 1,
                          .texture_slot = 0xF0,
                          .material = 4,
                          .variant = 1});
  const AssetAddress shield =
      Part(kEquippedBuffer, score::EnemyPreviewPart::kShield);
  CHECK(host.queued[2] == ActorAssetRequest{.kind = 4,
                                            .target_slot = 3,
                                            .model_id = 27,
                                            .destination = shield,
                                            .data = At(0x10090),
                                            .sub_kind = 1,
                                            .texture_slot = 0xF1,
                                            .material = 5,
                                            .variant = 1});
  CHECK(host.queued[3] == ActorAssetRequest{.kind = 8,
                                            .target_slot = 1,
                                            .model_id = 9,
                                            .destination = shield,
                                            .data = At(0x100C0),
                                            .sub_kind = 0,
                                            .texture_slot = 0xF1,
                                            .material = 5,
                                            .variant = 1});
  CHECK(host.queued[4].model_id == 0);
  CHECK(host.queued[4].data == At(0x100B0));
  CHECK(host.calls == std::vector<std::string>{"ReleaseRead"});
}

TEST_CASE("Companion enemies queue the companion record instead of weapons",
          "[encyclopedia-preview]") {
  FakeHost host;
  score::EnemyPreviewState state = LoadedState(host, 31);
  CHECK(state.variant == 1);
  CHECK(state.model_buffer == kCompanionBuffer);
  REQUIRE(score::LoadEnemyModelParts(host, state) == 1);
  REQUIRE(host.queued.size() == 4);
  const AssetAddress companion =
      Part(kCompanionBuffer, score::EnemyPreviewPart::kCompanion);
  CHECK(host.queued[1] == ActorAssetRequest{.kind = 1,
                                            .target_slot = 16,
                                            .model_id = 27,
                                            .destination = companion,
                                            .data = At(0x10030),
                                            .sub_kind = 1,
                                            .texture_slot = 0xFC,
                                            .material = 6,
                                            .variant = 0});
  CHECK(host.queued[2].destination == companion);
  CHECK(host.queued[2].variant == 0);
  CHECK(host.queued[3].texture_slot == 0xFC);
}

TEST_CASE("Companion variants follow the table and its first entry quirk",
          "[encyclopedia-preview]") {
  CHECK(score::CompanionVariant(30) == 0);
  CHECK(score::CompanionVariant(31) == 1);
  CHECK(score::CompanionVariant(33) == 3);
  CHECK(score::CompanionVariant(173) == 5);
  CHECK(score::CompanionVariant(0) == 0);
  CHECK(score::CompanionVariant(29) == 0);
  CHECK(score::CompanionVariant(30 + 256) == 0);
  CHECK(score::CompanionVariant(-1) == 0);
  FakeHost host;
  score::EnemyPreviewState state = LoadedState(host, 30);
  CHECK(state.model_buffer == kEquippedBuffer);
}

TEST_CASE("Enemy preview reads refuse ids past the file table",
          "[encyclopedia-preview]") {
  const int zud_id = GENERATE(-1, 256, 0x10000);
  FakeHost host;
  score::EnemyPreviewState state;
  score::QueueEnemyModelRead(host, state, zud_id);
  CHECK(state == score::EnemyPreviewState{});
  CHECK(host.calls.empty());
  CHECK(host.refusals.size() == 1);
}

TEST_CASE("Enemy preview reads refuse while an earlier preview holds blocks",
          "[encyclopedia-preview]") {
  FakeHost host;
  score::EnemyPreviewState state;
  score::QueueEnemyModelRead(host, state, 7);
  const score::EnemyPreviewState held = state;
  host.calls.clear();
  score::QueueEnemyModelRead(host, state, 8);
  CHECK(state == held);
  CHECK(host.calls.empty());
  CHECK(host.refusals.size() == 1);
}

TEST_CASE("Enemy preview reads fall back when the disc queue is full",
          "[encyclopedia-preview]") {
  FakeHost host;
  host.disc_queue_full = true;
  score::EnemyPreviewState state;
  score::QueueEnemyModelRead(host, state, 7);
  CHECK(state.read == kRefusedRead);
  CHECK(host.Called("ReadRefused"));
  CHECK_FALSE(host.Called("EnqueueRead"));
  host.loaded = true;
  CHECK(score::LoadEnemyModelParts(host, state) == 1);
  CHECK(host.Called("ReleaseRefusedRead"));
  CHECK(host.queued.size() == 5);
}

TEST_CASE("Enemy preview reads refuse an exhausted heap",
          "[encyclopedia-preview]") {
  FakeHost host;
  host.heap_full = true;
  score::EnemyPreviewState state;
  score::QueueEnemyModelRead(host, state, 7);
  CHECK(state.read == AssetAddress::kNone);
  CHECK(state.file == AssetAddress::kNone);
  CHECK(host.Called("ReleaseRead"));
  CHECK_FALSE(host.Called("AllocateEquippedBuffer"));
  CHECK(host.refusals.size() == 1);
  CHECK(score::LoadEnemyModelParts(host, state) == 0);
}

TEST_CASE("Enemy preview parts refuse short files and missing buffers",
          "[encyclopedia-preview]") {
  FakeHost host;
  const bool short_file = GENERATE(true, false);
  if (short_file) {
    host.file.bytes = 0x2F;
  } else {
    host.model_heap_full = true;
  }
  score::EnemyPreviewState state = LoadedState(host, 7);
  host.refusals.clear();
  CHECK(score::LoadEnemyModelParts(host, state) == 1);
  CHECK(host.queued.empty());
  CHECK(host.refusals.size() == 1);
  CHECK(state.read == AssetAddress::kNone);
  CHECK(state.file == kFile);
}

TEST_CASE("Enemy preview parts refuse sections past the file",
          "[encyclopedia-preview]") {
  FakeHost host;
  PutWord(host.bytes, 0x14, 0x1000);
  PutWord(host.bytes, 0x28, 0xFFFFFFF0);
  score::EnemyPreviewState state = LoadedState(host, 7);
  CHECK(score::LoadEnemyModelParts(host, state) == 1);
  CHECK(host.refusals.size() == 2);
  REQUIRE(host.queued.size() == 3);
  CHECK(host.queued[1].target_slot == 3);
  CHECK(host.queued[2].data == At(0x100B0));
  PutWord(host.bytes, 0x0C, 0x1D1);
  host.queued.clear();
  state = LoadedState(host, 7);
  CHECK(score::LoadEnemyModelParts(host, state) == 1);
  CHECK(host.queued.empty());
}

TEST_CASE("ZUD sections fit only within the file", "[encyclopedia-preview]") {
  CHECK(score::ZudSectionFits({.offset = 0x30, .bytes = 0x10}, 0x40));
  CHECK(score::ZudSectionFits({.offset = 0x40, .bytes = 0}, 0x40));
  CHECK_FALSE(score::ZudSectionFits({.offset = 0x30, .bytes = 0x11}, 0x40));
  CHECK_FALSE(score::ZudSectionFits({.offset = 0x41, .bytes = 0}, 0x40));
  CHECK_FALSE(
      score::ZudSectionFits({.offset = 0x10, .bytes = 0xFFFFFFF8}, 0x40));
  const std::vector<std::byte> file = ZudFile(0x30);
  const auto header = score::ParseZudHeader(file);
  REQUIRE(header.has_value());
  if (header.has_value()) {
    CHECK(header->battle_sequence_model == 9);
    CHECK(header->shield_model ==
          score::ZudSection{.offset = 0x90, .bytes = 0x20});
  }
  CHECK_FALSE(score::ParseZudHeader(std::span(file).first(0x2F)).has_value());
}

TEST_CASE("Freeing the preview releases both actors and the buffer once",
          "[encyclopedia-preview]") {
  FakeHost host;
  score::EnemyPreviewState state = LoadedState(host, 7);
  score::FreeEnemyPreview(host, state);
  CHECK(host.calls == std::vector<std::string>{"ReleaseModel:1",
                                               "ReleaseModel:16",
                                               "FreeModelBuffer"});
  CHECK(state.model_buffer == AssetAddress::kNone);
  host.calls.clear();
  score::FreeEnemyModelBuffer(host, state);
  CHECK(host.calls.empty());
}

TEST_CASE("Switching enemies reads the next enemy after the release",
          "[encyclopedia-preview]") {
  FakeHost host;
  score::EnemyPreviewState state = LoadedState(host, 31);
  REQUIRE(score::LoadEnemyModelParts(host, state) == 1);
  REQUIRE(score::LoadEnemyModelParts(host, state) == 1);
  REQUIRE(score::LoadEnemyModelParts(host, state) == 0);
  score::FreeEnemyPreview(host, state);
  host.calls.clear();
  host.loaded = false;
  score::QueueEnemyModelRead(host, state, 7);
  CHECK(host.refusals.empty());
  CHECK(state.zud_id == 7);
  CHECK(state.variant == 0);
  CHECK(state.read == kRead);
  CHECK(state.model_buffer == kEquippedBuffer);
}

TEST_CASE("Ashley preview pose sets the encyclopedia animation",
          "[encyclopedia-preview]") {
  FakeHost host;
  score::InitializeAshleyPreviewPose(host);
  CHECK(host.calls ==
        std::vector<std::string>{"Animation:1:33", "WeaponDrawn:1:0",
                                 "InitializePose:1", "Display:1:3",
                                 "ClearCompanion:1"});
}

namespace {
// Saves the live disc requests, asset queue, load step and preview state,
// and swaps in a fresh game heap.
struct LivePreviewFixture {
  std::vector<std::byte> arena = std::vector<std::byte>(0x400000);
  VS::Core::Memory::ScopedLiveGameHeap heap{arena};
  std::array<Content_DiscQueueSlot, 32> saved_slots{};
  Content_DiscRequestCounts saved_counts = Content_discRequestCounts;
  std::array<Content_ActorAssetRequest, 16> saved_requests{};
  std::uint8_t saved_step = VS::Content::objectDataLoadStep;
  score::EnemyPreviewState saved_state = score::LiveEnemyPreviewState();

  LivePreviewFixture() {
    std::memcpy(saved_slots.data(), Content_discRequestSlots,
                sizeof Content_discRequestSlots);
    std::memset(Content_discRequestSlots, 0, sizeof Content_discRequestSlots);
    Content_discRequestCounts = Content_DiscRequestCounts{};
    std::copy_n(Content_legacyActorAssetRequests, saved_requests.size(),
                saved_requests.begin());
    std::fill_n(Content_legacyActorAssetRequests, saved_requests.size(),
                Content_ActorAssetRequest{});
    VS::Content::objectDataLoadStep = 0;
    score::ResetLiveEnemyPreview();
  }
  ~LivePreviewFixture() {
    std::memcpy(Content_discRequestSlots, saved_slots.data(),
                sizeof Content_discRequestSlots);
    Content_discRequestCounts = saved_counts;
    std::copy(saved_requests.begin(), saved_requests.end(),
              Content_legacyActorAssetRequests);
    VS::Content::objectDataLoadStep = saved_step;
    score::LiveEnemyPreviewState() = saved_state;
  }
  LivePreviewFixture(const LivePreviewFixture&) = delete;
  LivePreviewFixture& operator=(const LivePreviewFixture&) = delete;
  LivePreviewFixture(LivePreviewFixture&&) = delete;
  LivePreviewFixture& operator=(LivePreviewFixture&&) = delete;
};

template <typename T = std::byte>
T* Pointer(AssetAddress address) {
  return reinterpret_cast<T*>(static_cast<std::uintptr_t>(address));
}

// The companion record, or the enemy's weapon and shield, follow the body at
// the native record strides.
void CheckAttachmentStrides(bool companion, const u_char* buffer) {
  const Content_ActorAssetRequest& attachment =
      Content_legacyActorAssetRequests[1];
  CHECK(attachment.destination == buffer + sizeof(Actors_ActorRecord));
  CHECK(attachment.assetKind == (companion ? 1 : 4));
  CHECK(attachment.targetSlot == (companion ? 16 : 2));
  if (!companion) {
    const Content_ActorAssetRequest& shield =
        Content_legacyActorAssetRequests[2];
    CHECK(shield.destination ==
          buffer + sizeof(Actors_ActorRecord) + sizeof(Actors_WeaponModel));
    CHECK(shield.targetSlot == 3);
  }
}

void CheckColdPreviewRelease(const score::EnemyPreviewState& before,
                             std::uint64_t expected_read_generation = 0) {
  CHECK(score::LiveEnemyPreviewState() == score::EnemyPreviewState{});
  CHECK_FALSE(
      VS::Core::Memory::LivePayloadCapacity(Pointer(before.file)).has_value());
  CHECK_FALSE(
      VS::Core::Memory::LivePayloadCapacity(Pointer(before.model_buffer))
          .has_value());
  CHECK(VS::Content::LiveDiscRequestGeneration(Pointer<Content_DiscQueueSlot>(
            before.read)) == expected_read_generation);
  CHECK(VS::Content::objectDataLoadStep == 0);
}

void DiscardUnparsedPreview(const score::EnemyPreviewState& before,
                            std::uint64_t generation) {
  auto* file = Pointer(before.file);
  const bool owns_file =
      VS::Core::Memory::LivePayloadCapacity(file).has_value();
  if (owns_file) {
    REQUIRE(VS::Content::CancelLiveDiscWrites({file, before.file_bytes}));
  }
  auto* request = Pointer<Content_DiscQueueSlot>(before.read);
  if (VS::Content::LiveDiscRequestGeneration(request) == generation) {
    VS::Content::ReleaseLiveDiscRequest(request);
  }
  if (owns_file) VS::Core::Memory::FreeLiveFromHighEnd(file);
  auto* model = Pointer(before.model_buffer);
  if (VS::Core::Memory::LivePayloadCapacity(model).has_value()) {
    VS::Core::Memory::FreeLiveFromHighEnd(model);
  }
  score::ResetLiveEnemyPreview();
  VS::Content::objectDataLoadStep = 0;
}

struct PreviewLifecycleFixture : LivePreviewFixture {
  game_clear_live_tests::SavedModuleData saved_module;
  game_clear_live_tests::SavedRuntime saved_runtime;
  game_clear_live_tests::SavedGlobals<
      Actors_actorRecords, VS::Actors::weaponModels,
      Content_legacyModelLoadPhase, VS::Content::modelParseResetFlag,
      Content_legacySequenceParsePhase, Content_legacyShpFileBuffer,
      Content_legacyShpAllocated, Content_legacyShpFileLocation,
      Content_legacyShpDiscRequest, Content_legacyShpParseCursor,
      Content_legacyShpCopiedWordCount, Content_legacyShpHeaderWord,
      Content_legacyShpTextureWidth, Content_legacyShpTextureHeight,
      Content_legacyWeaponParseCursor, Content_legacySequenceParseData,
      Content_legacyShpDiscOffsets, Content_legacyShpSectorCounts>
      saved_loader;
  equipment_detail_tests::ValueRestore<int> saved_zud_size{score::zudSizes[7]};

  PreviewLifecycleFixture() {
    std::ranges::fill(Actors_actorRecords, nullptr);
    std::ranges::fill(VS::Actors::weaponModels, nullptr);
    Content_legacyModelLoadPhase = 0;
    VS::Content::modelParseResetFlag = 0;
    Content_legacySequenceParsePhase = 0;
    Content_legacyShpAllocated = 0;
    Content_legacyShpFileBuffer = nullptr;
    Content_legacyShpParseCursor = nullptr;
    Content_legacyWeaponParseCursor = nullptr;
    Content_legacySequenceParseData = nullptr;
  }
  ~PreviewLifecycleFixture() {
    CHECK(score::CleanupLiveEnemyPreviewOwnedState());
  }
};

score::EnemyPreviewState QueuePreviewParts(bool companion,
                                           bool modelless = false) {
  REQUIRE(score::LoadLiveEnemyPreview(companion ? 31 : 7) == 1);
  const auto before = score::LiveEnemyPreviewState();
  auto bytes = ZudFile();
  if (modelless) bytes[0] = std::byte{0x7F};
  std::ranges::copy(bytes, Pointer(before.file));
  auto* request = Pointer<Content_DiscQueueSlot>(before.read);
  request->state = Content_DiscRequestState_Loaded;
  REQUIRE(score::LoadLiveEnemyModelParts() == 1);
  REQUIRE(VS::Content::LiveDiscRequestGeneration(request) == 0);
  return before;
}

score::EnemyPreviewState QueueOriginalPreview() {
  const auto image = gazette_times_live_tests::ReadImage(
      R"(Native\data\disc\MAP\Z031U00.ZUD)");
  REQUIRE(image.size() < 0x400000);
  score::zudSizes[7] = static_cast<int>(image.size());
  REQUIRE(score::LoadLiveEnemyPreview(7) == 1);
  const auto before = score::LiveEnemyPreviewState();
  std::ranges::copy(image, Pointer<unsigned char>(before.file));
  Pointer<Content_DiscQueueSlot>(before.read)->state =
      Content_DiscRequestState_Loaded;
  REQUIRE(score::LoadLiveEnemyModelParts() == 1);
  return before;
}

score::EnemyPreviewState StartCompanionRead() {
  const auto before = QueuePreviewParts(true, true);
  const auto model =
      static_cast<std::uint16_t>(Content_legacyActorAssetRequests[1].modelId);
  REQUIRE(model < std::size(Content_legacyShpDiscOffsets));
  if (model >= std::size(Content_legacyShpDiscOffsets)) {
    throw std::runtime_error("companion model is outside its file table");
  }
  Content_legacyShpSectorCounts[model] = 1;
  Content_legacyShpDiscOffsets[model] = 7;
  REQUIRE(VS::Content::UpdateLiveActorAssetQueue() == 1);
  REQUIRE(Content_legacyActorAssetRequests[0].targetSlot == 16);
  REQUIRE(VS::Content::UpdateLiveActorAssetQueue() == 1);
  REQUIRE(Content_legacyModelLoadPhase == 1);
  REQUIRE(Content_legacyShpAllocated == 1);
  return before;
}

void CheckEmptyPreviewQueue() {
  CHECK(std::ranges::all_of(
      Content_legacyActorAssetRequests,
      [](const auto& item) { return item.assetKind == 0; }));
  CHECK(VS::Content::modelParseResetFlag == 0);
  CHECK(Content_legacyShpParseCursor == nullptr);
  CHECK(Content_legacySequenceParseData == nullptr);
}

std::span<std::byte> StartForeignModelRead() {
  auto* foreign = static_cast<std::byte*>(
      VS::Core::Memory::AllocateLiveFromLowEnd(sizeof(Actors_ActorRecord)));
  REQUIRE(foreign != nullptr);
  Content_legacyShpSectorCounts[20] = 1;
  Content_legacyShpDiscOffsets[20] = 7;
  ActorAssetRequest request;
  request.kind =
      static_cast<std::uint8_t>(VS::Content::ActorAssetKind::kActorModelLoad);
  request.model_id = 20;
  request.destination = At(reinterpret_cast<std::uintptr_t>(foreign));
  REQUIRE(VS::Content::QueueLiveActorAsset(request) == 0);
  REQUIRE(VS::Content::UpdateLiveActorAssetQueue() == 1);
  return {foreign, sizeof(Actors_ActorRecord)};
}

void CheckForeignModelRead(const void* file, const Content_DiscQueueSlot* read,
                           std::uint64_t generation) {
  CHECK(Content_legacyModelLoadPhase == 1);
  CHECK(Content_legacyShpFileBuffer == file);
  CHECK(VS::Content::LiveDiscRequestGeneration(read) == generation);
  CHECK(VS::Content::FindLiveFreeActorAssetRequest() == 1);
  CHECK(VS::Core::Memory::LivePayloadCapacity(file).has_value());
}

const void* BeginPreviewParser() {
  REQUIRE(VS::Content::UpdateLiveActorAssetQueue() == 1);
  REQUIRE(VS::Content::modelParseResetFlag == 1);
  auto* actor = Actors_actorRecords[1];
  if (actor == nullptr) throw std::runtime_error("preview actor was not bound");
  const auto* geometry = actor->instance.model;
  REQUIRE(VS::Core::Memory::LivePayloadCapacity(geometry).has_value());
  return geometry;
}

void FinishPreviewParts() {
  for (int step = 0;
       step < 32 && VS::Content::FindLiveFreeActorAssetRequest() != 0; ++step) {
    REQUIRE(score::LoadLiveEnemyModelParts() == 1);
  }
  REQUIRE(VS::Content::FindLiveFreeActorAssetRequest() == 0);
}

std::span<std::byte> BindForeignPreviewActor() {
  auto* record = static_cast<std::byte*>(
      VS::Core::Memory::AllocateLiveFromLowEnd(sizeof(Actors_ActorRecord)));
  REQUIRE(record != nullptr);
  ActorAssetRequest request;
  request.kind =
      static_cast<std::uint8_t>(VS::Content::ActorAssetKind::kActorModelParse);
  request.target_slot = 1;
  request.model_id = 0x7F;
  request.destination = At(reinterpret_cast<std::uintptr_t>(record));
  REQUIRE(VS::Content::QueueLiveActorAsset(request) == 0);
  REQUIRE(VS::Content::UpdateLiveActorAssetQueue() == 1);
  REQUIRE(static_cast<void*>(Actors_actorRecords[1]) == record);
  return {record, sizeof(Actors_ActorRecord)};
}

void FillForeignPreviewQueue(std::span<std::byte> record) {
  ActorAssetRequest request;
  request.kind =
      static_cast<std::uint8_t>(VS::Content::ActorAssetKind::kActorModelParse);
  request.target_slot = 1;
  request.model_id = 0x7F;
  request.destination = At(reinterpret_cast<std::uintptr_t>(record.data()));
  const int first = VS::Content::FindLiveFreeActorAssetRequest();
  for (int slot = first; slot < VS::Content::kActorAssetRequestCount; ++slot) {
    request.variant = static_cast<std::uint8_t>(slot);
    REQUIRE(VS::Content::QueueLiveActorAsset(request) == slot);
  }
}
}  // namespace

TEST_CASE("Live enemy preview releases the read after queueing parts",
          "[encyclopedia-preview][encyclopedia-preview-live]") {
  LivePreviewFixture fixture;
  REQUIRE(score::LoadLiveEnemyPreview(31) == 1);
  CHECK(VS::Content::objectDataLoadStep == 1);
  const score::EnemyPreviewState& state = score::LiveEnemyPreviewState();
  REQUIRE(state.read != AssetAddress::kNone);
  REQUIRE(state.file_bytes >= 0x200);
  auto* read = Pointer<Content_DiscQueueSlot>(state.read);
  CHECK(read->vram == Pointer(state.file));
  std::ranges::copy(ZudFile(), Pointer(state.file));
  read->state = Content_DiscRequestState_Loaded;
  REQUIRE(score::LoadLiveEnemyModelParts() == 1);
  CHECK(read->state == Content_DiscRequestState_Free);
  CHECK(VS::Content::FindLiveFreeActorAssetRequest() == 4);
}

TEST_CASE("Live enemy preview places parts at native record strides",
          "[encyclopedia-preview][encyclopedia-preview-live]") {
  const bool companion = GENERATE(true, false);
  LivePreviewFixture fixture;
  REQUIRE(score::LoadLiveEnemyPreview(companion ? 31 : 7) == 1);
  const score::EnemyPreviewState& state = score::LiveEnemyPreviewState();
  REQUIRE(state.read != AssetAddress::kNone);
  std::ranges::copy(ZudFile(), Pointer(state.file));
  Pointer<Content_DiscQueueSlot>(state.read)->state =
      Content_DiscRequestState_Loaded;
  auto* buffer = Pointer<u_char>(state.model_buffer);
  auto* file = Pointer<u_char>(state.file);
  REQUIRE(score::LoadLiveEnemyModelParts() == 1);
  const Content_ActorAssetRequest& body = Content_legacyActorAssetRequests[0];
  CHECK(body.destination == buffer);
  CHECK(body.dataAddress == reinterpret_cast<std::uintptr_t>(file + 0x30));
  CHECK(std::bit_cast<u_int>(body.placement) == 0);
  CheckAttachmentStrides(companion, buffer);
}

TEST_CASE("Live enemy preview releases the file after the queue drains",
          "[encyclopedia-preview][encyclopedia-preview-live]") {
  LivePreviewFixture fixture;
  score::QueueLiveEnemyModelRead(7);
  score::EnemyPreviewState& state = score::LiveEnemyPreviewState();
  REQUIRE(state.read != AssetAddress::kNone);
  const auto* file = Pointer(state.file);
  const auto* buffer = Pointer(state.model_buffer);
  Pointer<Content_DiscQueueSlot>(state.read)->state =
      Content_DiscRequestState_Loaded;
  std::ranges::copy(ZudFile(), Pointer(state.file));
  REQUIRE(score::LoadLiveEnemyModelParts() == 1);
  std::fill_n(Content_legacyActorAssetRequests, 16,
              Content_ActorAssetRequest{});
  REQUIRE(score::LoadLiveEnemyModelParts() == 1);
  CHECK(state.file == AssetAddress::kNone);
  CHECK_FALSE(VS::Core::Memory::LivePayloadCapacity(file).has_value());
  CHECK(score::LoadLiveEnemyModelParts() == 0);
  score::FreeLiveEnemyModelBuffer();
  CHECK(state.model_buffer == AssetAddress::kNone);
  CHECK_FALSE(VS::Core::Memory::LivePayloadCapacity(buffer).has_value());
}

TEST_CASE("Live enemy preview refuses ids past the file table",
          "[encyclopedia-preview][encyclopedia-preview-live]") {
  LivePreviewFixture fixture;
  const std::uint64_t before = score::LiveEnemyPreviewRefusals();
  score::QueueLiveEnemyModelRead(256);
  CHECK(score::LiveEnemyPreviewRefusals() == before + 1);
  CHECK(score::LiveEnemyPreviewState() == score::EnemyPreviewState{});
  CHECK(std::ranges::all_of(Content_discRequestSlots, [](const auto& slot) {
    return slot.state == Content_DiscRequestState_Free;
  }));
}

TEST_CASE("Actual cold session drains the pending enemy preview owner",
          "[encyclopedia-preview][gazette-preview-lifecycle]") {
  const game_clear_live_tests::SavedModuleData saved_module;
  const game_clear_live_tests::SavedGlobals<
      Menu_textBoxSelector, Battle_stringContext, Event_isTextBoxAwaitingClose,
      Menu_sceneNotificationBuffer, Menu_sceneNotificationTimer,
      Menu_sceneNotificationTimed>
      saved_ui;
  LivePreviewFixture fixture;
  REQUIRE(score::LoadLiveEnemyPreview(7) == 1);
  const auto before = score::LiveEnemyPreviewState();
  REQUIRE(before.read != AssetAddress::kNone);
  const auto generation = VS::Content::LiveDiscRequestGeneration(
      Pointer<Content_DiscQueueSlot>(before.read));
  const Content_DiscFile foreign_file{1, 16};
  auto* foreign_request = VS::Content::AllocateLiveDiscRequest(&foreign_file);
  REQUIRE(foreign_request != nullptr);
  const auto foreign_generation =
      VS::Content::LiveDiscRequestGeneration(foreign_request);
  void* const foreign = VS::Core::Memory::AllocateLiveFromLowEnd(64);
  REQUIRE(foreign != nullptr);
  const auto entry = +[] { return 0; };
  CHECK(VS::Boot::RunLiveSessionLoop(entry, entry) == 0);
  CheckColdPreviewRelease(before);
  CHECK(VS::Content::LiveDiscRequestGeneration(foreign_request) ==
        foreign_generation);
  CHECK(VS::Core::Memory::LivePayloadCapacity(foreign).has_value());
  DiscardUnparsedPreview(before, generation);
  VS::Content::ReleaseLiveDiscRequest(foreign_request);
  VS::Core::Memory::FreeLiveFromLowEnd(foreign);
}

TEST_CASE("Only completed MENU9 abandons a pending or loaded preview read",
          "[encyclopedia-preview][gazette-preview-lifecycle]") {
  const bool loaded = GENERATE(false, true);
  PreviewLifecycleFixture fixture;
  REQUIRE(score::LoadLiveEnemyPreview(7) == 1);
  const auto before = score::LiveEnemyPreviewState();
  if (loaded) {
    Pointer<Content_DiscQueueSlot>(before.read)->state =
        Content_DiscRequestState_Loaded;
  }
  {
    menu_overlay_tests::Load other(3, {VS_MENU2_PRG_LBA, VS_MENU2_PRG_SIZE});
    REQUIRE(other.Complete() == 1);
    CHECK(score::LiveEnemyPreviewState() == before);
  }
  {
    menu_overlay_tests::Load menu(10, {VS_MENU9_PRG_LBA, VS_MENU9_PRG_SIZE});
    REQUIRE(menu.Poll() == 0);
    CHECK(score::LiveEnemyPreviewState() == before);
    REQUIRE(menu.Complete() == 1);
  }
  CheckColdPreviewRelease(before);
}

TEST_CASE("Actual heap replacement abandons queued preview borrows",
          "[encyclopedia-preview][gazette-preview-lifecycle]") {
  PreviewLifecycleFixture fixture;
  const auto before = QueuePreviewParts(true);
  const auto refusals = VS::Core::Memory::RefusedLiveHeapRequestCount();
  VS::Boot::ResetLiveSessionHeap();
  CheckColdPreviewRelease(before);
  CheckEmptyPreviewQueue();
  CHECK(VS::Core::Memory::GameHeap().Empty());
  CHECK(VS::Core::Memory::RefusedLiveHeapRequestCount() == refusals);
}

TEST_CASE("Preview cleanup preserves a foreign active model read",
          "[encyclopedia-preview][gazette-preview-lifecycle]") {
  PreviewLifecycleFixture fixture;
  const auto foreign = StartForeignModelRead();
  const auto* file = Content_legacyShpFileBuffer;
  auto* read = Content_legacyShpDiscRequest;
  const auto generation = VS::Content::LiveDiscRequestGeneration(read);
  const auto before = QueuePreviewParts(false);
  REQUIRE(score::CleanupLiveEnemyPreviewOwnedState());
  CheckColdPreviewRelease(before);
  CheckForeignModelRead(file, read, generation);
  REQUIRE(VS::Content::CancelLiveActorAssetBorrows({}, foreign));
  VS::Core::Memory::FreeLiveFromLowEnd(foreign.data());
}

TEST_CASE("Preview cleanup cancels the companion model-file owner",
          "[encyclopedia-preview][gazette-preview-lifecycle]") {
  const bool loaded = GENERATE(false, true);
  PreviewLifecycleFixture fixture;
  const auto before = StartCompanionRead();
  auto* request = Content_legacyShpDiscRequest;
  const auto* file = Content_legacyShpFileBuffer;
  REQUIRE(VS::Content::LiveDiscRequestGeneration(request) != 0);
  if (loaded) request->state = Content_DiscRequestState_Loaded;
  REQUIRE(score::CleanupLiveEnemyPreviewOwnedState());
  CheckColdPreviewRelease(before);
  CheckEmptyPreviewQueue();
  CHECK(VS::Content::LiveDiscRequestGeneration(request) == 0);
  CHECK_FALSE(VS::Core::Memory::LivePayloadCapacity(file).has_value());
  CHECK(Content_legacyModelLoadPhase == 0);
  CHECK(Content_legacyShpAllocated == 0);
  CHECK(Actors_actorRecords[1] == nullptr);
}

TEST_CASE("Preview cleanup releases partial and completed original model parts",
          "[encyclopedia-preview][gazette-preview-lifecycle]") {
  const bool complete = GENERATE(false, true);
  PreviewLifecycleFixture fixture;
  const auto before = QueueOriginalPreview();
  const auto refused = VS::Content::LiveActorAssetRefusals();
  const auto* geometry = BeginPreviewParser();
  if (complete) FinishPreviewParts();
  REQUIRE(score::CleanupLiveEnemyPreviewOwnedState());
  CheckColdPreviewRelease(before);
  CheckEmptyPreviewQueue();
  CHECK(Actors_actorRecords[1] == nullptr);
  CHECK_FALSE(VS::Core::Memory::LivePayloadCapacity(geometry).has_value());
  CHECK(VS::Content::LiveActorAssetRefusals() == refused);
}

TEST_CASE("Preview cleanup preserves reused disc slots and empty foreign loads",
          "[encyclopedia-preview][gazette-preview-lifecycle]") {
  PreviewLifecycleFixture fixture;
  REQUIRE(score::LoadLiveEnemyPreview(7) == 1);
  const auto before = score::LiveEnemyPreviewState();
  auto* old = Pointer<Content_DiscQueueSlot>(before.read);
  VS::Content::ReleaseLiveDiscRequest(old);
  const Content_DiscFile location{1, 16};
  auto* foreign = VS::Content::AllocateLiveDiscRequest(&location);
  REQUIRE(foreign == old);
  const auto generation = VS::Content::LiveDiscRequestGeneration(foreign);
  REQUIRE(generation != before.read_generation);
  REQUIRE(score::CleanupLiveEnemyPreviewOwnedState());
  CHECK(VS::Content::LiveDiscRequestGeneration(foreign) == generation);
  CHECK_FALSE(
      VS::Core::Memory::LivePayloadCapacity(Pointer(before.file)).has_value());
  VS::Content::objectDataLoadStep = 7;
  REQUIRE(score::CleanupLiveEnemyPreviewOwnedState());
  CHECK(VS::Content::objectDataLoadStep == 7);
  VS::Content::ReleaseLiveDiscRequest(foreign);
}

TEST_CASE("Preview cleanup refuses an invalid allocation before losing owners",
          "[encyclopedia-preview][gazette-preview-lifecycle]") {
  PreviewLifecycleFixture fixture;
  REQUIRE(score::LoadLiveEnemyPreview(7) == 1);
  auto& state = score::LiveEnemyPreviewState();
  const auto before = state;
  const auto refused = score::LiveEnemyPreviewRefusals();
  state.model_buffer = At(4);
  CHECK_FALSE(score::CleanupLiveEnemyPreviewOwnedState());
  CHECK(score::LiveEnemyPreviewRefusals() == refused + 1);
  CHECK(VS::Content::LiveDiscRequestGeneration(Pointer<Content_DiscQueueSlot>(
            before.read)) == before.read_generation);
  CHECK(
      VS::Core::Memory::LivePayloadCapacity(Pointer(before.file)).has_value());
  state = before;
  REQUIRE(score::CleanupLiveEnemyPreviewOwnedState());
  CheckColdPreviewRelease(before);
}

TEST_CASE("Preview cleanup preserves foreign actors queue order and tail",
          "[encyclopedia-preview][gazette-preview-lifecycle]") {
  PreviewLifecycleFixture fixture;
  const auto foreign = BindForeignPreviewActor();
  const auto before = QueuePreviewParts(false);
  const int first_foreign = VS::Content::FindLiveFreeActorAssetRequest();
  FillForeignPreviewQueue(foreign);
  const auto tail = Content_legacyActorAssetRequests[15];
  REQUIRE(score::CleanupLiveEnemyPreviewOwnedState());
  CheckColdPreviewRelease(before);
  CHECK(static_cast<void*>(Actors_actorRecords[1]) == foreign.data());
  CHECK(VS::Core::Memory::LivePayloadCapacity(foreign.data()).has_value());
  CHECK(Content_legacyActorAssetRequests[0].variant == first_foreign);
  CHECK(Content_legacyActorAssetRequests[15].variant == tail.variant);
  CHECK(Content_legacyActorAssetRequests[15].destination == tail.destination);
  REQUIRE(VS::Content::CancelLiveActorAssetBorrows({}, foreign));
  CHECK(VS::Content::ReleaseLiveActorModel(1) == 0);
  VS::Core::Memory::FreeLiveFromLowEnd(foreign.data());
}

TEST_CASE("Preview cleanup preserves a reused companion read generation",
          "[encyclopedia-preview][gazette-preview-lifecycle]") {
  PreviewLifecycleFixture fixture;
  const auto before = StartCompanionRead();
  auto* original = Content_legacyShpDiscRequest;
  const auto* file = Content_legacyShpFileBuffer;
  VS::Content::ReleaseLiveDiscRequest(original);
  const Content_DiscFile location{1, 16};
  auto* foreign = VS::Content::AllocateLiveDiscRequest(&location);
  REQUIRE(foreign == original);
  const auto generation = VS::Content::LiveDiscRequestGeneration(foreign);
  REQUIRE(score::CleanupLiveEnemyPreviewOwnedState());
  CheckColdPreviewRelease(
      before,
      Pointer<Content_DiscQueueSlot>(before.read) == foreign ? generation : 0);
  CHECK(VS::Content::LiveDiscRequestGeneration(foreign) == generation);
  CHECK_FALSE(VS::Core::Memory::LivePayloadCapacity(file).has_value());
  CHECK(Content_legacyShpAllocated == 0);
  VS::Content::ReleaseLiveDiscRequest(foreign);
}
