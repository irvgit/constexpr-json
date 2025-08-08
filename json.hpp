#pragma once

#if __cplusplus < 202302L
    #error out of date c++ version, compile with -stdc++=2c
#elif defined(__clang__) && __clang_major__ < 19
    #error out of date clang, compile with latest version
#elif !defined(__clang__) && defined(__GNUC__) && __GNUC__ < 15
    #error out of date g++, compile with latest version
#elif defined(_MSC_VER) && _MSC_VER < 1943
    #error out of date msvc, compile with latest version
#else

#include <algorithm>
#include <charconv>
#include <cinttypes>
#include <concepts>
#include <cstddef>
#include <cstring>
#include <memory>
#include <ranges>
#include <type_traits>
#include <utility>

namespace json {
    namespace detail {
        template <
            typename tp_type_t,
            typename tp_as_type_t
        >
        concept input_range_of =
        std::ranges::input_range<tp_type_t> &&
        std::same_as<
            tp_as_type_t,
            std::ranges::range_value_t<tp_type_t>
        >;
        template <bool tp_condition, typename tp_type_t>
        auto constexpr conditional_value = std::conditional_t<tp_condition, tp_type_t, decltype(std::ignore)>{};

        struct is_whitespace_fn {
            auto constexpr operator()(const char p_char) const noexcept -> bool {
                return p_char == ' ' || p_char == '\n';
            }
        };
        auto constexpr is_whitespace = is_whitespace_fn{};
    }

    enum class result_code : std::uint8_t {
        none,
        success,
        data_was_empty,
        invalid_opening_delimiter,
        unexpected_end_of_data_after_opening_delimiter,
        expected_opening_quote_delimiter_for_key_in_json_object,
        expected_closing_quote_delimiter_for_key_in_json_object,
        expected_colon_after_key_but_reached_end_of_data,
        expected_colon_after_key_in_json_object,
        expected_value_after_key_but_reached_end_of_data,
        expected_closing_delimiter_for_nested_array_or_object_value_but_reached_end_of_data,
        expected_closing_quote_delimiter_for_string_value_but_reached_end_of_data,
        unexpected_end_of_data_after_value,
        value_not_found,
        invalid_closing_delimiter,
        expected_closing_delimiter_but_reached_end_of_data
    };
    enum class entity_type : std::uint8_t {
        none,
        value,
        object,
        array
    };
    namespace detail {
        template <input_range_of<char> tp_view_t>
        struct read_t {
            template <input_range_of<char>>
            friend struct read_t;
        private:
            result_code m_result_code = result_code::success;
            entity_type m_entity_type = entity_type::object;
            tp_view_t   m_data        = tp_view_t{};

