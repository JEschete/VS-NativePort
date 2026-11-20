#include "Menu/MapSceneStorageLive.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Menu/TextBoxStateLive.hpp"

namespace VS::Menu::map {
namespace {
std::span<std::byte> loadedScene;
}

void BindLiveMapScene(void* scene, std::size_t bytes) noexcept {
  loadedScene = scene == nullptr
                    ? std::span<std::byte>{}
                    : std::span{static_cast<std::byte*>(scene), bytes};
}

void ForgetLiveMapScene(const void* scene) noexcept {
  if (loadedScene.data() == scene) loadedScene = {};
}

std::optional<std::span<std::byte>> BoundLiveMapScene(
    const void* scene) noexcept {
  if (scene == nullptr || scene != loadedScene.data()) return std::nullopt;
  return loadedScene;
}

void ClearLiveMapTextBorrow(std::span<const std::byte> bytes) {
  if (bytes.empty()) return;
  auto box = ReadLiveTextBox(0);
  const auto address = reinterpret_cast<std::uintptr_t>(box.text);
  const auto first = reinterpret_cast<std::uintptr_t>(bytes.data());
  if (address >= first && address - first < bytes.size()) {
    box.text = nullptr;
    box.state = 0;
    WriteLiveTextBox(0, box);
  }
}
}  // namespace VS::Menu::map
