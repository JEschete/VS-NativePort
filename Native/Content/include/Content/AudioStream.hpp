#pragma once

#include <array>
#include <expected>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "Content/AudioBank.hpp"

namespace VS::Content {

inline constexpr u32 kXaSectorsPerSecond = 75;
inline constexpr usize kAudioStreamHeaderBytes = 64;

struct EmbeddedAudioStream {
  usize offset = 0;
  std::span<const u8> bytes;
};

struct AudioStreamInfo {
  std::string id;
  u32 source_pitch = 0;
  u32 sample_rate = 0;
  u32 channels = 0;
  u32 frames = 0;
  bool repeat = false;
  u32 loop_frame = 0;
  std::array<u8, kAudioStreamHeaderBytes> source_header{};
};

struct AudioStreamAsset {
  AudioStreamInfo info;
  std::vector<AudioSample> channels;
};

// Effect block 13: BATTLE/BATTLE.PRG/6F014.c:543,746.
std::expected<std::vector<EmbeddedAudioStream>, std::string>
FindEffectAudioStreams(std::span<const u8> package);

// Stream header/interleave: SLUS_010.40/25AC.c:6294-6499.
std::expected<AudioStreamInfo, std::string> InspectAudioStream(
    std::span<const u8> bytes);
std::expected<AudioStreamAsset, std::string> DecodeAudioStream(
    std::span<const u8> bytes);
std::expected<void, std::string> ExportAudioStream(
    const AudioStreamAsset& stream, const std::filesystem::path& data_root);
std::expected<void, std::string> ValidateAudioStreamAssets(
    const AudioStreamInfo& info, const std::filesystem::path& data_root);
std::filesystem::path AudioStreamInstrumentPath(
    const std::filesystem::path& data_root, const AudioStreamInfo& info,
    usize channel);
std::filesystem::path XaAudioPath(const std::filesystem::path& data_root,
                                  std::string_view source_path);
std::expected<std::vector<u8>, std::string> SelectXaAudioSectors(
    std::span<const u8> raw_sectors);

}  // namespace VS::Content
