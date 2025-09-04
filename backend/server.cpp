// server.cpp
#include <iostream>
#include <string>
#include <memory>
#include <thread>
#include <filesystem>

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/http/file_body.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio.hpp>

#include "sessionmanager.h"
#include "database.h"

namespace beast = boost::beast;
namespace http  = beast::http;
namespace websocket = beast::websocket;
namespace net   = boost::asio;
using tcp = net::ip::tcp;
namespace fs = std::filesystem;

// simple mime type map
static std::string mime_type(const fs::path& path) {
    auto ext = path.extension().string();
    if (ext == ".htm" || ext == ".html") return "text/html";
    if (ext == ".css")  return "text/css";
    if (ext == ".js")   return "application/javascript";
    if (ext == ".json") return "application/json";
    if (ext == ".png")  return "image/png";
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".svg")  return "image/svg+xml";
    if (ext == ".txt")  return "text/plain";
    return "application/octet-stream";
}

// Serve a static file under /app/static. If not found and `spa_fallback` true,
// serve /app/static/index.html (useful for client-side routing).
static bool serve_static_or_fallback(tcp::socket &socket,
                                     const http::request<http::string_body>& req,
                                     const std::string& root_dir = "/app/static",
                                     bool spa_fallback = true)
{
    try {
        std::string target(req.target().data(), req.target().size());

        // Normalize target; don't allow .. traversal
        if (target.empty() || target == "/") target = "/index.html";

        // Remove query string if present
        auto qpos = target.find('?');
        if (qpos != std::string::npos) target = target.substr(0, qpos);

        // Ensure the path doesn't try to escape root (basic lexical normalize)
        fs::path full = fs::path(root_dir) / fs::path(target).relative_path();
        full = full.lexically_normal();

        // Basic containment check: full path must start with root_dir
        auto root_norm = fs::path(root_dir).lexically_normal().string();
        auto full_str = full.string();
        if (full_str.rfind(root_norm, 0) != 0) {
            // path traversal attempt
            http::response<http::string_body> forbidden{http::status::forbidden, req.version()};
            forbidden.set(http::field::content_type, "text/plain");
            forbidden.body() = "Forbidden";
            forbidden.prepare_payload();
            http::write(socket, forbidden);
            return true;
        }

        // If file doesn't exist and SPA fallback enabled, try index.html
        if (!fs::exists(full) || !fs::is_regular_file(full)) {
            if (spa_fallback) {
                fs::path idx = fs::path(root_dir) / "index.html";
                if (fs::exists(idx) && fs::is_regular_file(idx)) {
                    full = idx;
                } else {
                    // not found
                    http::response<http::string_body> nf{http::status::not_found, req.version()};
                    nf.set(http::field::content_type, "text/plain");
                    nf.body() = "Not found";
                    nf.prepare_payload();
                    http::write(socket, nf);
                    return true;
                }
            } else {
                http::response<http::string_body> nf{http::status::not_found, req.version()};
                nf.set(http::field::content_type, "text/plain");
                nf.body() = "Not found";
                nf.prepare_payload();
                http::write(socket, nf);
                return true;
            }
        }

        // Open file and stream it
        beast::error_code ec;
        http::file_body::value_type body;
        body.open(full.string().c_str(), beast::file_mode::scan, ec);
        if (ec) {
            http::response<http::string_body> err{http::status::internal_server_error, req.version()};
            err.set(http::field::content_type, "text/plain");
            err.body() = std::string("File open error: ") + ec.message();
            err.prepare_payload();
            http::write(socket, err);
            return true;
        }

        auto const size = body.size();

        http::response<http::file_body> res{
            std::piecewise_construct,
            std::make_tuple(std::move(body)),
            std::make_tuple(http::status::ok, req.version())
        };
        res.set(http::field::server, "concurrency-server");
        res.set(http::field::content_type, mime_type(full));
        res.content_length(size);
        http::write(socket, res);
        return true;

    } catch (std::exception const& e) {
        http::response<http::string_body> err{http::status::internal_server_error, req.version()};
        err.set(http::field::content_type, "text/plain");
        err.body() = std::string("Server error: ") + e.what();
        err.prepare_payload();
        beast::error_code ec;
        http::write(socket, err, ec);
        return true;
    }
}

// Main session handler: accepts websocket upgrades or serves static files
void do_session(tcp::socket socket, SessionManager& manager, Database& db) {
    try {
        beast::flat_buffer buffer;
        http::request<http::string_body> req;

        // Read the HTTP request from the socket
        http::read(socket, buffer, req);

        // Log the request line & headers for debugging
        std::cerr << "Incoming request: " << req.method_string() << " " << req.target() << "\n";
        for (auto const& field : req) {
            std::cerr << std::string(field.name_string()) << ": " << field.value() << "\n";
        }

        // If it's not a websocket upgrade, serve static files (or SPA)
        if (!websocket::is_upgrade(req)) {
            serve_static_or_fallback(socket, req, "/app/static", /*spa_fallback=*/true);
            // After serving HTTP, close connection (function handled response)
            return;
        }

        // It's a websocket upgrade; construct websocket stream from the socket
        auto ws = std::make_shared<websocket::stream<tcp::socket>>(std::move(socket));

        // Accept the websocket handshake (this sends the 101 response with Connection: Upgrade)
        ws->accept(req);

        manager.add(ws);
        std::cout << "New client connected\n";

        // Send recent messages (text frames)
        auto recent = db.get_recent_messages(100);
        for (auto& m : recent) {
            ws->text(true);
            ws->write(net::buffer(m.text));
        }

        // WebSocket message loop
        beast::flat_buffer read_buf;
        for (;;) {
            ws->read(read_buf);

            std::string message = beast::buffers_to_string(read_buf.data());
            std::cout << "Received: " << message << std::endl;

            db.insert_message(message, false);
            manager.broadcast(message);

            // consume what we read
            read_buf.consume(read_buf.size());
        }

    } catch (beast::system_error const& se) {
        if (se.code() != websocket::error::closed) {
            std::cerr << "Error (beast): " << se.what() << std::endl;
        } else {
            std::cerr << "Connection closed by client\n";
        }
    } catch (std::exception const& e) {
        std::cerr << "Error (std): " << e.what() << std::endl;
    }
}

int main() {
    try {
        net::io_context io_context;
        SessionManager session_manager;

        Database db("messages.db");
        if (!db.open()) {
            std::cerr << "Unable to open database; exiting now\n";
            return 1;
        }

        // Determine port from environment (Heroku provides PORT)
        int port = [] {
            const char* p = std::getenv("PORT");
            return p ? std::atoi(p) : 8080;
        }();

        // Bind acceptor to port
        tcp::acceptor acceptor(io_context, {tcp::v4(), static_cast<unsigned short>(port)});
        std::cout << "WebSocket / HTTP server listening on port " << port << "...\n";

        while (true) {
            tcp::socket socket(io_context);
            acceptor.accept(socket);

            // Handle the connection in a separate thread
            std::thread(
                [sock = std::move(socket), &session_manager, &db]() mutable {
                    do_session(std::move(sock), session_manager, db);
                }
            ).detach();
        }

    } catch (std::exception const& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
