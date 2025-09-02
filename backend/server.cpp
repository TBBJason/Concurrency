#include <iostream>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/websocket.hpp>
#include <vector>
#include <memory>
#include <mutex>
#include <set>

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace asio = boost::asio;
using tcp = asio::ip::tcp;


class SessionManager { 
public:
    void add(std::shared_ptr<websocket::stream<tcp::socket>> session) {
        std::lock_guard<std::mutex> lock(mutex_);
        sessions_.insert(session);
    }    
private:
    std::set<std::shared_ptr<websocket::stream<tcp::socket>>> sessions_;
    std::mutex mutex_;
};


int main() {
    std::cout<< "everything compiled okay";
    return 0;
}
