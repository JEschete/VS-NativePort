#pragma once

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace VS::combat_lab {

class Canvas {
 public:
  static constexpr int kWidth = 640;
  static constexpr int kHeight = 480;
  static constexpr std::uint32_t kTextColor = 0xFFE8E0CE;
  Canvas();
  void Clear();
  void Rectangle(int x, int y, int width, int height, std::uint32_t color);
  void Text(int x, int y, std::string_view text,
            std::uint32_t color = kTextColor);
  const std::uint32_t* Pixels() const { return pixels_.data(); }

 private:
  std::vector<std::uint32_t> pixels_;
  std::vector<std::uint16_t> font_;
  int font_width_ = 0;
  int font_height_ = 0;
};

}  // namespace VS::combat_lab
