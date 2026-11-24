#include "Platform/AtomicFile.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <format>
#include <fstream>
#include <limits>
#include <string>
#include <system_error>

#include "Platform/Paths.hpp"

namespace VS::Platform {
namespace {
std::string CleanTemporary(const std::filesystem::path& temporary,
                           std::string error) {
  std::error_code removed;
  std::filesystem::remove(temporary, removed);
  if (removed) {
    error +=
        "; cannot remove " + PathText(temporary) + ": " + removed.message();
  }
  return error;
}
}  // namespace

std::expected<void, std::string> WriteFileAtomically(
    const std::filesystem::path& path, std::string_view contents) {
  if (contents.size() > static_cast<std::uintmax_t>(
                            std::numeric_limits<std::streamsize>::max())) {
    return std::unexpected("file is too large to write: " + PathText(path));
  }
  static std::atomic<std::uint64_t> sequence{0};
  auto temporary = path;
  temporary += std::format(
      ".{}.{}.tmp", std::chrono::steady_clock::now().time_since_epoch().count(),
      sequence.fetch_add(1));
  std::ofstream output(temporary,
                       std::ios::binary | std::ios::out | std::ios::noreplace);
  if (!output) {
    return std::unexpected("cannot create exclusive temporary file " +
                           PathText(temporary));
  }
  output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
  output.close();
  if (!output) {
    return std::unexpected(
        CleanTemporary(temporary, "cannot write " + PathText(temporary)));
  }
  std::error_code error;
  std::filesystem::rename(temporary, path, error);
  if (error) {
    return std::unexpected(CleanTemporary(
        temporary,
        "cannot replace " + PathText(path) + ": " + error.message()));
  }
  return {};
}
}  // namespace VS::Platform
