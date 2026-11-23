// The sound-bank, XA and movie converters (ASSET_PIPELINE.md §6.5, #504):
// their manifest records, the runtime's check of them, and the extracted
// disc's outputs against independent decodes of their sources.
#include <dr_flac.h>
#include <mkvparser/mkvparser.h>
#include <mkvparser/mkvreader.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Content/AudioBank.hpp"
#include "Content/AudioStream.hpp"
#include "Content/ConversionManifest.hpp"
#include "Content/DataStore.hpp"
#include "Content/Disc.hpp"
#include "Content/MovieEncoder.hpp"
#include "Core/Sha256.hpp"
#include "Extract/AudioBankConversion.hpp"
#include "Extract/ConverterBatch.hpp"
#include "Extract/EsqConversion.hpp"
#include "Extract/EtmConversion.hpp"
#include "Extract/ImageConversion.hpp"
#include "Extract/ShpModelConversion.hpp"
#include "Extract/WepModelConversion.hpp"
#include "Extract/WorkshopRecipeConversion.hpp"
#include "Extract/ZudConversion.hpp"
#include "TestSupport/AudioConversionFixture.hpp"

namespace {
namespace fs = std::filesystem;
namespace Content = VS::Content;
namespace Extract = VS::Extract;
using Bytes = std::vector<std::byte>;

constexpr std::string_view kRaw = "raw";
constexpr std::string_view kBankPath = "SOUND/WAVE0001.DAT";
constexpr std::string_view kSecondBankPath = "SOUND/WAVE0002.DAT";
constexpr std::string_view kXaPath = "ENDING/ENDING.XA";
constexpr std::string_view kMoviePath = "MOV/TITLE.STR";
constexpr std::uint32_t kBankId = 5;

std::uint16_t Read16(std::span<const std::uint8_t> bytes, std::size_t offset) {
  return static_cast<std::uint16_t>(bytes[offset] | (bytes[offset + 1] << 8));
}

std::uint32_t Read32(std::span<const std::uint8_t> bytes, std::size_t offset) {
  return static_cast<std::uint32_t>(Read16(bytes, offset)) |
         (static_cast<std::uint32_t>(Read16(bytes, offset + 2)) << 16);
}

std::vector<std::uint8_t> Unsigned(std::span<const std::byte> bytes) {
  std::vector<std::uint8_t> out(bytes.size());
  std::ranges::transform(bytes, out.begin(), [](std::byte value) {
    return std::to_integer<std::uint8_t>(value);
  });
  return out;
}

void Put32(Bytes& bytes, std::size_t offset, std::uint32_t value) {
  for (std::size_t index = 0; index < sizeof(value); ++index) {
    bytes.at(offset + index) = static_cast<std::byte>(value >> (index * 8));
  }
}

// A one-instrument AKAO wave bank with id `id`, its sample two ADPCM blocks
// that loop from the second (AudioTests.cpp's BankBytes).
Bytes BankBytes(std::uint32_t id) {
  Bytes bytes(112);
  Put32(bytes, 0, 0x4F414B41);
  Put32(bytes, 4, id);
  Put32(bytes, 16, 0x3100);
  Put32(bytes, 20, 32);
  Put32(bytes, 28, 1);
  Put32(bytes, 68, 16);
  bytes.at(80) = std::byte{12};
  bytes.at(82) = std::byte{0x17};
  bytes.at(96) = std::byte{12};
  bytes.at(97) = std::byte{7};
  bytes.at(98) = std::byte{0xF8};
  return bytes;
}

// A files.tsv row. Raw rows' bytes are written under disc\; an interleaved
// row has only its size.
struct ListedFile {
  std::string_view path;
  std::string_view form;
  Bytes bytes;
  std::uint64_t size;
};

ListedFile RawFile(std::string_view path, Bytes bytes) {
  const std::uint64_t size = bytes.size();
  return {.path = path, .form = kRaw, .bytes = std::move(bytes), .size = size};
}

ListedFile InterleavedFile(std::string_view path, std::string_view form) {
  return {.path = path, .form = form, .bytes = {}, .size = 4096};
}

Content::ConversionManifest ManifestAt(const fs::path& root) {
  auto loaded = Content::LoadConversionManifest(root);
  INFO((loaded ? std::string{} : loaded.error()));
  if (!loaded || !*loaded) {
    FAIL("no manifest.json");
    return {};
  }
  return std::move(**loaded);
}

// A temporary data root.
class AudioRoot {
 public:
  AudioRoot()
      : root_(fs::temp_directory_path() /
              std::format(
                  "vs-audio-conversion-{}-{}",
                  std::chrono::steady_clock::now().time_since_epoch().count(),
                  sequence++)) {
    fs::create_directories(root_ / "disc");
  }
  // NOLINTNEXTLINE(bugprone-exception-escape): errors use the error_code API.
  ~AudioRoot() {
    std::error_code error;
    fs::remove_all(root_, error);
    if (error) {
      std::fprintf(stderr, "audio conversion fixture left files: %s\n",
                   error.message().c_str());
    }
  }
  AudioRoot(const AudioRoot&) = delete;
  AudioRoot& operator=(const AudioRoot&) = delete;
  AudioRoot(AudioRoot&&) = delete;
  AudioRoot& operator=(AudioRoot&&) = delete;

