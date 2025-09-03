#include "database.h"
#include <iostream>
#include <algorithm>

Database::Database(const std::string& path) : path_(path), db_(nullptr){}
Database::~Database() {
    close();
}

bool Database::open() {
    std::lock_guard<std::mutex> lock(mtx_);
    if (db_) return true;
    
    if (sqlite3_open(path_.c_str(), &db_) != SQLITE_OK) {
        std::cerr << "Failed to open DB: " << sqlite3_errmsg(db_) << std::endl;
        sqlite3_close(db_);
        db_= nullptr;
        return false;
    }

// Fix the typo here:
    sqlite3_exec(db_, "PRAGMA journal_mode=WAL", nullptr, nullptr, nullptr);
    sqlite3_busy_timeout(db_, 2000);

    return ensure_table(); // This returns the result of ensure_table()
}
bool Database::close() {
    std::lock_guard<std::mutex> lock(mtx_);
    if (!db_) return true;

    if (sqlite3_close(db_) != SQLITE_OK) {
        std::cerr << "Failed to close DB: " << sqlite3_errmsg(db_) << std::endl;
        db_ = nullptr;
        return false;
    }
    db_ = nullptr;
    return true;
}

bool Database::ensure_table() {
    static const char* sql =
        "CREATE TABLE IF NOT EXISTS messages ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "text TEXT NOT NULL,"
        "is_own INTEGER NOT NULL,"
        "created_at DATETIME DEFAULT CURRENT_TIMESTAMP"
        ");";
    char* errmsg = nullptr;
    int rc = sqlite3_exec(db_, sql, nullptr, nullptr, &errmsg);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to create table: " << (errmsg ? errmsg : "unknown") << std::endl;
        sqlite3_free(errmsg);
        return false;
    }
    return true;
}

bool Database::insert_message(const std::string& text, bool isOwn) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (!db_) return false;

    const char* sql = "INSERT INTO messages (text, is_own) VALUES (?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "prepare failed: " << sqlite3_errmsg(db_) << std::endl;
        return false;
    }
    sqlite3_bind_text(stmt, 1, text.c_str(), (int)text.size(), SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, isOwn ? 1 : 0);

    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        std::cerr << "insert failed: " << sqlite3_errmsg(db_) << std::endl;
        sqlite3_finalize(stmt);
        return false;
    }

    sqlite3_finalize(stmt);
    return true;
}

std::vector<ChatMessage> Database::get_recent_messages(int limit) {
    std::lock_guard<std::mutex> lock(mtx_);
    std::vector<ChatMessage> out;
    if (!db_) return out;

    const char* sql =
    "SELECT id, text, is_own, datetime(created_at) as created_at "
    "FROM messages ORDER BY id DESC LIMIT ?;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "prepare failed: " << sqlite3_errmsg(db_) << std::endl;
        return out;
    }

    sqlite3_bind_int(stmt, 1, limit);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        ChatMessage m;
        m.id = sqlite3_column_int(stmt, 0);
        const unsigned char* txt = sqlite3_column_text(stmt, 1);
        m.text = txt ? reinterpret_cast<const char*> (txt) : "";
        m.isOwn = sqlite3_column_int(stmt, 2) != 0;
        const unsigned char* ts = sqlite3_column_text(stmt, 3);
        m.timestamp = ts ? reinterpret_cast<const char*>(ts) : "";
        out.push_back(std::move(m));
    }

    sqlite3_finalize(stmt);

    std::reverse(out.begin(), out.end());
    return out;
}