#include "WsServer.hpp"

int main() {
    try {
        WsServer server_instance;

        // Start a thread to run the processing loop
        thread t(bind(&WsServer::process_messages, &server_instance));

        // Run the asio loop with the main thread
        server_instance.run(1145);

        t.join();
    }
    catch (websocketpp::exception const& e) {
        std::cout << e.what() << std::endl;
    }
}
