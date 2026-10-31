#include <Windows.h>
#include <bcrypt.h>

#include <array>
#include <format>
#include <memory>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "Core/Log.hpp"
#include "Launcher/Setup.hpp"
#include "Platform/Paths.hpp"

namespace VS::Launcher {
namespace {
std::string FileError(const std::filesystem::path& path) {
  return "Cannot read disc file " + Platform::PathText(path) + ": " +
         std::system_category().message(static_cast<int>(GetLastError())) +
         "\nClose programs that are modifying the image and try again.";
}

std::expected<std::unique_ptr<void, CloseDiscHandle>, std::string> Lock(
    const std::filesystem::path& path) {
  const HANDLE handle =
      CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                  OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
  if (handle == INVALID_HANDLE_VALUE) return std::unexpected(FileError(path));
  return std::unique_ptr<void, CloseDiscHandle>(handle);
}

struct CryptoHandles {
  BCRYPT_ALG_HANDLE algorithm = nullptr;
  BCRYPT_HASH_HANDLE hash = nullptr;
  ~CryptoHandles() {
    if (hash && BCryptDestroyHash(hash) < 0) {
      VS_LOG_WARN("launcher.verify", "Cannot release SHA-256 hash handle");
    }
    if (algorithm && BCryptCloseAlgorithmProvider(algorithm, 0) < 0) {
      VS_LOG_WARN("launcher.verify", "Cannot release SHA-256 provider");
    }
  }
};

std::expected<std::uint64_t, std::string> FileSize(
    HANDLE handle, const std::filesystem::path& path) {
  LARGE_INTEGER bytes{};
  if (!GetFileSizeEx(handle, &bytes) || bytes.QuadPart < 0) {
    return std::unexpected(FileError(path));
  }
  return static_cast<std::uint64_t>(bytes.QuadPart);
}

std::expected<std::string, std::string> Hash(HANDLE handle,
                                             const std::filesystem::path& path,
                                             std::uint64_t bytes,
                                             const HashProgress& progress) {
  CryptoHandles crypto;
  NTSTATUS status = BCryptOpenAlgorithmProvider(
      &crypto.algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0);
  if (status >= 0) {
    status = BCryptCreateHash(crypto.algorithm, &crypto.hash, nullptr, 0,
                              nullptr, 0, 0);
  }
  if (status < 0) {
    return std::unexpected(std::format("Cannot initialize SHA-256 (0x{:08X}).",
                                       static_cast<std::uint32_t>(status)));
  }
  constexpr std::size_t kChunkBytes = 1024 * 1024;
  std::vector<unsigned char> buffer(kChunkBytes);
  std::uint64_t total = 0;
  int previous_percent = 0;
  if (progress) progress(0);
  for (;;) {
    DWORD count = 0;
    if (!ReadFile(handle, buffer.data(), static_cast<DWORD>(buffer.size()),
                  &count, nullptr)) {
      return std::unexpected(FileError(path));
    }
    if (count == 0) break;
    status = BCryptHashData(crypto.hash, buffer.data(), count, 0);
    if (status < 0) {
      return std::unexpected(std::format("Cannot hash disc bytes (0x{:08X}).",
                                         static_cast<std::uint32_t>(status)));
    }
    total += count;
    const int percent =
        bytes == 0 ? 100
                   : static_cast<int>(static_cast<double>(total) / bytes * 100);
    if (progress && percent != previous_percent) {
      progress(percent);
      previous_percent = percent;
    }
  }
  if (total != bytes)
    return std::unexpected("The disc file changed while being read.");
  std::array<unsigned char, 32> digest{};
  status = BCryptFinishHash(crypto.hash, digest.data(),
                            static_cast<ULONG>(digest.size()), 0);
  if (status < 0) return std::unexpected("Cannot finish the SHA-256 checksum.");
  constexpr char kHex[] = "0123456789ABCDEF";
  std::string result;
  result.reserve(digest.size() * 2);
  for (const auto value : digest) {
    result.push_back(kHex[value >> 4]);
    result.push_back(kHex[value & 15]);
  }
  if (progress && previous_percent != 100) progress(100);
  return result;
}
}  // namespace

void CloseDiscHandle::operator()(void* handle) const {
  if (handle && !CloseHandle(handle)) {
    VS_LOG_WARN("launcher.verify", "Cannot close disc verification handle");
  }
}

std::expected<std::string, std::string> Sha256File(
    const std::filesystem::path& file, const HashProgress& progress) {
  auto handle = Lock(file);
  if (!handle) return std::unexpected(handle.error());
  const auto bytes = FileSize(handle->get(), file);
  if (!bytes) return std::unexpected(bytes.error());
  return Hash(handle->get(), file, *bytes, progress);
}

std::expected<VerifiedDisc, std::string> VerifyDisc(
    const std::filesystem::path& cue, const HashProgress& progress) {
  const auto image = ValidateCue(cue);
  if (!image) return std::unexpected(image.error());
  auto handle = Lock(*image);
  if (!handle) return std::unexpected(handle.error());
  const auto bytes = FileSize(handle->get(), *image);
  if (!bytes) return std::unexpected(bytes.error());
  if (*bytes != kSupportedDiscBytes) {
    return std::unexpected(std::format(
        "Wrong BIN size: {} bytes.\n"
        "This build requires the unmodified Vagrant Story USA (SLUS-01040) "
        "raw dump: exactly {} bytes.\n"
        "Renaming a different image will not make it compatible. "
        "No extraction or game launch was started.",
        *bytes, kSupportedDiscBytes));
  }
  const auto checksum = Hash(handle->get(), *image, *bytes, progress);
  if (!checksum) return std::unexpected(checksum.error());
  if (*checksum != kSupportedDiscSha256) {
    return std::unexpected(std::format(
        "Unsupported or modified BIN: checksum mismatch.\n"
        "Required: Vagrant Story USA (SLUS-01040), unmodified raw dump.\n"
        "Expected SHA-256: {}\n"
        "Selected SHA-256: {}\n"
        "Expected SHA-1: {}\n"
        "Use your own matching dump. No extraction or game launch was started.",
        kSupportedDiscSha256, *checksum, kSupportedDiscSha1));
  }
  return VerifiedDisc{*image, *checksum, std::move(*handle)};
}
}  // namespace VS::Launcher