  [[nodiscard]] const fs::path& Root() const { return root_; }

  // Writes files.tsv listing `files`, and each raw file under disc\.
  void List(const std::vector<ListedFile>& files) const {
    std::ofstream manifest(root_ / Content::DataStore::ManifestName);
    manifest << "lba\tsize\tform\tpath\n";
    std::uint64_t lba = kFirstSector;
    for (const ListedFile& file : files) {
      manifest << lba << '\t' << file.size << '\t' << file.form << '\t'
               << file.path << '\n';
      lba += (file.size + Content::DataStore::kDataSectorBytes - 1) /
                 Content::DataStore::kDataSectorBytes +
             1;
      if (file.form == kRaw) {
        Write(std::format("disc/{}", file.path), file.bytes);
      }
    }
    REQUIRE(manifest.good());
  }

  // Writes `bytes` as the root's file `name`.
  void Write(std::string_view name, std::span<const std::byte> bytes) const {
    const fs::path path = root_ / fs::path(name);
    fs::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
    REQUIRE(output.good());
  }

  // Records the image, table, model, weapon, animation set and actor texture
  // converters, with nothing of theirs to convert here.
  void ConvertOthers() const {
    REQUIRE(Extract::ConvertDiscImages(root_, false).has_value());
    REQUIRE(Extract::ConvertDiscTables(root_, false).has_value());
    REQUIRE(Extract::ConvertDiscModels(root_, false).has_value());
    REQUIRE(Extract::ConvertDiscWeapons(root_, false).has_value());
    REQUIRE(Extract::ConvertDiscAnimationSets(root_, false).has_value());
    REQUIRE(Extract::ConvertDiscActorTextures(root_, false).has_value());
    REQUIRE(Extract::ConvertDiscArchetypes(root_, false).has_value());
  }

  // Why a store over the root refuses its conversions; empty if it does not.
  [[nodiscard]] std::string Refusal() const {
    Content::DataStore::ScopedOverride restore;
    const auto opened = Content::DataStore::Open(root_);
    INFO((opened ? std::string{} : opened.error()));
    REQUIRE(opened.has_value());
    const Content::DataStore* store = Content::DataStore::Get();
    if (store == nullptr) {
      FAIL("no store is open");
      return {};
    }
    const auto required = store->RequireConversions();
    return required ? std::string{} : required.error();
  }

  // Records `output` as `converter`'s only output.
  void Publish(std::string_view converter, std::uint32_t version,
               Content::ConversionOutput output) const {
    const Content::ConversionManifest previous = ManifestAt(root_);
    Extract::ConverterBatch batch =
        Extract::StartBatch(previous, converter, version, false);
    batch.outputs.push_back(std::move(output));
    const auto published =
        Extract::PublishBatches(root_, previous, std::span(&batch, 1));
    INFO((published ? std::string{} : published.error()));
    REQUIRE(published.has_value());
  }

