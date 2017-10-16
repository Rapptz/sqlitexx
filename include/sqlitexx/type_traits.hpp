// The MIT License (MIT)

// Copyright (c) 2017 Danny Y.

//  Permission is hereby granted, free of charge, to any person obtaining a
//  copy of this software and associated documentation files (the "Software"),
//  to deal in the Software without restriction, including without limitation
//  the rights to use, copy, modify, merge, publish, distribute, sublicense,
//  and/or sell copies of the Software, and to permit persons to whom the
//  Software is furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
//  OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
//  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
//  DEALINGS IN THE SOFTWARE.

#pragma once

#include <string>
#include <cstddef>
#include <type_traits>
#include <sqlite3.h>

namespace sqlite {
namespace meta {
template<typename...>
struct dependent_false : std::false_type {};

template<unsigned I>
struct rank : rank<I + 1> {};

template<>
struct rank<4> {};

struct select_overload : rank<0> {};

struct otherwise {
    otherwise(...) {}
};

namespace detail {
template<typename T>
struct string_traits {
    static_assert(dependent_false<T>::value, "Must be a string type such as std::string or a literal.");
};

template<typename... Rest>
struct string_traits<std::basic_string<char, Rest...>> {
    static const char* c_str(const std::basic_string<char, Rest...>& str) noexcept {
        return str.c_str();
    }

    static size_t size(const std::basic_string<char, Rest...>& str) noexcept {
        return str.size();
    }
};

template<size_t N>
struct string_traits<char[N]> {
    static const char* c_str(const char (&arr)[N]) noexcept {
        return arr;
    }

    static size_t size(const char (&)[N]) noexcept {
        return N - 1;
    }
};

template<>
struct string_traits<const char*> {
    static const char* c_str(const char* str) noexcept {
        return str;
    }

    static size_t size(const char* str) noexcept {
        return std::char_traits<char>::length(str);
    }
};
} // detail

template<typename T>
struct unqualified {
    using type = std::remove_cv_t<std::remove_reference_t<T>>;
};

template<typename T>
using unqualified_t = typename unqualified<T>::type;

template<typename T>
struct string_traits : detail::string_traits<unqualified_t<T>> {};

template<typename...> struct and_ : std::true_type {};
template<typename T> struct and_<T> : T {};
template<typename T, typename... Args>
struct and_<T, Args...> : std::conditional_t<bool(T::value), and_<Args...>, T> {};

template<typename...> struct or_ : std::false_type {};
template<typename T> struct or_<T> : T {};
template<typename T, typename... Args>
struct or_<T, Args...> : std::conditional_t<bool(T::value), T, or_<Args...>>  {};

template<typename T>
struct not_ : std::integral_constant<bool, !T::value> {};

template<typename T, typename... Args>
struct is_any_of : or_<std::is_same<T, Args>...> {};

template<typename T>
struct is_integer : is_any_of<T, bool, short, int, long, long long> {};

template<typename T>
struct is_character : is_any_of<T, char, wchar_t, char16_t, char32_t> {};

template<typename T, typename = void>
struct bind_traits;

template<>
struct bind_traits<double> {
    static int bind(sqlite3_stmt* ptr, int index, double value) noexcept {
        return sqlite3_bind_double(ptr, index, value);
    }
};

template<>
struct bind_traits<float> : bind_traits<double> {};

template<typename T>
struct bind_traits<T, std::enable_if_t<is_integer<T>::value>> {
    static int do_integer_bind(sqlite3_stmt* ptr, int index, T value, std::true_type) noexcept {
        return sqlite3_bind_int(ptr, index, value);
    }

    static int do_integer_bind(sqlite3_stmt* ptr, int index, T value, std::false_type) noexcept {
        return sqlite3_bind_int64(ptr, index, value);
    }

    static int bind(sqlite3_stmt* ptr, int index, T value) noexcept {
        return do_integer_bind(ptr, index, value, std::integral_constant<bool, (sizeof(T) < 8)>{});
    }
};

template<>
struct bind_traits<decltype(nullptr)> {
    static int bind(sqlite3_stmt* ptr, int index, decltype(nullptr)) noexcept {
        return sqlite3_bind_null(ptr, index);
    }
};

template<typename T, unsigned N>
struct bind_traits<T[N], std::enable_if_t<is_any_of<T, char, char16_t>::value>> {
    static int bind(sqlite3_stmt* ptr, int index, const char* str) noexcept {
        return sqlite3_bind_text(ptr, index, str, N * sizeof(char) - 1, SQLITE_STATIC);
    }