            template <typename tp_self_t>
            auto constexpr impl(
                this tp_self_t&& p_self,
                auto&&           p_key_or_index
            ) -> auto { 
                if (p_self.m_result_code != result_code::success && p_self.m_result_code != result_code::none)
                    return detail::read_t{
                        std::forward<tp_self_t>(p_self).m_result_code,
                        std::forward<tp_self_t>(p_self).m_entity_type,
                        std::ranges::subrange{std::forward<tp_self_t>(p_self).m_data}
                    };
                auto constexpr l_is_array          = std::integral<std::remove_cvref_t<decltype(p_key_or_index)>>;
                auto constexpr l_opening_delimiter = l_is_array ? '[' : '{';
                auto constexpr l_closing_delimiter = l_is_array ? ']' : '}';
                auto l_index = conditional_value<l_is_array, std::size_t>;
                auto l_key   = conditional_value<!l_is_array, std::string_view>;
                auto l_first = std::ranges::begin(std::forward<tp_self_t>(p_self).m_data);
                auto l_last  = std::ranges::end(std::forward<tp_self_t>(p_self).m_data);
                auto l_found = false;
                auto l_error = [&](auto p_error_code) { return detail::read_t{p_error_code, entity_type::none, std::ranges::subrange{std::move(l_first), std::move(l_last)}}; };
                for (; l_first != l_last && is_whitespace(*l_first); ++l_first);
                if (l_first == l_last)
                    return l_error(result_code::data_was_empty);
                if (*l_first != l_opening_delimiter)
                    return l_error(result_code::invalid_opening_delimiter);
                ++l_first;
                for (; l_first != l_last && is_whitespace(*l_first); ++l_first);
                if (l_first == l_last)
                    return l_error(result_code::unexpected_end_of_data_after_opening_delimiter);
                while (l_first != l_last) {
                    if constexpr (!l_is_array) {
                        if (*l_first != '\"')
                            return l_error(result_code::expected_opening_quote_delimiter_for_key_in_json_object);
                        ++l_first;
                        auto l_key_quote_senintel = std::ranges::adjacent_find(l_first, l_last, [](auto a, auto b) noexcept { return a != '\\' && b == '\"'; });
                        if (l_key_quote_senintel == l_last)
                            return l_error(result_code::expected_closing_quote_delimiter_for_key_in_json_object);
                        ++l_key_quote_senintel;
                        l_found = std::ranges::equal(p_key_or_index, std::ranges::subrange{l_first, l_key_quote_senintel});
                        if (++(l_first = std::move(l_key_quote_senintel)) == l_last)
                            return l_error(result_code::expected_colon_after_key_but_reached_end_of_data);
                        for (; l_first != l_last && is_whitespace(*l_first); ++l_first);
                        if (*l_first != ':')
                            return l_error(result_code::expected_colon_after_key_in_json_object);
                        for (++l_first; l_first != l_last && is_whitespace(*l_first); ++l_first);
                        if (l_first == l_last)
                            return l_error(result_code::expected_value_after_key_but_reached_end_of_data);
                    }
                    else if constexpr (l_is_array)
                        l_found = l_index == p_key_or_index;
                    if (*l_first == '{' || *l_first == '[') {
                        if (l_found)
                            return detail::read_t{
                                result_code::success,
                                *l_first == '{' ? entity_type::object : entity_type::array,
                                std::ranges::subrange{std::move(l_first), std::move(l_last)}
                            };
                        auto const l_opening = *l_first;
                        auto const l_closing = l_opening == '{' ? '}' : ']';
                        for (auto l_count = std::intmax_t{0}; l_first != l_last && (l_count != 0 || *l_first == l_opening); ++l_first)
                            l_count += *l_first == l_opening ? 1 : *l_first == l_closing && l_count != 0 ? -1 : 0;
                        if (l_first == l_last)
                            return l_error(result_code::expected_closing_delimiter_for_nested_array_or_object_value_but_reached_end_of_data);
                    }
                    else {
                        auto const l_is_value_string = *l_first == '\"';
                        auto l_value_sentinel = std::ranges::iterator_t<tp_view_t>{};
                        if (l_is_value_string) {
                            l_value_sentinel = std::ranges::adjacent_find(++l_first, l_last, [](auto a, auto b) noexcept { return a != '\\' && b == '\"'; });
                            if (l_value_sentinel == l_last)
                                return l_error(result_code::expected_closing_quote_delimiter_for_string_value_but_reached_end_of_data);
                            ++l_value_sentinel;
                        }
                        else {
                            l_value_sentinel = std::ranges::find_if(l_first, l_last, [](auto a) noexcept { return is_whitespace(a) || a == ',' || a == '}' || a == ']'; });
                            if (l_value_sentinel == l_last)
                                return l_error(result_code::unexpected_end_of_data_after_value);
                        }
                        if (l_found)
                            return detail::read_t{
                                result_code::success,
                                entity_type::value,
                                std::ranges::subrange{std::move(l_first), std::move(l_value_sentinel)}
                            };
                        l_first = std::move(l_value_sentinel);
                        if (l_is_value_string)
                            l_first++;
                    }
                    for (; l_first != l_last && is_whitespace(*l_first); ++l_first);
                    if (l_first == l_last)
                        return l_error(result_code::unexpected_end_of_data_after_value);
                    if (*l_first == ',')
                        ++l_first;
                    else if (*l_first == l_closing_delimiter)
                        return l_error(result_code::value_not_found);
                    else return l_error(result_code::invalid_closing_delimiter);
                    for (; l_first != l_last && is_whitespace(*l_first); ++l_first); 
                    if constexpr (l_is_array)
                        ++l_index;
                }
                return l_error(result_code::expected_closing_delimiter_but_reached_end_of_data);
            }
        public:
            template <
                typename             tp_self_t,
                input_range_of<char> tp_input_range_of_char_t
            >
            auto constexpr at_key[[nodiscard]](
                this tp_self_t&&           p_self,
                tp_input_range_of_char_t&& p_key
            ) -> read_t<std::ranges::subrange<
                    std::ranges::iterator_t<tp_view_t>,
                    std::ranges::sentinel_t<tp_view_t>>
            > {
                return std::forward<tp_self_t>(p_self).impl(p_key);
            }
            template <typename tp_self_t>
            auto constexpr at_index[[nodiscard]](
                this tp_self_t&&  p_self,
                const std::size_t p_index
            ) -> read_t<std::ranges::subrange<
                std::ranges::iterator_t<tp_view_t>,
                std::ranges::sentinel_t<tp_view_t>>
            > {
                return std::forward<tp_self_t>(p_self).impl(p_index);
            }

