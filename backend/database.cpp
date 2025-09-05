// database.cpp
#include "database.h"

#include <iostream>
#include <chrono>
#include <algorithm>

static long long now_ms_ll() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

Database::Database(const std::string& path) : path_(path), db_(nullptr) {}
Database::~Database() {
    close();
}

bool Database::open() {
    std::lock_guard<std::mutex> lock(mtx_);
    if (db_) return true;

    // open as usual after requesting SQLITE_CONFIG_SERIALIZED in main()
    int rc = sqlite3_open_v2(path_.c_str(), &db_,
                             SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
                             nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to open DB (" << path_ << "): " << sqlite3_errmsg(db_) << std::endl;
        if (db_) {
            sqlite3_close(db_);
            db_ = nullptr;
        }
        return false;
    }

    // set polite pragmas
    char* errmsg = nullptr;
    rc = sqlite3_exec(db_, "PRAGMA journal_mode=WAL;", nullptr, nullptr, &errmsg);
    if (rc != SQLITE_OK) {
        std::cerr << "PRAGMA journal_mode=WAL failed: " << (errmsg ? errmsg : "unknown") << std::endl;
        sqlite3_free(errmsg);
    }
    rc = sqlite3_exec(db_, "PRAGMA synchronous=NORMAL;", nullptr, nullptr, &errmsg);
    if (rc != SQLITE_OK) {
        std::cerr << "PRAGMA synchronous failed: " << (errmsg ? errmsg : "unknown") << std::endl;
        sqlite3_free(errmsg);
    }

    sqlite3_busy_timeout(db_, 2000);
    return ensure_table();
}
bool Database::close() {
    std::lock_guard<std::mutex> lock(mtx_);
    if (!db_) return true;

    int rc = sqlite3_close_v2(db_);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to close DB: " << sqlite3_errmsg(db_) << std::endl;
        // clear pointer to avoid further use
        db_ = nullptr;
        return false;
    }
    db_ = nullptr;
    return true;
}

bool Database::ensure_table() {
    // std::lock_guard<std::mutex> lock(mtx_);
    if (!db_) return false;

    static const char* sql =
        "CREATE TABLE IF NOT EXISTS messages ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "room TEXT NOT NULL,"
        "username TEXT NOT NULL,"
        "text TEXT NOT NULL,"
        "ts INTEGER NOT NULL"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_messages_room_ts ON messages (room, ts DESC);";

    char* errmsg = nullptr;
    int rc = sqlite3_exec(db_, sql, nullptr, nullptr, &errmsg);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to create messages table/index: " << (errmsg ? errmsg : "unknown") << std::endl;
        sqlite3_free(errmsg);
        return false;
    }
    return true;
}

void Database::insert_message(const std::string& room,
                              const std::string& username,
                              const std::string& text,
                              long long ts) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (!db_) {
        std::cerr << "DB not open in insert_message\n";
        return;
    }

    const char* sql = "INSERT INTO messages (room, username, text, ts) VALUES (?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "insert prepare failed: " << sqlite3_errmsg(db_) << std::endl;
        if (stmt) sqlite3_finalize(stmt);
        return;
    }

    rc = sqlite3_bind_text(stmt, 1, room.c_str(), -1, SQLITE_TRANSIENT);
    if (rc != SQLITE_OK) {
        std::cerr << "bind room failed: " << sqlite3_errmsg(db_) << std::endl;
        sqlite3_finalize(stmt);
        return;
    }

    rc = sqlite3_bind_text(stmt, 2, username.c_str(), -1, SQLITE_TRANSIENT);
    if (rc != SQLITE_OK) {
        std::cerr << "bind username failed: " << sqlite3_errmsg(db_) << std::endl;
        sqlite3_finalize(stmt);
        return;
    }

    rc = sqlite3_bind_text(stmt, 3, text.c_str(), -1, SQLITE_TRANSIENT);
    if (rc != SQLITE_OK) {
        std::cerr << "bind text failed: " << sqlite3_errmsg(db_) << std::endl;
        sqlite3_finalize(stmt);
        return;
    }

    rc = sqlite3_bind_int64(stmt, 4, static_cast<sqlite3_int64>(ts));
    if (rc != SQLITE_OK) {
        std::cerr << "bind ts failed: " << sqlite3_errmsg(db_) << std::endl;
        sqlite3_finalize(stmt);
        return;
    }

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        std::cerr << "insert step failed: " << sqlite3_errmsg(db_) << std::endl;
    }

    sqlite3_finalize(stmt);
}

std::vector<ChatMessage> Database::get_recent_messages(const std::string &room, int limit) {
    std::lock_guard<std::mutex> lock(mtx_);
    std::vector<ChatMessage> out;
    if (!db_) return out;

    const char* sql =
        "SELECT username, text, ts, room "
        "FROM messages "
        "WHERE room = ? "
        "ORDER BY ts DESC "
        "LIMIT ?;";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "get_recent prepare failed: " << sqlite3_errmsg(db_) << std::endl;
        return out;
    }

    rc = sqlite3_bind_text(stmt, 1, room.c_str(), -1, SQLITE_TRANSIENT);
    if (rc != SQLITE_OK) {
        std::cerr << "bind room failed: " << sqlite3_errmsg(db_) << std::endl;
        sqlite3_finalize(stmt);
        return out;
    }

    rc = sqlite3_bind_int(stmt, 2, limit);
    if (rc != SQLITE_OK) {
        std::cerr << "bind limit failed: " << sqlite3_errmsg(db_) << std::endl;
        sqlite3_finalize(stmt);
        return out;
    }

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        ChatMessage m;
        const unsigned char* cu = sqlite3_column_text(stmt, 0);
        const unsigned char* ct = sqlite3_column_text(stmt, 1);
        sqlite3_int64 ts_col = sqlite3_column_int64(stmt, 2);
        const unsigned char* crow = sqlite3_column_text(stmt, 3);

        m.username = cu ? reinterpret_cast<const char*>(cu) : std::string();
        m.text     = ct ? reinterpret_cast<const char*>(ct) : std::string();
        m.ts       = static_cast<long long>(ts_col);
        m.room     = crow ? reinterpret_cast<const char*>(crow) : std::string();

        out.push_back(std::move(m));
    }

    if (rc != SQLITE_DONE) {
        std::cerr << "get_recent step ended with rc=" << rc << ": " << sqlite3_errmsg(db_) << std::endl;
    }

    sqlite3_finalize(stmt);

    // reverse to chronological (oldest -> newest)
    std::reverse(out.begin(), out.end());
    return out;
}
