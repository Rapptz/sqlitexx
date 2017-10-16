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
#include <sqlitexx/error.hpp>
#include <sqlite3.h>
#include <iterator>
#include <memory>

namespace sqlite {
struct blob {
    const unsigned char* data;
    int length;
};

template<typename String, typename T>
struct named_parameter {
    template<typename X, typename = std::enable_if_t<std::is_convertible<T, X>::value>>
    named_parameter(const String& name, X&& value): _name(name), value(std::forward<X>(value)) {}

    T get() const& noexcept {
        return std::move(value);
    }

    const String& name() const noexcept {
        return _name;
    }
private:
    const String& _name;
    T value;
};

template<typename>
struct is_named_parameter : std::false_type {};

template<typename T, typename U>
struct is_named_parameter<named_parameter<T, U>> : std::true_type {};

template<typename String, typename T>
inline named_parameter<String, T> named(const String& name, T&& value) {
    return { name, std::forward<T>(value) };
}

namespace meta {
template<>
struct column_traits<blob> {
    static blob convert(sqlite3_stmt* ptr, int index) noexcept {
        return {
            static_cast<const unsigned char*>(sqlite3_column_blob(ptr, index)),
            sqlite3_column_bytes(ptr, index)
        };
    }
};
} // meta

struct column {
    column(sqlite3_stmt* ptr) noexcept: ptr(ptr) {}

    sqlite3_stmt* data() const noexcept { return ptr; }

    const char* name(int index) const noexcept {
        return sqlite3_column_name(ptr, index);
    }

    int count() const noexcept {
        return sqlite3_column_count(ptr);
    }

    template<typename T>
    T get(int index) const {
        return meta::column_traits<meta::unqualified_t<T>>::convert(ptr, index);
    }
private:
    sqlite3_stmt* ptr;
};

namespace detail {
struct end_tag {};

struct statement_iterator {
    using difference_type = std::ptrdiff_t;
    using value_type = column;
    using reference = value_type&;
    using pointer = value_type*;
    using iterator_category = std::input_iterator_tag;

    statement_iterator(sqlite3_stmt* ptr): _current_column(ptr) {
        advance();
    }

    statement_iterator(end_tag, sqlite3_stmt* ptr) noexcept: _current_column(ptr), ret(SQLITE_DONE) {}

    bool operator==(const statement_iterator& other) const noexcept {
        return data() == other.data() && ret == other.ret;
    }

    bool operator!=(const statement_iterator& other) const noexcept {
        return !(*this == other);
    }

    statement_iterator& operator++() {
        advance();
        return *this;
    }

    statement_iterator operator++(int) {
        auto copy = *this;
        advance();
        return copy;
    }

    value_type operator*() const noexcept {
        return _current_column;
    }

    const value_type* operator->() const noexcept {
        return &_current_column;
    }

    value_type operator*() noexcept {
        return _current_column;
    }

    pointer operator->() noexcept {
        return &_current_column;
    }
private:
    value_type _current_column;
    int ret = SQLITE_OK;

    sqlite3_stmt* data() const noexcept {
        return _current_column.data();
    }

    void advance() {
        if(ret != SQLITE_DONE) {
            ret = sqlite3_step(data());
            if(ret != SQLITE_ROW && ret != SQLITE_DONE) {
                // bad attempt to stream so
                throw error(ret);
            }
        }
    }
};

template<typename Pointer>
struct statement_range {
    using iterator = statement_iterator;
    Pointer _ptr;

    iterator begin() const {
        reset();
        return { _ptr.get() };
    }

    iterator end() const {
        return { end_tag{}, _ptr.get() };
    }

    void reset() const {
        int ret = sqlite3_reset(_ptr.get());
        if(ret != SQLITE_OK) {
            throw error(ret);
        }
    }
};
} // detail

struct statement {
    template<typename T>
    void bind_to(int index, T&& value) {
        int ret = meta::bind_traits<meta::unqualified_t<T>>::bind(_ptr.get(), index, std::forward<T>(value));
        if(ret != SQLITE_OK) {
            throw error(ret);
        }
    }

    template<typename String, typename T>
    void bind_to(const String& name, T&& value) {
        int index = sqlite3_bind_parameter_index(_ptr.get(), meta::string_traits<String>::c_str(name));
        if(index == 0) {
            // not found so bail
            return;
        }

        bind_to(index, std::forward<T>(value));
    }

    template<typename... Args>
    void bind(Args&&... args) {
        bind_impl(meta::and_<is_named_parameter<meta::unqualified_t<Args>>...>{}, std::forward<Args>(args)...);
    }

    int count() const noexcept {
        return sqlite3_bind_parameter_count(_ptr.get());
    }

    void clear_bindings() const noexcept {
        sqlite3_clear_bindings(_ptr.get());
    }

    void reset() const {
        int ret = sqlite3_reset(_ptr.get());
        if(ret != SQLITE_OK) {
            throw error(ret);
        }
    }

    void execute() {
        int ret = sqlite3_step(_ptr.get());
        if(ret != SQLITE_DONE && ret != SQLITE_ROW) {
            throw error(ret);
        }
        reset();
    }

    template<typename... Args>
    void execute(Args&&... args) {
        bind(std::forward<Args>(args)...);
        int ret = sqlite3_step(_ptr.get());
        if(ret != SQLITE_DONE && ret != SQLITE_ROW) {
            throw error(ret);
        }
        reset();
    }

    auto fetch() const& {
        return detail::statement_range<const decltype(_ptr)&>{_ptr};
    }

    auto fetch() && {
        return detail::statement_range<decltype(_ptr)>{std::move(_ptr)};
    }
private:
    friend struct connection;

    template<typename... Args>
    void bind_impl(std::true_type, Args&&... args) {
        using dummy = int[];
        (void)dummy{ (bind_to(std::forward<Args>(args).name(), std::forward<Args>(args).get()), 0)... };
    }

    template<typename... Args>
    void bind_impl(std::false_type, Args&&... args) {
        bind_parameters(std::index_sequence_for<Args...>{}, std::forward<Args>(args)...);
    }

    template<typename String>
    statement(sqlite3* db, const String& statement) {
        using Traits = meta::string_traits<String>;
        auto ptr = _ptr.get();
        int ret = sqlite3_prepare_v2(db, Traits::c_str(statement), Traits::size(statement), &ptr, nullptr);
        if(ret != SQLITE_OK) {
            throw error(ret);
        }

        _ptr.reset(ptr);
    }

    struct deleter {
        void operator()(sqlite3_stmt* ptr) const noexcept {
            sqlite3_finalize(ptr);
        }
    };

    template<typename... Args, size_t... I>
    void bind_parameters(std::index_sequence<I...>, Args&&... args) {
        using dummy = int[]; // expand helper
        (void)dummy{ (bind_to(static_cast<int>(I + 1), std::forward<Args>(args)), 0)... };
    }

    std::unique_ptr<sqlite3_stmt, deleter> _ptr;
};
} // sqlite
