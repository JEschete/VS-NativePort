#pragma once

#include <array>
#include <cstdint>
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
// A stereo stream interleaves its channels in chunks of this many bytes.
inline constexpr usize kAudioStreamChunkBytes = 0x800;
// The highest stream SPU pitch; 4096 plays 44,100 frames a second.
inline constexpr u32 kAudioStreamMaximumPitch = 16384;

// The xa-flac converter (ASSET_PIPELINE.md §6.6, #504): an XA file's selected
// sectors become audio\streams\xa\<path>.flac, decoded losslessly. XA files
// are interleaved: files.tsv lists them as "stream" with nothing under disc\,
// and the manifest hashes their raw 2352-byte sectors.
inline constexpr std::string_view kXaFlacConverterName = "xa-flac";
inline constexpr std::uint32_t kXaFlacConverterVersion = 1;
inline constexpr std::string_view kXaExtension = ".XA";

struct EmbeddedAudioStream {
  usize offset = 0;
  std::span<const u8> bytes;
};

// What an effect stream's 64-byte AKAO header holds (Sound_Cutscene_StartStream
// reads words +16 to +32): every other header byte is zero, so
// AudioStreamHeaderOf rebuilds the header from these values.
struct AudioStreamInfo {
  std::string id;        // FNV-1a 64 of the whole stream block, header included
  u32 source_pitch = 0;  // SPU pitch at +28; 4096 plays 44,100 frames a second
  u32 sample_rate = 0;   // source_pitch in Hz, rounded: the WAV sample rate
  u32 channels = 0;      // +24 bit 0: stereo
  u32 frames = 0;        // per channel; +16 holds 16 bytes per 28 frames
  bool repeat = false;   // +24 bit 1
  u32 loop_frame = 0;    // +20 when repeating, as channel-interleaved bytes
  u32 start_page = 0;    // +32: the player stores it and never reads it

  bool operator==(const AudioStreamInfo&) const = default;
};

struct AudioStreamAsset {
  AudioStreamInfo info;
  std::vector<AudioSample> channels;
};

// Effect block 13: BATTLE/BATTLE.PRG/6F014.c:543,746.
std::expected<std::vector<EmbeddedAudioStream>, std::string>
FindEffectAudioStreams(std::span<const u8> package);

// The values of the AKAO stream header that starts `header`, without an id;
// the payload is not checked. A header byte the player does not read must be
// zero.
// decomp: Sound_Cutscene_StartStream (src/SLUS_010.40/25AC.c:6432)
std::expected<AudioStreamInfo, std::string> ReadAudioStreamHeader(
    std::span<const u8> header);
// The header ReadAudioStreamHeader reads as `info`.
std::array<u8, kAudioStreamHeaderBytes> AudioStreamHeaderOf(
    const AudioStreamInfo& info);
// The header values and id of the stream `bytes` holds, once its channel
// payload fits. Interleave: SLUS_010.40/25AC.c:6294-6499.
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
// The stream.json of the stream whose id is `id`, '/'-separated under the
// data root (Content/AudioStreamJson.hpp).
std::string AudioStreamFile(std::string_view id);
std::filesystem::path XaAudioPath(const std::filesystem::path& data_root,
                                  std::string_view source_path);
// XaAudioPath's file, '/'-separated under the data root.
std::string XaAudioFile(std::string_view source_path);
std::expected<std::vector<u8>, std::string> SelectXaAudioSectors(
    std::span<const u8> raw_sectors);

}  // namespace VS::Content
