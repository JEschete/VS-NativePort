#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <iostream>
#include <optional>
#include <span>
#include <vector>

#include "Menu/SceneNotifications.hpp"
#include "Render/EncodedText.hpp"

namespace {
namespace menu = VS::Menu;
constexpr std::uint32_t kBuffer = 0x80500000;
constexpr std::uint32_t kActionName = 0x8004B9F8;
constexpr std::uint32_t kItemNames = 0x800EBF58;
constexpr std::int32_t kFirstItem = 0x1CA;

struct Input {
    int operation = 0;
    std::int32_t kind = 0;
    std::int32_t subject = 0;
    std::int32_t timed_argument = 0;
    unsigned active = 0;
    unsigned timer = 0;
    unsigned timed = 0;
    std::uint32_t flags = 0;
    std::uint32_t context = 0;
    std::int32_t poll = 0;
    std::int32_t tick = 0;
    unsigned seed = 0;
    unsigned type = 0;
};

class Host final : public menu::SceneNotificationHost {
public:
    Host(menu::SceneNotificationState& state, const Input& input)
        : flags(input.flags)
        , context(input.context)
        , state_(state)
        , input_(input)
    {
        InitializeName(action_name_, input.seed);
        InitializeName(item_name_, input.seed + 31);
        for (std::size_t index = 0; index < notifications_.size(); ++index)
            notifications_[index] = Text(input.seed, index);
        for (std::size_t index = 0; index < actions_.size(); ++index)
            actions_[index] = Text(input.seed, 20 + index);
        mastered_ = Text(input.seed, 40);
        for (std::size_t index = 0; index < buffer.size(); ++index)
            buffer[index] = static_cast<std::uint8_t>(input.seed + index * 5);
    }
    std::optional<menu::SceneNotificationAction> Action(std::int32_t index) const override
    {
        if (index != input_.subject || index < 0 || index >= 256)
            return std::nullopt;
        return menu::SceneNotificationAction { action_name_,
            static_cast<std::uint8_t>(input_.type) };
    }
    std::optional<menu::SceneNotificationText> ItemName(
        std::int32_t subject) const override
    {
        if (subject != input_.subject || subject < kFirstItem
            || subject > kFirstItem + 32)
            return std::nullopt;
        return item_name_;
    }
    std::optional<menu::SceneNotificationText> NotificationText(
        std::int32_t kind) const override
    {
        if (kind < 0 || static_cast<std::size_t>(kind) >= notifications_.size())
            return std::nullopt;
        return notifications_[static_cast<std::size_t>(kind)];
    }
    std::optional<menu::SceneNotificationText> ActionText(
        std::uint8_t type) const override
    {
        if (type >= actions_.size())
            return std::nullopt;
        return actions_[type];
    }
    std::optional<menu::SceneNotificationText> SpellMasteredText() const override
    {
        return mastered_;
    }
    bool CanFormat(menu::SceneNotificationText source,
        menu::SceneNotificationText subject, std::size_t capacity) const override
    {
        VS::Render::EncodedTextArguments arguments;
        arguments.strings[0] = subject;
        const auto size = VS::Render::EncodedTextBytes(source, arguments);
        return size && *size <= capacity;
    }
    std::span<std::uint8_t> Allocate() override
    {
        Record(0, { menu::kSceneNotificationBytes });
        return buffer;
    }
    void InitializeBox() override { Record(1, { 7, 0x304, 0, 1, 0, 3, 0, 0 }); }
    void BindSubject(menu::SceneNotificationText subject) override
    {
        subject_ = subject;
        context = subject.data() == action_name_.data()
                    ? kActionName + static_cast<std::uint32_t>(input_.subject) * 52
                    : kItemNames
                          + static_cast<std::uint32_t>(input_.subject - kFirstItem) * 24;
    }
    void ClearFrameShape() override { flags &= ~0xF00U; }
    bool Format(
        std::span<std::uint8_t> destination, menu::SceneNotificationText source) override
    {
        VS::Render::EncodedTextArguments arguments;
        arguments.strings[0] = subject_;
        return VS::Render::FormatEncodedText(destination, source, arguments).has_value();
    }
    void SetText(std::span<std::uint8_t> text) override
    {
        Record(
            2, { 7, kBuffer + static_cast<std::uint32_t>(text.data() - buffer.data()) });
    }
    bool BoxDone() const override { return (flags & 0x40000000U) != 0; }
    std::int32_t PollText() override
    {
        Record(2, { 7, 0 });
        return input_.poll;
    }
    void DismissBox() override { Record(3, { 7 }); }
    void Release(std::span<std::uint8_t> allocation) override
    {
        Record(4,
            { kBuffer + static_cast<std::uint32_t>(allocation.data() - buffer.data()) });
    }

