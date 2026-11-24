#include "Save/NativeFileText.hpp"

#include <array>
#include <functional>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

#include "Content/DataStore.hpp"
#include "Content/NativeText.hpp"
#include "Core/Log.hpp"
#include "Render/NativeTextDraw.hpp"
#include "Render/NativeTextLayout.hpp"

namespace VS::Game::Save {
namespace {
// NOLINTNEXTLINE(bugprone-exception-escape): std::map's noexcept move.
struct TextSession {
  Content::NativeTextAssets assets;
  std::map<std::string, Render::TextLayout, std::less<>> layouts;
  std::string selected;
};
std::optional<TextSession>& Session() {
  static std::optional<TextSession> session;
  return session;
}
constexpr std::string_view kYes = "text.save.choice.yes";
constexpr std::string_view kNo = "text.save.choice.no";
}  // namespace

void ResetNativeFileText() { Session().reset(); }

void BeginNativeFileText() {
  ResetNativeFileText();
  const auto* store = Content::DataStore::Get();
  // Headless policy/host fixtures have no asset catalog or renderer.
  if (store == nullptr || store->Catalog() == nullptr) return;
  const auto* source = store->SaveText();
  if (source == nullptr) {
    throw std::runtime_error(
        "Native save text is missing; run VS.Extract text <datadir>");
  }
  TextSession session{*source, {}, {}};
  for (const auto& [id, text] : source->catalog.strings) {
    if (!id.starts_with("text.save.")) continue;
    const bool choice = id == kYes || id == kNo;
    const Render::TextBox box{choice ? 100.0 : 304.0, choice ? 16.0 : 30.0,
                              source->catalog.scale};
    auto bound = Content::BindNativeText(text, {}, id);
    if (!bound) throw std::runtime_error(bound.error());
    auto layout = Render::LayoutNativeText(*bound, source->font, box);
    if (!layout) throw std::runtime_error(id + ": " + layout.error());
    session.layouts.emplace(id, std::move(*layout));
  }
  if (!session.layouts.contains(kYes) || !session.layouts.contains(kNo)) {
    throw std::runtime_error("Native save text: missing Yes/No choice ID");
  }
  Session() = std::move(session);
}

void SelectNativeFileText(std::string_view id) {
  auto& session = Session();
  if (!session) return;
  const auto found = session->layouts.find(id);
  if (found == session->layouts.end()) {
    throw std::runtime_error(std::string(id) + ": missing save prompt ID");
  }
  session->selected = id;
  Core::LogInfo("save.text", "{} scale={} glyphs={} size={}x{}", id,
                session->assets.catalog.scale, found->second.glyphs.size(),
                found->second.width, found->second.height);
}

void DrawNativeFileText(bool confirmation) {
  const auto& session = Session();
  if (!session || session->selected.empty()) return;
  const auto& layouts = session->layouts;
  const std::array draws{
      Render::NativeTextDraw{&layouts.at(session->selected), 8, 190},
      Render::NativeTextDraw{&layouts.at(std::string(kYes)), 24, 18},
      Render::NativeTextDraw{&layouts.at(std::string(kNo)), 24, 38}};
  auto result = Render::DrawLiveNativeText(
      std::span(draws).first(confirmation ? draws.size() : 1),
      session->assets.atlas.image);
  if (!result) throw std::runtime_error(result.error());
}
}  // namespace VS::Game::Save
