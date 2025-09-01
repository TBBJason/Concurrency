#include <iostream>
#include <boost/asio.hpp>

int main() {
    try {
        boost::asio::io_context io_context;

        // create an acceptor that listens on port 3333 on IPv4
        boost::asio::ip::tcp::acceptor acceptor(
            io_context,
            boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), 3333)
        );

        std::cout << "Listening on port 3333...\n";

        // accept one connection (blocking)
        boost::asio::ip::tcp::socket socket(io_context);
        acceptor.accept(socket);

        std::string msg = "Hello from Boost.Asio\n";
        boost::asio::write(socket, boost::asio::buffer(msg));

    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
