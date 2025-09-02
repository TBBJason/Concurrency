#include "sessionmanager.h"

class SessionManager { 
public:
    void add(std::shared_ptr<websocket::stream<tcp::socket>> session) {
        std::lock_guard<std::mutex> lock(mutex_);
        sessions_.insert(session);
    }    

    void remove(std::shared_ptr<websocket::stream<tcp::socket>> session) {
        std::lock_guard<std::mutex> lock(mutex_);
        sessions_.erase(session);
    }

    void broadcast(std::string &message) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& session : sessions_) {
            session->write(net::buffer(message));
        }
    }
private:
    std::set<std::shared_ptr<websocket::stream<tcp::socket>>> sessions_;
    std::mutex mutex_;
};