            template <
                typename tp_type_t,
                typename tp_self_t,
                class... tp_extra_arguments_ts
            >
            auto constexpr to[[nodiscard]](
                this tp_self_t&&           p_self,
                tp_extra_arguments_ts&&... p_extra_arguments
            ) {
                if constexpr (std::ranges::input_range<tp_type_t>) {
                    if constexpr (std::ranges::view<tp_type_t>)
                        return tp_type_t{
                            std::forward<tp_self_t>(p_self).m_data,
                            std::forward<tp_extra_arguments_ts>(p_extra_arguments)...
                        };
                    else if constexpr (std::is_array_v<tp_type_t>) {
                        auto l_result = tp_type_t{};
                        std::ranges::copy_n(
                            std::ranges::begin(std::forward<tp_self_t>(p_self).m_data),
                            tp_type_t::size,
                            std::ranges::data(l_result)
                        );
                    }
                    else return std::ranges::to<tp_type_t>(
                        std::forward<tp_self_t>(p_self).m_data,
                        std::forward<tp_extra_arguments_ts>(p_extra_arguments)...
                    );
                }
                else if constexpr (std::integral<tp_type_t>) {
                    auto l_result = tp_type_t{};
                    return !std::to_underlying(std::from_chars(
                        std::to_address(std::ranges::begin(std::forward<tp_self_t>(p_self).m_data)),
                        std::to_address(std::ranges::data(std::forward<tp_self_t>(p_self).m_data)),
                        std::forward<tp_extra_arguments_ts>(p_extra_arguments)...
                    ).ec) ? std::optional<tp_type_t>{l_result, std::forward<tp_extra_arguments_ts>(p_extra_arguments)...} : std::optional<tp_type_t>{};
                }
                else if constexpr (std::floating_point<tp_type_t>) {
                    static_assert(false, "constexpr from chars float requires c++26");
                }
                else if constexpr (std::is_null_pointer_v<tp_type_t>)
                    return nullptr;
                else if constexpr (std::is_pointer_v<tp_type_t>) {
                    if (auto l_as_integer = std::forward<tp_self_t>(p_self).template to<std::size_t>())
                        return std::optional<tp_type_t>{
                            reinterpret_cast<tp_type_t>(l_as_integer.value()),
                            std::forward<tp_extra_arguments_ts>(p_extra_arguments)...
                        };
                    else return std::optional<tp_type_t>{};
                }
                else if constexpr (
                    std::is_trivially_copy_constructible_v<tp_type_t> &&
                    std::is_default_constructible_v<tp_type_t> &&
                    std::ranges::contiguous_range<tp_view_t>
                ) {
                    if (std::ranges::size(std::forward<tp_self_t>(p_self).m_data) >= sizeof(tp_type_t)) {
                        auto l_result = tp_type_t{};
                        std::memcpy(
                            std::addressof(l_result),
                            std::ranges::data(std::forward<tp_self_t>(p_self).m_data),
                            sizeof(tp_type_t)
                        );
                        std::optional<tp_type_t>{
                            std::move(l_result),
                            std::forward<tp_extra_arguments_ts>(p_extra_arguments)...
                        };
                    }
                    else std::optional<tp_type_t>{};
                }
                else static_assert(false);
            }
            
            auto constexpr result[[nodiscard]]() const noexcept -> result_code { return m_result_code; }
            auto constexpr entity[[nodiscard]]() const noexcept -> entity_type { return m_entity_type; }
            auto constexpr value[[nodiscard]]()  const noexcept -> tp_view_t   { return m_data; }
            
            template <input_range_of<char> tp_input_range_t>
            constexpr explicit read_t(
                result_code        p_result_code,
                entity_type        p_entity_type,
                tp_input_range_t&& p_data
            ) noexcept :
            m_result_code{p_result_code},
            m_entity_type{p_entity_type},
            m_data{std::views::all(p_data)}
            {}
        };
        template <input_range_of<char> tp_input_range_t>
        read_t(result_code, entity_type, tp_input_range_t&&) -> read_t<std::views::all_t<tp_input_range_t>>;
    
        struct read_fn {
            template <input_range_of<char> tp_input_range_of_char_t>
            auto constexpr operator()(tp_input_range_of_char_t&& p_data)
            const noexcept(noexcept(
                read_t{
                    result_code::none,
                    entity_type::object,
                    std::declval<tp_input_range_of_char_t>()
                }
            ))
            -> decltype(
                read_t{
                    result_code::none,
                    entity_type::object,
                    std::forward<tp_input_range_of_char_t>(p_data)
                }
            ){
                return read_t{
                    result_code::none,
                    entity_type::object,
                    std::forward<tp_input_range_of_char_t>(p_data)
                };
            }
        };
    }
    auto constexpr read = detail::read_fn{};
}

#endif
