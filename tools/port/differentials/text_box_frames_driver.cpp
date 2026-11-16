#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <stdexcept>
#include <vector>

#include "Menu/TextBoxFrames.hpp"
#include "Render/Sprites.hpp"

namespace {
namespace menu = VS::Menu;
namespace render = VS::Render;
constexpr std::uint32_t kPrimitiveAddress = 0x80600000;
constexpr std::uint32_t kLinkMask = 0xFFFFFF;

std::int64_t Read()
{
    std::int64_t value = 0;
    if (!(std::cin >> value)) {
        throw std::runtime_error("truncated text-frame differential vector");
    }
    return value;
}

std::uint32_t Pack(std::int32_t low, std::int32_t high)
{
    return static_cast<std::uint16_t>(low) | (static_cast<std::uint32_t>(high) << 16);
}

struct Memory {
    std::vector<std::uint32_t> words;
    std::array<std::uint32_t, 3> heads { };
};

class Queue final : public render::PacketQueue {
public:
    Queue(Memory& memory, std::size_t entry)
        : memory_(memory)
        , entry_(entry)
    {
    }
    std::uint32_t ChainLink() const override { return memory_.heads.at(entry_); }
    bool Emit(std::span<const std::uint32_t> packet) override
    {
        memory_.heads.at(entry_) = (kPrimitiveAddress
                                       + static_cast<std::uint32_t>(
                                           memory_.words.size() * sizeof(std::uint32_t)))
                                 & kLinkMask;
        memory_.words.insert(memory_.words.end(), packet.begin(), packet.end());
        return true;
    }

private:
    Memory& memory_;
    std::size_t entry_;
};

class World final : public menu::InformationBoxWorld {
public:
    World(Memory& memory, menu::TextBox value, bool alternate,
        const std::array<std::uint32_t, 3>& sizes,
        const std::array<std::uint32_t, 3>& textures)
        : box(value)
        , alternate_(alternate)
        , sizes_(sizes)
        , textures_(textures)
        , header_(memory, 0)
        , backdrop_(memory, 1)
    {
    }

    menu::TextBox ReadBox() const override { return box; }
    bool AlternateFramebuffer() const override { return alternate_; }
    bool DrawHeader(std::size_t kind, std::int32_t x, std::int32_t y) override
    {
        return Sprite(header_, render::kDefaultSpriteDrawMode,
            render::kDefaultSpriteBrightness, Pack(x, y), sizes_.at(kind),
            textures_.at(kind));
    }
    bool DrawFramebufferRow(std::int32_t x, std::int32_t y, std::int32_t width,
        std::int32_t source_x, std::uint8_t brightness) override
    {
        return Sprite(backdrop_, 0xE1000100 | (static_cast<std::uint32_t>(source_x) >> 6),
            brightness, Pack(x, y), Pack(width, 1), static_cast<std::uint32_t>(y) << 8);
    }
    bool DrawBlackFill(
        std::int32_t x, std::int32_t y, std::int32_t width, std::int32_t height) override
    {
        return render::InsertThreeWordPrimitive(
            backdrop_, { 0x60000000, Pack(x, y), Pack(width, height) });
    }

    menu::TextBox box;

private:
    static bool Sprite(Queue& queue, std::uint32_t mode, int brightness,
        std::uint32_t position, std::uint32_t size, std::uint32_t texture)
    {
        const render::SpriteRequest request { mode, brightness,
            std::bit_cast<std::int32_t>(position), std::bit_cast<std::int32_t>(size) };
        auto packet = render::BuildSpritePacket(request, queue.ChainLink());
        packet[render::kSpriteTextureWord] = texture;
        return queue.Emit(packet);
    }
    bool alternate_;
    std::array<std::uint32_t, 3> sizes_;
    std::array<std::uint32_t, 3> textures_;
    Queue header_;
    Queue backdrop_;
};

menu::TextBox ReadBox()
{
    menu::TextBox box;
    box.y = static_cast<std::int16_t>(Read());
    box.frame_x = static_cast<std::int16_t>(Read());
    box.frame_y = static_cast<std::int16_t>(Read());
    box.frame_width = static_cast<std::int16_t>(Read());
    box.frame_height = static_cast<std::int16_t>(Read());
    box.line_count = std::bit_cast<std::int8_t>(static_cast<std::uint8_t>(Read()));
    box.open_duration = std::bit_cast<std::int8_t>(static_cast<std::uint8_t>(Read()));
    box.state = static_cast<std::int16_t>(Read());
    box.brightness = static_cast<std::uint8_t>(Read());
    box.flags.alignment = static_cast<std::uint8_t>(Read());
    box.flags.frame_shape = static_cast<std::uint8_t>(Read());
    box.flags.frame_part = static_cast<std::uint8_t>(Read());
    return box;
}

void PrintPackets(const Memory& memory, std::size_t start)
{
    std::cout << memory.words.size() * sizeof(std::uint32_t) << ' '
              << memory.words.size() - start;
    for (const auto word : std::span(memory.words).subspan(start)) {
        std::cout << ' ' << std::bit_cast<std::int32_t>(word);
    }
    for (const auto head : memory.heads) {
        std::cout << ' ' << std::bit_cast<std::int32_t>(head);
    }
}
} // namespace

int main()
{
    int mode = 0;
    while (std::cin >> mode) {
        const auto first = static_cast<std::uint32_t>(Read());
        const auto second = static_cast<std::uint32_t>(Read());
        const bool alternate = Read() != 0;
        const auto box = ReadBox();
        std::array<std::uint32_t, 3> sizes { };
        std::array<std::uint32_t, 3> textures { };
        for (auto& size : sizes) {
            size = static_cast<std::uint32_t>(Read());
        }
        for (auto& texture : textures) {
            texture = static_cast<std::uint32_t>(Read());
        }
        const auto start = static_cast<std::size_t>(Read());
        Memory memory;
        memory.words.resize(start);
        for (auto& head : memory.heads) {
            head = static_cast<std::uint32_t>(Read());
        }
        const auto count = static_cast<std::size_t>(Read());
        std::vector<std::uint8_t> shape(count);
        for (auto& byte : shape) {
            byte = static_cast<std::uint8_t>(Read());
        }
        World world(memory, box, alternate, sizes, textures);
        bool accepted = true;
        switch (mode) {
        case 0:
            accepted = menu::DrawInformationBoxHeader(
                world, std::bit_cast<std::int32_t>(first), second);
            break;
        case 1:
            accepted = menu::DrawInformationBoxBackdrop(world);
            break;
        case 2:
            std::cout << std::bit_cast<std::int32_t>(
                menu::FlipTextBoxShapePoint(first, second));
            break;
        case 3: {
            Queue foreground(memory, 2);
            Queue shadow(memory, 1);
            accepted = menu::DrawTextBoxFrame(foreground, shadow, shape,
                { box.frame_x, box.frame_y, box.frame_width, box.frame_height,
                    box.flags.alignment, box.brightness, box.flags.frame_part })
                           .has_value();
            break;
        }
        default:
            throw std::runtime_error("invalid text-frame differential mode");
        }
        if (!accepted) {
            throw std::runtime_error("native core refused a valid original vector");
        }
        if (mode != 2) {
            PrintPackets(memory, start);
        }
        std::cout << " 0\n";
    }
}
