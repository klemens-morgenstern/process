#ifndef PROCESS_PROCESS_ENV_HPP
#define PROCESS_PROCESS_ENV_HPP

#include <detail/process/config.hpp>
#include <ranges>
#include <utility>
#include <tuple>
#include <string_view>
#include <initializer_list>
#include <iterator>
#include <numeric>


namespace PROCESS_NAMESPACE
{

namespace detail {

template<typename T>
concept convertible_to_string_view =
        std::convertible_to<T,    std::string_view>
     || std::convertible_to<T,   std::wstring_view>
     || std::convertible_to<T,  std::u8string_view>
     || std::convertible_to<T, std::u16string_view>
     || std::convertible_to<T, std::u32string_view> ;
}

template<typename T>
    requires
        detail::convertible_to_string_view<T> ||
         (requires (T t)
          {
              {std::tuple_size<T>{}} -> std::convertible_to<std::integral_constant<size_t,2u>>;
              {std::get<0>(t)} -> detail::convertible_to_string_view;
              {std::get<1>(t)} -> detail::convertible_to_string_view;
          } ||
          requires (T t)
          {
              {std::tuple_size<T>{}} -> std::convertible_to<std::integral_constant<size_t,2u>>;
              {std::get<0>(t)} -> detail::convertible_to_string_view;
              {std::get<1>(t)} -> std::ranges::range;
              {*std::ranges::begin(std::get<1>(t))} -> detail::convertible_to_string_view;
          })
struct process_env
{
#if defined(__unix__)
    using char_type = char;
#else
    using char_type = wchar_t;
#endif
    template<typename Char>
    static auto convert(std::basic_string_view<Char> view, const std::locale &loc = std::locale{})
    {
        if (std::is_same_v<Char, char_type> && loc == std::locale{})
            return std::string(view.begin(), view.end());
        //somehow not yet working. no idea why.


        auto &fct = std::use_facet<std::codecvt<char_type, Char, std::mbstate_t>>(loc);
        std::mbstate_t mb = std::mbstate_t();
        const auto len = fct.length(mb, view.data(), view.data() + view.size(), view.max_size());

        const Char *from_next;
        char_type *to_next;
        std::basic_string<char_type> res;
        res.resize(len);

        mb = std::mbstate_t();
        fct.in(mb, view.data(), view.data() + view.size(), from_next,
                    res.data(),  res.data() +  res.size(), to_next);

        res.resize(to_next - &res[0]);
        return res;
    }

    template<typename Range>
    requires (
            requires(Range t)
            {
                { std::tuple_size<Range>{}} -> std::convertible_to<std::integral_constant<size_t, 2u>>;
                { std::get<0>(t) } -> detail::convertible_to_string_view;
                { std::get<1>(t) } -> detail::convertible_to_string_view;
            })
    static auto convert(Range t, const std::locale &loc = std::locale{})
    {
        auto[key, value] = t;
        auto key_ = convert(key, loc), value_ = convert(value, loc);
        return key_ + detail::process::equal_sign<char_type> + value_;
    }

    template<typename Range>
    requires (
            requires(Range t)
            {
                { std::tuple_size<Range>{}} -> std::convertible_to<std::integral_constant<size_t, 2u>>;
                { std::get<0>(t) } -> detail::convertible_to_string_view;
                { std::get<1>(t) } -> std::ranges::range;
                { *std::ranges::begin(std::get<1>(t)) } -> detail::convertible_to_string_view;
            })
    static auto convert(T t, const std::locale &loc = std::locale{})
    {
        auto[key, values] = t;
        auto key_ = convert(key);
        auto value = values | std::views::transform([&](auto &val) { return convert(val, loc) + detail::process::env_sep<char_type>; })
                     | std::views::join;
        std::basic_string<char_type> value_{std::ranges::begin(value), std::ranges::end(value)};
        value_.pop_back();
        return key_ + detail::process::equal_sign<char_type> + value_;
    }


    template<typename P>
    static auto conv(P && val, const std::locale & loc = std::locale{})
    {
        return build_data(val | std::views::transform([&](auto &val) { return convert(val, loc); }));
    }
#if defined(__unix__)
    std::vector<std::string> data;
    std::vector<char*> ptrs{build_ptrs()};

    std::vector<char*> build_ptrs() {
        auto tv = data | std::views::transform([](std::string & data){return data.data();});
        std::vector<char*> res;
        res.assign(std::ranges::begin(tv), std::ranges::end(tv));
        res.push_back(nullptr);
        return res;
    }
#else
    std::vector<wchar_t> data;
#endif

public:

#if defined(__unix__)

    template <class Executor>
    void on_setup(Executor &exec)
    {
        exec.env = ptrs.data();
    }

#else

