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

#include <exception>
#include <utility>
#include <sqlite3.h>

namespace sqlite {
struct error : std::exception {
    error(int ec) noexcept: ec(ec) {}

    const char* what() const noexcept override {
        return sqlite3_errstr(ec);
    }

    int code() const noexcept {
        return ec;
    }
private:
    int ec;
};

namespace detail {
struct error_string {
    error_string() noexcept = default;
    error_string(const error_string&) = delete;
    error_string& operator=(const error_string&) = delete;

    error_string(error_string&& o) noexcept: str(o.str) {
        o.str = nullptr;
    }

    error_string& operator=(error_string&& o) noexcept {
        str = o.str;
        o.str = nullptr;
        return *this;
    }

    ~error_string() {
        sqlite3_free(str);
    }

    explicit operator bool() const noexcept {
        return valid();
    }

    bool valid() const noexcept { return str != nullptr; }
    char* data() const noexcept { return str; }
    char** as_ptr() noexcept { return &str; }
private:
    char* str = nullptr;
};
} // detail

struct execute_error : error {
    execute_error(int ec, detail::error_string msg) noexcept: error(ec), msg(std::move(msg)) {}

    const char* message() const noexcept {
        return msg.data();
    }

    bool has_message() const noexcept {
        return msg.valid();
    }
private:
    detail::error_string msg;
};
} // sqlite
