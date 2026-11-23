#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <format>
#include <map>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Actors/ActorRecord.hpp"
#include "Actors/Animation.hpp"
#include "Actors/SkeletonLiveState.hpp"
#include "Battle/TargetingState.hpp"
#include "Boot/SessionLive.hpp"
#include "Content/ActorAssets.hpp"
#include "Content/ActorAssetsLive.hpp"
#include "Content/CdDriveLive.hpp"
#include "Content/DataStore.hpp"
#include "Content/DiscLayout.hpp"
#include "Content/DiscQueue.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Event/ScriptVmLiveState.hpp"
#include "Legacy/Psx.hpp"
#include "Menu/GameClearLifecycleLiveFixture.hpp"
#include "Menu/GazetteTimesLiveFixture.hpp"
#include "Menu/OverlayLoadFixture.hpp"
#include "Menu/Score/Encyclopedia.hpp"
#include "Menu/Score/EncyclopediaPreview.hpp"
#include "Menu/Score/EncyclopediaPreviewLive.hpp"
#include "Menu/TextBox.hpp"
#include "TestSupport/ModelFixtures.hpp"

using VS::Content::disc::Locate;

namespace {
namespace score = VS::Menu::Score;
using VS::Content::ActorAssetOutcome;
using VS::Content::ActorAssetRequest;
using VS::Content::ActorAssetTicket;
using VS::Content::AssetAddress;

constexpr auto At(std::uintptr_t value) {
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  return static_cast<AssetAddress>(value);
}

// The fake host issues tickets from 1, in submission order.
constexpr auto Ticket(std::uint32_t value) {
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  return static_cast<ActorAssetTicket>(value);
}

// Whether a preview holds no read, heap block or ticket.
bool HoldsNothing(const score::EnemyPreviewState& state) {
  return state.read == AssetAddress::kNone &&
         state.file == AssetAddress::kNone &&
         state.model_buffer == AssetAddress::kNone &&
         state.tickets == score::EnemyPreviewState{}.tickets;
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
  bool read_records_full = false;
  bool heap_full = false;
  bool model_heap_full = false;
  score::EnemyPreviewRead read_status = score::EnemyPreviewRead::kPending;
  // Submissions past this count are refused.
  std::size_t asset_capacity = 16;
  bool cancel_refused = false;
  std::vector<score::EnemyPreviewPart> bound_parts;
  std::vector<std::byte> bytes = ZudFile();
  std::vector<ActorAssetRequest> queued;
  std::map<ActorAssetTicket, ActorAssetOutcome> outcomes;
  std::uint32_t next_ticket = 1;
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
    return read_records_full ? AssetAddress::kNone : kRefusedRead;
  }
  [[nodiscard]] score::EnemyPreviewRead ReadStatus(
      AssetAddress /*read*/) const override {
    return read_status;
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
  [[nodiscard]] std::optional<ActorAssetTicket> SubmitAsset(
      const ActorAssetRequest& request) override {
    if (queued.size() >= asset_capacity) return std::nullopt;
    queued.push_back(request);
    const auto ticket = static_cast<ActorAssetTicket>(next_ticket++);
    outcomes[ticket] = ActorAssetOutcome::kPending;
    return ticket;
  }
  void UpdateAssetQueue() override { calls.emplace_back("UpdateAssetQueue"); }
  [[nodiscard]] ActorAssetOutcome AssetOutcome(
      ActorAssetTicket ticket) override {
    const auto found = outcomes.find(ticket);
    return found == outcomes.end() ? ActorAssetOutcome::kUnknown
                                   : found->second;
  }
  void ForgetAsset(ActorAssetTicket ticket) override {
    calls.push_back("Forget:" +
                    std::to_string(static_cast<std::uint32_t>(ticket)));
    outcomes.erase(ticket);
  }
  [[nodiscard]] bool CancelAssets() override {
    calls.emplace_back("CancelAssets");
    if (cancel_refused) return false;
    for (auto& [ticket, outcome] : outcomes) {
      if (outcome == ActorAssetOutcome::kPending) {
        outcome = ActorAssetOutcome::kCancelled;
      }
    }
    return true;
  }
  [[nodiscard]] bool ReleasePart(AssetAddress /*buffer*/,
                                 score::EnemyPreviewPart part) override {
    calls.push_back("ReleasePart:" + std::to_string(static_cast<int>(part)));
    return std::ranges::find(bound_parts, part) == bound_parts.end();
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

  void LoadAll() {
    for (auto& [ticket, outcome] : outcomes) {
      outcome = ActorAssetOutcome::kLoaded;
    }
  }
};

AssetAddress Part(AssetAddress buffer, score::EnemyPreviewPart part) {
  return VS::Content::AdvanceAddress(buffer,
                                     0x100 * static_cast<std::ptrdiff_t>(part));
}

// The calls abandoning a load whose parts are queued: the parts are
// cancelled and forgotten, the records released and both blocks freed.
std::vector<std::string> AbandonCalls(std::size_t parts) {
  std::vector<std::string> calls{"CancelAssets"};
  for (std::size_t ticket = 1; ticket <= parts; ++ticket) {
    calls.push_back("Forget:" + std::to_string(ticket));
  }
  for (const char* part : {"ReleasePart:0", "ReleasePart:1", "ReleasePart:2",
                           "ReleasePart:3", "FreeFile", "FreeModelBuffer"}) {
    calls.emplace_back(part);
  }
  return calls;
}

// A state whose ZUD read has completed.
score::EnemyPreviewState LoadedState(FakeHost& host, int zud_id) {
  score::EnemyPreviewState state;
  CHECK(score::QueueEnemyModelRead(host, state, zud_id));
  host.read_status = score::EnemyPreviewRead::kLoaded;
  host.calls.clear();
  return state;
}

// A preview whose first step has queued its ZUD read.
score::EnemyPreviewState ReadingState(FakeHost& host, int zud_id) {
  score::EnemyPreviewState state;
  REQUIRE(score::LoadEnemyPreview(host, state, zud_id) == 1);
  host.calls.clear();
  return state;
}

// A state whose parts have all loaded and whose file block is released.
score::EnemyPreviewState PosedState(FakeHost& host, int zud_id) {
  score::EnemyPreviewState state = LoadedState(host, zud_id);
  CHECK(score::LoadEnemyModelParts(host, state) == 1);
  host.LoadAll();
  CHECK(score::LoadEnemyModelParts(host, state) == 1);
  CHECK(score::LoadEnemyModelParts(host, state) == 0);
  host.calls.clear();
  return state;
}

// A state on the shared parts step whose five parts are queued and pending.
score::EnemyPreviewState QueuedState(FakeHost& host, int zud_id) {
  host.step = 1;
  score::EnemyPreviewState state = LoadedState(host, zud_id);
  CHECK(score::LoadEnemyPreview(host, state, zud_id) == 1);
  host.calls.clear();
  return state;
}

// A preview abandoned after its second part failed, whose queued parts the
// asset queue refuses to cancel.
score::EnemyPreviewState StuckState(FakeHost& host) {
  score::EnemyPreviewState state = QueuedState(host, 7);
  host.outcomes[Ticket(2)] = ActorAssetOutcome::kFailed;
  host.cancel_refused = true;
  CHECK(score::LoadEnemyPreview(host, state, 7) == score::kEnemyPreviewFailed);
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
  // Both blocks exist before the read starts writing the file block.
  CHECK(host.calls ==
        std::vector<std::string>{"AllocateRead", "AllocateFile:512",
                                 "AllocateEquippedBuffer", "EnqueueRead"});
}

TEST_CASE("Enemy preview parts wait for the read", "[encyclopedia-preview]") {
  FakeHost host;
  score::EnemyPreviewState state;
  host.step = 1;
  REQUIRE(score::QueueEnemyModelRead(host, state, 7));
  REQUIRE(score::LoadEnemyPreview(host, state, 7) == 1);
  CHECK(host.queued.empty());
  host.read_status = score::EnemyPreviewRead::kLoaded;
  REQUIRE(score::LoadEnemyPreview(host, state, 7) == 1);
  CHECK(host.queued.size() == 5);
  CHECK(state.read == AssetAddress::kNone);
  CHECK(state.tickets ==
        std::array{Ticket(1), Ticket(2), Ticket(3), Ticket(4), Ticket(5)});
}

TEST_CASE("Enemy preview keeps the file while its own parts are loading",
          "[encyclopedia-preview]") {
  FakeHost host;
  score::EnemyPreviewState state = QueuedState(host, 7);
  host.outcomes[Ticket(1)] = ActorAssetOutcome::kLoaded;
  REQUIRE(score::LoadEnemyPreview(host, state, 7) == 1);
  CHECK(state.file == kFile);
  CHECK(state.tickets[4] == Ticket(5));
  CHECK(host.calls == std::vector<std::string>{"UpdateAssetQueue"});
  CHECK(host.step == 1);
}

TEST_CASE("Enemy preview releases the file once its own parts have loaded",
          "[encyclopedia-preview]") {
  FakeHost host;
  score::EnemyPreviewState state = QueuedState(host, 7);
  host.LoadAll();
  REQUIRE(score::LoadEnemyPreview(host, state, 7) == 1);
  CHECK(state.file == AssetAddress::kNone);
  CHECK(host.calls == std::vector<std::string>{
                          "UpdateAssetQueue", "Forget:1", "Forget:2",
                          "Forget:3", "Forget:4", "Forget:5", "FreeFile"});
  CHECK(state.tickets == score::EnemyPreviewState{}.tickets);
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
  // Native (#48): the parts name no memory; their records bind into the
  // model buffer (EnemyPreviewRecordMemory).
  const ActorAssetRequest& body = host.queued[0];
  CHECK(body == ActorAssetRequest{.kind = 2,
                                  .target_slot = 1,
                                  .model_id = 13,
                                  .data = At(0x10030),
                                  .sub_kind = 0xFF,
                                  .material = 6,
                                  .variant = 1});
  CHECK(host.queued[1] == ActorAssetRequest{.kind = 4,
                                            .target_slot = 2,
                                            .model_id = 17,
                                            .data = At(0x10070),
                                            .sub_kind = 1,
                                            .texture_slot = 0xF0,
                                            .material = 4,
                                            .variant = 1});
  CHECK(host.queued[2] == ActorAssetRequest{.kind = 4,
                                            .target_slot = 3,
                                            .model_id = 27,
                                            .data = At(0x10090),
                                            .sub_kind = 1,
                                            .texture_slot = 0xF1,
                                            .material = 5,
                                            .variant = 1});
  CHECK(host.queued[3] == ActorAssetRequest{.kind = 8,
                                            .target_slot = 1,
                                            .model_id = 9,
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
  CHECK(host.queued[1] == ActorAssetRequest{.kind = 1,
                                            .target_slot = 16,
                                            .model_id = 27,
                                            .data = At(0x10030),
                                            .sub_kind = 1,
                                            .texture_slot = 0xFC,
                                            .material = 6,
                                            .variant = 0});
  CHECK(host.queued[2].destination == AssetAddress::kNone);
  CHECK(host.queued[2].variant == 0);
  CHECK(host.queued[3].texture_slot == 0xFC);
}

TEST_CASE("Preview records bind into the model buffer by part",
          "[encyclopedia-preview]") {
  using Table = VS::Content::ActorRecordTable;
  using PreviewPart = score::EnemyPreviewPart;
  FakeHost host;
  const score::EnemyPreviewState equipped = LoadedState(host, 7);
  CHECK(score::EnemyPreviewRecordMemory(host, equipped, Table::kActor, 1) ==
        Part(kEquippedBuffer, PreviewPart::kEnemy));
  CHECK(score::EnemyPreviewRecordMemory(host, equipped, Table::kWeapon, 2) ==
        Part(kEquippedBuffer, PreviewPart::kWeapon));
  CHECK(score::EnemyPreviewRecordMemory(host, equipped, Table::kWeapon, 3) ==
        Part(kEquippedBuffer, PreviewPart::kShield));
  // Records that are not the layout's parts bind nothing.
  CHECK(score::EnemyPreviewRecordMemory(host, equipped, Table::kActor, 16) ==
        AssetAddress::kNone);
  CHECK(score::EnemyPreviewRecordMemory(host, equipped, Table::kWeapon, 4) ==
        AssetAddress::kNone);
  const score::EnemyPreviewState companion = LoadedState(host, 31);
  CHECK(score::EnemyPreviewRecordMemory(host, companion, Table::kActor, 16) ==
        Part(kCompanionBuffer, PreviewPart::kCompanion));
  CHECK(score::EnemyPreviewRecordMemory(host, companion, Table::kWeapon, 2) ==
        AssetAddress::kNone);
  CHECK(score::EnemyPreviewRecordMemory(host, {}, Table::kActor, 1) ==
        AssetAddress::kNone);
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
  CHECK_FALSE(score::QueueEnemyModelRead(host, state, zud_id));
  CHECK(state == score::EnemyPreviewState{});
  CHECK(host.calls.empty());
  CHECK(host.refusals.size() == 1);
}

TEST_CASE("Enemy preview reads refuse while an earlier preview holds blocks",
          "[encyclopedia-preview]") {
  FakeHost host;
  score::EnemyPreviewState state;
  REQUIRE(score::QueueEnemyModelRead(host, state, 7));
  const score::EnemyPreviewState held = state;
  host.calls.clear();
  CHECK_FALSE(score::QueueEnemyModelRead(host, state, 8));
  CHECK(state == held);
  CHECK(host.calls.empty());
  CHECK(host.refusals.size() == 1);
}

TEST_CASE("Enemy preview reads fall back when the disc queue is full",
          "[encyclopedia-preview]") {
  FakeHost host;
  host.disc_queue_full = true;
  score::EnemyPreviewState state;
  REQUIRE(score::QueueEnemyModelRead(host, state, 7));
  CHECK(state.read == kRefusedRead);
  CHECK(host.Called("ReadRefused"));
  CHECK_FALSE(host.Called("EnqueueRead"));
  host.read_status = score::EnemyPreviewRead::kLoaded;
  CHECK(score::LoadEnemyModelParts(host, state) == 1);
  CHECK(host.Called("ReleaseRefusedRead"));
  CHECK(host.queued.size() == 5);
}

TEST_CASE("Enemy preview reads free both blocks when no read is recorded",
          "[encyclopedia-preview]") {
  FakeHost host;
  host.disc_queue_full = true;
  host.read_records_full = true;
  score::EnemyPreviewState state;
  CHECK_FALSE(score::QueueEnemyModelRead(host, state, 7));
  CHECK(state == score::EnemyPreviewState{});
  CHECK(host.calls ==
        std::vector<std::string>{"AllocateRead", "AllocateFile:512",
                                 "AllocateEquippedBuffer", "ReadRefused",
                                 "FreeModelBuffer", "FreeFile"});
  CHECK(host.refusals.size() == 1);
}

TEST_CASE("Enemy preview reads refuse an exhausted heap before reading",
          "[encyclopedia-preview]") {
  FakeHost host;
  const bool model_heap = GENERATE(false, true);
  if (model_heap) {
    host.model_heap_full = true;
  } else {
    host.heap_full = true;
  }
  score::EnemyPreviewState state;
  CHECK_FALSE(score::QueueEnemyModelRead(host, state, 7));
  CHECK(state == score::EnemyPreviewState{});
  // No read started, so neither block has a write in flight.
  CHECK_FALSE(host.Called("EnqueueRead"));
  CHECK(host.Called("ReleaseRead"));
  CHECK(host.Called("AllocateEquippedBuffer") == model_heap);
  CHECK(host.Called("FreeFile") == model_heap);
  CHECK(host.refusals.size() == 1);
  CHECK(score::LoadEnemyModelParts(host, state) == 0);
}

TEST_CASE("Enemy preview abandons short files and missing buffers",
          "[encyclopedia-preview]") {
  FakeHost host;
  const bool short_file = GENERATE(true, false);
  if (short_file) host.file.bytes = 0x2F;
  score::EnemyPreviewState state = LoadedState(host, 7);
  if (!short_file) state.model_buffer = AssetAddress::kNone;
  host.refusals.clear();
  CHECK(score::LoadEnemyModelParts(host, state) == score::kEnemyPreviewFailed);
  CHECK(host.queued.empty());
  CHECK(host.refusals.size() == 1);
  CHECK(state.read == AssetAddress::kNone);
  CHECK(state.file == AssetAddress::kNone);
  CHECK(state.model_buffer == AssetAddress::kNone);
  std::vector<std::string> expected{"ReleaseRead"};
  if (short_file) {
    for (const char* part :
         {"ReleasePart:0", "ReleasePart:1", "ReleasePart:2", "ReleasePart:3"}) {
      expected.emplace_back(part);
    }
  }
  expected.emplace_back("FreeFile");
  if (short_file) expected.emplace_back("FreeModelBuffer");
  CHECK(host.calls == expected);
}

TEST_CASE("Enemy preview abandons parts queued before a section past the file",
          "[encyclopedia-preview]") {
  struct Case {
    std::size_t word;
    std::uint32_t value;
    std::size_t queued;
  };
  const Case test = GENERATE(Case{0x14, 0x1000, 1},      // the weapon WEP
                             Case{0x28, 0xFFFFFFF0, 3},  // the battle SEQ
                             Case{0x0C, 0x1D1, 0});      // the enemy SHP
  FakeHost host;
  PutWord(host.bytes, test.word, test.value);
  score::EnemyPreviewState state = LoadedState(host, 7);
  CHECK(score::LoadEnemyModelParts(host, state) == score::kEnemyPreviewFailed);
  CHECK(host.refusals.size() == 1);
  CHECK(host.queued.size() == test.queued);
  std::vector<std::string> expected = AbandonCalls(test.queued);
  // With nothing queued there is nothing to cancel.
  if (test.queued == 0) expected.erase(expected.begin());
  expected.insert(expected.begin(), "ReleaseRead");
  CHECK(host.calls == expected);
  CHECK(host.outcomes.empty());
  CHECK(state.tickets == score::EnemyPreviewState{}.tickets);
  CHECK(state.file == AssetAddress::kNone);
  CHECK(state.model_buffer == AssetAddress::kNone);
}

TEST_CASE("A part the asset queue refuses abandons the parts before it",
          "[encyclopedia-preview]") {
  FakeHost host;
  host.asset_capacity = 2;
  score::EnemyPreviewState state = LoadedState(host, 7);
  CHECK(score::LoadEnemyModelParts(host, state) == score::kEnemyPreviewFailed);
  CHECK(host.queued.size() == 2);
  std::vector<std::string> expected = AbandonCalls(2);
  expected.insert(expected.begin(), "ReleaseRead");
  CHECK(host.calls == expected);
  CHECK(host.outcomes.empty());
  CHECK(host.refusals.size() == 1);
}

TEST_CASE("A failed ZUD read abandons the preview and allows a new load",
          "[encyclopedia-preview]") {
  FakeHost host;
  score::EnemyPreviewState state = ReadingState(host, 7);
  host.read_status = score::EnemyPreviewRead::kFailed;
  CHECK(score::LoadEnemyPreview(host, state, 7) == score::kEnemyPreviewFailed);
  CHECK(host.step == 0);
  CHECK(host.queued.empty());
  CHECK(host.calls == std::vector<std::string>{"ReleaseRead", "ReleasePart:0",
                                               "ReleasePart:1", "ReleasePart:2",
                                               "ReleasePart:3", "FreeFile",
                                               "FreeModelBuffer"});
  CHECK(HoldsNothing(state));
  host.read_status = score::EnemyPreviewRead::kPending;
  CHECK(score::LoadEnemyPreview(host, state, 8) == 1);
  CHECK(host.step == 1);
  CHECK(state.zud_id == 8);
  CHECK(state.read == kRead);
}

TEST_CASE("A failed, cancelled or lost part abandons the preview",
          "[encyclopedia-preview]") {
  const ActorAssetOutcome outcome =
      GENERATE(ActorAssetOutcome::kFailed, ActorAssetOutcome::kCancelled,
               ActorAssetOutcome::kUnknown);
  FakeHost host;
  score::EnemyPreviewState state = QueuedState(host, 7);
  host.outcomes[Ticket(1)] = ActorAssetOutcome::kLoaded;
  if (outcome == ActorAssetOutcome::kUnknown) {
    host.outcomes.erase(Ticket(3));
  } else {
    host.outcomes[Ticket(3)] = outcome;
  }
  CHECK(score::LoadEnemyPreview(host, state, 7) == score::kEnemyPreviewFailed);
  CHECK(host.step == 0);
  std::vector<std::string> expected = AbandonCalls(5);
  expected.insert(expected.begin(), "UpdateAssetQueue");
  CHECK(host.calls == expected);
  CHECK(host.outcomes.empty());
  CHECK(HoldsNothing(state));
}

TEST_CASE("An abandoned preview keeps its blocks while its parts stay queued",
          "[encyclopedia-preview]") {
  FakeHost host;
  score::EnemyPreviewState state = QueuedState(host, 7);
  host.outcomes[Ticket(2)] = ActorAssetOutcome::kFailed;
  host.cancel_refused = true;
  CHECK(score::LoadEnemyPreview(host, state, 7) == score::kEnemyPreviewFailed);
  CHECK(host.calls ==
        std::vector<std::string>{"UpdateAssetQueue", "CancelAssets"});
  CHECK(state.file == kFile);
  CHECK(state.model_buffer == kEquippedBuffer);
  CHECK(state.tickets[0] == Ticket(1));
}

TEST_CASE("A new load retries a refused cancellation and is refused with it",
          "[encyclopedia-preview]") {
  FakeHost host;
  score::EnemyPreviewState state = StuckState(host);
  const score::EnemyPreviewState stuck = state;
  CHECK(score::LoadEnemyPreview(host, state, 8) == score::kEnemyPreviewFailed);
  CHECK(std::ranges::count(host.calls, "CancelAssets") == 1);
  CHECK(host.step == 0);
  CHECK(state == stuck);
}

TEST_CASE("A new load abandons the old parts once they can be cancelled",
          "[encyclopedia-preview]") {
  FakeHost host;
  score::EnemyPreviewState state = StuckState(host);
  REQUIRE(score::LoadEnemyPreview(host, state, 8) ==
          score::kEnemyPreviewFailed);
  host.cancel_refused = false;
  host.calls.clear();
  host.read_status = score::EnemyPreviewRead::kPending;
  CHECK(score::LoadEnemyPreview(host, state, 8) == 1);
  std::vector<std::string> expected = AbandonCalls(5);
  for (const char* call : {"AllocateRead", "AllocateFile:512",
                           "AllocateEquippedBuffer", "EnqueueRead"}) {
    expected.emplace_back(call);
  }
  CHECK(host.calls == expected);
  CHECK(host.outcomes.empty());
  CHECK(host.step == 1);
  CHECK(state.zud_id == 8);
  CHECK(state.tickets == score::EnemyPreviewState{}.tickets);
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

TEST_CASE("Freeing the preview releases its own records and the buffer once",
          "[encyclopedia-preview]") {
  FakeHost host;
  score::EnemyPreviewState state = PosedState(host, 7);
  score::FreeEnemyPreview(host, state);
  CHECK(host.calls == std::vector<std::string>{"ReleasePart:0", "ReleasePart:1",
                                               "ReleasePart:2", "ReleasePart:3",
                                               "FreeModelBuffer"});
  CHECK(state.model_buffer == AssetAddress::kNone);
  host.calls.clear();
  score::FreeEnemyModelBuffer(host, state);
  score::FreeEnemyPreview(host, state);
  CHECK(host.calls.empty());
  CHECK(host.refusals.empty());
}

TEST_CASE("Freeing the preview keeps a buffer a record stays bound in",
          "[encyclopedia-preview]") {
  FakeHost host;
  score::EnemyPreviewState state = PosedState(host, 7);
  host.bound_parts = {score::EnemyPreviewPart::kWeapon};
  score::FreeEnemyPreview(host, state);
  CHECK(host.calls == std::vector<std::string>{"ReleasePart:0", "ReleasePart:1",
                                               "ReleasePart:2",
                                               "ReleasePart:3"});
  CHECK(state.model_buffer == kEquippedBuffer);
  CHECK(host.refusals.size() == 1);
}

TEST_CASE("Freeing a loading preview cancels its parts before its blocks",
          "[encyclopedia-preview]") {
  FakeHost host;
  host.step = 1;
  score::EnemyPreviewState state = LoadedState(host, 7);
  REQUIRE(score::LoadEnemyModelParts(host, state) == 1);
  host.calls.clear();
  score::FreeEnemyPreview(host, state);
  CHECK(host.calls == AbandonCalls(5));
  CHECK(host.outcomes.empty());
  CHECK(host.step == 0);
  CHECK(state.file == AssetAddress::kNone);
  CHECK(state.model_buffer == AssetAddress::kNone);
  CHECK(state.tickets == score::EnemyPreviewState{}.tickets);
}

TEST_CASE("Freeing the preview keeps blocks a read or queued part still uses",
          "[encyclopedia-preview]") {
  FakeHost host;
  const bool reading = GENERATE(true, false);
  score::EnemyPreviewState state = LoadedState(host, 7);
  if (!reading) {
    REQUIRE(score::LoadEnemyModelParts(host, state) == 1);
    host.cancel_refused = true;
  }
  host.calls.clear();
  host.refusals.clear();
  const score::EnemyPreviewState held = state;
  score::FreeEnemyPreview(host, state);
  CHECK(state == held);
  CHECK(host.refusals.size() == 1);
  CHECK(host.calls == (reading ? std::vector<std::string>{}
                               : std::vector<std::string>{"CancelAssets"}));
}

TEST_CASE("Switching enemies reads the next enemy after the release",
          "[encyclopedia-preview]") {
  FakeHost host;
  score::EnemyPreviewState state = PosedState(host, 31);
  score::FreeEnemyPreview(host, state);
  host.calls.clear();
  host.read_status = score::EnemyPreviewRead::kPending;
  REQUIRE(score::QueueEnemyModelRead(host, state, 7));
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
  std::array<VS::Content::DiscQueueSlot, 32> saved_slots{};
  VS::Content::DiscRequestCounts saved_counts = VS::Content::discRequestCounts;
  decltype(VS::Content::legacyActorAssetRequests) saved_requests{};
  std::uint8_t saved_step = VS::Content::objectDataLoadStep;
  score::EnemyPreviewState saved_state = score::LiveEnemyPreviewState();

  LivePreviewFixture() {
    std::memcpy(saved_slots.data(), VS::Content::discRequestSlots.data(),
                sizeof VS::Content::discRequestSlots);
    std::memset(VS::Content::discRequestSlots.data(), 0,
                sizeof VS::Content::discRequestSlots);
    VS::Content::discRequestCounts = VS::Content::DiscRequestCounts{};
    saved_requests = VS::Content::legacyActorAssetRequests;
    VS::Content::legacyActorAssetRequests.fill({});
    VS::Content::objectDataLoadStep = 0;
    score::ResetLiveEnemyPreview();
  }
  ~LivePreviewFixture() {
    // Completion entries outlive the queue; a test's pending tickets would
    // fill the table for the tests after it.
    VS::Content::ClearLiveActorAssetQueue();
    for (const ActorAssetTicket ticket :
         score::LiveEnemyPreviewState().tickets) {
      VS::Content::ForgetLiveActorAsset(ticket);
    }
    std::memcpy(VS::Content::discRequestSlots.data(), saved_slots.data(),
                sizeof VS::Content::discRequestSlots);
    VS::Content::discRequestCounts = saved_counts;
    VS::Content::legacyActorAssetRequests = saved_requests;
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

// Where the live preview's owned request binds `slot` of `table`.
const std::uint8_t* RecordMemory(VS::Content::ActorRecordTable table,
                                 int slot) {
  return Pointer<std::uint8_t>(score::LiveEnemyPreviewRecordMemory(
      VS::Content::kEnemyPreviewActorAssetOwner, table, slot));
}

// The companion record, or the enemy's weapon and shield, follow the body at
// the native record strides. The parts name no memory (#48).
void CheckAttachmentStrides(bool companion, const std::uint8_t* buffer) {
  using Table = VS::Content::ActorRecordTable;
  const VS::Content::LegacyActorAssetRequest& attachment =
      VS::Content::legacyActorAssetRequests[1];
  CHECK(attachment.destination == nullptr);
  CHECK(attachment.asset_kind == (companion ? 1 : 4));
  CHECK(attachment.target_slot == (companion ? 16 : 2));
  CHECK(RecordMemory(companion ? Table::kActor : Table::kWeapon,
                     attachment.target_slot) ==
        buffer + sizeof(VS::Actors::ActorRecord));
  if (!companion) {
    const VS::Content::LegacyActorAssetRequest& shield =
        VS::Content::legacyActorAssetRequests[2];
    CHECK(shield.destination == nullptr);
    CHECK(shield.target_slot == 3);
    CHECK(RecordMemory(Table::kWeapon, 3) ==
          buffer + sizeof(VS::Actors::ActorRecord) +
              sizeof(VS::Actors::WeaponModel));
  }
}

// Both of a live preview's heap blocks have been returned.
void CheckLiveBlocksFreed(const score::EnemyPreviewState& before) {
  CHECK_FALSE(
      VS::Core::Memory::LivePayloadCapacity(Pointer(before.file)).has_value());
  CHECK_FALSE(
      VS::Core::Memory::LivePayloadCapacity(Pointer(before.model_buffer))
          .has_value());
}

void CheckColdPreviewRelease(const score::EnemyPreviewState& before,
                             std::uint64_t expected_read_generation = 0) {
  CHECK(score::LiveEnemyPreviewState() == score::EnemyPreviewState{});
  CheckLiveBlocksFreed(before);
  CHECK(VS::Content::LiveDiscRequestGeneration(
            Pointer<VS::Content::DiscQueueSlot>(before.read)) ==
        expected_read_generation);
  CHECK(VS::Content::objectDataLoadStep == 0);
}

// A live preview abandoned during its ZUD read holds nothing, has released
// the read's queue slot and both blocks, and has reset the shared step.
void CheckLiveReadAbandoned(const score::EnemyPreviewState& before) {
  CHECK(HoldsNothing(score::LiveEnemyPreviewState()));
  CHECK(VS::Content::LiveDiscRequestGeneration(
            Pointer<VS::Content::DiscQueueSlot>(before.read)) == 0);
  CheckLiveBlocksFreed(before);
  CHECK(VS::Content::objectDataLoadStep == 0);
}

// Whether the live asset queue has forgotten every one of the tickets.
bool LiveTicketsForgotten(
    const std::array<ActorAssetTicket, score::kEnemyPreviewParts>& tickets) {
  return std::ranges::all_of(tickets, [](ActorAssetTicket ticket) {
    return VS::Content::LiveActorAssetCompletion(ticket).outcome ==
           ActorAssetOutcome::kUnknown;
  });
}

void DiscardUnparsedPreview(const score::EnemyPreviewState& before,
                            std::uint64_t generation) {
  auto* file = Pointer(before.file);
  const bool owns_file =
      VS::Core::Memory::LivePayloadCapacity(file).has_value();
  if (owns_file) {
    REQUIRE(VS::Content::CancelLiveDiscWrites({file, before.file_bytes}));
  }
  auto* request = Pointer<VS::Content::DiscQueueSlot>(before.read);
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
      VS::Actors::actorRecords, VS::Actors::weaponModels,
      VS::Content::legacyModelLoadPhase, VS::Content::modelParseResetFlag,
      VS::Content::modelParseBoundRecord, VS::Content::legacySequenceParsePhase,
      VS::Content::legacyShpFileBuffer, VS::Content::legacyShpAllocated,
      VS::Content::legacyShpFileLocation, VS::Content::legacyShpDiscRequest,
      VS::Content::legacyShpParseCursor, VS::Content::legacyShpCopiedWordCount,
      VS::Content::legacyShpHeaderWord, VS::Content::legacyShpTextureWidth,
      VS::Content::legacyShpTextureHeight, VS::Content::legacyWeaponParseCursor,
      VS::Content::legacySequenceParseData, VS::Content::legacyShpDiscOffsets,
      VS::Content::legacyShpSectorCounts, VS::Content::legacyWepDiscOffsets,
      VS::Content::legacyWepSectorCounts>
      saved_loader;
  equipment_detail_tests::ValueRestore<int> saved_zud_size{score::zudSizes[7]};
  std::vector<ActorAssetTicket> extra_tickets;

  PreviewLifecycleFixture() {
    std::ranges::fill(VS::Actors::actorRecords, nullptr);
    std::ranges::fill(VS::Actors::weaponModels, nullptr);
    VS::Content::legacyModelLoadPhase = 0;
    VS::Content::modelParseResetFlag = 0;
    VS::Content::modelParseBoundRecord = false;
    VS::Content::legacySequenceParsePhase = 0;
    VS::Content::legacyShpAllocated = 0;
    VS::Content::legacyShpFileBuffer = nullptr;
    VS::Content::legacyShpParseCursor = nullptr;
    VS::Content::legacyWeaponParseCursor = nullptr;
    VS::Content::legacySequenceParseData = nullptr;
  }
  ~PreviewLifecycleFixture() {
    CHECK(score::CleanupLiveEnemyPreviewOwnedState());
    for (const ActorAssetTicket ticket : extra_tickets) {
      VS::Content::ForgetLiveActorAsset(ticket);
    }
  }
  ActorAssetTicket Submit(const ActorAssetRequest& request) {
    const auto ticket = VS::Content::SubmitLiveActorAsset(
        request, VS::Content::kEnemyPreviewActorAssetOwner,
        score::LiveEnemyPreviewRecordMemory);
    REQUIRE(ticket.has_value());
    if (!ticket) throw std::runtime_error("preview request was refused");
    extra_tickets.push_back(*ticket);
    return *ticket;
  }
};

score::EnemyPreviewState QueuePreviewParts(bool companion,
                                           bool modelless = false) {
  REQUIRE(score::LoadLiveEnemyPreview(companion ? 31 : 7) == 1);
  const auto before = score::LiveEnemyPreviewState();
  auto bytes = ZudFile();
  if (modelless) bytes[0] = std::byte{0x7F};
  std::ranges::copy(bytes, Pointer(before.file));
  auto* request = Pointer<VS::Content::DiscQueueSlot>(before.read);
  request->state = VS::Content::DiscRequestState::kLoaded;
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
  Pointer<VS::Content::DiscQueueSlot>(before.read)->state =
      VS::Content::DiscRequestState::kLoaded;
  REQUIRE(score::LoadLiveEnemyModelParts() == 1);
  return before;
}

// The model loader's phase once its file has loaded and its parse is pending.
constexpr std::uint8_t kModelParsePending = 2;

// Start a real converted read, without opening the store during the fixture's
// separately injected ZUD read.
int ReadQueuedConvertedModel(std::uint16_t model) {
  constexpr std::uint32_t kFirstShpSector = 95000;
  game_clear_live_tests::SavedGlobals<VS::Actors::animationEventLengths>
      saved_lengths;
  std::ranges::copy(
      VS::test_support::ExtractedEventLengths(R"(Native\data\disc\OBJ)"),
      VS::Actors::animationEventLengths.begin());
  VS::Content::DataStore::ScopedOverride restore;
  const auto opened = VS::Content::DataStore::Open(R"(Native\data)");
  INFO(VS::test_support::ErrorOf(opened));
  REQUIRE(opened.has_value());
  const auto* store = VS::Content::DataStore::Get();
  if (store == nullptr) throw std::runtime_error("no preview data store");
  const auto* file = store->Find(std::format("OBJ/{:02X}.SHP", model));
  if (file == nullptr) throw std::runtime_error("no preview model file");
  REQUIRE(file->SectorCount() > 0);
  REQUIRE(file->SectorCount() <= 255);
  REQUIRE(file->lba >= kFirstShpSector);
  REQUIRE(file->lba - kFirstShpSector <= 65535);
  VS::Content::legacyShpSectorCounts[model] =
      static_cast<std::uint8_t>(file->SectorCount());
  VS::Content::legacyShpDiscOffsets[model] =
      static_cast<std::uint16_t>(file->lba - kFirstShpSector);
  return VS::Content::UpdateLiveActorAssetQueue();
}

void CheckFailedReadThenNext(ActorAssetTicket failed, ActorAssetTicket next) {
  CHECK(VS::Content::UpdateLiveActorAssetQueue() == -1);
  CHECK(VS::Content::LiveActorAssetCompletion(failed).outcome ==
        ActorAssetOutcome::kFailed);
  CHECK(VS::Actors::weaponModels[2] == nullptr);
  CHECK(VS::Actors::actorRecords[1] == nullptr);
  CHECK(VS::Content::LiveActorAssetCompletion(next).outcome ==
        ActorAssetOutcome::kPending);
  CHECK(VS::Content::UpdateLiveActorAssetQueue() == 1);
  CHECK(VS::Content::LiveActorAssetCompletion(next).outcome ==
        ActorAssetOutcome::kLoaded);
}

// The companion's converted model loads at once (#203), so the update that
// starts its read leaves the file held, the parse pending and no disc request.
score::EnemyPreviewState StartCompanionRead() {
  const auto before = QueuePreviewParts(true, true);
  const auto model = static_cast<std::uint16_t>(
      VS::Content::legacyActorAssetRequests[1].model_id);
  REQUIRE(model < std::size(VS::Content::legacyShpDiscOffsets));
  if (model >= std::size(VS::Content::legacyShpDiscOffsets)) {
    throw std::runtime_error("companion model is outside its file table");
  }
  REQUIRE(VS::Content::UpdateLiveActorAssetQueue() == 1);
  REQUIRE(VS::Content::legacyActorAssetRequests[0].target_slot == 16);
  REQUIRE(ReadQueuedConvertedModel(model) == 1);
  REQUIRE(VS::Content::legacyModelLoadPhase == kModelParsePending);
  REQUIRE(VS::Content::legacyShpAllocated == 1);
  REQUIRE(VS::Content::legacyShpDiscRequest == nullptr);
  return before;
}

void CheckEmptyPreviewQueue() {
  CHECK(std::ranges::all_of(
      VS::Content::legacyActorAssetRequests,
      [](const auto& item) { return item.asset_kind == 0; }));
  CHECK(VS::Content::modelParseResetFlag == 0);
  CHECK(VS::Content::legacyShpParseCursor == nullptr);
  CHECK(VS::Content::legacySequenceParseData == nullptr);
}

std::span<std::byte> StartForeignModelLoad() {
  auto* foreign =
      static_cast<std::byte*>(VS::Core::Memory::AllocateLiveFromLowEnd(
          sizeof(VS::Actors::ActorRecord)));
  REQUIRE(foreign != nullptr);
  ActorAssetRequest request;
  request.kind =
      static_cast<std::uint8_t>(VS::Content::ActorAssetKind::kActorModelLoad);
  request.model_id = 20;
  request.destination = At(reinterpret_cast<std::uintptr_t>(foreign));
  REQUIRE(VS::Content::QueueLiveActorAsset(request) == 0);
  REQUIRE(ReadQueuedConvertedModel(20) == 1);
  return {foreign, sizeof(VS::Actors::ActorRecord)};
}

void CheckForeignModelLoad(const void* file) {
  CHECK(VS::Content::legacyModelLoadPhase == kModelParsePending);
  CHECK(VS::Content::legacyShpFileBuffer == file);
  CHECK(VS::Content::FindLiveFreeActorAssetRequest() == 1);
  CHECK(VS::Core::Memory::LivePayloadCapacity(file).has_value());
}

const void* BeginPreviewParser() {
  REQUIRE(VS::Content::UpdateLiveActorAssetQueue() == 1);
  REQUIRE(VS::Content::modelParseResetFlag == 1);
  auto* actor = VS::Actors::actorRecords[1];
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
  // Every part loaded: the tickets are forgotten and the file freed.
  const score::EnemyPreviewState& state = score::LiveEnemyPreviewState();
  CHECK(state.file == AssetAddress::kNone);
  CHECK(state.tickets == score::EnemyPreviewState{}.tickets);
}

std::span<std::byte> BindForeignPreviewActor() {
  auto* record =
      static_cast<std::byte*>(VS::Core::Memory::AllocateLiveFromLowEnd(
          sizeof(VS::Actors::ActorRecord)));
  REQUIRE(record != nullptr);
  ActorAssetRequest request;
  request.kind =
      static_cast<std::uint8_t>(VS::Content::ActorAssetKind::kActorModelParse);
  request.target_slot = 1;
  request.model_id = 0x7F;
  request.destination = At(reinterpret_cast<std::uintptr_t>(record));
  REQUIRE(VS::Content::QueueLiveActorAsset(request) == 0);
  REQUIRE(VS::Content::UpdateLiveActorAssetQueue() == 1);
  REQUIRE(static_cast<void*>(VS::Actors::actorRecords[1]) == record);
  return {record, sizeof(VS::Actors::ActorRecord)};
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
  auto* read = Pointer<VS::Content::DiscQueueSlot>(state.read);
  CHECK(read->destination == Pointer(state.file));
  std::ranges::copy(ZudFile(), Pointer(state.file));
  read->state = VS::Content::DiscRequestState::kLoaded;
  REQUIRE(score::LoadLiveEnemyModelParts() == 1);
  CHECK(read->state == VS::Content::DiscRequestState::kFree);
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
  Pointer<VS::Content::DiscQueueSlot>(state.read)->state =
      VS::Content::DiscRequestState::kLoaded;
  auto* buffer = Pointer<std::uint8_t>(state.model_buffer);
  auto* file = Pointer<std::uint8_t>(state.file);
  REQUIRE(score::LoadLiveEnemyModelParts() == 1);
  const VS::Content::LegacyActorAssetRequest& body =
      VS::Content::legacyActorAssetRequests[0];
  CHECK(body.destination == nullptr);
  CHECK(RecordMemory(VS::Content::ActorRecordTable::kActor, 1) == buffer);
  // Another owner's records do not bind into the preview's buffer.
  CHECK(score::LiveEnemyPreviewRecordMemory(
            {.value = 0x101}, VS::Content::ActorRecordTable::kActor, 1) ==
        AssetAddress::kNone);
  CHECK(body.data_address == reinterpret_cast<std::uintptr_t>(file + 0x30));
  CHECK(body.placement == 0);
  CheckAttachmentStrides(companion, buffer);
}

TEST_CASE("Live enemy preview abandons parts a queue reset drops",
          "[encyclopedia-preview][encyclopedia-preview-live]") {
  LivePreviewFixture fixture;
  REQUIRE(score::QueueLiveEnemyModelRead(7));
  score::EnemyPreviewState& state = score::LiveEnemyPreviewState();
  const score::EnemyPreviewState before = state;
  Pointer<VS::Content::DiscQueueSlot>(state.read)->state =
      VS::Content::DiscRequestState::kLoaded;
  std::ranges::copy(ZudFile(), Pointer(state.file));
  REQUIRE(score::LoadLiveEnemyModelParts() == 1);
  const auto tickets = state.tickets;
  REQUIRE(tickets[4] != ActorAssetTicket::kNone);
  // The battle start-up's reset drops every queued request; the preview
  // must not wait on, or free its blocks under, parts that never complete.
  VS::Content::ClearLiveActorAssetQueue();
  const std::uint64_t refusals = score::LiveEnemyPreviewRefusals();
  CHECK(score::LoadLiveEnemyModelParts() == score::kEnemyPreviewFailed);
  CHECK(score::LiveEnemyPreviewRefusals() > refusals);
  CHECK(HoldsNothing(state));
  CheckLiveBlocksFreed(before);
  CHECK(LiveTicketsForgotten(tickets));
  CHECK(score::LoadLiveEnemyModelParts() == 0);
}

TEST_CASE("Live enemy preview abandons a failed ZUD read and loads again",
          "[encyclopedia-preview][encyclopedia-preview-live]") {
  LivePreviewFixture fixture;
  REQUIRE(score::LoadLiveEnemyPreview(7) == 1);
  const score::EnemyPreviewState before = score::LiveEnemyPreviewState();
  Pointer<VS::Content::DiscQueueSlot>(before.read)->state =
      VS::Content::DiscRequestState::kError;
  CHECK(score::LoadLiveEnemyPreview(7) == score::kEnemyPreviewFailed);
  CheckLiveReadAbandoned(before);
  REQUIRE(score::LoadLiveEnemyPreview(8) == 1);
  const score::EnemyPreviewState& state = score::LiveEnemyPreviewState();
  CHECK(VS::Content::objectDataLoadStep == 1);
  CHECK(state.zud_id == 8);
  CHECK(state.read != AssetAddress::kNone);
}

TEST_CASE("Live enemy preview refuses ids past the file table",
          "[encyclopedia-preview][encyclopedia-preview-live]") {
  LivePreviewFixture fixture;
  const std::uint64_t before = score::LiveEnemyPreviewRefusals();
  CHECK_FALSE(score::QueueLiveEnemyModelRead(256));
  CHECK(score::LiveEnemyPreviewRefusals() == before + 1);
  CHECK(score::LiveEnemyPreviewState() == score::EnemyPreviewState{});
  CHECK(
      std::ranges::all_of(VS::Content::discRequestSlots, [](const auto& slot) {
        return slot.state == VS::Content::DiscRequestState::kFree;
      }));
}

TEST_CASE("Actual cold session drains the pending enemy preview owner",
          "[encyclopedia-preview][gazette-preview-lifecycle]") {
  const game_clear_live_tests::SavedModuleData saved_module;
  const game_clear_live_tests::SavedGlobals<
      VS::Menu::textBoxSelector, VS::Battle::stringContext,
      VS::Event::isTextBoxAwaitingClose, VS::Menu::sceneNotificationBuffer,
      VS::Menu::sceneNotificationTimer, VS::Menu::sceneNotificationTimed>
      saved_ui;
  LivePreviewFixture fixture;
  REQUIRE(score::LoadLiveEnemyPreview(7) == 1);
  const auto before = score::LiveEnemyPreviewState();
  REQUIRE(before.read != AssetAddress::kNone);
  const auto generation = VS::Content::LiveDiscRequestGeneration(
      Pointer<VS::Content::DiscQueueSlot>(before.read));
  const VS::Content::DiscFile foreign_file{1, 16};
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
    Pointer<VS::Content::DiscQueueSlot>(before.read)->state =
        VS::Content::DiscRequestState::kLoaded;
  }
  {
    menu_overlay_tests::Load other(
        3, {Locate("MENU/MENU2.PRG").lba, Locate("MENU/MENU2.PRG").size});
    REQUIRE(other.Complete() == 1);
    CHECK(score::LiveEnemyPreviewState() == before);
  }
  {
    menu_overlay_tests::Load menu(
        10, {Locate("MENU/MENU9.PRG").lba, Locate("MENU/MENU9.PRG").size});
    REQUIRE(menu.Poll() == 0);
    CHECK(score::LiveEnemyPreviewState() == before);
    REQUIRE(menu.Complete() == 1);
  }
  CheckColdPreviewRelease(before);
}

TEST_CASE("Actual heap replacement cancels queued preview parts",
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

TEST_CASE("Preview cleanup preserves a foreign pending model load",
          "[encyclopedia-preview][gazette-preview-lifecycle]") {
  PreviewLifecycleFixture fixture;
  const auto foreign = StartForeignModelLoad();
  const auto* file = VS::Content::legacyShpFileBuffer;
  const auto before = QueuePreviewParts(false);
  REQUIRE(score::CleanupLiveEnemyPreviewOwnedState());
  CheckColdPreviewRelease(before);
  CheckForeignModelLoad(file);
  // The foreign request is unowned, so nothing cancels it but a reset; the
  // fixture restores the loader state it leaves.
  VS::Content::ClearLiveActorAssetQueue();
  VS::Core::Memory::FreeLiveFromLowEnd(foreign.data());
}

TEST_CASE("Preview cleanup frees the companion's pending model file",
          "[encyclopedia-preview][gazette-preview-lifecycle]") {
  PreviewLifecycleFixture fixture;
  const auto before = StartCompanionRead();
  const auto* file = VS::Content::legacyShpFileBuffer;
  REQUIRE(score::CleanupLiveEnemyPreviewOwnedState());
  CheckColdPreviewRelease(before);
  CheckEmptyPreviewQueue();
  CHECK_FALSE(VS::Core::Memory::LivePayloadCapacity(file).has_value());
  CHECK(VS::Content::legacyModelLoadPhase == 0);
  CHECK(VS::Content::legacyShpAllocated == 0);
  CHECK(VS::Actors::actorRecords[1] == nullptr);
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
  CHECK(VS::Actors::actorRecords[1] == nullptr);
  CHECK_FALSE(VS::Core::Memory::LivePayloadCapacity(geometry).has_value());
  CHECK(VS::Content::LiveActorAssetRefusals() == refused);
}

TEST_CASE(
    "A queued model parser does not resume from module snapshots",
    "[encyclopedia-preview][gazette-preview-lifecycle][asset-ownership]") {
  const bool in_progress = GENERATE(false, true);
  PreviewLifecycleFixture fixture;
  const auto before = QueueOriginalPreview();
  const auto refused = VS::Content::LiveActorAssetRefusals();
  const void* geometry = in_progress ? BeginPreviewParser() : nullptr;
  VS::Content::modelParseResetFlag = in_progress ? 0 : 1;
  VS::Content::modelParseBoundRecord = !in_progress;
  VS::Content::legacyShpParseCursor = nullptr;
  FinishPreviewParts();
  const auto* actor = VS::Actors::actorRecords[1];
  REQUIRE(actor != nullptr);
  if (in_progress) CHECK(actor->instance.model == geometry);
  CHECK(VS::Content::LiveActorAssetRefusals() == refused);
  REQUIRE(score::CleanupLiveEnemyPreviewOwnedState());
  CheckColdPreviewRelease(before);
  CheckEmptyPreviewQueue();
}

TEST_CASE(
    "Cancelling a model parser ignores overwritten module snapshots",
    "[encyclopedia-preview][gazette-preview-lifecycle][asset-ownership]") {
  PreviewLifecycleFixture fixture;
  const auto before = QueueOriginalPreview();
  const void* geometry = BeginPreviewParser();
  const auto refused = VS::Content::LiveActorAssetRefusals();
  VS::Content::modelParseResetFlag = 0;
  VS::Content::modelParseBoundRecord = false;
  VS::Content::legacyShpParseCursor = nullptr;
  REQUIRE(score::CleanupLiveEnemyPreviewOwnedState());
  CheckColdPreviewRelease(before);
  CheckEmptyPreviewQueue();
  CHECK(VS::Actors::actorRecords[1] == nullptr);
  CHECK_FALSE(VS::Core::Memory::LivePayloadCapacity(geometry).has_value());
  CHECK(VS::Content::LiveActorAssetRefusals() == refused);
}

TEST_CASE("Preview cleanup preserves reused disc slots and empty foreign loads",
          "[encyclopedia-preview][gazette-preview-lifecycle]") {
  PreviewLifecycleFixture fixture;
  REQUIRE(score::LoadLiveEnemyPreview(7) == 1);
  const auto before = score::LiveEnemyPreviewState();
  auto* old = Pointer<VS::Content::DiscQueueSlot>(before.read);
  VS::Content::ReleaseLiveDiscRequest(old);
  const VS::Content::DiscFile location{1, 16};
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

TEST_CASE(
    "Converted read failures fail owned tickets without blocking the next",
    "[encyclopedia-preview][asset-read-failure]") {
  using VS::Content::ActorAssetKind;
  const bool weapon = GENERATE(false, true);
  PreviewLifecycleFixture fixture;
  const auto before = QueuePreviewParts(false, true);
  VS::Content::ClearLiveActorAssetQueue();
  VS::Content::legacyShpSectorCounts[1] = 1;
  VS::Content::legacyShpDiscOffsets[1] = 0;
  VS::Content::legacyWepSectorCounts[1] = 1;
  VS::Content::legacyWepDiscOffsets[1] = 0;
  VS::test_support::ModelRoot root(
      {{.path = weapon ? "OBJ/01.WEP" : "OBJ/01.SHP",
        .bytes = std::vector<std::byte>(2048)}},
      weapon ? 104024 : 95000);
  VS::Content::DataStore::ScopedOverride restore;
  REQUIRE(VS::Content::DataStore::Open(root.Root()).has_value());
  const auto failed =
      fixture.Submit({.kind = static_cast<std::uint8_t>(
                          weapon ? ActorAssetKind::kWeaponModelLoad
                                 : ActorAssetKind::kActorModelLoad),
                      .target_slot = static_cast<std::int8_t>(weapon ? 2 : 1),
                      .model_id = 1});
  const auto next = fixture.Submit(
      {.kind = static_cast<std::uint8_t>(ActorAssetKind::kActorModelParse),
       .target_slot = 1,
       .model_id = VS::Content::kModellessActorId});
  CheckFailedReadThenNext(failed, next);
  REQUIRE(score::CleanupLiveEnemyPreviewOwnedState());
  CheckColdPreviewRelease(before);
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
  CHECK(VS::Content::LiveDiscRequestGeneration(
            Pointer<VS::Content::DiscQueueSlot>(before.read)) ==
        before.read_generation);
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
  const auto tail = VS::Content::legacyActorAssetRequests[15];
  REQUIRE(score::CleanupLiveEnemyPreviewOwnedState());
  CheckColdPreviewRelease(before);
  CHECK(static_cast<void*>(VS::Actors::actorRecords[1]) == foreign.data());
  CHECK(VS::Core::Memory::LivePayloadCapacity(foreign.data()).has_value());
  CHECK(VS::Content::legacyActorAssetRequests[0].variant == first_foreign);
  CHECK(VS::Content::legacyActorAssetRequests[15].variant == tail.variant);
  CHECK(VS::Content::legacyActorAssetRequests[15].destination ==
        tail.destination);
  VS::Content::ClearLiveActorAssetQueue();
  CHECK(VS::Content::ReleaseLiveActorModel(1) == 0);
  VS::Core::Memory::FreeLiveFromLowEnd(foreign.data());
}

TEST_CASE("Preview cleanup of the companion's model leaves foreign reads",
          "[encyclopedia-preview][gazette-preview-lifecycle]") {
  PreviewLifecycleFixture fixture;
  const auto before = StartCompanionRead();
  const auto* file = VS::Content::legacyShpFileBuffer;
  const VS::Content::DiscFile location{1, 16};
  auto* foreign = VS::Content::AllocateLiveDiscRequest(&location);
  REQUIRE(foreign != nullptr);
  const auto generation = VS::Content::LiveDiscRequestGeneration(foreign);
  REQUIRE(score::CleanupLiveEnemyPreviewOwnedState());
  CheckColdPreviewRelease(
      before, Pointer<VS::Content::DiscQueueSlot>(before.read) == foreign
                  ? generation
                  : 0);
  CHECK(VS::Content::LiveDiscRequestGeneration(foreign) == generation);
  CHECK_FALSE(VS::Core::Memory::LivePayloadCapacity(file).has_value());
  CHECK(VS::Content::legacyShpAllocated == 0);
  VS::Content::ReleaseLiveDiscRequest(foreign);
}
