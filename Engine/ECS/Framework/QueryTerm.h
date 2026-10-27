#pragma once

#include <cstdint>
#include <type_traits>

// Describe component access and optional chunk-level change filtering independently.
enum class QueryFlags : std::uint8_t
{
    None        = 0,
    Read        = 1 << 0,
    Write       = 1 << 1, // Combine with Read; mutable references do not enforce write-only access.
    ReadWrite   = (1 << 0) | (1 << 1),
    Changed     = 1 << 2
};

constexpr QueryFlags operator|(QueryFlags left, QueryFlags right) noexcept
{
    return static_cast<QueryFlags>(static_cast<std::uint8_t>(left) |
        static_cast<std::uint8_t>(right));
}

constexpr QueryFlags operator&(QueryFlags left, QueryFlags right) noexcept
{
    return static_cast<QueryFlags>(static_cast<std::uint8_t>(left) &
        static_cast<std::uint8_t>(right));
}

// Compile-time query metadata; no QueryTerm objects are stored in entity rows.
template <typename Component, QueryFlags Flags>
struct QueryTerm
{
    static_assert(std::is_object_v<Component> && !std::is_array_v<Component> &&
        std::is_same_v<Component, std::remove_cv_t<Component>>,
        "QueryTerm requires an unqualified component type; specify access with QueryFlags");
    static_assert((static_cast<std::uint8_t>(Flags) &
        ~static_cast<std::uint8_t>(QueryFlags::ReadWrite | QueryFlags::Changed)) == 0,
        "QueryTerm contains unsupported flags");
    static_assert((Flags & QueryFlags::Read) != QueryFlags::None,
        "QueryTerm requires Read or ReadWrite access");

    using ComponentType = Component;
    static constexpr bool writable = (Flags & QueryFlags::Write) != QueryFlags::None;
    static constexpr bool changed = (Flags & QueryFlags::Changed) != QueryFlags::None;
    using Reference = std::conditional_t<writable, Component&, const Component&>;
};

namespace QueryDetail
{
// Preserve plain T/const T queries as unwatched read-write/read access respectively.
template <typename Term>
struct TermTraits : QueryTerm<std::remove_const_t<Term>,
    std::is_const_v<Term> ? QueryFlags::Read : QueryFlags::ReadWrite>
{
};

template <typename Component, QueryFlags Flags>
struct TermTraits<QueryTerm<Component, Flags>> : QueryTerm<Component, Flags>
{
};

// Reject repeated component columns even when their query flags differ.
template <typename... Components>
inline constexpr bool uniqueComponentTypes = true;

template <typename First, typename... Rest>
inline constexpr bool uniqueComponentTypes<First, Rest...> =
    (!std::is_same_v<First, Rest> && ...) && uniqueComponentTypes<Rest...>;

template <typename... Terms>
concept UniqueTerms = sizeof...(Terms) > 0 &&
    uniqueComponentTypes<typename TermTraits<Terms>::ComponentType...>;

template <typename... Terms>
inline constexpr bool hasChangedTerms = (TermTraits<Terms>::changed || ...);
} // namespace QueryDetail