    template <class WindowsExecutor>
    void on_setup(WindowsExecutor &exec) const
    {
        if (data.empty() || (data[0] == detail::process::null_char<wchar_t>))
        {
            exec.set_error(std::error_code(ERROR_BAD_ENVIRONMENT, std::system_category()),
                    "Empty Environment");
        }

        exec.env = data.data();
        exec.creation_flags |= CREATE_UNICODE_ENVIRONMENT;
    }
#endif

private:


#if defined(__unix__)
    template<std::ranges::range Range>
    static std::vector<std::string> build_data(const Range & data) { return {std::ranges::begin(data), std::ranges::end(data)}; }
#else
    template<std::ranges::range Range>
        requires std::convertible_to<Range, std::wstring_view>
    static std::vector<wchar_t> build_data(const Range & data)
    {
        const auto r = data | std::ranges::views::transform([](const std::wstring_view sv){return sv.size() + 1;});
        const auto sz = std::accumulate(std::ranges::begin(r), std::ranges::end(r), 1); //overall size
        std::vector<wchar_t> res;
        res.reserve(sz);

        for (const std::wstring_view d : data)
        {
            res.insert(res.end(), d.begin(), d.end());
            res.push_back(detail::process::null_char<wchar_t>);
        }
        res.push_back(detail::process::null_char<wchar_t>);
        return res;
    }

#endif
public:

    process_env(std::initializer_list<T> r) : data(conv(r)) {}
    process_env(std::initializer_list<T> r, const std::locale & loc) : data(conv(r)) {}

    template<std::ranges::range Range>
        requires(std::convertible_to<std::ranges::range_value_t<Range>, T>)
    process_env(Range r) : data(conv(r)) {}

    template<std::ranges::range Range>
        requires(std::convertible_to<std::ranges::range_value_t<Range>, T>)
    process_env(Range r, const std::locale & loc) : data(conv(r, loc)) {}

    template<std::input_iterator InputIt>
        requires(std::convertible_to<typename std::iterator_traits<InputIt>::value_type, T>)
    process_env(InputIt first, InputIt last) : data(conv(std::ranges::subrange(first, last))) {}

    template<std::input_iterator InputIt>
        requires(std::convertible_to<typename std::iterator_traits<InputIt>::value_type, T>)
    process_env(InputIt first, InputIt last,  const std::locale & loc) : data(conv(std::ranges::subrange(first, last), loc)) {}

};


template<std::ranges::range Range>
process_env(Range r) -> process_env<std::ranges::range_value_t<Range>>;

template<std::ranges::range Range>
process_env(Range && r, const std::locale & loc)  -> process_env<std::ranges::range_value_t<Range>>;


template<std::input_iterator InputIt>
process_env(InputIt first, InputIt last) -> process_env<typename std::iterator_traits<InputIt>::value_type>;

template<std::input_iterator InputIt>
process_env(InputIt first, InputIt last, const std::locale & loc) -> process_env<typename std::iterator_traits<InputIt>::value_type>;

process_env(std::initializer_list<std::pair<std::   string_view, std::   string_view>> r) -> process_env<std::pair<std::   string_view, std::   string_view>>;
process_env(std::initializer_list<std::pair<std::  wstring_view, std::  wstring_view>> r) -> process_env<std::pair<std::  wstring_view, std::  wstring_view>>;
process_env(std::initializer_list<std::pair<std:: u8string_view, std:: u8string_view>> r) -> process_env<std::pair<std:: u8string_view, std:: u8string_view>>;
process_env(std::initializer_list<std::pair<std::u16string_view, std::u16string_view>> r) -> process_env<std::pair<std::u16string_view, std::u16string_view>>;
process_env(std::initializer_list<std::pair<std::u32string_view, std::u32string_view>> r) -> process_env<std::pair<std::u32string_view, std::u32string_view>>;

process_env(std::initializer_list<std::pair<std::   string_view, std::initializer_list<std::   string_view>>> r) -> process_env<std::pair<std::   string_view, std::initializer_list<std::   string_view>>>;
process_env(std::initializer_list<std::pair<std::  wstring_view, std::initializer_list<std::  wstring_view>>> r) -> process_env<std::pair<std::  wstring_view, std::initializer_list<std::  wstring_view>>>;
process_env(std::initializer_list<std::pair<std:: u8string_view, std::initializer_list<std:: u8string_view>>> r) -> process_env<std::pair<std:: u8string_view, std::initializer_list<std:: u8string_view>>>;
process_env(std::initializer_list<std::pair<std::u16string_view, std::initializer_list<std::u16string_view>>> r) -> process_env<std::pair<std::u16string_view, std::initializer_list<std::u16string_view>>>;
process_env(std::initializer_list<std::pair<std::u32string_view, std::initializer_list<std::u32string_view>>> r) -> process_env<std::pair<std::u16string_view, std::initializer_list<std::u32string_view>>>;

}

#endif //PROCESS_PROCESS_IO_HPP
