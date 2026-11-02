// Hand-maintained (tools/port/owned.csv).
// The disc drive, modernized: a read the request queue starts is served from
// the extracted game data on the next command update, with the phase
// transitions the queue and the blocking reader poll for. No drive, seek or
// sector callbacks remain; the live binding is DiscReaderLive.cpp.
#include "Content/DiscReader.hpp"

#include <cstdint>
#include <limits>
#include <optional>

namespace VS::Content {

std::optional<DiscReadPlan> PlanDiscRead(std::int32_t sector,
                                         std::uint32_t bytes) {
  if (bytes >
      static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max())) {
    return std::nullopt;
  }
  const std::uint32_t partial = bytes % kDiscDataSectorBytes != 0 ? 1U : 0U;
  return DiscReadPlan{
      .start_sector = sector,
      .byte_count = static_cast<std::int32_t>(bytes),
      .sector_count = (bytes / kDiscDataSectorBytes) + partial,
  };
}

// decomp: SLUS_010.40 _diskInitRead (src/SLUS_010.40/main.c:9140)
bool BeginDiscRead(DiscDriveHost& host, std::int32_t sector,
                   std::uint32_t bytes) {
  if (host.Phase() != DiscDrivePhase::kIdle) {
    return false;
  }
  const std::optional<DiscReadPlan> plan = PlanDiscRead(sector, bytes);
  if (!plan.has_value()) {
    return false;
  }
  host.ArmRead(*plan);
  host.SetPhase(DiscDrivePhase::kReadInit);
  return true;
}

namespace {

void FailCommand(DiscDriveHost& host) {
  host.CountFailure();
  host.SetPhase(DiscDrivePhase::kReadError);
}

}  // namespace

// decomp: SLUS_010.40 func_800443CC (src/SLUS_010.40/main.c:9164)
void UpdateDiscCommands(DiscDriveHost& host) {
  switch (host.Phase()) {
    case DiscDrivePhase::kReadInit: {
      const DiscReadPlan plan = host.PendingRead();
      if (plan.byte_count != 0 && !host.ReadPendingData()) {
        FailCommand(host);
        return;
      }
      host.MarkSectorsDelivered(plan.sector_count);
      host.SetPhase(DiscDrivePhase::kIdle);
      return;
    }
    case DiscDrivePhase::kReadXaInit:
      if (!host.StartXaStream(host.PendingXaRequest())) {
        FailCommand(host);
        return;
      }
      host.MarkXaStreamStarted();
      host.SetPhase(DiscDrivePhase::kStreamXa);
      return;
    case DiscDrivePhase::kStreamXa:
      switch (host.PollXaStream()) {
        case XaStreamStatus::kFailed:
          FailCommand(host);
          return;
        case XaStreamStatus::kFinished:
          host.StopXaStream();
          return;
        case XaStreamStatus::kPlaying:
          return;
      }
      return;
    case DiscDrivePhase::kStreamXaEnd:
      host.StopXaStream();
      return;
    default:
      return;
  }
}

// decomp: SLUS_010.40 _diskReset (src/SLUS_010.40/main.c:9385)
void ResetDiscDrive(DiscDriveHost& host) {
  host.StopXaStream();
  host.ClearXaMarkers();
  host.SetPhase(DiscDrivePhase::kIdle);
  host.UseDirectBuffer();
  host.WaitFrames(kDiscModeChangeFrames);
}

}  // namespace VS::Content
