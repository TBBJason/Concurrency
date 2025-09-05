#include "sessionmanager.h"
#include <iostream>

void SessionManager::add(ws_ptr ws) {
    std::lock_guard<std::mutex> lk(mtx_);
    sockets_.push_back(ws);
}

void SessionManager::remove(ws_ptr ws) {
    std::lock_guard<std::mutex> lk(mtx_);
    sockets_.erase(std::remove(sockets_.begin(), sockets_.end(), ws), sockets_.end());
    username_of_.erase(ws);
    room_of_.erase(ws);
}

void SessionManager::set_username(ws_ptr ws, const std::string& username) {
    std::lock_guard<std::mutex> lk(mtx_);
    username_of_[ws] = username;
}

void SessionManager::set_room(ws_ptr ws, const std::string& room) {
    std::lock_guard<std::mutex> lk(mtx_);
    room_of_[ws] = room;
}

std::vector<std::string> SessionManager::list_users(const std::string& room) {
    std::lock_guard<std::mutex> lk(mtx_);
    std::vector<std::string> users;
    for (auto &ws : sockets_) {
        auto it = room_of_.find(ws);
        if (it != room_of_.end() && it->second == room) {
            auto un = username_of_.find(ws);
            if (un != username_of_.end()) users.push_back(un->second);
        }
    }
    return users;
}

void SessionManager::safe_send(ws_ptr ws, const json& message) {
    try {
        if (!ws) return;
        ws->text(true);
        auto s = message.dump();
        ws->write(net::buffer(s));
    } catch (const std::exception& e) {
        std::cerr << "safe_spend error: " << e.what() << std::endl;
    }
}

void SessionManager::broadcast(const std::string &room, const json& message) {
    std::lock_guard<std::mutex> lk(mtx_);
    for (auto &ws : sockets_) {
        auto it = room_of_.find(ws);
        if (it != room_of_.end() && it->second == room) {
            safe_send(ws, message);
        }
    }
}


bool SessionManager::send_to_user(const std::string& username, const json& message) {
    std::lock_guard<std::mutex> lk(mtx_);
    for (auto &ws : sockets_) {
        auto un = username_of_.find(ws);
        if (un != username_of_.end() && un->second == username) {
            safe_send(ws, message);
            return true;
        }
    }
    return false;
}

