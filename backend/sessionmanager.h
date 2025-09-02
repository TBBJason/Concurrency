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


namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace asio = boost::asio;
namespace net = boost::asio;
using tcp = asio::ip::tcp;


class SessionManager {
public:    
    void add(std::shared_ptr<websocket::stream<tcp::socket>> session);
    void remove(std::shared_ptr<websocket::stream<tcp::socket>> session);
    void broadcast(const std::string& message);

private:
    std::set<std::shared_ptr<websocket::stream<tcp::socket>>> sessions_;
    std::mutex mutex_;
};

#endif