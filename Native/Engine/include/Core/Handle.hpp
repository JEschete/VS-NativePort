#pragma once

#include "Core/Types.hpp"

#include <compare>
#include <limits>

namespace VS::Core {

// Dense runtime handle into a Registry<T> (plan §9.2). Data and saves use string IDs.
template <class T>
class Handle {
public:
    static constexpr u32 InvalidIndex = std::numeric_limits<u32>::max();

    constexpr Handle() = default;
    constexpr explicit Handle(u32 index) : m_index(index) {}

    constexpr u32 Index() const { return m_index; }
    constexpr bool Valid() const { return m_index != InvalidIndex; }
    constexpr explicit operator bool() const { return Valid(); }
    constexpr auto operator<=>(const Handle&) const = default;

private:
    u32 m_index = InvalidIndex;
};

} // namespace VS::Core
