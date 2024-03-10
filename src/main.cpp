#include <thread>

#include "WsServer.hpp"
#include "HttpServer.hpp"

int main() {
    try {
        WsServer ws_server;
        HttpServer http_server;

        std::thread ws_process_th(std::bind(&WsServer::process_message, &ws_server));
        std::thread http_th(std::bind(&HttpServer::run, &http_server, 1146));

        ws_server.run(1145);

        http_th.join();
        ws_process_th.join();
    }
    catch (const std::exception& e) {
        std::cout << e.what() << std::endl;
    }
}
