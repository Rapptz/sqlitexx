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
#include <sqlitexx/statement.hpp>

#include <memory>
#include <sqlite3.h>

namespace sqlite {
struct transaction {
    template<typename Connection>
    transaction(const Connection& con): _commit(con.prepare("COMMIT;")), _rollback(con.prepare("ROLLBACK;")) {
        con.execute("BEGIN TRANSACTION;");
    }

    transaction(const transaction&) = delete;
    transaction& operator=(const transaction&) = delete;

    transaction(transaction&& o) noexcept:
        _commit(std::move(o._commit)),
        _rollback(std::move(o._rollback)),
        needs_rollback(o.needs_rollback) {
        o.needs_rollback = false;
    }

    transaction& operator=(transaction&& o) noexcept {
        needs_rollback = o.needs_rollback;
        _commit = std::move(o._commit);
        _rollback = std::move(o._rollback);
        o.needs_rollback = false;
        return *this;
    }

    ~transaction() noexcept(false) {
        if(needs_rollback) {
            rollback();
        }
    }

    void commit() {
        if(needs_rollback) {
            _commit.execute();
            needs_rollback = false;
        }
    }

    void rollback() {
        if(needs_rollback) {
            _rollback.execute();
            needs_rollback = false;
        }
    }
private:
    statement _commit;
    statement _rollback;
    bool needs_rollback = true;
};

struct connection {
    enum open_mode : int {
        read_only     = SQLITE_OPEN_READONLY,
        read_write    = SQLITE_OPEN_READWRITE,
        create        = SQLITE_OPEN_CREATE,
        uri           = SQLITE_OPEN_URI,
        memory        = SQLITE_OPEN_MEMORY,
        no_mutex      = SQLITE_OPEN_NOMUTEX,
        full_mutex    = SQLITE_OPEN_FULLMUTEX,
        shared_cache  = SQLITE_OPEN_SHAREDCACHE,
        private_cache = SQLITE_OPEN_PRIVATECACHE
    };

    connection() noexcept = default;

    template<typename String>
    connection(const String& filename, int flags = open_mode::read_write | open_mode::uri) {
        open(filename, flags);
    }

    template<typename String>
    void open(const String& filename, int flags = open_mode::read_write | open_mode::uri) {
        auto ptr = db.get();
        int ret = sqlite3_open_v2(meta::string_traits<String>::c_str(filename), &ptr, flags, nullptr);
        if(ret != SQLITE_OK) {
            throw error(ret);
        }

        sqlite3_extended_result_codes(ptr, 1);
        db.reset(ptr);
    }

    sqlite3* data() const noexcept {
        return db.get();
    }

    bool is_open() const noexcept {
        return db != nullptr;
    }

    template<typename String>
    bool is_database_readonly(const String& name) const noexcept {
        return sqlite3_db_readonly(db.get(), meta::string_traits<String>::c_str(name));
    }

    void release_memory() const noexcept {
        sqlite3_db_release_memory(db.get());
    }

    template<typename String>
    void execute(const String& query) const {
        detail::error_string error_msg;
        int ret = sqlite3_exec(db.get(), meta::string_traits<String>::c_str(query), nullptr, nullptr, error_msg.as_ptr());
        if(ret != SQLITE_OK || error_msg.valid()) {
            throw execute_error(ret, std::move(error_msg));
        }
    }

    template<typename String>
    statement prepare(const String& sql) const {
        return { db.get(), sql };
    }

    template<typename String, typename... Args>
    auto fetch(const String& query, Args&&... args) const {
        auto stmt = prepare(query);
        stmt.bind(std::forward<Args>(args)...);
        return std::move(stmt).fetch();
    }

    template<typename String>
    auto fetch(const String& query) const {
        return prepare(query).fetch();
    }

    transaction transaction() const {
        return { *this };
    }
private:
    struct deleter {
        void operator()(sqlite3* db) const noexcept {
            sqlite3_close(db);
        }
    };

    std::unique_ptr<sqlite3, deleter> db;
};
} // sqlite