 private:
  static constexpr std::uint64_t kFirstSector = 100;
  static inline unsigned sequence = 0;
  fs::path root_;
};

// An interleaved file's output listing `file`. Its source hash covers raw
// sectors a data root does not hold, so any hash stands for it.
Content::ConversionOutput InterleavedOutput(std::string id,
                                            std::string_view converter,
                                            std::string_view source,
                                            std::string file) {
  return {.id = std::move(id),
          .source = std::string(source),
          .source_sha256 = std::string(64, 'a'),
          .converter = std::string(converter),
          .files = {std::move(file)},
          .dependencies = {},
          .unreferenced = false,
          .note = {},
          .alias_of = {}};
}

bool Contains(std::string_view text, std::string_view part) {
  return text.find(part) != std::string_view::npos;
}

// Checks `root` records the wave-bank converter and the one bank `bank`
// with its three files.
void CheckBankOutput(const fs::path& root, const Bytes& bank) {
  const Content::ConversionManifest manifest = ManifestAt(root);
  const auto version =
      manifest.converters.find(Content::kWaveBankConverterName);
  REQUIRE(version != manifest.converters.end());
  CHECK(version->second == Content::kWaveBankConverterVersion);
  const Content::ConversionOutput* output =
      manifest.Find("audio.bank.wave0001");
  REQUIRE(output != nullptr);
  CHECK(output->source == kBankPath);
  CHECK(output->source_sha256 == VS::Core::Sha256Hex(bank));
  CHECK(output->files == std::vector<std::string>{"audio/banks/5/0.sfz",
                                                  "audio/banks/5/0.wav",
                                                  "audio/banks/5/bank.json"});
  for (const std::string& file : output->files) {
    CHECK(fs::is_regular_file(root / fs::path(file)));
  }
}

// Checks a second conversion keeps the bank and a forced one converts it
// again.
void CheckBankReuse(const fs::path& root) {
  const auto second = Extract::ConvertDiscBanks(root, false);
  REQUIRE(second.has_value());
  CHECK(second->converted == 0);
  CHECK(second->unchanged == 1);
  const auto forced = Extract::ConvertDiscBanks(root, true);
  REQUIRE(forced.has_value());
  CHECK(forced->converted == 1);
}

// Checks a store refuses `root` once its first bank, `bank`, changes, and
// accepts it again after the bank converts again.
void CheckChangedBank(const AudioRoot& root, Bytes bank) {
  bank.at(82) ^= std::byte{1};
  root.Write(std::format("disc/{}", kBankPath), bank);
  CHECK(Contains(root.Refusal(),
                 "stale wave-bank source hash for SOUND/WAVE0001.DAT"));
  const auto reconverted = Extract::ConvertDiscBanks(root.Root(), false);
  REQUIRE(reconverted.has_value());
  CHECK(reconverted->converted == 1);
  CHECK(root.Refusal().empty());
}

// Checks a one-byte bank file holding 1 rather than a placeholder's 0 needs
// a conversion, which its bytes cannot give.
void CheckNonzeroOneByteBank(const AudioRoot& root, const Bytes& bank) {
  root.List({RawFile(kBankPath, bank),
             RawFile(kSecondBankPath, Bytes{std::byte{1}})});
  CHECK(Contains(root.Refusal(), "no wave-bank output for SOUND/WAVE0002.DAT"));
  const auto failed = Extract::ConvertDiscBanks(root.Root(), false);
  REQUIRE_FALSE(failed.has_value());
  CHECK(Contains(failed.error(), "SOUND/WAVE0002.DAT"));
}

}  // namespace

TEST_CASE(
    "Sound-bank conversion lists each bank's files and keeps unchanged "
    "banks",
    "[content-audio]") {
  AudioRoot root;
  const Bytes bank = BankBytes(kBankId);
  root.List({RawFile(kBankPath, bank),
             RawFile(kSecondBankPath, Bytes{std::byte{0}})});
  const auto first = Extract::ConvertDiscBanks(root.Root(), false);
  INFO((first ? std::string{} : first.error()));
  REQUIRE(first.has_value());
  CHECK(first->converted == 1);
  CHECK(first->skipped == 1);
  CheckBankOutput(root.Root(), bank);
  CheckBankReuse(root.Root());
}

TEST_CASE("Data stores require each sound bank's current conversion",
          "[content-audio]") {
  AudioRoot root;
  const Bytes bank = BankBytes(kBankId);
  root.List({RawFile(kBankPath, bank),
             RawFile(kSecondBankPath, Bytes{std::byte{0}})});
  root.ConvertOthers();
  VS::test_support::RecordAudioConversions(root.Root());
  CHECK(root.Refusal().empty());

  SECTION("a changed bank") { CheckChangedBank(root, bank); }
  SECTION("a missing sample") {
    fs::remove(root.Root() / "audio" / "banks" / "5" / "0.wav");
    CHECK(Contains(root.Refusal(), "missing wave-bank output"));
  }
  SECTION("a one-byte bank that is not a placeholder") {
    CheckNonzeroOneByteBank(root, bank);
  }
}

TEST_CASE("Sound banks with one bank id are refused", "[content-audio]") {
  AudioRoot root;
  root.List({RawFile(kBankPath, BankBytes(kBankId)),
             RawFile(kSecondBankPath, BankBytes(kBankId))});
  const auto converted = Extract::ConvertDiscBanks(root.Root(), false);
  REQUIRE_FALSE(converted.has_value());
  CHECK(Contains(converted.error(), "both write audio/banks/5/"));
}

TEST_CASE(
    "Data roots from before the audio converters are refused with "
    "instructions",
    "[content-audio]") {
  AudioRoot root;
  root.List({RawFile(kBankPath, BankBytes(kBankId))});
  root.ConvertOthers();
  const std::string refusal = root.Refusal();
  CHECK(Contains(refusal, "the converted data has no wave-bank output"));
  CHECK(Contains(refusal, "run VS.Extract extract"));
}

