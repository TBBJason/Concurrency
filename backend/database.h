#ifndef DATABASE_H
#define DATABASE_H

#include <string>
#include <vector>
#include <mutex>
#include <sqlite3.h>

struct ChatMessage {
    std::string username;
    std::string text;
    long long ts;
    std::string room;
};

class Database {
public:
    Database(const std::string& path);
    ~Database();


    // these commands remove the copy constructor
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;
    
    bool open();
    bool close();
    std::vector<ChatMessage> get_recent_messages(const std::string &room, int limit = 100);
    void insert_message(const std::string& room, const std::string& username, const std::string& text, long long ts);
private:
    bool ensure_table();

    std::string path_;
    sqlite3* db_ = nullptr;
    std::mutex mtx_;
};

#endif
