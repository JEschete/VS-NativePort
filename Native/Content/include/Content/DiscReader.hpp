#pragma once

#include <cstdint>
#include <optional>

namespace VS::Content {

// The drive phases the request queue, the blocking reader and the XA stream
// poll for (decomp vs_main_disk_t.state, SLUS_010.40 main.c:117).
enum class DiscDrivePhase : std::uint8_t {
  kIdle,
  kSeekReady,
  kReadReady,
  kReadError,
  kReadInit,
  kReadXaInit,
  kStreamXa,
  kCommandFailed,
  kStreamXaEnd,
  // A stored value no reader acts on.
  kOther,
};

inline constexpr std::uint32_t kDiscDataSectorBytes = 2048;
// The drive's mode change took three frames; the reset keeps that wait so
// start-up timing matches.
inline constexpr std::uint32_t kDiscModeChangeFrames = 3;

struct DiscReadPlan {
  std::int32_t start_sector = 0;
  std::int32_t byte_count = 0;
  std::uint32_t sector_count = 0;
};

// An XA request (vs_main_streamXa) stores the stream volume in the drive's
// byte-count word.
struct DiscXaRequest {
  std::int32_t start_sector = 0;
  std::int32_t volume = 0;
};

enum class XaStreamStatus : std::uint8_t { kPlaying, kFinished, kFailed };

// Whole data sectors covering the request; nullopt when the byte count does
// not fit the drive's signed byte-count word.
[[nodiscard]] std::optional<DiscReadPlan> PlanDiscRead(std::int32_t sector,
                                                       std::uint32_t bytes);

// Drive state and services the read control flow needs. The live binding
// keeps the shared drive record and the destination of the armed read.
class DiscDriveHost {
 public:
  DiscDriveHost() = default;
  DiscDriveHost(const DiscDriveHost&) = delete;
  DiscDriveHost& operator=(const DiscDriveHost&) = delete;
  DiscDriveHost(DiscDriveHost&&) = delete;
  DiscDriveHost& operator=(DiscDriveHost&&) = delete;
  virtual ~DiscDriveHost() = default;

  [[nodiscard]] virtual DiscDrivePhase Phase() const = 0;
  virtual void SetPhase(DiscDrivePhase phase) = 0;

  // Records the plan and clears the transfer progress and failure counters.
  virtual void ArmRead(const DiscReadPlan& plan) = 0;
  [[nodiscard]] virtual DiscReadPlan PendingRead() const = 0;
  // Fills the armed destination from the extracted game data.
  [[nodiscard]] virtual bool ReadPendingData() = 0;
  virtual void MarkSectorsDelivered(std::uint32_t sectors) = 0;
  virtual void CountFailure() = 0;

  [[nodiscard]] virtual DiscXaRequest PendingXaRequest() const = 0;
  [[nodiscard]] virtual bool StartXaStream(const DiscXaRequest& request) = 0;
  [[nodiscard]] virtual XaStreamStatus PollXaStream() = 0;
  virtual void StopXaStream() = 0;
  virtual void MarkXaStreamStarted() = 0;
  virtual void ClearXaMarkers() = 0;

  virtual void UseDirectBuffer() = 0;
  virtual void WaitFrames(std::uint32_t frames) = 0;
};

[[nodiscard]] bool BeginDiscRead(DiscDriveHost& host, std::int32_t sector,
                                 std::uint32_t bytes);
void UpdateDiscCommands(DiscDriveHost& host);
void ResetDiscDrive(DiscDriveHost& host);

}  // namespace VS::Content
