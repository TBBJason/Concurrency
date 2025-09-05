// sessionmanager.cpp (simple, robust)
#include "sessionmanager.h"
#include <iostream>
#include <nlohmann/json.hpp>

void SessionManager::add(ws_ptr ws, const std::string& username, const std::string& room) {
    std::lock_guard<std::mutex> lock(mtx_);
    void* key = ws.get();
    SessionInfo info{ws, username, room};
    sessions_[key] = info;
    rooms_[room].insert(key);
    by_username_[username] = key;
    std::cerr << "SessionManager::add user=" << username << " room=" << room << " ws=" << key << "\n";
}

void SessionManager::remove(ws_ptr ws) {
    std::lock_guard<std::mutex> lock(mtx_);
    void* key = ws.get();
    auto it = sessions_.find(key);
    if (it == sessions_.end()) return;
    std::string room = it->second.room;
    std::string username = it->second.username;
    sessions_.erase(it);
    rooms_[room].erase(key);
    if (by_username_[username] == key) by_username_.erase(username);
    std::cerr << "SessionManager::remove user=" << username << " room=" << room << " ws=" << key << "\n";
}

void SessionManager::set_username(ws_ptr ws, const std::string& username) {
    std::lock_guard<std::mutex> lock(mtx_);
    void* key = ws.get();
    auto it = sessions_.find(key);
    if (it != sessions_.end()) {
        it->second.username = username;
        by_username_[username] = key;
    }
}

void SessionManager::set_room(ws_ptr ws, const std::string& room) {
    std::lock_guard<std::mutex> lock(mtx_);
    void* key = ws.get();
    auto it = sessions_.find(key);
    if (it != sessions_.end()) {
        std::string old = it->second.room;
        if (!old.empty()) rooms_[old].erase(key);
        it->second.room = room;
        rooms_[room].insert(key);
    }
}

std::vector<std::string> SessionManager::list_users(const std::string& room) {
    std::lock_guard<std::mutex> lock(mtx_);
    std::vector<std::string> out;
    auto rit = rooms_.find(room);
    if (rit == rooms_.end()) return out;
    for (void* key : rit->second) {
        auto sit = sessions_.find(key);
        if (sit != sessions_.end()) out.push_back(sit->second.username);
    }
    return out;
}

void SessionManager::broadcast(const std::string& room, const std::string& message) {
    std::vector<ws_ptr> targets;
    {
        std::lock_guard<std::mutex> lock(mtx_);
        auto it = rooms_.find(room);
        if (it == rooms_.end()) return;
        for (void* key : it->second) {
            auto sit = sessions_.find(key);
            if (sit != sessions_.end()) targets.push_back(sit->second.ws);
        }
    }

    // write outside lock; catch errors per-socket
    for (auto &wsp : targets) {
        try {
            wsp->text(true);
            wsp->write(boost::asio::buffer(message));
        } catch (const std::exception& e) {
            std::cerr << "broadcast write error: " << e.what() << "\n";
            // optionally remove this session asynchronously
        }
    }
}

void SessionManager::send_to_user(const std::string& username, const std::string& message) {
    ws_ptr target;
    {
        std::lock_guard<std::mutex> lock(mtx_);
        auto it = by_username_.find(username);
        if (it == by_username_.end()) return;
        void* key = it->second;
        auto sit = sessions_.find(key);
        if (sit != sessions_.end()) target = sit->second.ws;
    }
    if (target) {
        try {
            target->text(true);
            target->write(boost::asio::buffer(message));
        } catch (const std::exception& e) {
            std::cerr << "send_to_user error: " << e.what() << "\n";
        }
    }
}