TEST_CASE(
    "Data stores require XA and movie outputs without rehashing their "
    "sources",
    "[content-audio][content-movie]") {
  AudioRoot root;
  root.List({InterleavedFile(kXaPath, "stream"),
             InterleavedFile(kMoviePath, Content::kMovieForm)});
  root.ConvertOthers();
  VS::test_support::RecordAudioConversions(root.Root());
  CHECK(Contains(root.Refusal(), "no xa-flac output for ENDING/ENDING.XA"));

  const std::string flac = Content::XaAudioFile(kXaPath);
  const std::string mkv =
      std::format("disc/{}.{}", kMoviePath, Content::kMovieForm);
  root.Write(flac, Bytes{std::byte{1}});
  root.Write(mkv, Bytes{std::byte{2}});
  root.Publish(Content::kXaFlacConverterName, Content::kXaFlacConverterVersion,
               InterleavedOutput("audio.xa.ending",
                                 Content::kXaFlacConverterName, kXaPath, flac));
  CHECK(Contains(root.Refusal(), "no str-mkv output for MOV/TITLE.STR"));
  root.Publish(Content::kStrMkvConverterName, Content::kStrMkvConverterVersion,
               InterleavedOutput("movie.title", Content::kStrMkvConverterName,
                                 kMoviePath, mkv));
  CHECK(root.Refusal().empty());

  SECTION("a missing FLAC") {
    fs::remove(root.Root() / fs::path(flac));
    CHECK(Contains(root.Refusal(), "missing xa-flac output"));
  }
  SECTION("a movie listed as unconverted beside its conversion") {
    root.List({InterleavedFile(kXaPath, "stream"),
               InterleavedFile(kMoviePath, "stream")});
    CHECK(
        Contains(root.Refusal(), "str-mkv output names a missing or non-mkv"));
  }
}

