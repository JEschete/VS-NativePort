#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Content/DiscReader.hpp"

namespace {
using VS::Content::DiscDrivePhase;
using VS::Content::DiscReadPlan;
using VS::Content::DiscXaRequest;
using VS::Content::XaStreamStatus;

class FakeDiscDrive final : public VS::Content::DiscDriveHost {
 public:
  [[nodiscard]] DiscDrivePhase Phase() const override { return phase; }
  void SetPhase(DiscDrivePhase next) override { phase = next; }

  void ArmRead(const DiscReadPlan& armed) override {
    plan = armed;
    events.emplace_back("arm");
  }
  [[nodiscard]] DiscReadPlan PendingRead() const override { return plan; }
  [[nodiscard]] bool ReadPendingData() override {
    events.emplace_back("read");
    return read_succeeds;
  }
  void MarkSectorsDelivered(std::uint32_t sectors) override {
    delivered = sectors;
  }
  void CountFailure() override { ++failures; }

  [[nodiscard]] DiscXaRequest PendingXaRequest() const override {
    return xa_request;
  }
  [[nodiscard]] bool StartXaStream(const DiscXaRequest& request) override {
    started = request;
    events.emplace_back("start xa");
    return xa_starts;
  }
  [[nodiscard]] XaStreamStatus PollXaStream() override { return xa_status; }
  void StopXaStream() override { events.emplace_back("stop xa"); }
  void MarkXaStreamStarted() override { events.emplace_back("xa markers 0"); }
  void ClearXaMarkers() override { events.emplace_back("xa markers clear"); }

  void UseDirectBuffer() override { events.emplace_back("direct buffer"); }
  void WaitFrames(std::uint32_t frames) override {
    events.push_back("wait " + std::to_string(frames));
  }

  DiscDrivePhase phase = DiscDrivePhase::kIdle;
  DiscReadPlan plan;
  bool read_succeeds = true;
  std::uint32_t delivered = 0;
  int failures = 0;
  DiscXaRequest xa_request;
  DiscXaRequest started;
  bool xa_starts = true;
  XaStreamStatus xa_status = XaStreamStatus::kPlaying;
  std::vector<std::string> events;
};

constexpr std::uint32_t kSector = 2048;
constexpr std::uint32_t kNoPlan = UINT32_MAX;

std::uint32_t SectorsFor(std::uint32_t bytes) {
  const std::optional<DiscReadPlan> plan = VS::Content::PlanDiscRead(9, bytes);
  return plan.has_value() ? plan->sector_count : kNoPlan;
}

void Arm(FakeDiscDrive& drive, std::uint32_t bytes, bool read_succeeds) {
  drive.read_succeeds = read_succeeds;
  const bool armed = VS::Content::BeginDiscRead(drive, 4, bytes);
  CHECK(armed);
}

}  // namespace

TEST_CASE("Disc read plans cover whole sectors within the signed byte count",
          "[content][disc][core]") {
  CHECK(SectorsFor(0) == 0);
  CHECK(SectorsFor(kSector) == 1);
  CHECK(SectorsFor(kSector + 1) == 2);
  CHECK(SectorsFor(INT32_MAX) == (INT32_MAX / kSector) + 1);
  CHECK(SectorsFor(1U + INT32_MAX) == kNoPlan);
  const DiscReadPlan widest =
      VS::Content::PlanDiscRead(9, INT32_MAX).value_or(DiscReadPlan{});
  CHECK(widest.start_sector == 9);
  CHECK(widest.byte_count == INT32_MAX);
}

TEST_CASE("Disc read is refused on a busy drive or an oversized request",
          "[content][disc][core]") {
  FakeDiscDrive drive;
  drive.phase = DiscDrivePhase::kReadError;
  CHECK_FALSE(VS::Content::BeginDiscRead(drive, 1, kSector));
  CHECK(drive.phase == DiscDrivePhase::kReadError);

  drive.phase = DiscDrivePhase::kIdle;
  CHECK_FALSE(VS::Content::BeginDiscRead(drive, 1, UINT32_MAX));
  CHECK(drive.phase == DiscDrivePhase::kIdle);
  CHECK(drive.events.empty());
}

