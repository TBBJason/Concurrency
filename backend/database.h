#ifndef DATABASE_H
#define DATABASE_H

#include <string>
#include <vector>
#include <mutex>
#include <sqlite3.h>

struct ChatMessage {
    int id;
    std::string text;
    bool isOwn;
    std::string timestamp; 
};

class Database {
public:
    Database(const std::string& path);
    ~Database();


    // these commands remove the copy constructor
    Database(const Database&) = delete;
    // this line removes the assignment operator
    Database& operator=(const Database&) = delete;
    // Because of these lines, we can't assign a database the values of 
    // another database since we deleted the command that would normally handle that


    bool open();
    bool close();

    bool insert_message(const std::string& text, bool isOwn);
    std::vector<ChatMessage> get_recent_messages(int limit = 100);

private:
    bool ensure_table();

    std::string_path_;
    sqlite3*db_ = nullptr;
    std::mutex mtx_;

}

#endif
