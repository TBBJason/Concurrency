#include "sessionmanager.h"


void SessionManager::add(std::shared_ptr<websocket::stream<tcp::socket>> session) {
    std::lock_guard<std::mutex> lock(mutex_);
    sessions_.insert(session);
}    

void SessionManager::remove(std::shared_ptr<websocket::stream<tcp::socket>> session) {
    std::lock_guard<std::mutex> lock(mutex_);
    sessions_.erase(session);
}

void SessionManager::broadcast(const std::string &message) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& session : sessions_) {
        session->write(net::buffer(message));
    }
}


