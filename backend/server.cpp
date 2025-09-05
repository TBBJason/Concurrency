// server.cpp
#include <iostream>
#include <string>
#include <memory>
#include <thread>
#include <filesystem>
#include <chrono>

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/http/file_body.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio.hpp>

#include <nlohmann/json.hpp>

#include "sessionmanager.h"
#include "database.h" // your Database header

namespace beast = boost::beast;
namespace http  = beast::http;
namespace websocket = beast::websocket;
namespace net   = boost::asio;
using tcp = net::ip::tcp;
using json = nlohmann::json;
namespace fs = std::filesystem;

using ws_ptr = std::shared_ptr<websocket::stream<tcp::socket>>;

// Helper to get epoch ms
inline long long now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
           std::chrono::system_clock::now().time_since_epoch()).count();
}

// convert beast string_view target => std::string
static std::string sv_to_string(const beast::string_view& sv) {
    return std::string(sv.data(), sv.size());
}

// serve static file like earlier example (returns true if handled)
static bool serve_static_or_fallback(beast::tcp_stream &stream,
                                    const http::request<http::string_body>& req,
                                    const std::string& root_dir = "/app/static",
                                    bool spa_fallback = true)
{
    try {
        std::string target = sv_to_string(req.target());
        if (target.empty() || target == "/") target = "/index.html";
        auto qpos = target.find('?');
        if (qpos != std::string::npos) target = target.substr(0, qpos);

        fs::path full = fs::path(root_dir) / fs::path(target).relative_path();
        full = full.lexically_normal();

        auto root_norm = fs::path(root_dir).lexically_normal().string();
        auto full_str = full.string();
        if (full_str.rfind(root_norm, 0) != 0) {
            http::response<http::string_body> forbidden{http::status::forbidden, req.version()};
            forbidden.set(http::field::content_type, "text/plain");
            forbidden.body() = "Forbidden";
            forbidden.prepare_payload();
            http::write(stream.socket(), forbidden);
            return true;
        }

        if (!fs::exists(full) || !fs::is_regular_file(full)) {
            if (spa_fallback) {
                fs::path idx = fs::path(root_dir) / "index.html";
                if (fs::exists(idx) && fs::is_regular_file(idx)) full = idx;
                else {
                    http::response<http::string_body> nf{http::status::not_found, req.version()};
                    nf.set(http::field::content_type, "text/plain");
                    nf.body() = "Not found";
                    nf.prepare_payload();
                    http::write(stream.socket(), nf);
                    return true;
                }
            } else {
                http::response<http::string_body> nf{http::status::not_found, req.version()};
                nf.set(http::field::content_type, "text/plain");
                nf.body() = "Not found";
                nf.prepare_payload();
                http::write(stream.socket(), nf);
                return true;
            }
        }

        beast::error_code ec;
        http::file_body::value_type body;
        body.open(full.string().c_str(), beast::file_mode::scan, ec);
        if (ec) {
            http::response<http::string_body> err{http::status::internal_server_error, req.version()};
            err.set(http::field::content_type, "text/plain");
            err.body() = std::string("File open error: ") + ec.message();
            err.prepare_payload();
            http::write(stream.socket(), err);
            return true;
        }

        auto const size = body.size();
        http::response<http::file_body> res{
            std::piecewise_construct,
            std::make_tuple(std::move(body)),
            std::make_tuple(http::status::ok, req.version())
        };
        res.set(http::field::server, "concurrency-server");
        // quick mime mapping
        auto ext = full.extension().string();
        if(ext == ".html") res.set(http::field::content_type, "text/html");
        else if(ext == ".js") res.set(http::field::content_type, "application/javascript");
        else if(ext == ".css") res.set(http::field::content_type, "text/css");
        else res.set(http::field::content_type, "application/octet-stream");
        res.content_length(size);
        http::write(stream.socket(), res);
        return true;
    } catch (std::exception const& e) {
        beast::error_code ec;
        http::response<http::string_body> err{http::status::internal_server_error, req.version()};
        err.set(http::field::content_type, "text/plain");
        err.body() = std::string("Server error: ") + e.what();
        err.prepare_payload();
        http::write(stream.socket(), err, ec);
        return true;
    }
}