// The extracted disc's conversions, checked against their sources.
namespace {

// Native/data, found from the working directory up, once it has a
// conversion manifest.
std::optional<fs::path> ExtractedData() {
  for (auto directory = fs::current_path();
       directory.has_parent_path() && directory != directory.parent_path();
       directory = directory.parent_path()) {
    const fs::path data = directory / "Native" / "data";
    if (fs::exists(data / Content::kConversionManifestName)) return data;
  }
  return std::nullopt;
}

std::optional<fs::path> RepoDisc() {
  for (auto directory = fs::current_path();
       directory.has_parent_path() && directory != directory.parent_path();
       directory = directory.parent_path()) {
    const fs::path cue = directory / "disks" / "SLUS-01040.cue";
    if (fs::exists(cue)) return cue;
  }
  return std::nullopt;
}

// `path`'s raw 2352-byte sectors on the disc image `cue`: what an
// interleaved file's manifest hash covers.
std::vector<std::uint8_t> RawSectors(const fs::path& cue,
                                     std::string_view path) {
  const auto disc = Content::Disc::Open(cue);
  INFO((disc ? std::string{} : disc.error()));
  if (!disc || *disc == nullptr) {
    FAIL("the disc image does not open");
    return {};
  }
  const auto file = (*disc)->Find(path);
  if (!file) {
    FAIL(std::format("{} is not on the disc", path));
    return {};
  }
  const std::size_t sectors =
      (file->size + Content::Disc::SectorData - 1) / Content::Disc::SectorData;
  std::vector<std::uint8_t> raw(sectors * Content::Disc::SectorRaw);
  const auto read = (*disc)->ReadRawSectors(
      file->lba, static_cast<std::uint32_t>(sectors), raw);
  INFO((read ? std::string{} : read.error()));
  REQUIRE(read.has_value());
  return raw;
}

std::string ReadText(const fs::path& path) {
  std::ifstream input(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(input),
          std::istreambuf_iterator<char>()};
}

// The value of `key` in an SFZ region, or empty without one.
std::string SfzValue(std::string_view text, std::string_view key) {
  const std::string token = std::format(" {}=", key);
  const std::size_t at = text.find(token);
  if (at == std::string_view::npos) return {};
  const std::size_t start = at + token.size();
  const std::size_t end = text.find_first_of(" \r\n", start);
  return std::string(text.substr(start, end - start));
}

struct Wave {
  std::uint16_t format = 0;
  std::uint16_t channels = 0;
  std::uint32_t rate = 0;
  std::uint16_t bits = 0;
  std::vector<std::int16_t> pcm;
};

bool HasTag(std::span<const std::uint8_t> bytes, std::size_t offset,
            std::string_view tag) {
  return offset + tag.size() <= bytes.size() &&
         std::ranges::equal(bytes.subspan(offset, tag.size()), tag,
                            [](std::uint8_t byte, char letter) {
                              return byte == static_cast<std::uint8_t>(letter);
                            });
}

// A RIFF WAVE file's format and 16-bit samples.
std::optional<Wave> ParseWave(std::span<const std::uint8_t> bytes) {
  constexpr std::size_t kChunkHeader = 8;
  if (!HasTag(bytes, 0, "RIFF") || !HasTag(bytes, 8, "WAVE")) {
    return std::nullopt;
  }
  Wave wave;
  bool data = false;
  for (std::size_t offset = 12; offset + kChunkHeader <= bytes.size();) {
    const std::size_t size = std::min<std::size_t>(
        Read32(bytes, offset + 4), bytes.size() - offset - kChunkHeader);
    const auto body = bytes.subspan(offset + kChunkHeader, size);
    if (HasTag(bytes, offset, "fmt ") && size >= 16) {
      wave.format = Read16(body, 0);
      wave.channels = Read16(body, 2);
      wave.rate = Read32(body, 4);
      wave.bits = Read16(body, 14);
    } else if (HasTag(bytes, offset, "data")) {
      for (std::size_t at = 0; at + 1 < body.size(); at += 2) {
        wave.pcm.push_back(static_cast<std::int16_t>(Read16(body, at)));
      }
      data = true;
    }
    offset += kChunkHeader + size + (size & 1U);
  }
  if (!data) return std::nullopt;
  return wave;
}

// Checks `wave` is mono 16-bit PCM at the sound banks' sample rate.
void CheckWaveFormat(const Wave& wave) {
  CHECK(wave.format == 1);
  CHECK(wave.channels == 1);
  CHECK(wave.bits == 16);
  CHECK(wave.rate == Content::kAudioBankSampleRate);
}

// Checks the SFZ region `text` loops as `sample` does.
void CheckLoop(std::string_view text, const Content::AudioSample& sample) {
  if (sample.repeat) {
    CHECK(SfzValue(text, "loop_mode") == "loop_continuous");
    CHECK(SfzValue(text, "loop_start") == std::to_string(sample.loop_frame));
    CHECK(SfzValue(text, "loop_end") == std::to_string(sample.pcm.size() - 1));
  } else {
    CHECK(SfzValue(text, "loop_mode") == "no_loop");
    CHECK(SfzValue(text, "loop_start").empty());
  }
}

// Checks the WAV beside the SFZ instrument `sfz` holds `sample`'s PCM and
// the SFZ its loop.
void CheckSample(const fs::path& sfz, const Content::AudioSample& sample) {
  INFO(sfz.generic_string());
  fs::path wav = sfz;
  wav.replace_extension(".wav");
  const auto bytes = Extract::ReadFileBytes(wav);
  if (!bytes) {
    FAIL(bytes.error());
    return;
  }
  const auto wave = ParseWave(Unsigned(*bytes));
  if (!wave) {
    FAIL("not a RIFF WAVE file with data");
    return;
  }
  CheckWaveFormat(*wave);
  CHECK(wave->pcm == sample.pcm);
  CheckLoop(ReadText(sfz), sample);
}

// Checks `bank`'s converted instruments under `root` are its decode; how
// many samples it holds.
std::size_t CheckInstruments(const fs::path& root,
                             const Content::AudioBank& bank) {
  const auto loaded = Content::LoadAudioBank(root, bank.id);
  if (!loaded) {
    FAIL(loaded.error());
    return 0;
  }
  REQUIRE(loaded->instruments.size() == bank.instruments.size());
  std::size_t samples = 0;
  for (std::size_t index = 0; index < bank.instruments.size(); ++index) {
    const bool available = bank.instruments.at(index).available;
    CHECK(loaded->instruments.at(index).available == available);
    if (!available) continue;
    CheckSample(Content::AudioInstrumentPath(root, bank.id, index),
                bank.samples.at(index));
    ++samples;
  }
  return samples;
}

// Checks `output`'s files are its current source's decode; how many samples
// it holds.
std::size_t CheckBank(const fs::path& root,
                      const Content::ConversionOutput& output) {
  INFO(output.source);
  const auto source = Extract::ReadSource(root, output.source);
  if (!source) {
    FAIL(source.error());
    return 0;
  }
  CHECK(Extract::HashOf(*source) == output.source_sha256);
  const auto bank = Content::DecodeAudioBank(Unsigned(source->bytes));
  if (!bank) {
    FAIL(bank.error());
    return 0;
  }
  CHECK(output.files == Content::AudioBankFiles(*bank));
  return CheckInstruments(root, *bank);
}

struct BankCounts {
  std::size_t banks = 0;
  std::size_t placeholders = 0;
  std::size_t samples = 0;
};

// Checks `store` accepts its conversions and each sound bank it lists is
// its manifest output's exact decode.
BankCounts CheckStoreBanks(const Content::DataStore& store,
                           const fs::path& root,
                           const Content::ConversionManifest& manifest) {
  const auto required = store.RequireConversions();
  INFO((required ? std::string{} : required.error()));
  CHECK(required.has_value());
  BankCounts counts;
  for (const Content::DataStore::Entry& entry : store.Entries()) {
    if (!entry.path.starts_with(Content::kWaveBankPrefix) ||
        !entry.path.ends_with(Content::kWaveBankExtension)) {
      continue;
    }
    if (entry.size <= 1) {
      ++counts.placeholders;
      continue;
    }
    const auto output = std::ranges::find_if(
        manifest.outputs, [&entry](const Content::ConversionOutput& candidate) {
          return candidate.converter == Content::kWaveBankConverterName &&
                 candidate.source == entry.path;
        });
    INFO(entry.path);
    REQUIRE(output != manifest.outputs.end());
    counts.samples += CheckBank(root, *output);
    ++counts.banks;
  }
  return counts;
}

// PS1 CD-XA ADPCM, decoded independently of FFmpeg's adpcm_xa.
constexpr std::size_t kRawSectorBytes = Content::Disc::SectorRaw;
constexpr std::size_t kXaSubheaderOffset = 16;
constexpr std::size_t kXaPayloadOffset = 24;
constexpr std::size_t kXaGroups = 18;
constexpr std::size_t kXaGroupBytes = 128;
constexpr std::size_t kXaUnits = 8;
constexpr std::size_t kXaUnitSamples = 28;
// The prediction filters' coefficients for the previous sample and the one
// before it, in 64ths.
constexpr std::array<int, 5> kXaPreviousWeights{0, 60, 115, 98, 122};
constexpr std::array<int, 5> kXaBeforeWeights{0, 0, -52, -55, -60};

struct XaHistory {
  int previous = 0;
  int before = 0;
};

// Appends the 28 samples of sound unit `unit` of a 128-byte sound group:
// even units are the low nibbles. False on a reserved filter or range.
bool DecodeXaUnit(std::span<const std::uint8_t> group, std::size_t unit,
                  XaHistory& history, std::vector<std::int16_t>& out) {
  constexpr int kMaxRange = 12;
  const std::uint8_t parameters = group[4 + unit];
  const int range = parameters & 15;
  const std::size_t filter = parameters >> 4;
  if (range > kMaxRange || filter >= kXaPreviousWeights.size()) return false;
  const int k0 = kXaPreviousWeights.at(filter);
  const int k1 = kXaBeforeWeights.at(filter);
  for (std::size_t sample = 0; sample < kXaUnitSamples; ++sample) {
    const std::uint8_t byte = group[16 + (unit / 2) + (sample * 4)];
    const int nibble = (unit % 2 == 0 ? byte : byte >> 4) & 15;
    const int delta = (nibble ^ 8) - 8;
    const int value = (delta * (1 << (kMaxRange - range))) +
                      ((history.previous * k0 + history.before * k1 + 32) >> 6);
    history.before = history.previous;
    history.previous = std::clamp(value, -32768, 32767);
    out.push_back(static_cast<std::int16_t>(history.previous));
  }
  return true;
}

// Appends a sector's frames, interleaved when stereo. False on reserved
// parameters.
bool DecodeXaSector(std::span<const std::uint8_t> sector, bool stereo,
                    std::array<XaHistory, 2>& history,
                    std::vector<std::int16_t>& frames) {
  bool valid = true;
  std::vector<std::int16_t> left;
  std::vector<std::int16_t> right;
  for (std::size_t group = 0; group < kXaGroups; ++group) {
    const auto bytes = sector.subspan(
        kXaPayloadOffset + (group * kXaGroupBytes), kXaGroupBytes);
    for (std::size_t unit = 0; unit < kXaUnits; unit += 2) {
      if (!stereo) {
        valid = DecodeXaUnit(bytes, unit, history[0], frames) && valid;
        valid = DecodeXaUnit(bytes, unit + 1, history[0], frames) && valid;
        continue;
      }
      left.clear();
      right.clear();
      valid = DecodeXaUnit(bytes, unit, history[0], left) && valid;
      valid = DecodeXaUnit(bytes, unit + 1, history[1], right) && valid;
      for (std::size_t sample = 0; sample < left.size(); ++sample) {
        frames.push_back(left.at(sample));
        frames.push_back(right.at(sample));
      }
    }
  }
  return valid;
}

// The frames FFmpeg's psxstr demuxer completes from an STR's raw sectors:
// valid video or data sectors that are the last chunk of their frame.
std::size_t StrFrames(std::span<const std::uint8_t> raw) {
  constexpr std::uint8_t kTypeMask = 0x0E;
  constexpr std::uint8_t kVideo = 0x02;
  constexpr std::uint8_t kData = 0x08;
  constexpr std::size_t kChunkBytes = 2016;
  std::size_t frames = 0;
  for (std::size_t offset = 0; offset + kRawSectorBytes <= raw.size();
       offset += kRawSectorBytes) {
    const auto sector = raw.subspan(offset, kRawSectorBytes);
    const std::uint8_t type = sector[kXaSubheaderOffset + 2] & kTypeMask;
    if (type != kVideo && type != kData) continue;
    const std::uint16_t chunk = Read16(sector, 0x1C);
    const std::uint16_t chunks = Read16(sector, 0x1E);
    const std::uint32_t size = Read32(sector, 0x24);
    if (chunk < chunks && chunk + 1 == chunks &&
        size <= static_cast<std::size_t>(chunks) * kChunkBytes) {
      ++frames;
    }
  }
  return frames;
}

struct MkvVideo {
  std::size_t frames = 0;
  std::int64_t last_ns = -1;
};

// The number of `segment`'s video track.
std::int64_t VideoTrack(const mkvparser::Segment& segment) {
  const mkvparser::Tracks* tracks = segment.GetTracks();
  REQUIRE(tracks != nullptr);
  std::int64_t video = -1;
  for (decltype(tracks->GetTracksCount()) index = 0;
       index < tracks->GetTracksCount(); ++index) {
    const mkvparser::Track* track = tracks->GetTrackByIndex(index);
    if (track != nullptr && track->GetType() == mkvparser::Track::kVideo) {
      video = track->GetNumber();
    }
  }
  REQUIRE(video >= 0);
  return video;
}

// The frame count and last frame time of `segment`'s track `video`.
MkvVideo CountFrames(mkvparser::Segment& segment, std::int64_t video) {
  MkvVideo result;
  for (const mkvparser::Cluster* cluster = segment.GetFirst();
       cluster != nullptr && !cluster->EOS();
       cluster = segment.GetNext(cluster)) {
    const mkvparser::BlockEntry* entry = nullptr;
    if (cluster->GetFirst(entry) < 0) break;
    while (entry != nullptr && !entry->EOS()) {
      const mkvparser::Block* block = entry->GetBlock();
      if (block->GetTrackNumber() == video) {
        result.frames += static_cast<std::size_t>(block->GetFrameCount());
        result.last_ns = std::max(result.last_ns, block->GetTime(cluster));
      }
      if (cluster->GetNext(entry, entry) < 0) break;
    }
  }
  return result;
}

// The video track's frame count and last frame time in a Matroska file.
MkvVideo ReadMkvVideo(const fs::path& path) {
  const std::unique_ptr<std::FILE, decltype(&std::fclose)> input(
      _wfopen(path.c_str(), L"rb"), std::fclose);
  REQUIRE(input != nullptr);
  mkvparser::MkvReader reader(input.get());
  std::int64_t position = 0;
  mkvparser::EBMLHeader header;
  REQUIRE(header.Parse(&reader, position) >= 0);
  mkvparser::Segment* created = nullptr;
  REQUIRE(mkvparser::Segment::CreateInstance(&reader, position, created) == 0);
  const std::unique_ptr<mkvparser::Segment> segment(created);
  REQUIRE(segment != nullptr);
  REQUIRE(segment->Load() >= 0);
  return CountFrames(*segment, VideoTrack(*segment));
}

// The output `id` in `manifest`, which must list exactly `files`.
const Content::ConversionOutput& RequireOutput(
    const Content::ConversionManifest& manifest, std::string_view id,
    const std::vector<std::string>& files) {
  const Content::ConversionOutput* output = manifest.Find(id);
  REQUIRE(output != nullptr);
  REQUIRE(output->files == files);
  return *output;
}

// The ending XA's selected sectors, once its manifest hash is checked
// against its raw sectors on the disc image `cue`.
std::vector<std::uint8_t> SelectedXaSectors(
    const fs::path& cue, const Content::ConversionOutput& output) {
  const std::vector<std::uint8_t> raw = RawSectors(cue, kXaPath);
  CHECK(VS::Core::Sha256Hex(std::as_bytes(std::span(raw))) ==
        output.source_sha256);
  auto selected = Content::SelectXaAudioSectors(raw);
  if (!selected) {
    FAIL(selected.error());
    return {};
  }
  const std::size_t sectors = selected->size() / kRawSectorBytes;
  // ENDING.XA stores each sector four times over; one plays.
  CHECK(sectors == 8316);
  REQUIRE(sectors > 0);
  return std::move(*selected);
}

struct XaFormat {
  bool stereo = false;
  std::uint32_t rate = 0;
  std::size_t frames_per_sector = 0;
};

// The coding of the first selected sector, which every sector shares.
XaFormat XaFormatOf(const std::vector<std::uint8_t>& selected) {
  const std::uint8_t coding = selected.at(kXaSubheaderOffset + 3);
  REQUIRE((coding & 0x10U) == 0);  // 4-bit samples
  const bool stereo = (coding & 1U) != 0;
  return {.stereo = stereo,
          .rate = (coding & 4U) != 0 ? 18900U : 37800U,
          .frames_per_sector =
              kXaGroups * kXaUnits / 2 * kXaUnitSamples * (stereo ? 1 : 2)};
}

// Checks `flac` has the channels, rate and length of `sectors` sectors in
// `format`.
void CheckFlacFormat(const drflac& flac, const XaFormat& format,
                     std::size_t sectors) {
  CHECK(static_cast<unsigned>(flac.channels) == (format.stereo ? 2U : 1U));
  CHECK(flac.sampleRate == format.rate);
  CHECK(flac.totalPCMFrameCount == sectors * format.frames_per_sector);
}

struct XaComparison {
  bool valid = true;
  std::size_t mismatched = 0;
};

// Compares `flac`'s PCM, sector by sector, with an independent decode of
// the selected sectors.
XaComparison CompareXa(drflac& flac, std::span<const std::uint8_t> selected,
                       const XaFormat& format) {
  const std::size_t sectors = selected.size() / kRawSectorBytes;
  std::array<XaHistory, 2> history{};
  std::vector<std::int16_t> expected;
  std::vector<std::int16_t> decoded;
  XaComparison result;
  for (std::size_t sector = 0; sector < sectors; ++sector) {
    expected.clear();
    result.valid = DecodeXaSector(selected.subspan(sector * kRawSectorBytes,
                                                   kRawSectorBytes),
                                  format.stereo, history, expected) &&
                   result.valid;
    decoded.assign(expected.size(), 0);
    const auto read = drflac_read_pcm_frames_s16(
        &flac, format.frames_per_sector, decoded.data());
    if (read != format.frames_per_sector || decoded != expected) {
      ++result.mismatched;
    }
  }
  return result;
}

}  // namespace

