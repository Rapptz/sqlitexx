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

#include <sqlitexx/type_traits.hpp>
#include <tuple>

namespace sqlite {
template<typename... Args>
struct column {
    column(sqlite3_stmt* ptr) noexcept: ptr(ptr) {}

    sqlite3_stmt* data() const noexcept { return ptr; }

    const char* name(int index) const noexcept {
        return sqlite3_column_name(ptr, index);
    }

    int count() const noexcept {
        return sqlite3_column_count(ptr);
    }

    template<size_t N>
    auto get() const {
        static_assert(N < sizeof...(Args), "Out of bounds");

        using T = std::tuple_element_t<N, std::tuple<Args...>>;
        return meta::column_traits<meta::unqualified_t<T>>::convert(ptr, N);
    }

    template<size_t N>
    auto operator[](std::integral_constant<size_t, N>) const {
        return get<N>();
    }
private:
    sqlite3_stmt* ptr;
};

template<size_t N, typename... Args>
inline auto get(const column<Args...>& c) {
    return c.template get<N>();
}
} // sqlite

namespace std {
template<typename... Args>
struct tuple_size<::sqlite::column<Args...>> : std::integral_constant<size_t, sizeof...(Args)> {};

template<size_t N, typename... Args>
struct tuple_element<N, ::sqlite::column<Args...>> {
    using type = decltype(std::declval<::sqlite::column<Args...>>().template get<N>());
};
} // std
