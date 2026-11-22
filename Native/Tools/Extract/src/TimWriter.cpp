#include "Extract/TimWriter.hpp"

#include <cstddef>
#include <cstdint>
#include <format>
#include <string>
#include <type_traits>
#include <vector>

namespace VS::Extract {
namespace {
constexpr std::uint32_t kIdMagicMask = 0xFF;
constexpr std::uint32_t kModeMask = 0x7;
constexpr std::uint32_t kClutFlag = 0x8;
constexpr std::uint32_t kKnownFlags = kModeMask | kClutFlag;
constexpr std::uint32_t kFirstUnsupportedMode = 4;
constexpr std::size_t kHalfwordBytes = 2;
constexpr std::size_t kByteBits = 8;
constexpr unsigned kByteMask = 0xFF;

template <typename T>
void AppendLe(std::vector<std::byte>& out, T value) {
  const auto bits =
      static_cast<std::uint32_t>(static_cast<std::make_unsigned_t<T>>(value));
  for (std::size_t index = 0; index < sizeof(T); ++index) {
    out.push_back(
        static_cast<std::byte>((bits >> (kByteBits * index)) & kByteMask));
  }
}

std::expected<void, std::string> AppendBlock(std::vector<std::byte>& out,
                                             const Content::TimBlock& block,
                                             std::string_view name) {
  const std::size_t words = std::size_t{block.rect.width} * block.rect.height;
  if (words == 0 || block.words.size() != words) {
    return std::unexpected(
        std::format("the {} block has {} words for a {}x{} rectangle", name,
                    block.words.size(), block.rect.width, block.rect.height));
  }
  AppendLe(out, static_cast<std::uint32_t>(Content::kTimBlockHeaderBytes +
                                           (words * kHalfwordBytes)));
  AppendLe(out, block.rect.x);
  AppendLe(out, block.rect.y);
  AppendLe(out, block.rect.width);
  AppendLe(out, block.rect.height);
  for (const std::uint16_t word : block.words) AppendLe(out, word);
  return {};
}
}  // namespace

std::expected<std::vector<std::byte>, std::string> EncodeTim(
    const Content::TimFile& tim) {
  if ((tim.id & kIdMagicMask) != Content::kTimMagic) {
    return std::unexpected(
        std::format("the id word 0x{:08X} is not a TIM id", tim.id));
  }
  if ((tim.flags & ~kKnownFlags) != 0 ||
      (tim.flags & kModeMask) >= kFirstUnsupportedMode) {
    return std::unexpected(std::format(
        "the flag word 0x{:08X} is not a TIM flag word", tim.flags));
  }
  if (tim.HasClut() != tim.clut.has_value()) {
    return std::unexpected(std::format(
        "the flag word 0x{:08X} {} a CLUT block, but the TIM {} one", tim.flags,
        tim.HasClut() ? "declares" : "omits", tim.clut ? "has" : "has no"));
  }
  std::vector<std::byte> out;
  AppendLe(out, tim.id);
  AppendLe(out, tim.flags);
  if (tim.clut) {
    if (auto appended = AppendBlock(out, *tim.clut, "CLUT"); !appended) {
      return std::unexpected(appended.error());
    }
  }
  if (auto appended = AppendBlock(out, tim.image, "image"); !appended) {
    return std::unexpected(appended.error());
  }
  return out;
}

}  // namespace VS::Extract