TEST_CASE("Extracted sound banks hold their WAVE files' exact ADPCM decode",
          "[content-audio][extract]") {
  const auto root = ExtractedData();
  if (!root) SKIP("Extracted Native/data is unavailable");
  const Content::ConversionManifest manifest = ManifestAt(*root);
  Content::DataStore::ScopedOverride restore;
  const auto opened = Content::DataStore::Open(*root);
  INFO((opened ? std::string{} : opened.error()));
  REQUIRE(opened.has_value());
  const Content::DataStore* store = Content::DataStore::Get();
  if (store == nullptr) {
    FAIL("no store is open");
    return;
  }
  const BankCounts counts = CheckStoreBanks(*store, *root, manifest);
  CHECK(counts.banks == 83);
  CHECK(counts.placeholders == 18);
  CHECK(counts.samples > 0);
}

TEST_CASE("The extracted ending XA FLAC is its selected sectors' exact decode",
          "[content-audio][extract]") {
  const auto root = ExtractedData();
  const auto cue = RepoDisc();
  if (!root || !cue) {
    SKIP("Extracted Native/data or the disc image is unavailable");
  }
  const Content::ConversionManifest manifest = ManifestAt(*root);
  const Content::ConversionOutput& output = RequireOutput(
      manifest, "audio.xa.ending", {Content::XaAudioFile(kXaPath)});
  const std::vector<std::uint8_t> selected = SelectedXaSectors(*cue, output);
  const XaFormat format = XaFormatOf(selected);
  const fs::path path = *root / fs::path(output.files.front());
  const std::unique_ptr<drflac, decltype(&drflac_close)> flac(
      drflac_open_file_w(path.c_str(), nullptr), drflac_close);
  REQUIRE(flac != nullptr);
  CheckFlacFormat(*flac, format, selected.size() / kRawSectorBytes);
  const XaComparison comparison = CompareXa(*flac, selected, format);
  CHECK(comparison.valid);
  CHECK(comparison.mismatched == 0);
}