    static int bind(sqlite3_stmt* ptr, int index, const char16_t* str) noexcept {
        return sqlite3_bind_text16(ptr, index, str, N * sizeof(char16_t) - 1, SQLITE_STATIC);
    }
};

template<typename T>
struct bind_traits<const T*, std::enable_if_t<is_any_of<T, char, char16_t>::value>> {
    using Traits = std::char_traits<T>;

    static int bind(sqlite3_stmt* ptr, int index, const char* str) noexcept {
        return sqlite3_bind_text(ptr, index, str, Traits::length(str), SQLITE_TRANSIENT);
    }

    static int bind(sqlite3_stmt* ptr, int index, const char16_t* str) noexcept {
        return sqlite3_bind_text16(ptr, index, str, Traits::length(str) * sizeof(char16_t), SQLITE_TRANSIENT);
    }
};

template<typename CharT, typename... Rest>
struct bind_traits<std::basic_string<CharT, Rest...>, std::enable_if_t<is_any_of<CharT, char, char16_t>::value>> {
    static int bind(sqlite3_stmt* ptr, int index, const std::basic_string<char, Rest...>& str) noexcept {
        return sqlite3_bind_text(ptr, index, str.c_str(), str.size(), SQLITE_TRANSIENT);
    }

    static int bind(sqlite3_stmt* ptr, int index, const std::basic_string<char16_t, Rest...>& str) noexcept {
        return sqlite3_bind_text16(ptr, index, str.c_str(), str.size() * sizeof(char16_t), SQLITE_TRANSIENT);
    }
};

template<typename T, typename = void>
struct column_traits;

template<typename T>
struct column_traits<T, std::enable_if_t<std::is_floating_point<T>::value>> {
    static T convert(sqlite3_stmt* ptr, int index) noexcept {
        return sqlite3_column_double(ptr, index);
    }
};

template<typename T>
struct column_traits<T, std::enable_if_t<is_integer<T>::value>> {
    static T do_integer_convert(sqlite3_stmt* ptr, int index, std::true_type) noexcept {
        return sqlite3_column_int(ptr, index);
    }

    static T do_integer_convert(sqlite3_stmt* ptr, int index, std::false_type) noexcept {
        return sqlite3_column_int64(ptr, index);
    }

    static T convert(sqlite3_stmt* ptr, int index) noexcept {
        return do_integer_convert(ptr, index, std::integral_constant<bool, (sizeof(T) < 8)>{});
    }
};

template<>
struct column_traits<const char*> {
    static const char* convert(sqlite3_stmt* ptr, int index) noexcept {
        return reinterpret_cast<const char*>(sqlite3_column_text(ptr, index));
    }
};

template<>
struct column_traits<const char16_t*> {
    static const char16_t* convert(sqlite3_stmt* ptr, int index) noexcept {
        return static_cast<const char16_t*>(sqlite3_column_text16(ptr, index));
    }
};

template<typename... Args>
struct column_traits<std::basic_string<char, Args...>> {
    using return_type = std::basic_string<char, Args...>;

    static return_type convert(sqlite3_stmt* ptr, int index) {
        return_type ret;
        int bytes = sqlite3_column_bytes(ptr, index);
        if(bytes) {
            ret.reserve(bytes);
            ret.insert(0, reinterpret_cast<const char*>(sqlite3_column_text(ptr, index)), bytes);
        }
        return ret;
    }
};

template<typename... Args>
struct column_traits<std::basic_string<char16_t, Args...>> {
    using return_type = std::basic_string<char16_t, Args...>;

    static return_type convert(sqlite3_stmt* ptr, int index) {
        return_type ret;
        int bytes = sqlite3_column_bytes16(ptr, index);
        if(bytes) {
            ret.reserve(bytes / 2);
            ret.insert(0, static_cast<const char16_t*>(sqlite3_column_text16(ptr, index)), bytes / 2);
        }
        return ret;
    }
};
} // meta
} // sqlite
