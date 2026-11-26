#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <functional>
#include <span>
#include <string>
#include <string_view>

#include "Content/JsonText.hpp"

namespace VS::Extract {

// Reads the JSON output `file` under `root` and writes it back formatted when
// `edit` reports that it changed it; whether it was written. A missing file
// is left for its converter to write again.
[[nodiscard]] std::expected<bool, std::string> EditJsonOutput(
    const std::filesystem::path& root, const std::string& file,
    const std::function<
        std::expected<bool, std::string>(Content::JsonDocument&)>& edit);

// A converter or schema version an authoring upgrade moves on from.
struct AuthoringVersionStep {
  std::string_view name;
  std::uint32_t from = 0;
  std::uint32_t to = 0;
};

// `value` as a whole number from 0 to `max`; the error names `where`.
[[nodiscard]] std::expected<std::int64_t, std::string> AuthoringNumber(
    const Content::JsonDocument& value, std::int64_t max,
    const std::string& where);

// Rewrites, one file at a time, each file `takes` accepts among the outputs
// of every converter at its step's `from` version, by `upgrade`, which
// reports whether it changed the file (EditJsonOutput). Those converters move
// to their `to` versions in the manifest, which is written last, so an
// interrupted run resumes; nothing is written when no converter is at its
// `from` version. Returns the files written, with the manifest.
[[nodiscard]] std::expected<std::size_t, std::string> UpgradeConverterOutputs(
    const std::filesystem::path& root,
    std::span<const AuthoringVersionStep> steps,
    const std::function<bool(std::string_view)>& takes,
    const std::function<std::expected<bool, std::string>(
        const std::string&, Content::JsonDocument&)>& upgrade);

// `named`, led by the display_name label `numbered` carries, when it has one.
[[nodiscard]] Content::JsonDocument LabeledJson(
    const Content::JsonDocument& numbered, Content::JsonDocument named);

struct AnimationAuthoringReport {
  std::size_t documents = 0;  // documents moved to the current schema version
  std::size_t events = 0;     // events rewritten in their named form
  std::size_t written = 0;    // files written, with the manifest
};

// Rewrites the animation event scripts of existing character model, animation
// set and archetype outputs in their named form (Content/SeqEventForms.hpp)
// without reconverting source assets or replacing authored values. Each
// event is checked to read back as the same bytes; the converter versions are
// published last, and a file whose documents are already current is left as
// it is, so an interrupted run resumes.
[[nodiscard]] std::expected<AnimationAuthoringReport, std::string>
UpgradeAnimationAuthoring(const std::filesystem::path& root);

}  // namespace VS::Extract