TEST_CASE(
    "The extracted title movie keeps every STR frame at 15 frames a "
    "second",
    "[content-movie][extract]") {
  const auto root = ExtractedData();
  const auto cue = RepoDisc();
  if (!root || !cue) {
    SKIP("Extracted Native/data or the disc image is unavailable");
  }
  const Content::ConversionManifest manifest = ManifestAt(*root);
  const Content::ConversionOutput* output = manifest.Find("movie.title");
  REQUIRE(output != nullptr);
  const std::string mkv =
      std::format("disc/{}.{}", kMoviePath, Content::kMovieForm);
  REQUIRE(output->files == std::vector{mkv});
  const std::vector<std::uint8_t> raw = RawSectors(*cue, kMoviePath);
  CHECK(VS::Core::Sha256Hex(std::as_bytes(std::span(raw))) ==
        output->source_sha256);
  const std::size_t frames = StrFrames(raw);
  CHECK(frames == 593);
  REQUIRE(frames > 0);

  const MkvVideo video = ReadMkvVideo(*root / fs::path(mkv));
  CHECK(video.frames == frames);
  // Frame k shows at k/15 s, in the container's millisecond ticks.
  constexpr std::int64_t kNanosecondsPerSecond = 1'000'000'000;
  constexpr std::int64_t kTick = 1'000'000;
  constexpr std::int64_t kFramesPerSecond = 15;
  const std::int64_t last = static_cast<std::int64_t>(frames - 1) *
                            kNanosecondsPerSecond / kFramesPerSecond;
  CHECK(std::abs(video.last_ns - last) <= kTick);
}
