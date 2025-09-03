#include "database.h"
#include <iostream>

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