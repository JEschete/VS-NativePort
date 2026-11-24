#include "Extract/FarConversion.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "Content/BackgroundLayout.hpp"
#include "Content/ConversionManifest.hpp"
#include "Content/ImageFiles.hpp"
#include "Extract/ConverterBatch.hpp"
#include "Extract/ImageConversion.hpp"

namespace VS::Extract {
namespace {
namespace Content = VS::Content;

struct PlannedLayout {
  Content::ConversionOutput output;
  std::string file;
  std::string text;
};

std::expected<Content::ImageDocument, std::string> PairedImage(
    const std::filesystem::path& root,
    const Content::ConversionManifest& manifest, const SourceFile& image,
    std::string_view id) {
  const auto* output = manifest.Find(id);
  if (output == nullptr || output->source != image.path ||
      output->source_sha256 != HashOf(image) ||
      output->converter != kTimConverterName) {
    return std::unexpected(
        image.path +
        ": missing/stale paired image; run VS.Extract images first");
  }
  auto location = ImageLocationForDiscPath(image.path);
  if (!location) return std::unexpected(location.error());
  const std::string stem = location->directory + "/" + location->stem;
  if (std::ranges::find(output->files, stem + ".png") == output->files.end() ||
      std::ranges::find(output->files, stem + ".image.json") ==
          output->files.end()) {
    return std::unexpected(image.path +
                           ": paired PNG/sidecar is not registered");
  }
  auto document = Content::LoadConvertedImageById(root, manifest, id);
  if (!document) return std::unexpected(document.error());
  return document;
}

std::expected<PlannedLayout, std::string> PlanLayout(
    const std::filesystem::path& root, const RunStart& start,
    std::string_view path) {
  // MPD room section 15 binds these pairs: MAP001/215, MAP002/216,
  // MAP007 and MAP008 respectively (fresh disc audit in #210).
  constexpr std::array<std::string_view, 4> kBoundLayouts{
      "BG/001OP01A.FAR", "BG/002OP01A.FAR", "BG/007OP01A.FAR",
      "BG/008OP01A.FAR"};
  if (std::ranges::find(kBoundLayouts, path) == kBoundLayouts.end()) {
    return std::unexpected(std::string(path) +
                           ": no verified background image binding");
  }
  auto source = ReadSource(root, path);
  const std::string image_path =
      std::string(path.substr(0, path.size() - 4)) + ".TIM";
  if (std::ranges::none_of(
          start.entries, [&](const Content::DataStore::Entry& entry) {
            return entry.path == image_path && entry.form == "raw";
          })) {
    return std::unexpected(std::string(path) +
                           ": paired image is missing from files.tsv");
  }
  auto image_source = ReadSource(root, image_path);
  if (!source) return std::unexpected(source.error());
  if (!image_source) return std::unexpected(image_source.error());
  auto image_id = ImageIdForDiscPath(image_path);
  if (!image_id) return std::unexpected(image_id.error());
  auto image = PairedImage(root, start.previous, *image_source, *image_id);
  if (!image) return std::unexpected(image.error());
  if (std::ranges::none_of(
          image->provenance.dependencies, [&](const auto& dependency) {
            return dependency.source == source->path &&
                   dependency.source_sha256 == HashOf(*source) &&
                   dependency.role == "layout";
          })) {
    return std::unexpected(
        image_path +
        ": stale FAR depth dependency; run VS.Extract images first");
  }
  // These are explicitly import identities until room/artwork meanings are
  // resolved (#210/D26), not fabricated semantic background names.
  const std::string id = "background.import." + image_id->substr(6);
  auto layout = Content::DecodeFar(
      source->bytes, {.id = id,
                      .image_id = *image_id,
                      .source = source->path,
                      .source_sha256 = HashOf(*source),
                      .image_source = image_path,
                      .image_source_sha256 = HashOf(*image_source),
                      .unreferenced = false});
  if (!layout) {
    return std::unexpected(std::string(path) + ": " + layout.error());
  }
  if (auto valid = Content::ValidateBackgroundImage(*layout, *image); !valid) {
    return std::unexpected(valid.error());
  }
  auto text = Content::WriteBackgroundLayout(*layout);
  if (!text) return std::unexpected(text.error());
  auto reloaded =
      Content::ReadBackgroundLayout(std::as_bytes(std::span(*text)), id);
  if (!reloaded) return std::unexpected(reloaded.error());
  auto rebuilt = Content::EncodeFar(*reloaded);
  if (!rebuilt || *rebuilt != source->bytes || *reloaded != *layout) {
    return std::unexpected(std::string(path) +
                           ": native FAR round trip differs");
  }
  auto location = ImageLocationForDiscPath(path);
  if (!location) return std::unexpected(location.error());
  return PlannedLayout{
      .output = {.id = id,
                 .source = source->path,
                 .source_sha256 = layout->source_sha256,
                 .converter = std::string(Content::kFarConverterName),
                 .files = {},
                 .dependencies = {{.source = image_path,
                                   .source_sha256 =
                                       layout->image_source_sha256}},
                 .unreferenced = layout->unreferenced,
                 .note = "Typed camera-wrapped grid; semantic background "
                         "identity unresolved (#210)",
                 .alias_of = {}},
      .file = "backgrounds/" + location->stem + ".background.json",
      .text = std::move(*text)};
}

std::expected<void, std::string> ValidateKept(
    const std::filesystem::path& root,
    const Content::ConversionManifest& manifest, const PlannedLayout& plan) {
  auto reader = Content::DirectoryFileReader(root);
  auto bytes = reader(plan.file);
  if (!bytes) {
    return std::unexpected("cannot read native background " + plan.file);
  }
  auto layout = Content::ReadBackgroundLayout(*bytes, plan.file);
  if (!layout) return std::unexpected(layout.error());
  auto original = Content::ReadBackgroundLayout(
      std::as_bytes(std::span(plan.text)), plan.file);
  if (!original) return std::unexpected(original.error());
  if (layout->id != original->id || layout->source != original->source ||
      layout->source_sha256 != original->source_sha256 ||
      layout->image_source != original->image_source ||
      layout->image_source_sha256 != original->image_source_sha256 ||
      layout->unreferenced != original->unreferenced) {
    return std::unexpected(plan.file +
                           ": authored layout has stale import provenance");
  }
  auto image = Content::LoadBackgroundLayoutImage(root, manifest, *layout);
  if (!image) return std::unexpected(image.error());
  return {};
}
}  // namespace

std::expected<FarConversionReport, std::string> ConvertDiscBackgrounds(
    const std::filesystem::path& root, bool force) {
  auto start = StartRun(root);
  if (!start) return std::unexpected(start.error());
  auto batch = StartBatch(start->previous, Content::kFarConverterName,
                          Content::kFarConverterVersion, force);
  FarConversionReport report;
  std::vector<PlannedLayout> planned;
  for (const auto& entry : start->entries) {
    if (entry.form != "raw" || !entry.path.ends_with(".FAR")) continue;
    auto plan = PlanLayout(root, *start, entry.path);
    if (!plan) return std::unexpected(plan.error());
    const auto* kept = batch.reuse
                           ? MatchingOutput(start->previous, root, plan->output)
                           : nullptr;
    if (kept != nullptr) {
      if (kept->files != std::vector<std::string>{plan->file}) {
        return std::unexpected(entry.path +
                               ": unexpected native layout output files");
      }
      if (auto valid = ValidateKept(root, start->previous, *plan); !valid) {
        return std::unexpected(valid.error());
      }
      batch.outputs.push_back(*kept);
      ++report.unchanged;
    } else {
      plan->output.files = {plan->file};
      batch.outputs.push_back(plan->output);
      planned.push_back(std::move(*plan));
      ++report.converted;
    }
  }
  std::vector<OutputBytes> files;
  for (const auto& plan : planned) {
    files.push_back(
        {.name = plan.file, .bytes = std::as_bytes(std::span(plan.text))});
  }
  auto published = PublishBatch(root, start->previous, batch,
                                [&] { return WriteOutputFiles(root, files); });
  if (!published) return std::unexpected(published.error());
  report.removed = *published;
  return report;
}
}  // namespace VS::Extract
