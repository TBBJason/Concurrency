#include <iostream>
#include "sessionmanager.h"

void do_session(tcp::socket socket, SessionManager& manager) {
    try {
        // creating a websocket
        auto ws = std::make_shared<websocket::stream<tcp::socket>>(std::move(socket));

        ws->accept();

        manager.add(ws);

        std::cout << "New client connected";

        beast::flat_buffer buffer;
        while (true) {
            ws->read(buffer);

            std::string message = beast::buffers_to_string(buffer.data());
            std::cout << "Receieved: " << message << std::endl;

            manager.broadcast(message);
            buffer.consume(buffer.size());
        }
    } catch (beast::system_error const& se) {
        if (se.code() != websocket::error::closed) {
            std::cerr << "Error: " << se.what() << std::endl;
        }
    } catch (std::exception const& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

int main() {
    try {
        net::io_context io_context;
        SessionManager session_manager;
        
        // Create an acceptor that listens on port 8080
        tcp::acceptor acceptor(io_context, {tcp::v4(), 8080});
        std::cout << "WebSocket server listening on port 8080..." << std::endl;
        
        while (true) {
            // Accept a new connection
            tcp::socket socket(io_context);
            acceptor.accept(socket);
            
            // Handle the connection in a separate thread
            std::thread(
                do_session, 
                std::move(socket), 
                std::ref(session_manager)
            ).detach();
        }
    } catch (std::exception const& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