void handle_connection(tcp::socket socket, SessionManager& manager, Database& db) {
    try {
        beast::flat_buffer buffer;
        // Use tcp_stream wrapper because http and file_body want a stream
        beast::tcp_stream stream(std::move(socket));

        http::request<http::string_body> req;
        http::read(stream, buffer, req);

        std::cerr << "Incoming request: " << sv_to_string(req.method_string())
                  << " " << sv_to_string(req.target()) << "\n";
        for (auto const& field : req) {
            std::string name(field.name_string().data(), field.name_string().size());
            std::cerr << name << ": " << field.value() << "\n";
        }

        // If not a websocket upgrade -> serve static
        if (!websocket::is_upgrade(req)) {
            serve_static_or_fallback(stream, req, "/app/static", true);
            return; // close connection after serving
        }

        // Websocket path: create ws from underlying socket
        auto ws = std::make_shared<websocket::stream<tcp::socket>>(stream.release_socket());

        // Accept handshake
        ws->accept(req);

        manager.add(ws);

        std::string username;
        std::string room = "lobby";

        // Message loop
        beast::flat_buffer read_buf;
        for (;;) {
            ws->read(read_buf);
            std::string raw = beast::buffers_to_string(read_buf.data());
            read_buf.consume(read_buf.size());

            json j;
            try { j = json::parse(raw); }
            catch (std::exception& e) {
                std::cerr << "JSON parse error: " << e.what() << "\n";
                continue;
            }

            std::string type = j.value("type", "");
            if (type == "join") {
                username = j.value("username", std::string("guest"));
                room = j.value("room", std::string("lobby"));

                manager.set_username(ws, username);
                manager.set_room(ws, room);

                // Fetch recent messages from DB and send joined payload
                auto recent = db.get_recent_messages(room, 50);
                json recent_json = json::array();
                for (auto &m : recent) {
                    recent_json.push_back({
                        {"username", m.username},
                        {"text", m.text},
                        {"ts", m.ts}
                    });
                }

                json joined = {
                    {"type", "joined"},
                    {"username", username},
                    {"room", room},
                    {"recent", recent_json}
                };
                ws->text(true);
                ws->write(net::buffer(joined.dump()));

                // broadcast presence to room
                json pres = { {"type","presence"}, {"users", manager.list_users(room)} };
                manager.broadcast(room, pres);

            } else if (type == "message") {
                std::string text = j.value("text", "");
                if (text.empty() || username.empty()) continue;

                long long ts = now_ms();
                db.insert_message(room, username, text, ts);

                json out = {
                    {"type", "message"},
                    {"username", username},
                    {"room", room},
                    {"text", text},
                    {"ts", ts}
                };
                manager.broadcast(room, out);

            } else if (type == "private") {
                std::string to = j.value("to", "");
                std::string text = j.value("text", "");
                if (to.empty() || text.empty() || username.empty()) continue;

                json out = {
                    {"type", "private"},
                    {"username", username},
                    {"text", text},
                    {"ts", now_ms()}
                };
                // send to recipient and echo to sender
                manager.send_to_user(to, out);
                manager.send_to_user(username, out);

            } else if (type == "list") {
                json out = { {"type","list"}, {"users", manager.list_users(room)} };
                ws->text(true);
                ws->write(net::buffer(out.dump()));
            }
        }
    } catch (beast::system_error const& se) {
        std::cerr << "Beast error: " << se.what() << "\n";
    } catch (std::exception const& e) {
        std::cerr << "Conn exception: " << e.what() << "\n";
    }
}

int main() {
    try {
        net::io_context ioc{1};
        SessionManager manager;
        Database db("messages.db");
        if (!db.open()) {
            std::cerr << "DB open failed\n";
            return 1;
        }

        int port = [] {
            const char* p = std::getenv("PORT");
            return p ? std::atoi(p) : 8080;
        }();

        tcp::acceptor acceptor(ioc, {tcp::v4(), static_cast<unsigned short>(port)});
        std::cout << "Listening on port " << port << "\n";

        while (true) {
            tcp::socket socket(ioc);
            acceptor.accept(socket);
            std::thread([sock = std::move(socket), &manager, &db]() mutable {
                handle_connection(std::move(sock), manager, db);
            }).detach();
        }

    } catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