    std::array<std::uint8_t, menu::kSceneNotificationBytes> buffer { };
    std::uint32_t flags = 0;
    std::uint32_t context = 0;
    std::vector<std::int64_t> events;

private:
    template <std::size_t Size>
    static void InitializeName(std::array<std::uint8_t, Size>& name, unsigned seed)
    {
        for (std::size_t index = 0; index < name.size(); ++index)
            name[index] = static_cast<std::uint8_t>(1 + (seed + index * 7) % 200);
        name[1 + seed % (Size - 1)] = 0xE7;
    }
    static std::array<std::uint8_t, 5> Text(unsigned seed, std::size_t index)
    {
        return { static_cast<std::uint8_t>(1 + (seed + index * 13) % 200), 0xFF, 0,
            static_cast<std::uint8_t>(1 + (seed + index * 17) % 200), 0xE7 };
    }
    void Record(std::int64_t code, std::initializer_list<std::int64_t> arguments)
    {
        events.push_back(code);
        events.push_back(static_cast<std::int64_t>(arguments.size()));
        events.insert(events.end(), arguments);
        events.insert(events.end(), { state_.buffer.empty() ? 0 : kBuffer, state_.timer,
                                        state_.timed, context, flags });
    }
    menu::SceneNotificationState& state_;
    const Input& input_;
    std::array<std::uint8_t, 24> action_name_ { };
    std::array<std::uint8_t, 12> item_name_ { };
    std::array<std::array<std::uint8_t, 5>, 11> notifications_ { };
    std::array<std::array<std::uint8_t, 5>, 8> actions_ { };
    std::array<std::uint8_t, 5> mastered_ { };
    menu::SceneNotificationText subject_;
};

bool RunVector()
{
    Input input;
    if (!(std::cin >> input.operation >> input.kind >> input.subject
            >> input.timed_argument >> input.active >> input.timer >> input.timed
            >> input.flags >> input.context >> input.poll >> input.tick >> input.seed
            >> input.type))
        return false;
    menu::SceneNotificationState state;
    Host host(state, input);
    if (input.active)
        state.buffer = host.buffer;
    state.timer = static_cast<std::uint8_t>(input.timer);
    state.timed = static_cast<std::uint8_t>(input.timed);
    int refused = 0;
    std::int32_t result = 0;
    switch (input.operation) {
    case 0:
        refused = menu::ShowSceneNotification(
                      state, host, input.kind, input.subject, input.timed_argument)
               != menu::SceneNotificationResult::kShown;
        break;
    case 1:
        result = menu::IsSceneNotificationPending(state, host, input.tick);
        break;
    default:
        menu::ReleaseSceneNotification(state, host);
        break;
    }
    std::cout << refused << ' ' << result << ' ' << (state.buffer.empty() ? 0 : kBuffer)
              << ' ' << static_cast<unsigned>(state.timer) << ' '
              << static_cast<unsigned>(state.timed) << ' ' << host.context << ' '
              << host.flags;
    for (auto byte : host.buffer)
        std::cout << ' ' << static_cast<unsigned>(byte);
    std::cout << ' ' << host.events.size();
    for (auto word : host.events)
        std::cout << ' ' << word;
    std::cout << '\n';
    return true;
}
} // namespace

int main()
{
    while (RunVector()) { }
}
