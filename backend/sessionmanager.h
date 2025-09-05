#ifndef SESSIONMANAGER_H
#define SESSIONMANAGER_H

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif

#include <iostream>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/websocket.hpp>
#include <vector>
#include <memory>
#include <mutex>
#include <thread>
#include <set>
#include <unordered_set>
#include <iostream>
#include <nlohmann/json.hpp>


namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace asio = boost::asio;
namespace net = boost::asio;
using tcp = asio::ip::tcp;

using json = nlohmann::json;
using ws_ptr = std::shared_ptr<websocket::stream<tcp::socket>>;

class SessionManager {
public:
    SessionManager() = default;
    ~SessionManager() = default;

    void add(ws_ptr ws, const std::string& username, const std::string& room);
    void remove(ws_ptr ws);
    void set_username(ws_ptr ws, const std::string& username);
    void set_room(ws_ptr ws, const std::string& room);
    std::vector<std::string> list_users(const std::string& room);
    void broadcast(const std::string& room, const std::string& message);
    void send_to_user(const std::string& username, const std::string& message);

private:
    struct SessionInfo {
        ws_ptr ws;
        std::string username;
        std::string room;
    };

    std::mutex mtx_;
    // map ws.get() pointer address string (or use ws_ptr) -> info
    std::unordered_map<void*, SessionInfo> sessions_;
    // room -> set of ws pointers (raw keys into sessions_)
    std::unordered_map<std::string, std::unordered_set<void*>> rooms_;
    // username -> ws pointer (one-to-one in this simple model)
    std::unordered_map<std::string, void*> by_username_;
};

#endif