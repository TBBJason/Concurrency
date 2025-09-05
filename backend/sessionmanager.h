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
    void add(ws_ptr ws);
    void remove(ws_ptr ws);

    void set_username(ws_ptr ws, const std::string& username);
    void set_room(ws_ptr, const std::string& room);

    std::vector<std::string> list_users(const std::string& room);
    void broadcast(const std::string& room, const json& message);
    bool send_to_user(const std::string& username, const json& message);

private:
    // std::set<std::shared_ptr<websocket::stream<tcp::socket>>> sessions_;
    std::mutex mutex_;
    std::vector<ws_ptr> sockets_;
    std::unordered_map<ws_ptr, std::string> username_of_;
    std::unordered_map<ws_ptr, std::string> room_of_;

    void safe_send(ws_ptr ws, const json& message);
};

#endif