TEST_CASE("Disc read on an idle drive arms the plan and waits for the update",
          "[content][disc][core]") {
  constexpr std::int32_t kStart = 23;
  FakeDiscDrive drive;
  REQUIRE(VS::Content::BeginDiscRead(drive, kStart, kSector + 1));
  CHECK(drive.phase == DiscDrivePhase::kReadInit);
  CHECK(drive.plan.start_sector == kStart);
  CHECK(drive.plan.sector_count == 2);
  CHECK(drive.events == std::vector<std::string>{"arm"});
}

TEST_CASE("Disc command update serves an armed read on the next update",
          "[content][disc][core]") {
  FakeDiscDrive drive;
  Arm(drive, 3 * kSector, true);
  VS::Content::UpdateDiscCommands(drive);
  CHECK(drive.phase == DiscDrivePhase::kIdle);
  CHECK(drive.delivered == 3);
  CHECK(drive.failures == 0);

  FakeDiscDrive empty;
  Arm(empty, 0, true);
  VS::Content::UpdateDiscCommands(empty);
  CHECK(empty.phase == DiscDrivePhase::kIdle);
  CHECK(empty.events == std::vector<std::string>{"arm"});
}

TEST_CASE("Disc command update counts a failed read without delivering",
          "[content][disc][core]") {
  FakeDiscDrive failing;
  Arm(failing, kSector, false);
  VS::Content::UpdateDiscCommands(failing);
  CHECK(failing.phase == DiscDrivePhase::kReadError);
  CHECK(failing.failures == 1);
  CHECK(failing.delivered == 0);
}

TEST_CASE("Disc command update starts an XA stream with its volume",
          "[content][disc][xa][core]") {
  constexpr std::int32_t kXaSector = 77;
  constexpr std::int32_t kVolume = 0x60;
  FakeDiscDrive drive;
  drive.phase = DiscDrivePhase::kReadXaInit;
  drive.xa_request = {.start_sector = kXaSector, .volume = kVolume};
  VS::Content::UpdateDiscCommands(drive);
  CHECK(drive.phase == DiscDrivePhase::kStreamXa);
  CHECK(drive.started.start_sector == kXaSector);
  CHECK(drive.started.volume == kVolume);
  CHECK(drive.events == std::vector<std::string>{"start xa", "xa markers 0"});
}

TEST_CASE("Disc command update polls a playing XA stream until it ends",
          "[content][disc][xa][core]") {
  FakeDiscDrive drive;
  drive.phase = DiscDrivePhase::kStreamXa;
  VS::Content::UpdateDiscCommands(drive);
  CHECK(drive.events.empty());
  drive.xa_status = XaStreamStatus::kFinished;
  VS::Content::UpdateDiscCommands(drive);
  CHECK(drive.events == std::vector<std::string>{"stop xa"});
  CHECK(drive.phase == DiscDrivePhase::kStreamXa);
  drive.xa_status = XaStreamStatus::kFailed;
  VS::Content::UpdateDiscCommands(drive);
  CHECK(drive.phase == DiscDrivePhase::kReadError);
  CHECK(drive.failures == 1);
}

TEST_CASE("Disc command update reports XA start failures and drains the end",
          "[content][disc][xa][core]") {
  FakeDiscDrive refused;
  refused.phase = DiscDrivePhase::kReadXaInit;
  refused.xa_starts = false;
  VS::Content::UpdateDiscCommands(refused);
  CHECK(refused.phase == DiscDrivePhase::kReadError);
  CHECK(refused.failures == 1);
  CHECK(refused.events == std::vector<std::string>{"start xa"});

  FakeDiscDrive ending;
  ending.phase = DiscDrivePhase::kStreamXaEnd;
  VS::Content::UpdateDiscCommands(ending);
  CHECK(ending.events == std::vector<std::string>{"stop xa"});
}

TEST_CASE("Disc command update ignores phases it does not serve",
          "[content][disc][core]") {
  FakeDiscDrive other;
  other.phase = DiscDrivePhase::kCommandFailed;
  VS::Content::UpdateDiscCommands(other);
  CHECK(other.events.empty());
  CHECK(other.phase == DiscDrivePhase::kCommandFailed);
}

TEST_CASE("Disc drive reset stops XA and idles before the mode-change wait",
          "[content][disc][core]") {
  FakeDiscDrive drive;
  drive.phase = DiscDrivePhase::kStreamXa;
  VS::Content::ResetDiscDrive(drive);
  CHECK(drive.phase == DiscDrivePhase::kIdle);
  CHECK(drive.events == std::vector<std::string>{"stop xa", "xa markers clear",
                                                 "direct buffer", "wait 3"});
}
