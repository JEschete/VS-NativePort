#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "Content/AssetHandle.hpp"

// The runtime content contract (#47, plan §6.6/§9.4): stable asset ids and
// versioned catalogs that VS.Extract writes to <data>\catalogs\ from the
// user's disc. The game names files by id or by a typed catalog record; disc
// sectors stay in the provenance the importer recorded.
//
// catalogs\assets.json (schema "vs.assets", version 1):
//   {"schema": "vs.assets", "schema_version": 1, "assets": [
//     {"id": "asset.music.music000.dat", "file": "disc/MUSIC/MUSIC000.DAT",
//      "bytes": 12345,
//      "provenance": {"disc_path": "MUSIC/MUSIC000.DAT", "lba": 46000}}, ...]}
// catalogs\audio.json (schema "vs.audio", version 1): the original's music,
// sound-effect bank and wave bank file tables and the files _loadMenuSound
// reads, each entry naming an asset id:
//   {"schema": "vs.audio", "schema_version": 1,
//    "music": [{"legacyId": 0, "asset": "asset.music.music000.dat"}, ...200],
//    "sfx_banks": [...100], "wave_banks": [...100],
//    "menu": {"sample_banks": [id, id], "effect_sample_bank": id,
//             "effect_programs": id}}
//
// Loading accepts these versions only and rejects unknown keys, invalid or
// duplicate ids, unresolved references, paths outside the data root, missing
// files and size mismatches. Errors name the file, the JSON pointer and the
// rule. A loaded catalog is immutable.
namespace VS::Content {

inline constexpr std::string_view kCatalogDirectory = "catalogs";
inline constexpr std::string_view kAssetCatalogFile = "assets.json";
inline constexpr std::string_view kAudioCatalogFile = "audio.json";
inline constexpr std::string_view kAssetCatalogSchema = "vs.assets";
inline constexpr std::string_view kAudioCatalogSchema = "vs.audio";
inline constexpr int kAssetCatalogSchemaVersion = 1;
inline constexpr int kAudioCatalogSchemaVersion = 1;
// The original read whole 2048-byte sectors.
inline constexpr std::uint32_t kAssetSectorBytes = 2048;
// The original's audio file tables (_musicLBAs, _seLBAs, _soundLBAs).
inline constexpr std::size_t kMusicAssetCount = 200;
inline constexpr std::size_t kSfxBankAssetCount = 100;
inline constexpr std::size_t kWaveBankAssetCount = 100;

// "asset." followed by '.'-separated segments of [a-z0-9_-].
[[nodiscard]] bool IsValidAssetId(std::string_view id) noexcept;
// The id the importer gives a disc file: "asset." and the lowercase disc
// path with '/' as '.' ("MUSIC/MUSIC000.DAT" -> "asset.music.music000.dat").
// Nullopt when the path cannot form a valid id.
[[nodiscard]] std::optional<std::string> AssetIdForDiscPath(
    std::string_view disc_path);

struct AssetProvenance {
  std::string disc_path;
  std::uint32_t lba = 0;
};

struct AssetRecord {
  std::string id;
  // Relative to the data root, '/'-separated.
  std::string file;
  std::uint32_t bytes = 0;
  AssetProvenance provenance;

  // The bytes a load reads: whole sectors, the rest past `bytes` zero.
  [[nodiscard]] std::uint32_t LoadBytes() const noexcept;
};

// The files _loadMenuSound (main.c:10452) reads.
struct MenuSoundAssets {
  std::array<AssetHandle, 2> sample_banks{};
  AssetHandle effect_sample_bank{};
  AssetHandle effect_programs{};
};

// The audio file tables indexed by the original ids.
struct AudioCatalog {
  std::array<AssetHandle, kMusicAssetCount> music{};
  std::array<AssetHandle, kSfxBankAssetCount> sfx_banks{};
  std::array<AssetHandle, kWaveBankAssetCount> wave_banks{};
  MenuSoundAssets menu{};
};

class AssetCatalog {
 public:
  // Parses the two catalog texts; files resolve under `root`. `generation`
  // (nonzero) is stamped into every handle this catalog issues.
  [[nodiscard]] static std::expected<AssetCatalog, std::string> Parse(
      std::string_view assets_json, std::string_view audio_json,
      const std::filesystem::path& root, std::uint32_t generation);
  // Reads root\catalogs\assets.json and audio.json.
  [[nodiscard]] static std::expected<AssetCatalog, std::string> Load(
      const std::filesystem::path& root, std::uint32_t generation);

  [[nodiscard]] std::optional<AssetHandle> Find(std::string_view id) const;
  // Null for an empty handle or one this catalog did not issue.
  [[nodiscard]] const AssetRecord* Record(AssetHandle handle) const;
  [[nodiscard]] const AudioCatalog& Audio() const noexcept { return audio_; }
  [[nodiscard]] std::filesystem::path FileOf(const AssetRecord& record) const;
  [[nodiscard]] std::size_t Size() const noexcept { return records_.size(); }
  [[nodiscard]] std::uint32_t Generation() const noexcept {
    return generation_;
  }

 private:
  std::filesystem::path root_;
  std::uint32_t generation_ = 0;
  std::vector<AssetRecord> records_;
  // Record indexes ordered by id.
  std::vector<std::uint32_t> by_id_;
  AudioCatalog audio_;
};

}  // namespace VS::Content
